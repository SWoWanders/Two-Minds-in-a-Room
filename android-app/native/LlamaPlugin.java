package io.github.swowanders.twominds;

import com.getcapacitor.JSArray;
import com.getcapacitor.JSObject;
import com.getcapacitor.Plugin;
import com.getcapacitor.PluginCall;
import com.getcapacitor.PluginMethod;
import com.getcapacitor.annotation.CapacitorPlugin;

import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Lets the web page download GGUF models and run them natively with llama.cpp. */
@CapacitorPlugin(name = "Llama")
public class LlamaPlugin extends Plugin {
    private final ExecutorService ai = Executors.newSingleThreadExecutor();
    private final ExecutorService net = Executors.newSingleThreadExecutor();

    private File modelDir() {
        File d = new File(getContext().getFilesDir(), "models");
        if (!d.exists()) d.mkdirs();
        return d;
    }

    private static boolean safeName(String f) { return f != null && f.matches("[A-Za-z0-9._-]+"); }

    @PluginMethod
    public void info(PluginCall call) {
        JSObject r = new JSObject();
        r.put("cores", Runtime.getRuntime().availableProcessors());
        call.resolve(r);
    }

    @PluginMethod
    public void has(PluginCall call) {
        String file = call.getString("file");
        if (!safeName(file)) { call.reject("bad file name"); return; }
        File f = new File(modelDir(), file);
        JSObject r = new JSObject();
        r.put("exists", f.exists() && f.length() > 1_000_000);
        r.put("bytes", f.length());
        call.resolve(r);
    }

    @PluginMethod
    public void download(PluginCall call) {
        String url = call.getString("url"), file = call.getString("file");
        if (url == null || !url.startsWith("https://") || !safeName(file)) { call.reject("bad arguments"); return; }
        net.execute(() -> {
            File out = new File(modelDir(), file), part = new File(modelDir(), file + ".part");
            try {
                if (out.exists() && out.length() > 1_000_000) { JSObject r = new JSObject(); r.put("bytes", out.length()); call.resolve(r); return; }
                HttpURLConnection c = open(url);
                long total = c.getContentLengthLong(), done = 0, lastEmit = 0;
                try (InputStream in = new BufferedInputStream(c.getInputStream()); OutputStream os = new BufferedOutputStream(new FileOutputStream(part))) {
                    byte[] buf = new byte[1 << 16];
                    int n;
                    while ((n = in.read(buf)) > 0) {
                        os.write(buf, 0, n);
                        done += n;
                        if (done - lastEmit > (2 << 20)) {
                            lastEmit = done;
                            JSObject p = new JSObject();
                            p.put("file", file); p.put("loaded", done); p.put("total", total);
                            notifyListeners("progress", p);
                        }
                    }
                }
                if (out.exists()) out.delete();
                if (!part.renameTo(out)) throw new IOException("could not save the model");
                JSObject r = new JSObject();
                r.put("bytes", out.length());
                call.resolve(r);
            } catch (Exception e) {
                part.delete();
                call.reject("Download failed: " + e.getMessage());
            }
        });
    }

    private HttpURLConnection open(String u) throws IOException {
        for (int i = 0; i < 8; i++) {
            HttpURLConnection c = (HttpURLConnection) new URL(u).openConnection();
            c.setInstanceFollowRedirects(false);
            c.setConnectTimeout(20000);
            c.setReadTimeout(60000);
            c.setRequestProperty("User-Agent", "TwoMinds-Android/1.0");
            int code = c.getResponseCode();
            if (code >= 300 && code < 400) {
                String loc = c.getHeaderField("Location");
                c.disconnect();
                if (loc == null) throw new IOException("redirect without a location");
                u = new URL(new URL(u), loc).toString();
                continue;
            }
            if (code != 200) { c.disconnect(); throw new IOException("server said " + code); }
            return c;
        }
        throw new IOException("too many redirects");
    }

    @PluginMethod
    public void load(PluginCall call) {
        String file = call.getString("file");
        int nCtx = call.getInt("nCtx", 4096), threads = call.getInt("threads", 4);
        if (!safeName(file)) { call.reject("bad file name"); return; }
        ai.execute(() -> {
            File f = new File(modelDir(), file);
            if (!f.exists()) { call.reject("model file is missing"); return; }
            long h = LlamaNative.load(f.getAbsolutePath(), nCtx, threads);
            if (h == 0) { call.reject("could not load " + file); return; }
            JSObject r = new JSObject();
            r.put("id", String.valueOf(h));
            call.resolve(r);
        });
    }

    @PluginMethod
    public void complete(PluginCall call) {
        final long h;
        try { h = Long.parseLong(call.getString("id", "0")); } catch (Exception e) { call.reject("bad id"); return; }
        JSArray msgs = call.getArray("messages");
        int max = call.getInt("maxTokens", 160);
        float temp = call.getFloat("temperature", 0.8f), topP = call.getFloat("topP", 0.9f);
        String grammar = call.getString("grammar", "");
        if (h == 0 || msgs == null) { call.reject("bad arguments"); return; }
        ai.execute(() -> {
            try {
                int n = msgs.length();
                byte[][] roles = new byte[n][], contents = new byte[n][];
                for (int i = 0; i < n; i++) {
                    JSONObject m = msgs.getJSONObject(i);
                    roles[i] = m.optString("role", "user").getBytes(StandardCharsets.UTF_8);
                    contents[i] = m.optString("content", "").getBytes(StandardCharsets.UTF_8);
                }
                long t0 = System.currentTimeMillis();
                byte[] g = grammar == null || grammar.isEmpty() ? null : grammar.getBytes(StandardCharsets.UTF_8);
                byte[] out = LlamaNative.complete(h, roles, contents, max, temp, topP, g);
                if (out == null) { call.reject("AI failed: " + LlamaNative.lastError(h)); return; }
                int[] st = LlamaNative.stats(h);
                JSObject r = new JSObject();
                r.put("content", new String(out, StandardCharsets.UTF_8));
                r.put("promptTokens", st[0]);
                r.put("completionTokens", st[1]);
                r.put("reusedTokens", st[2]);
                r.put("ms", System.currentTimeMillis() - t0);
                call.resolve(r);
            } catch (Exception e) {
                call.reject("AI failed: " + e.getMessage());
            }
        });
    }

    @PluginMethod
    public void unload(PluginCall call) {
        final long h;
        try { h = Long.parseLong(call.getString("id", "0")); } catch (Exception e) { call.reject("bad id"); return; }
        ai.execute(() -> { if (h != 0) LlamaNative.free(h); call.resolve(); });
    }
}
