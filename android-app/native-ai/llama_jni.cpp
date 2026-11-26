// JNI bridge between the app (LlamaNative.java) and llama.cpp. Runs entirely on the phone's processor.
//
// Each loaded model ("mind") keeps one context alive between turns and remembers which tokens are already
// in its KV cache. A new prompt that starts the same way as the last one (same system prompt, same
// instructions) only has to read the part that changed, so the long rules are read once, not every turn.
// An optional GBNF grammar locks the reply to the exact line format the app expects.
#include <jni.h>
#include <cmath>
#include <string>
#include <vector>
#include <android/log.h>
#include "llama.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "TwoMindsLlama", __VA_ARGS__)

struct Mind {
    llama_model *model = nullptr;
    llama_context *ctx = nullptr;
    int n_ctx = 4096, n_batch = 512, n_threads = 4;
    std::vector<llama_token> cache;   // tokens currently held in the KV cache (sequence 0)
    int last_prompt = 0, last_gen = 0, last_reused = 0;
    std::string err;
};
static bool g_backend = false;

static std::string bytes(JNIEnv *env, jbyteArray a) {
    if (!a) return std::string();
    jsize n = env->GetArrayLength(a);
    std::string s(n, '\0');
    if (n) env->GetByteArrayRegion(a, 0, n, reinterpret_cast<jbyte *>(&s[0]));
    return s;
}

