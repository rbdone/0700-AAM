# Developing Kick without the hardware

This guide gets the Kick application and its software simulator running on a desktop machine
(macOS is what it was verified on; Linux desktop should be close). It also records every change
that was needed to make the public release build, and why, so the next person does not have to
rediscover it.

## 1. How the software is put together

| Piece | Where | Role |
|---|---|---|
| `Kick/` | JUCE C++ application | The whole product: touchscreen UI, sequencer, sampler, audio engine |
| `AudIO/` | JUCE module | Audio graph / DAW engine library used by Kick |
| `ComProtocol/PktMgr` | small C++ class | Framed packet protocol between the app and the control-surface microcontroller |
| `Linux/` | Buildroot tree | The i.MX8MM Linux image that runs the app on the device |

On the device one process, `KT_Kick`, draws the 1024x600 screen and runs the audio engine. The
pads, buttons, knobs, slider and LEDs live on a separate microcontroller that talks to the app over
a serial port (`/dev/ttymxc0`) using `PktMgr` packets. That boundary is what makes desktop work
possible: everything above the serial port is portable JUCE code.

Build targets in `Kick/apps/`:

| Target | What it is | Desktop? |
|---|---|---|
| `Kick` | The monolithic app, the one the device runs | Yes. Hardware input comes from the keyboard map (section 4) |
| `KT_KickEM` | **The simulator**: `Kick` plus an on-screen control surface around the real screen | Yes, `-DKICK_SIMULATOR=ON` |
| `KickMain`, `KickUserInterface`, `KickAudioEngine`, `KickAudioModel`, `KickDeviceControl` | A split multi-process variant using the `ipc` module | Not needed for development |
| `kickvst` | VST plugin wrapper | No: depends on `tracktion_engine`, which nothing fetches |

## 2. Prerequisites

macOS:

- Xcode (or Command Line Tools). Verified with Xcode 26.5 on macOS 26.
- CMake 3.16 or newer (`brew install cmake`). Ninja is optional.
- git.

Linux desktop: the package list in `Kick/how-to/Kick_Compilation.txt` (gcc, libasound2-dev,
libjack-jackd2-dev, libfreetype6-dev, the X11 dev packages, libwebkit2gtk, mesa) plus CMake.

Network access on the first configure: CPM downloads JUCE 7.0.12 (about 150 MB of git history)
into `../.3rdPartySourceCache` next to `Kick/` (that is `0700-AAM/.3rdPartySourceCache`, which is
git-ignored). Later configures reuse it.

## 3. Build

From the repository root:

```sh
cd Kick
MACOSX_DEPLOYMENT_TARGET=12.0 cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DKICK_SIMULATOR=ON \
    -DTIMESTAMP=OFF \
    -DCPM_AudIO_SOURCE=$PWD/../AudIO \
    -DCPM_COM_PROTOCOL_SOURCE=$PWD/../ComProtocol

cmake --build build --target KT_KickEM -j10     # the simulator
cmake --build build --target Kick -j10          # the plain app
```

What the flags do:

- `MACOSX_DEPLOYMENT_TARGET=12.0` (environment variable, macOS only). JUCE 7.0.12 builds a helper
  tool called `juceaide` in a nested CMake run that does not inherit the project's deployment
  target. Against the macOS 15+ SDK it then fails on an obsoleted CoreGraphics call
  (`CGWindowListCreateImage`). The environment variable reaches the nested run. Without it the
  configure stops at "Failed to build juceaide".
- `CPM_AudIO_SOURCE` / `CPM_COM_PROTOCOL_SOURCE`: point the dependency fetcher at the copies of
  AudIO and ComProtocol in this repository instead of the internal git server named in
  `3rdParty.cmake`.
- `KICK_SIMULATOR=ON`: adds the `KT_KickEM` target.
- `TIMESTAMP=OFF`: skips the "fresh timestamp every build" helper; it is harmless either way.

Outputs land in `Kick/build/Kick.app` and `Kick/build/KT_KickEM.app` (on Linux, plain executables
in the same place). A full build takes a few minutes; the first one also compiles JUCE.

## 4. Run

