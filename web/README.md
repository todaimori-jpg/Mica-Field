# Mica Field for iPad

A touch instrument for Safari using the same C++ DSP as the desktop Standalone and VST3 builds. No DAW, server audio processing, account for synthesis, sample downloads or JavaScript framework is required. Hosted private access may require signing in to ChatGPT.

## Play on iPad

1. Open the hosted instrument in **Safari**, in landscape orientation.
2. Tap **Enable audio**, choose a factory preset and touch **Keyboard**.
3. Turn on **Hold** to latch a drone. Tap a held note again to release it; **Panic** clears notes, sequencer and tails.
4. Drag knobs vertically. Tap a colored modulation source, then a destination knob. Adjust Depth in Routes.
5. **REC** starts recording; **Stop** finishes it. Tap **Save WAV**, then **Save to Files** in the iPad share sheet. Keep the instrument in the foreground while playing or recording.
6. Safari **Share → Add to Home Screen** installs the web app. After a complete first load the app assets are cached for offline use. Safari storage settings, private browsing and cleared website data affect this cache.

The menu opens and saves portable `.mica` patches containing parameters, routes, sequence and imported samples. These files use the desktop patch format. Browser storage restores the latest patch locally; export patches and recordings to keep your own copies. The last recording is retained until you explicitly replace it.

Sample and Granular modes accept audio from Files through the engine's clip-name menu. Browser-supported audio formats are accepted, including WAV and MP3 on current Safari. Audio is decoded locally; only the first eight seconds, downmixed to mono at 48 kHz, are kept. Decoding a long compressed file can temporarily use substantially more memory than the stored sample.

## Performance

The browser edition defaults to four voices; the menu offers eight or twelve. The WebAssembly engine has a fixed **16 MiB heap** and its binary is about **71 KB**. Total Safari memory is higher because it includes the page, audio context, decoded samples, browser and recording exports. Reactive visuals can be paused from the menu. Audio capture writes PCM chunks to IndexedDB while recording and exports stereo 16-bit WAV; assembling a long export uses additional memory.

This is a browser instrument, not an iOS VST3 or AUv3 plugin. Current desktop Chromium verification covers Worklet startup, touch-style keyboard interactions, presets, recording and WAV export. Safari on a physical iPad still needs device testing. iPadOS can suspend audio when the app is backgrounded or the screen locks.

## Host the existing build

Serve the contents of `app/` over **HTTPS** (or localhost for development). All runtime assets are included. Preserve filenames and relative paths, serve `.wasm` as `application/wasm`, and serve `.js`/`.mjs` as JavaScript. GitHub Pages can publish `app/` as a static site. Opening `index.html` directly through Files is not supported.

## Rebuild the engine

Install the official Emscripten SDK and activate **3.1.74**. With its environment loaded, run from the repository root:

```sh
bash web/build.sh
node web/checks.mjs
```

A C++17 host compiler and Python 3 are used to generate the parameter/preset metadata. The build uses `Source/Patch.cpp` and `Source/Dsp.cpp` directly; it does not build JUCE. `app/engine.mjs`, `app/engine.wasm` and `app/meta.json` are checked in so hosting requires no compiler or dependencies. `node web/checks.mjs` tests all six engine modes, immediate startup notes, maximum sample buffers, sequencing, panic, portable patch round-trips and malformed patch rejection.

Sources are licensed **AGPL-3.0-only**. Keep an accessible link to the corresponding source when hosting a modified edition.