static bool make_ctx(Mind *m) {
    llama_context_params cp = llama_context_default_params();
    cp.n_ctx = m->n_ctx;
    cp.n_batch = m->n_batch;
    cp.n_ubatch = m->n_batch;
    cp.n_seq_max = 1;
    cp.n_threads = m->n_threads;
    cp.n_threads_batch = m->n_threads;
    cp.swa_full = true;              // keep the whole history for sliding-window models (Gemma) so the prefix can be reused
    m->ctx = llama_init_from_model(m->model, cp);
    m->cache.clear();
    return m->ctx != nullptr;
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_swowanders_twominds_LlamaNative_load(JNIEnv *env, jclass, jstring jpath, jint nCtx, jint nThreads) {
    if (!g_backend) { llama_backend_init(); g_backend = true; }
    const char *path = env->GetStringUTFChars(jpath, nullptr);
    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    llama_model *model = llama_model_load_from_file(path, mp);
    env->ReleaseStringUTFChars(jpath, path);
    if (!model) return 0;
    Mind *m = new Mind();
    m->model = model;
    m->n_ctx = nCtx > 256 ? nCtx : 4096;
    m->n_threads = nThreads > 0 ? nThreads : 4;
    if (!make_ctx(m)) { llama_model_free(model); delete m; return 0; }
    LOGI("model loaded, ctx %d, threads %d", m->n_ctx, m->n_threads);
    return reinterpret_cast<jlong>(m);
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_swowanders_twominds_LlamaNative_free(JNIEnv *, jclass, jlong h) {
    Mind *m = reinterpret_cast<Mind *>(h);
    if (!m) return;
    if (m->ctx) llama_free(m->ctx);
    llama_model_free(m->model);
    delete m;
}

// [tokens in the last prompt, tokens written, prompt tokens reused from the cache]
extern "C" JNIEXPORT jintArray JNICALL
Java_io_github_swowanders_twominds_LlamaNative_stats(JNIEnv *env, jclass, jlong h) {
    Mind *m = reinterpret_cast<Mind *>(h);
    jint v[3] = {m ? m->last_prompt : 0, m ? m->last_gen : 0, m ? m->last_reused : 0};
    jintArray out = env->NewIntArray(3);
    env->SetIntArrayRegion(out, 0, 3, v);
    return out;
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_swowanders_twominds_LlamaNative_lastError(JNIEnv *env, jclass, jlong h) {
    Mind *m = reinterpret_cast<Mind *>(h);
    return env->NewStringUTF(m ? m->err.c_str() : "no model");
}

// Decode tokens [from, to) of toks at their positions, in batches, adding them to the cache.
static bool feed(Mind *m, const std::vector<llama_token> &toks, size_t from) {
    for (size_t i = from; i < toks.size(); i += m->n_batch) {
        int n = (int) std::min((size_t) m->n_batch, toks.size() - i);
        llama_batch b = llama_batch_get_one(const_cast<llama_token *>(toks.data() + i), n);
        if (llama_decode(m->ctx, b) != 0) return false;
        m->cache.insert(m->cache.end(), toks.begin() + i, toks.begin() + i + n);
    }
    return true;
}

static void fail(Mind *m, const std::string &e) { m->err = e; LOGI("%s", e.c_str()); }

// Chat completion: applies the model's own chat template, reuses the cached prefix, then samples a reply.
// grammar: optional GBNF text (root rule "root"); null or empty means free text.
extern "C" JNIEXPORT jbyteArray JNICALL
Java_io_github_swowanders_twominds_LlamaNative_complete(JNIEnv *env, jclass, jlong h, jobjectArray roles, jobjectArray contents,
                                                       jint maxTokens, jfloat temp, jfloat topP, jbyteArray jgrammar) {
    Mind *m = reinterpret_cast<Mind *>(h);
    if (!m || !m->ctx) return nullptr;
    m->err.clear();
    const int n = env->GetArrayLength(roles);
    std::vector<std::string> r(n), c(n);
    for (int i = 0; i < n; i++) {
        jbyteArray a = (jbyteArray) env->GetObjectArrayElement(roles, i), b = (jbyteArray) env->GetObjectArrayElement(contents, i);
        r[i] = bytes(env, a); c[i] = bytes(env, b);
        env->DeleteLocalRef(a); env->DeleteLocalRef(b);
    }
    std::vector<llama_chat_message> msgs(n);
    for (int i = 0; i < n; i++) msgs[i] = {r[i].c_str(), c[i].c_str()};

    std::string prompt;
    const char *tmpl = llama_model_chat_template(m->model, nullptr);
    std::vector<char> buf(32768);
    int len = tmpl ? llama_chat_apply_template(tmpl, msgs.data(), msgs.size(), true, buf.data(), (int) buf.size()) : -1;
    if (len > (int) buf.size()) { buf.resize(len + 1); len = llama_chat_apply_template(tmpl, msgs.data(), msgs.size(), true, buf.data(), (int) buf.size()); }
    if (len > 0) prompt.assign(buf.data(), len);
    else { // ChatML fallback (used by SmolLM2 and Qwen)
        for (int i = 0; i < n; i++) prompt += "<|im_start|>" + r[i] + "\n" + c[i] + "<|im_end|>\n";
        prompt += "<|im_start|>assistant\n";
    }

    const llama_vocab *vocab = llama_model_get_vocab(m->model);
    int nTok = -llama_tokenize(vocab, prompt.c_str(), (int) prompt.size(), nullptr, 0, true, true);
    if (nTok <= 0) { fail(m, "could not read the prompt"); return nullptr; }
    std::vector<llama_token> toks(nTok);
    if (llama_tokenize(vocab, prompt.c_str(), (int) prompt.size(), toks.data(), (int) toks.size(), true, true) < 0) { fail(m, "could not read the prompt"); return nullptr; }
    if (nTok + maxTokens + 4 > m->n_ctx) { fail(m, "prompt too long: " + std::to_string(nTok) + " tokens"); return nullptr; }

    // Grammar (optional). Parse it before touching the cache so a bad grammar costs nothing.
    std::string gtxt = bytes(env, jgrammar);
    llama_sampler *gram = nullptr;
    if (!gtxt.empty()) {
        gram = llama_sampler_init_grammar(vocab, gtxt.c_str(), "root");
        if (!gram) LOGI("grammar did not parse, replying without it");
    }

    // Reuse the longest common prefix already in the cache; always re-read at least the last prompt token.
    size_t keep = 0;
    while (keep < m->cache.size() && keep < toks.size() && m->cache[keep] == toks[keep]) keep++;
    if (keep >= toks.size()) keep = toks.size() - 1;
    if (!llama_kv_self_seq_rm(m->ctx, 0, (llama_pos) keep, -1)) { llama_kv_self_clear(m->ctx); keep = 0; }
    m->cache.resize(keep);
    m->last_reused = (int) keep;
    if (!feed(m, toks, keep)) {
        // Something went wrong mid-read: start this mind's context over so the next turn is clean.
        llama_free(m->ctx); make_ctx(m);
        if (gram) llama_sampler_free(gram);
        fail(m, "could not read the prompt (decode failed)");
        return nullptr;
    }

    llama_sampler *chain = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(chain, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(chain, llama_sampler_init_top_p(topP, 1));
    llama_sampler_chain_add(chain, llama_sampler_init_temp(temp));
    llama_sampler_chain_add(chain, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    const int n_vocab = llama_vocab_n_tokens(vocab);
    std::vector<llama_token_data> cand(n_vocab);
    auto fill = [&]() {
        const float *logits = llama_get_logits_ith(m->ctx, -1);
        for (int t = 0; t < n_vocab; t++) cand[t] = {t, logits[t], 0.0f};
        return llama_token_data_array{cand.data(), (size_t) n_vocab, -1, false};
    };

    std::string out;
    int gen = 0;
    while (gen < maxTokens) {
        llama_token_data_array arr = fill();
        llama_sampler_apply(chain, &arr);
        llama_token tok = arr.data[arr.selected].id;
        if (gram) {
            // Fast path: check only the chosen token against the grammar; if it's not allowed, filter everything and pick again.
            llama_token_data one = {tok, 1.0f, 0.0f};
            llama_token_data_array a1 = {&one, 1, -1, false};
            llama_sampler_apply(gram, &a1);
            if (std::isinf(one.logit) && one.logit < 0) {
                arr = fill();
                llama_sampler_apply(gram, &arr);
                llama_sampler_apply(chain, &arr);
                if (arr.selected < 0 || std::isinf(arr.data[arr.selected].logit)) break;   // nothing allowed: stop cleanly
                tok = arr.data[arr.selected].id;
            }
            llama_sampler_accept(gram, tok);
        }
        llama_sampler_accept(chain, tok);
        if (llama_vocab_is_eog(vocab, tok)) break;
        char piece[256];
        int k = llama_token_to_piece(vocab, tok, piece, sizeof(piece), 0, false);
        if (k > 0) out.append(piece, k);
        gen++;
        if ((int) m->cache.size() + 1 >= m->n_ctx) break;
        llama_batch b = llama_batch_get_one(&tok, 1);
        if (llama_decode(m->ctx, b) != 0) break;
        m->cache.push_back(tok);
    }
    llama_sampler_free(chain);
    if (gram) llama_sampler_free(gram);
    m->last_prompt = nTok; m->last_gen = gen;

    jbyteArray res = env->NewByteArray((jsize) out.size());
    env->SetByteArrayRegion(res, 0, (jsize) out.size(), reinterpret_cast<const jbyte *>(out.data()));
    return res;
}
