// JNI bridge between the app (LlamaNative.java) and llama.cpp. Runs entirely on the phone's processor.
#include <jni.h>
#include <string>
#include <vector>
#include <android/log.h>
#include "llama.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "TwoMindsLlama", __VA_ARGS__)

struct Mind { llama_model *model; int n_ctx; int n_threads; int last_prompt; int last_gen; };
static bool g_backend = false;

static std::string bytes(JNIEnv *env, jbyteArray a) {
    if (!a) return std::string();
    jsize n = env->GetArrayLength(a);
    std::string s(n, '\0');
    env->GetByteArrayRegion(a, 0, n, reinterpret_cast<jbyte *>(&s[0]));
    return s;
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
    Mind *m = new Mind{model, nCtx, nThreads, 0, 0};
    LOGI("model loaded, ctx %d, threads %d", nCtx, nThreads);
    return reinterpret_cast<jlong>(m);
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_swowanders_twominds_LlamaNative_free(JNIEnv *, jclass, jlong h) {
    Mind *m = reinterpret_cast<Mind *>(h);
    if (!m) return;
    llama_model_free(m->model);
    delete m;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_io_github_swowanders_twominds_LlamaNative_stats(JNIEnv *env, jclass, jlong h) {
    Mind *m = reinterpret_cast<Mind *>(h);
    jint v[2] = {m ? m->last_prompt : 0, m ? m->last_gen : 0};
    jintArray out = env->NewIntArray(2);
    env->SetIntArrayRegion(out, 0, 2, v);
    return out;
}

// Chat completion: applies the model's own chat template, decodes the prompt, then samples a reply.
extern "C" JNIEXPORT jbyteArray JNICALL
Java_io_github_swowanders_twominds_LlamaNative_complete(JNIEnv *env, jclass, jlong h, jobjectArray roles, jobjectArray contents,
                                                       jint maxTokens, jfloat temp, jfloat topP) {
    Mind *m = reinterpret_cast<Mind *>(h);
    if (!m) return nullptr;
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
    std::vector<char> buf(16384);
    int len = tmpl ? llama_chat_apply_template(tmpl, msgs.data(), msgs.size(), true, buf.data(), (int) buf.size()) : -1;
    if (len > (int) buf.size()) { buf.resize(len + 1); len = llama_chat_apply_template(tmpl, msgs.data(), msgs.size(), true, buf.data(), (int) buf.size()); }
    if (len > 0) prompt.assign(buf.data(), len);
    else { // ChatML fallback (used by SmolLM2 and Qwen)
        for (int i = 0; i < n; i++) prompt += "<|im_start|>" + r[i] + "\n" + c[i] + "<|im_end|>\n";
        prompt += "<|im_start|>assistant\n";
    }

    const llama_vocab *vocab = llama_model_get_vocab(m->model);
    int nTok = -llama_tokenize(vocab, prompt.c_str(), (int) prompt.size(), nullptr, 0, true, true);
    if (nTok <= 0) return nullptr;
    std::vector<llama_token> toks(nTok);
    if (llama_tokenize(vocab, prompt.c_str(), (int) prompt.size(), toks.data(), (int) toks.size(), true, true) < 0) return nullptr;

    llama_context_params cp = llama_context_default_params();
    int ctxSize = m->n_ctx;
    if (nTok + maxTokens + 8 > ctxSize) ctxSize = nTok + maxTokens + 8;
    cp.n_ctx = ctxSize; cp.n_batch = ctxSize;
    cp.n_threads = m->n_threads; cp.n_threads_batch = m->n_threads;
    llama_context *ctx = llama_init_from_model(m->model, cp);
    if (!ctx) return nullptr;

    llama_sampler *smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(smpl, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(smpl, llama_sampler_init_top_p(topP, 1));
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(temp));
    llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::string out;
    int gen = 0;
    llama_batch batch = llama_batch_get_one(toks.data(), (int) toks.size());
    if (llama_decode(ctx, batch) == 0) {
        while (gen < maxTokens) {
            llama_token tok = llama_sampler_sample(smpl, ctx, -1);
            if (llama_vocab_is_eog(vocab, tok)) break;
            char piece[256];
            int k = llama_token_to_piece(vocab, tok, piece, sizeof(piece), 0, false);
            if (k > 0) out.append(piece, k);
            gen++;
            batch = llama_batch_get_one(&tok, 1);
            if (llama_decode(ctx, batch) != 0) break;
        }
    }
    llama_sampler_free(smpl);
    llama_free(ctx);
    m->last_prompt = nTok; m->last_gen = gen;

    jbyteArray res = env->NewByteArray((jsize) out.size());
    env->SetByteArrayRegion(res, 0, (jsize) out.size(), reinterpret_cast<const jbyte *>(out.data()));
    return res;
}