The app resolves its data relative to the **current working directory** on desktop
(`./assets`, `./templates`, `./demos`, `./projects`, `./logs`, `./prefs`; see
`Kick/modules/common/source/kick_paths.cpp`). CMake copies assets, templates and demos into the
build directory, so always start the app from there:

```sh
cd Kick/build
./KT_KickEM.app/Contents/MacOS/KT_KickEM     # simulator
./Kick.app/Contents/MacOS/Kick               # plain app
```

Do not use `open Kick.app`: Finder launches it with `/` as the working directory and nothing loads.
Logs go to stdout; redirect to a file if you want them.

The first launch asks for microphone access (the engine opens a stereo input). Denying it is fine,
the engine just runs with silent inputs.

### 4.1 The simulator (`KT_KickEM`)

The window is a to-scale front panel of the unit: the 1024x600 Kick screen sits in the display
bezel, with MASTER / REC pots, the camera module and LINK on the left, P1-P4 on the right, and
below them the slider with FULL LEVEL / 16 LEVELS / ERASE, the 4x4 pads (pad 1 bottom-left), the
BANK row, the function keys around the glowing jog wheel, SHIFT, and the five round transport
keys. Every control sends exactly the packet the microcontroller would send; pad colours and key
LEDs sent by the app are shown.

- The window is resizable and keeps the unit's proportions; the whole panel, screen included,
  scales with it. It opens as tall as the display allows.
- The small printed captions under the keys are the SHIFT functions (GRID, SAVE, PAD MIX, ...,
  banks E-H). They are clickable and send that function's id directly, so you do not have to hold
  SHIFT while developing. SHIFT itself is a toggle.
- The jog wheel and P1-P4 are endless encoders, as on the unit: the app only ever receives a
  direction and a step count (`DclContractsHelper::getKnobValue`), never a position. Drag to
  turn; the small PUSH key under the wheel is the wheel's click (ENTER). MASTER and REC are
  analogue pots on the unit and do nothing here.
- Every pad and key is a `juce::Button` with its printed name, so the panel is scriptable through
  macOS accessibility, e.g. from AppleScript:
  `tell application "System Events" to tell process "KT_KickEM" to click button "Pad 1" of window 1`.
- Known limitation: the app's full-screen popups (`kick_utils_popup.cpp`) are separate desktop
  windows sized to the primary display, so they open at native size over the whole screen rather
  than inside the scaled bezel. They work, they just do not scale.

What was verified on macOS: the simulator starts, the app opens the default audio device at
48 kHz with a 480-sample block, the control-surface handshake completes, pressing the jog wheel
loads the selected project, and the LED packets the app sends back light the matching keys
(STOP, bank A, MAIN, FULL LEVEL). Pads show no colours in a fresh checkout because the factory
samples are not part of the release, so no program has anything assigned.

Where it lives: `Kick/apps/kickem/`. `simulator/kick_Simulator.cpp` holds the panel geometry (in
canvas units measured from a top-down photo of the unit), the drawing, and the packet
encoding/decoding; `kick_SimulatorInterfaceImpl.cpp` is the bridge that `modules/input_events`
talks to when compiled with `SIM_BUILD`.

### 4.2 Keyboard control in both apps

`Kick/modules/contracts/utils/keyboard/keyboard_detector.cpp` maps keys to control-surface
packets. It used to be compiled on Windows only; it is now enabled on macOS too. The main
bindings:

| Keys | Control |
|---|---|
| `Z X C V` / `A S D F` / `Q W E R` / `1 2 3 4` | Pads 1-16 (bottom row first, like the unit) |
| `F1`-`F4`, `F6`-`F8` | Banks A-D, F-H |
| `T Y U I O` | MAIN, BROWSE, TRACK MIX, TRACK MUTE, NEXT SEQ |
| `G H J K L` | STEP SEQ, TC, SAMPLER, SAMPLE EDIT, PROGRAM EDIT |
| `5 6 7 8 9` | GRID, SAVE, PAD MIX, PAD MUTE, FX |
| `B N M , .` | AUTOMATION, ON/OFF, LOOPER, LINK EDIT, MIDI CONTROL |
| `Space`, `F9`-`F12` | PLAY, RECORD, OVERDUB, STOP, PLAY START |
| Arrow up/down, Enter, Left/Right | Jog wheel, ENTER, - / + |
| `Esc`, `Insert`, `Delete`, `Tab`, `PgUp`, `PgDn` | MENU, PREF, ERASE, LINK, BACK, COPY |
| `0 - = P [ ]` | FULL LEVEL, NOTE REPEAT, 16 LEVELS, HALF LEVEL, LATCH, NOTES |
| Shift | SHIFT |

