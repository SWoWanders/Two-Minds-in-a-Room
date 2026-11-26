#!/usr/bin/env bash
# Wraps this repo's web app (index.html + assets) in a Capacitor Android shell
# and builds an unsigned release APK. Run by .github/workflows/android-apk.yml.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
cd "$HERE"

APP_ID="$(node -p "require('./capacitor.config.json').appId")"
VERSION_CODE="${VERSION_CODE:-1}"
VERSION_NAME="${VERSION_NAME:-1.0.$VERSION_CODE}"

echo "==> Copying web files into www/"
rm -rf www && mkdir www
tar -C "$ROOT" -cf - \
  --exclude './.git' --exclude './.github' --exclude './android-app' \
  --exclude './README.md' --exclude './.nojekyll' --exclude './coi-sw.js' --exclude './worker' . | tar -C www -xf -
test -f www/index.html || { echo "::error::index.html not found in repo root"; exit 1; }

# App-only tweaks (the website itself is never changed):
#   overrides/  files copied over the web files
#   inject.html snippet inserted right after <head> in index.html
if [ -d overrides ]; then cp -r overrides/. www/; fi
if [ -f inject.html ]; then
  node -e '
    const fs=require("fs"); const f="www/index.html";
    const html=fs.readFileSync(f,"utf8"); const snip=fs.readFileSync("inject.html","utf8");
    if(!/<head[^>]*>/i.test(html)){console.error("::error::no <head> tag to inject into");process.exit(1)}
    fs.writeFileSync(f, html.replace(/<head[^>]*>/i, m=>m+"\n"+snip));
  '
fi

echo "==> Installing Capacitor"
npm install --no-audit --no-fund

echo "==> Creating Android project"
rm -rf android
npx cap add android
npx cap sync android

echo "==> Version $VERSION_NAME ($VERSION_CODE)"
sed -i -E "s/versionCode [0-9]+/versionCode $VERSION_CODE/; s/versionName \"[^\"]*\"/versionName \"$VERSION_NAME\"/" android/app/build.gradle

if [ -f permissions.txt ]; then
  echo "==> Android permissions"
  M=android/app/src/main/AndroidManifest.xml
  while read -r perm; do
    [ -z "$perm" ] && continue
    grep -q "\"$perm\"" "$M" || sed -i "s#</manifest>#    <uses-permission android:name=\"$perm\" />\n</manifest>#" "$M"
  done < permissions.txt
  grep uses-permission "$M"
fi

echo "==> Let the app find the phone's text-to-speech engine (Android 11+ package visibility)"
M=android/app/src/main/AndroidManifest.xml
grep -q "TTS_SERVICE" "$M" || sed -i 's#</manifest>#    <queries><intent><action android:name="android.intent.action.TTS_SERVICE" /></intent></queries>\n</manifest>#' "$M"
grep -n "TTS_SERVICE" "$M"

echo "==> App icons"
cp -r res/* android/app/src/main/res/

if [ -d native ]; then
  echo "==> Custom native code"
  PKG_DIR="android/app/src/main/java/$(echo "$APP_ID" | tr . /)"
  cp native/*.java "$PKG_DIR"/
fi

echo "==> Native AI engine: llama.cpp for arm64 phones"
NDK="${ANDROID_NDK_LATEST_HOME:-${ANDROID_NDK_HOME:-}}"
if [ ! -d "$NDK" ]; then NDK="$(ls -d "$ANDROID_HOME"/ndk/* 2>/dev/null | sort -V | tail -1)"; fi
test -d "$NDK" || { echo "::error::Android NDK not found"; exit 1; }
LLAMA_TAG="${LLAMA_TAG:-b5600}"
rm -rf /tmp/llama.cpp /tmp/llamajni-build
git clone -q --depth 1 --branch "$LLAMA_TAG" https://github.com/ggml-org/llama.cpp /tmp/llama.cpp
cmake -S native-ai -B /tmp/llamajni-build \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release \
  -DLLAMA_DIR=/tmp/llama.cpp -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DLLAMA_CURL=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_TOOLS=OFF \
  -DLLAMA_BUILD_SERVER=OFF -DLLAMA_BUILD_COMMON=OFF \
  -DGGML_OPENMP=OFF -DGGML_NATIVE=OFF -DGGML_CPU_ARM_ARCH=armv8.2-a+dotprod+fp16
cmake --build /tmp/llamajni-build --target llamajni -j "$(nproc)"
mkdir -p android/app/src/main/jniLibs/arm64-v8a
cp /tmp/llamajni-build/libllamajni.so android/app/src/main/jniLibs/arm64-v8a/
ls -la android/app/src/main/jniLibs/arm64-v8a/

echo "==> Gradle build"
cd android
chmod +x gradlew
./gradlew assembleRelease --no-daemon --stacktrace
ls -la app/build/outputs/apk/release/
