package io.github.swowanders.twominds;

/** Native llama.cpp calls (see android-app/native-ai/llama_jni.cpp). */
public class LlamaNative {
    static { System.loadLibrary("llamajni"); }
    public static native long load(String path, int nCtx, int nThreads);
    public static native byte[] complete(long handle, byte[][] roles, byte[][] contents, int maxTokens, float temp, float topP, byte[] grammar);
    public static native String lastError(long handle);
    public static native int[] stats(long handle);
    public static native void free(long handle);
}