## 5. What had to change to make the public release build

The release notes say the build "will not work out of the box". This is the complete list of
reasons, and what was done about each. All of it lives on the `simulator-macos` branch.

### 5.1 AudIO was packaged differently than Kick expects

Kick includes two JUCE modules, `AudIO_juce` and `AudIO_constants`, and expects a
`juce_modules.cmake` in the AudIO root. The public AudIO ships one module (`AudIO/AudIO/AudIO.h`)
and no cmake file. Added:

- `AudIO/juce_modules.cmake`: registers the three modules.
- `AudIO/AudIO_constants/AudIO_constants.h`: forwards to AudIO and **reconstructs the constants
  that only ever lived in the internal module**. Their values are inferred from use, not copied:

  | Constant | Value | Reasoning |
  |---|---|---|
  | `MIDI::MIN/MAX/STEP` | 0 / 127 / 1 | 7-bit MIDI |
  | `MIDI::NUMERIC_MAX` | 128 | used as an exclusive bound in loops |
  | `MIDI::SCALER` | 1/127 | source comment "(MidiLevel / 127)" |
  | `MIDI::LEVEL_DEFAULT` | 100 | default pad/track level, also an upper knob bound |
  | `MIDI::DEFAULT` | 0 | default modulation amount; also passed as a slider interval, where 0 means continuous |
  | `MIDI::MIDDLE_C`, `MIDI::NOTE_DEFAULT` | 60 | |
  | `MIDI_CH::MIN/MAX/DEFAULT` | 1 / 16 / 1 | shown 1-based in the UI |
  | `POLYPHONY::MIN/MAX/DEFAULT` | 1 / 64 / 16 | `ISynthObject` allocates `MAX` voices up front |
  | `GAIN::MIN/MAX/STEP/DEFAULT` | 0 / 1 / 0.01 / 1 | linear gain into `AudIO::Panner`; knobs use a 0.01 step |
  | `GAIN::ROOT_2` | 0.7071 (-3 dB) | default metronome gain |
  | `PAN::MIN/MAX/STEP/DEFAULT` | -1 / 1 / 0.01 / 0 | `AudIO::Panner` documents -1 (left) .. 1 (right) |
  | `DB::MIN/MAX` | -100 / 0 | `AudIO::utils::linearTolog` returns -100 for silence |
  | `UNKNOWN_VALUE` | -1 | sentinel for an unattached `juce::Value` |
  | `AUDIO_SAMPLE_RATE` | 48000 | |
  | `AUDIO_BLOCK_SIZE` | 480 | must equal AudIO's memory-pool block (`BLOCK_SIZE` in `Daw/DawEngine.cpp`) |
  | `AUDIO_NUM_INPUTS/OUTPUTS` | 2 / 2 | |
  | `AUDIO_NUM_SEND_RETURNS` | 4 | sends 1-4 are referenced |
  | `AUDIO_NUM_SUBMIX` | 4 | the MIDI control UI lists "Submix 1..4" |
  | `AUDIO_NUM_AUX` | 2 | a guess, nothing in the code pins it |

  If any of these turn out wrong, this header is the only place to fix them.

