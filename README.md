# FREE ANTICIP8

Free reverse-reverb transition plugin (VST3 on Windows/macOS/Linux, AU on macOS, plus a Standalone app).
Drop in a sample, pick a character, set the length, LISTEN, then drag the rendered 24-bit WAV into your DAW.

## How it works

| Mode | What is rendered |
|------|------------------|
| **FX**  | reverse the input -> pad with LENGTH seconds of silence -> reverb (RT60 = 0.8 x LENGTH) -> character -> reverse again -> the original is mixed in at the end of the swell. The swell builds into the original. |
| **RAW** | no reverb: the first LENGTH seconds of the original, reversed (+ character), faded in, leading into the original. |

Characters (all real DSP, applied to the reversed wet signal):
**CLASSIC** (clean-up HP 60 Hz / LP 16 kHz), **PHONE** (4th-order 450 Hz-3.3 kHz band-pass + level-independent tanh saturation),
**CHORUS** (two modulated delay voices, quarter-cycle stereo offset; chorus LFO = RATE/4), **TREMOLO** (smooth sine AM with RATE/DEPTH, quarter-cycle auto-pan on stereo).

Reverb: 4 all-pass input diffusers -> 8 parallel damped feedback combs (feedback computed from the RT60) -> 3 all-pass output diffusers.
The right channel uses different delay lengths (stereo decorrelation). DAMPING sets the high-frequency absorption in the comb loops.

Output: channel count and sample rate of the input are preserved (mono stays mono, any file rate works - tested 44.1/48/88.2/96/192 kHz),
sanitised, peak-normalised to -1 dBFS, 5 ms fade-in / 10 ms fade-out. The waveform, LISTEN and DRAG WAV all use the same rendered buffer.

Parameters (all in `AudioProcessorValueTreeState`, automatable, saved with the project together with the input file path):
LENGTH, FX/RAW, CHARACTER, RATE, DEPTH, DAMPING. Changing a parameter re-renders on a background thread (150 ms debounce, stale renders are cancelled).

## Layout

```
Source/PluginProcessor.*   parameters, background render, preview playback, state, export
Source/PluginEditor.*      UI, drop target, OPEN INPUT, native file drag
Source/DSP/                ReverseProcessor, ReverbEngine, Diffusion, CharacterProcessor, AudioRenderer
Source/UI/                 CustomLookAndFeel, Controls (knob, segmented control, DRAG WAV), WaveformComponent
Source/Export/WavExporter.h  24-bit WAV writer
```

## Build

Requires CMake >= 3.22 and a C++17 compiler. JUCE 8.0.6 is fetched automatically
(offline: `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE`).

**Windows** (Visual Studio 2022):
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
VST3: `build\FreeAnticip8_artefacts\Release\VST3\FREE ANTICIP8.vst3` -> copy to `C:\Program Files\Common Files\VST3`.

**macOS** (Xcode, universal arm64 + x86_64):
```
cmake -S . -B build -G Xcode
cmake --build build --config Release
```
Copy the `.vst3` to `~/Library/Audio/Plug-Ins/VST3` and the `.component` to `~/Library/Audio/Plug-Ins/Components`.
Unsigned builds may need `xattr -cr "FREE ANTICIP8.vst3"` / re-signing (`codesign --force --deep -s - ...`) for Logic/Ableton; AU: run `auval -v aufx Fa8x FrAu`.

### Build Mac sans Mac (GitHub Actions)

Le workflow `.github/workflows/build-macos.yml` compile VST3 + AU universels sur un runner `macos-14`.

1. Pousse le repo sur GitHub (`main`).
2. Onglet **Actions** → workflow **Build macOS** (aussi déclenchable à la main via *Run workflow*).
3. Télécharge l’artefact **FREE-ANTICIP8-macOS** (contient `.vst3` + `.component`).
4. Sur le Mac de destination :
   ```bash
   xattr -cr "FREE ANTICIP8.vst3" "FREE ANTICIP8.component"
   codesign --force --deep -s - "FREE ANTICIP8.vst3"
   codesign --force --deep -s - "FREE ANTICIP8.component"
   cp -R "FREE ANTICIP8.vst3" ~/Library/Audio/Plug-Ins/VST3/
   cp -R "FREE ANTICIP8.component" ~/Library/Audio/Plug-Ins/Components/
   ```
   Pas de compte Apple Developer requis pour cet ad-hoc sign local.

**Linux**: `sudo apt install libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev libxinerama-dev libxext-dev libxcursor-dev libxcomposite-dev libgl1-mesa-dev`, then
`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`.

## Testing checklist (DAW)

1. Load the plugin on a track, drop a WAV (or OPEN INPUT). The waveform appears after the render.
2. LISTEN plays the rendered buffer (needs the host's audio engine running; some hosts only process while the transport plays).
3. Change LENGTH / FX-RAW / CHARACTER: the waveform re-renders; LISTEN and DRAG WAV are disabled while rendering.
4. Drag DRAG WAV onto a track in REAPER / FL Studio / Ableton / Logic. The file is `<name>_ANTICIP8_<CHARACTER>.wav` in `<temp>/FreeAnticip8` (files older than 7 days are purged at startup; hosts normally copy dropped files into the project).
5. Save/reload the project: parameters and input file are restored (if the file still exists).

## Known limits

- Max input length 180 s; only the first 2 channels of multichannel files are used.
- Drag-and-drop *into* the plugin window and native drag *out* depend on the host forwarding OS drag events; OPEN INPUT is the fallback for hosts that block drops.
- Preview is added on top of the track's input and resampled (cubic) to the host rate.
- Uses JUCE 8.0.6's `WavAudioFormat::createWriterFor` overload (deprecated in later 8.0.x, still available).
