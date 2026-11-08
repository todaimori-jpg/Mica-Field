# Mica Field

A standalone desktop synthesizer and VST3 for ambient and experimental sound design. Both formats share the same C++ engine, interface and patch files.

## Use on iPad

Open **[Mica Field for iPad](https://mica-field-ipad.orwellianinstitute.chatgpt.site)** in Safari, rotate to landscape, tap **Enable audio**, then play the **Keyboard**. **Hold** latches drones. **REC → Stop → Save WAV → Save to Files** exports your take. Safari **Share → Add to Home Screen** adds the instrument to your iPad.

The iPad edition runs the same C++ DSP in WebAssembly, defaults to four voices and uses touch controls. It records stereo 16-bit WAV and exchanges `.mica` patches with the desktop instrument. Keep it in the foreground while playing or recording. See [web/README.md](web/README.md) for hosting, offline behavior, memory details and the reproducible engine build. Desktop VST3 downloads do not run on iPadOS.

## Play and record

1. Open **Mica Field** from the Standalone build. Use the gear in the top right to choose your audio output and enable a MIDI controller.
2. Click the instrument, then play with **A S D F G H J K L**. Use **W E T Y U O P** for sharps. **Z** and **X** move the octave. You can also use the onscreen **Keyboard** panel.
3. **Hold** latches notes played with the instrument keyboard. Play a held note again to release it. **Panic** or **Esc** clears held notes, the sequencer and effects tails.
4. Click **REC**, choose a destination and play. Click REC again to finish the stereo 24-bit WAV.

Open the standalone application to work without a DAW. The separate VST3 bundle is for compatible instrument hosts.

## Sound design

**Synth** has two interchangeable engines. Click an engine tab to edit it and its engine type to choose **Wavetable**, **Analog**, **Harmonic**, **Modal**, **Sample** or **Granular**. The utility section adds a sub oscillator and filtered noise.

Each engine's **Filter Mix** sends it toward Filter 1 or 2. **Routing** moves between series and parallel. Both filters have low-pass, high-pass, band-pass, notch and bypass modes.

Drag knobs vertically. Hold **Shift** for finer changes. Scroll over a knob for increments, or double-click to reset its default value.

Drag a colored modulation source onto a knob. You can also click the source, then click an outlined knob. Set positive or negative **Depth** in the **Routes** panel. There are 16 route slots. Factory patches already connect the four macros.

**Sample** and **Granular** load audio through the clip-name menu. WAV, AIFF, FLAC, MP3 and Ogg Vorbis are supported. The first eight seconds are downmixed to mono and stored at 48 kHz. C4 plays at source pitch. A procedural **Weathered Tape** texture is available immediately.

**FX** provides stereo chorus, ping-pong delay and reverb with octave-up shimmer. **SEQ** edits 16 gates, notes, velocities and probabilities. **Generate** uses the selected scale. Start the internal clock with its play button or press **Space** on that page.

Click the preset name for the factory bank. **Save patch as...** creates a portable `.mica` file including imported audio, modulation routes and the sequence. The VST3 embeds this state in host sessions.

Try **Harbor After Rain** with Hold enabled, **Tape Prayer** with a field recording, or **Moth Steps** with the sequencer.

## GitHub desktop builds

Place this folder's contents at the root of **todaimori-jpg/Mica-Field**, including the hidden **.github** folder. Push to **main** or **master** to start the workflow. You can also run **Build Mica Field** manually from the Actions tab.

When the run succeeds, open **Artifacts** and download your platform. Extract the artifact and the archive inside it.

| Download | Standalone format | Plugin format |
| --- | --- | --- |
| Windows-x64 | Mica Field.exe | Mica Field.vst3 |
| macOS-universal | Mica Field.app, Intel and Apple Silicon | Mica Field.vst3 |
| Linux-x64 | Mica Field executable | Mica Field.vst3 |

The workflow produces unsigned builds. Current local verification covers Linux. Windows and macOS need their workflow builds before those binaries are available.

## Local build

Use CMake 3.22 or later and a C++17 toolchain. Windows builds use Visual Studio 2022 with Desktop development with C++. macOS builds use Xcode command line tools.

Ubuntu or Debian build dependencies can be installed with this command.

```sh
sudo apt-get install build-essential cmake pkg-config libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxext-dev libxcomposite-dev libxcursor-dev libxinerama-dev libxrandr-dev libxrender-dev
```

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
```

On Windows, configure with `cmake -S . -B build -A x64` using the Visual Studio generator. The formats appear under `build/MicaField_artefacts/Release/`.

CMake fetches the pinned JUCE 8.0.12 source. Use `-DMICA_JUCE_DIR=/path/to/JUCE-8.0.12` for a local checkout or `-DMICA_DSP_ONLY=ON` for sound-engine tests without JUCE.

## Footprint and source

The built instrument runs offline. Set the voice limit to **4**, **8** or **12** from the footer. Pause reactive graphics from the main menu when desired. The audio engine preallocates its processing buffers, and generates the bundled texture at startup.

Mica Field follows Arturia Pigments' workspace layout with original interface graphics and DSP. This release covers the six sound-generation modes, modulation, dual filtering, three effects and internal sequencer described above.

Source is distributed under **AGPL-3.0-only**, with JUCE used under its AGPL option. CMake pins the exact JUCE source archive and checksum.