- `AudIO/AudIO_juce/`: the JUCE glue, **reconstructed from how Kick calls it**, all in
  namespace `mt`:
  - `mt::AudioDevice`: `AudIO::IAudioDevice` on top of `juce::AudioDeviceManager`. Re-blocks
    through FIFOs if the host device's callback size differs from AudIO's 480-sample block.
  - `mt::ADSR`: linear attack/hold/decay/sustain/release envelope, lengths in samples.
  - `mt::Filter` + `FilterType`: stereo state-variable filter (low/high/band pass).
  - `mt::Recorder`, `mt::SampleRecorder`, `mt::Looper`: an AudIO object that records its input bus
    into a `juce::AudioBuffer`, with threshold-armed start, overdub, loop, reverse, preview, WAV
    save, and (Looper) an insert chain and single-level undo.
  The behaviour is plausible, not bit-exact with the original. Known simplifications: looper undo
  is one level; the five `Recorder::Mode` channel modes are interpreted as documented in the
  header.

- `AudIO/AudIO/Includes/Platform.h`: adds `<thread>` and `<chrono>`, which the DAW sources use
  but never included (other platforms got them transitively).

### 5.2 `juce_serialport` was removed

`modules/3rdParty/juce_serialport` had no license and was deleted, but `modules/3rdParty/CMakeLists.txt`
still lists it and `modules/input_events` uses it. Added an API-compatible **stub** at the same
path. It compiles everywhere and talks to nothing. That is correct for desktop and simulator
builds (the serial path is compiled out there). **A device build needs a real implementation**;
the stub emits a compiler warning in non-simulator Linux builds so this cannot be forgotten.

### 5.3 Small Kick fixes

- `modules/common/source/kick_Constants.h`: `EMPTY::String`; `modules/engine/interface/interface.h`:
  `EMPTY::SampleBuffer`. Both were used but defined nowhere.
- `modules/hardware/source/kick_WifiFirmware.cpp`: `#include <sys/stat.h>`.
- `modules/contracts/utils/keyboard/keyboard_detector.{h,cpp}`: keyboard map enabled on macOS.
- `apps/kick/CMakeLists.txt`, `apps/kickem/CMakeLists.txt`: `MICROPHONE_PERMISSION_ENABLED TRUE`,
  otherwise macOS kills the app when the engine opens an input.
- `modules/input_events/kick_SimulatorInterface.h` and the `SIM_BUILD` branch of
  `input_events.cpp`: the simulator interface now carries whole `CPktMgr::HWMsg_t` packets, the
  same type the rest of the app uses.
- `apps/kickem/`: rewritten. The old simulator referenced enums and widgets that no longer exist
  (`mt::Kick::HwControlCommands`, `sig::Signal`, `AppHome`), so it predated the move to the
  `contracts` module. The new one boots like `apps/kick/Main.cpp` and reuses its adapters.
- Root `.gitignore` for the CPM cache.

### 5.4 Still missing from the release (unchanged)

- The Myriad Pro font: `assets/Default/theme.xml` asks for `Myriad Pro Regular.ttf`. If the file is
  absent the app silently uses JUCE's default sans-serif. Drop any `.ttf` you have rights to into
  `Kick/assets/Default/` and name it in `theme.xml`.
- Factory samples and the General MIDI SoundFont: the sampler starts empty.
- The cross-compilation SDK: device builds still need the Buildroot SDK as described in
  `Kick/README.md`. None of the desktop changes alter the device code paths, except the serial
  stub (5.2), which must be replaced for a device build.

## 6. Troubleshooting

- **"Failed to build juceaide ... CGWindowListCreateImage is unavailable"**: you forgot
  `MACOSX_DEPLOYMENT_TARGET=12.0` on the configure, or configured once without it. Delete
  `Kick/build` and configure again with it.
- **"include could not find requested file: .../AudIO/juce_modules.cmake"**: `CPM_AudIO_SOURCE`
  is pointing somewhere else. Pass the absolute path to `0700-AAM/AudIO`.
- **Black screen or missing graphics**: the app was not started from `Kick/build`, so `./assets`
  does not exist.
- **No sound**: check the log line `AudioDevice: using '...'`. If it says "no audio device
  available", JUCE could not open a default output. Pick one in the app's audio settings or check
  the macOS output device.
- **Re-downloading JUCE every time**: keep the default CPM cache (`0700-AAM/.3rdPartySourceCache`)
  or set `-DCPM_SOURCE_CACHE=<dir>` consistently.
