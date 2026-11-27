# Pip and Moss

A cozy 3D room where two robots, Pip and Moss, are each run by a different free AI model. They walk, move every joint and finger, pick things up, draw and write on a whiteboard, use a computer with real internet, talk out loud, remember, and learn. No API keys and no cost.

Play it at https://swowanders.github.io/Two-Minds-in-a-Room/

## Where the minds run

- **Android app (fastest):** llama.cpp compiled for the phone. Offers bigger minds (Gemma 3 1B, Qwen2.5 1.5B, Llama 3.2 3B), a locked reply format, and a prompt cache so each robot only reads what changed since its last turn. Download the APK from the Releases page.
- **Browser, phone's processor:** llama.cpp in WebAssembly. Slower, but works in any modern browser.
- **Browser, graphics chip:** WebLLM on WebGPU. Fast on desktops; crashes some phones.
- **Smart mode (optional):** bigger cloud minds on your own free Cloudflare Worker, with automatic fallback to the on-device minds. See [worker/README.md](worker/README.md).

## Memory and knowledge

Each robot has its own memory stream (what it saw, heard, did and reflected on) and its own **knowledge library** of facts about the world. The library fills up from what you teach (start a chat message with "remember that…", "did you know…" or "fact:", or paste text or add a text file in *Memories & learning*), from what the robot looks up on the computer, and, held loosely, from what the other robot says. Export and import keep everything.

## The workshop

A 3D printer prints in plastic, conductive metal and rubber: shapes, gears, wheels, motors, servos, lights, buttons, motion and sound sensors, buzzers and chips. A battery cupboard powers them, and a bottle of super glue joins two printed things into one ("Moss, glue the windmill onto the cart").

**Working circuits.** A printed *logic chip* runs real logic: AND, OR, XOR, NAND, NOR, NOT, clocks, toggles and latches, reading its inputs (buttons, switches, motion and sound sensors) and driving its outputs (lights, motors, servos, buzzers, tune chips). It runs live in the room: press a doorbell's button, walk a robot past a motion alarm. The computer tests every circuit and shows its truth table (tap *Circuit* in the Workshop panel).

**Know-how first.** A robot can only use the circuit parts and gates it has studied (basic circuits, microchips, logic gates, NAND/NOR/XOR, timers, flip-flops, sensors). Asked for a circuit it doesn't understand yet, it goes to the study shelf and reads the chapter it needs first. The *Logic & Circuit Design* book covers it all.

## The study shelf

Twenty research books the robots read a chapter at a time, from Wikipedia, Wikibooks and Wikiquote: music, Ottawa, Expo 67, writing, drawing, simple machines, 3D printing, circuits and logic, learning together (the Peeragogy Handbook), creativity and invention, famous inventors, and *Words of Inspiration*. When a robot reads that one, it picks a quote and pins it on the project board, and the quote can inspire their writing.

- **Chat:** talk to either robot or both; give them things to do.
