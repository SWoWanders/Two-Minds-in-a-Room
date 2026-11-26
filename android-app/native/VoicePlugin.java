package io.github.swowanders.twominds;

import android.os.Bundle;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.speech.tts.Voice;

import com.getcapacitor.JSArray;
import com.getcapacitor.JSObject;
import com.getcapacitor.Plugin;
import com.getcapacitor.PluginCall;
import com.getcapacitor.PluginMethod;
import com.getcapacitor.annotation.CapacitorPlugin;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** The phone's own text-to-speech voices (Android WebView has no speechSynthesis). Free and offline. */
@CapacitorPlugin(name = "Voice")
public class VoicePlugin extends Plugin {
    private TextToSpeech tts;
    private volatile int state = 0;            // 0 starting, 1 ready, -1 failed
    private final List<PluginCall> waiting = new ArrayList<>();
    private final List<Voice> english = new ArrayList<>();

    @Override
    public void load() {
        tts = new TextToSpeech(getContext(), status -> {
            state = status == TextToSpeech.SUCCESS ? 1 : -1;
            if (state == 1) {
                try { tts.setLanguage(Locale.getDefault().getLanguage().equals("en") ? Locale.getDefault() : Locale.US); } catch (Exception ignored) {}
                try {
                    for (Voice v : tts.getVoices()) {
                        if (v.getLocale() != null && "en".equals(v.getLocale().getLanguage()) && !v.isNetworkConnectionRequired()
                                && (v.getFeatures() == null || !v.getFeatures().contains(TextToSpeech.Engine.KEY_FEATURE_NOT_INSTALLED))) english.add(v);
                    }
                    java.util.Collections.sort(english, (a, b) -> a.getName().compareTo(b.getName()));
                } catch (Exception ignored) {}
                tts.setOnUtteranceProgressListener(new UtteranceProgressListener() {
                    @Override public void onStart(String id) { emit("start", id, -1, -1); }
                    @Override public void onDone(String id) { emit("done", id, -1, -1); }
                    @Override public void onError(String id) { emit("done", id, -1, -1); }
                    @Override public void onRangeStart(String id, int start, int end, int frame) { emit("range", id, start, end); }
                });
            }
            synchronized (waiting) { for (PluginCall c : waiting) info(c); waiting.clear(); }
        });
    }

    private void emit(String ev, String id, int start, int end) {
        JSObject o = new JSObject();
        o.put("id", id);
        if (start >= 0) { o.put("start", start); o.put("end", end); }
        notifyListeners(ev, o);
    }

    @PluginMethod
    public void info(PluginCall call) {
        if (state == 0) { synchronized (waiting) { waiting.add(call); } return; }
        JSObject r = new JSObject();
        r.put("ready", state == 1);
        JSArray names = new JSArray();
        for (Voice v : english) names.put(v.getName());
        r.put("voices", names);
        call.resolve(r);
    }

    @PluginMethod
    public void speak(PluginCall call) {
        if (state != 1) { call.reject(state == 0 ? "voices are still starting" : "this phone has no text-to-speech engine"); return; }
        String text = call.getString("text", ""), id = call.getString("id", "u"), voice = call.getString("voice", "");
        float pitch = call.getFloat("pitch", 1f), rate = call.getFloat("rate", 1f), volume = call.getFloat("volume", 1f);
        try {
            if (voice != null && !voice.isEmpty()) for (Voice v : english) if (v.getName().equals(voice)) { tts.setVoice(v); break; }
            tts.setPitch(pitch);
            tts.setSpeechRate(rate);
            Bundle p = new Bundle();
            p.putFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, Math.max(0f, Math.min(1f, volume)));
            int r = tts.speak(text, TextToSpeech.QUEUE_FLUSH, p, id);
            if (r != TextToSpeech.SUCCESS) { call.reject("the phone's voice couldn't speak"); return; }
            call.resolve();
        } catch (Exception e) { call.reject("voice error: " + e.getMessage()); }
    }

    @PluginMethod
    public void stop(PluginCall call) { if (tts != null) tts.stop(); call.resolve(); }

    @Override
    protected void handleOnDestroy() { if (tts != null) { tts.stop(); tts.shutdown(); } }
}
