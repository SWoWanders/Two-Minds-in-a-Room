# Android app (Two Minds)

Turns this web app into an installable Android APK using Capacitor, with the robots'
minds running natively (llama.cpp compiled for the phone) instead of inside the browser.
Every push to `main` triggers `.github/workflows/android-apk.yml`, which runs
`build.sh` on GitHub's servers and posts the APK under **Releases → Latest**.

- `capacitor.config.json`: app ID, name, background colour
- `res/`: launcher icons (Pip and Moss)
- `native-ai/`: llama.cpp build (CMake + Android NDK) and the JNI bridge `llama_jni.cpp`
- `native/`: `LlamaPlugin.java` (download / load / complete), `LlamaNative.java`, `MainActivity.java`
- `build.sh`: copies the web files, creates the Android project, builds llama.cpp, builds the APK

Signing: repo secrets `ANDROID_KEYSTORE_B64` and `ANDROID_KEYSTORE_PASSWORD`
(same key as the other apps). Without them each build uses a throwaway key.

Models (free GGUF files from Hugging Face) are downloaded inside the app on first start
and kept in the app's own storage. Nothing in here affects the GitHub Pages site.
