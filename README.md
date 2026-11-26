# Two Minds, One Room

A cozy 3D room where two robots, Pip and Moss, are each run by a different free AI model. They walk, move every joint and finger, pick things up, draw and write on a whiteboard, use a computer with real internet, talk out loud, remember, and learn. No API keys and no cost.

Play it at https://swowanders.github.io/Two-Minds-in-a-Room/

## Where the minds run

- **Android app (fastest):** llama.cpp compiled for the phone. Offers bigger minds (Gemma 3 1B, Qwen2.5 1.5B, Llama 3.2 3B), a locked reply format, and a prompt cache so each robot only reads what changed since its last turn. Download the APK from the Releases page.
- **Browser, phone's processor:** llama.cpp in WebAssembly. Slower, but works in any modern browser.
- **Browser, graphics chip:** WebLLM on WebGPU. Fast on desktops; crashes some phones.
- **Smart mode (optional):** bigger cloud minds on your own free Cloudflare Worker, with automatic fallback to the on-device minds. See [worker/README.md](worker/README.md).

## Memory and knowledge

Each robot has its own memory stream (what it saw, heard, did and reflected on) and its own **knowledge library** of facts about the world. The library fills up from what you teach (start a chat message with "remember that…", "did you know…" or "fact:", or paste text or add a text file in *Memories & learning*), from what the robot looks up on the computer, and, held loosely, from what the other robot says. Export and import keep everything.

- **Chat:** talk to either robot or both; give them things to do.
- **Scripted demo:** works anywhere, no AI needed.
