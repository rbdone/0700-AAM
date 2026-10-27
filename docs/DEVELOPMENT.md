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
| `Kick` | The monolithic app, the one the device runs | Yes. Hardware input comes from the keyboard map (section 4.3) |
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
  banks E-H). On the unit you hold SHIFT and press the key, and the control-surface firmware
  reports the secondary id (SHIFT + MAIN arrives in the app as `EN_GRID`). The simulator does the
  same: SHIFT is a toggle, keys pressed while it is on send their secondary id, and clicking a
  caption performs the whole hold-press-release for you.
- The jog wheel and P1-P4 are endless encoders, as on the unit: the app only ever receives a
  direction and a step count (`DclContractsHelper::getKnobValue`), never a position. Drag to
  turn; the small PUSH key under the wheel is the wheel's click (ENTER). MASTER and REC are
  analogue pots on the unit and do nothing here.
- Every pad and key is a `juce::Button` with its printed name, so the panel is scriptable through
  macOS accessibility: `Kick/tools/sim_press.sh "Pad 1" MAIN PUSH PLAY GRID` presses controls by
  name while the simulator runs; a caption name such as GRID is performed as SHIFT + key. The
  terminal needs Accessibility permission in System Settings.
- `/goal` in Claude Code (`.claude/commands/goal.md`) is a phased plan for making every control
  behave as on the device, starting with synthesised starter content.
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

### 4.2 Scripting, screenshots and audio measurement

Everything a check needs is in `Kick/tools/` and works from any directory:

| Script | What it does |
|---|---|
| `sim_launch.sh [LOG]` | Starts `KT_KickEM` from `Kick/build`, logs to `LOG` (default `Kick/build/logs/sim.log`), waits for the control-surface handshake (`Send Comm Live`) and prints the pid. |
| `sim_press.sh NAME...` | Presses controls by printed name (see 4.1). |
| `sim_shot.sh [--all] OUT.png` | Captures the panel window by window id (`screencapture -l`), so it works while other windows cover it. `--all` also captures the app's popup windows as `OUT-<n>.png`. |
| `sim_quit.sh` | Stops the simulator and releases the lock. |
| `sim_click.sh NAME...`, `--at X Y`, `--list [FILTER]` | Touches the **screen** rather than the panel: clicks an on-screen component by its accessibility title (Preferences tabs `Info`, `Hardware`, `CV`, `Ethernet`, `WiFi`; buttons such as `Firmware Update`, `Resources`), or pixel (X,Y) of the 1024x600 Kick screen wherever the window has scaled it; `--list` prints every accessible element with its frame. A real mouse click (CGEvent) through a small Swift helper (`sim_ax.swift`, compiled on first use into `Kick/build/sim_ax`). Needs Accessibility permission and an unlocked, visible window. |
| `sim_ctl.sh press\|click\|tap\|shot\|list\|echo ...` | The same jobs **from inside the app**, with no accessibility and no window server, so it keeps working while the Mac is locked or asleep or the window is covered. `press NAME` = panel control by printed name (as `sim_press.sh`), `click NAME` = on-screen component by title (as `sim_click.sh`), `tap X Y` = Kick-screen pixel, `shot OUT.png` = renders the panel to `OUT.png`, the Kick screen at its native 1024x600 to `OUT-screen.png` and each popup window to `OUT-<n>.png`, `list [FILTER]` = titled components, `echo TEXT` = marker in the log. Each command waits for the app's acknowledgement (`SimControl: ok <n>`), so a blocked message thread shows up as a timeout and exit status 1. `turn KNOB STEPS` turns encoder `P1`-`P4` or `JOG` by signed detents and `slider VALUE` sets the slider (0-100); see the next row. |
| `sim_turn.sh P1..P4\|JOG STEPS`, `sim_turn.sh SLIDER V` | Shorthand for `sim_ctl.sh turn` / `slider`: the encoders and the slider are not buttons, so `sim_press.sh` cannot reach them. Each detent is one `EN_PKT_TYPE_KNOBS` packet of magnitude 1 (cmd `EN_KNOB_QLINK` for P1-P4, `EN_KNOB_ROTARY` for the jog wheel), sent through the knob's own `onStep`, the same path as a mouse drag; positive is clockwise. The slider sends one `EN_PKT_TYPE_SLIDER` / `EN_SLIDER_TOUCH_INPUT` packet with the absolute value. (Arrow up/down in 4.3 also turn the jog wheel, but only while the window has keyboard focus.) |

`sim_ctl.sh` talks to `apps/kickem/simulator/kick_SimControl.cpp`, which `sim_launch.sh` always
enables (`KICK_SIM_CONTROL=Kick/build/.sim_ctl`; the log path is kept in `Kick/build/.sim_log`).
Buttons are pressed with `juce::Button::triggerClick()` (exactly what a macOS accessibility press
does), other components get `mouseDown` / `mouseUp` at the touched point. Events are delivered
to the component directly rather than through the window's `ComponentPeer`, because JUCE's peer
dispatch asks macOS whether the window is the topmost one at that point, which is false while
the screen is locked. A touch on a component behind a modal dialog is refused with
`blocked by modal component ...` rather than silently passed through.

Only one `KT_KickEM` can run on a machine (the app refuses a second instance, and the press
helper and audio device assume one). `sim_launch.sh` therefore holds a machine-wide lock
(`/tmp/kt_kickem.lock`, override with `KICK_SIM_LOCK`) and waits for another session's
simulator to quit; always pair it with `sim_quit.sh`.

Opt-in environment variables, all off by default:

| Variable | Effect |
|---|---|
| `KICK_SIM_AUDIO_PROBE=<file.wav>` | `mt::AudioDevice` writes the engine's master output to that WAV (32-bit float) and logs `AudioProbe: t=<n>s peak=<x>dBFS rms=<y>dBFS` once per second of audio. Silence logs `-120.0dBFS`. |
| `KICK_SIM_AUDIO_INPUT=tone`, `tone:<hz>`, `pulse` or `<file>` | `mt::AudioDevice` replaces the device input with a test source: a -12 dBFS sine (1 kHz by default), 50 ms 1 kHz bursts every 500 ms at -6 dBFS, or a looped audio file. Logged as `TestInput: ...`. |
| `KICK_SIM_CONTROL=<file>` | The simulator polls `<file>` for `sim_ctl.sh` commands (see above). `sim_launch.sh` sets it to `Kick/build/.sim_ctl`; without it nothing is polled. |
| `KICK_SIM_LOG_SURFACE=1` | The simulated surface logs every packet the app sends it: `SimSurface: LED cmd=0 data=44,1` (type, command, payload). LED commands are `DeviceControlContracts::LedCmd` (0 = set single LED: id, state), pad commands `PadCmd`. |

Input from the panel is always logged by the app as `Data[0] From DCL:<id> cmd From DCL : <cmd>
type From DCL : <type>`, followed by the screens the packet reached.

### 4.3 Keyboard control in both apps

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

### 4.5 Hardware-only screens in the simulator

Some screens exist only to drive the unit's own hardware or Linux. In the simulator each now opens,
shows an explicit **"<feature> - Not available in the simulator"** panel that says what the unit
does there, and closes normally. The panel (`ui::SimUnavailable`,
`modules/kick_ui_controls/source/kick_SimUnavailable.h`) swallows touches, so the device controls
underneath cannot be reached, and logs `SimUnavailable: <feature> shown` / `hidden`. All of it
is behind `#ifdef SIM_BUILD`; the device build is unchanged.

**How `SIM_BUILD` reaches the modules.** The Kick modules are JUCE modules
(`juce_add_modules` in `modules/CMakeLists.txt`), i.e. CMake INTERFACE libraries: their `.cpp`
files are compiled inside each app target that links them, not once into a shared static library.
`apps/kickem/CMakeLists.txt` defines `SIM_BUILD=1` privately on `KT_KickEM`, so every module is
compiled with it in the simulator (objects land in `build/apps/kickem/CMakeFiles/KT_KickEM.dir/__/__/modules/...`)
and without it in `Kick`.

| Screen | How to reach it | Device dependency | Before (observed on macOS) | Simulator behaviour now |
|---|---|---|---|---|
| Firmware update | PREFS > Info > Firmware Update | `.swu` image on `/mnt/sdcard` or `/mnt/flash`, PC app over USB FunctionFS (`/dev/usb-utp0`), `swupdate`, `reboot` | Opened, started the image-scan thread, then claimed "You are on latest Version / No updates available" | Notice over the update list; no scan thread, no USB gadget; header arrow and Go Back close it |
| Wi-Fi | PREFS > WiFi | `wlan0` via `iwlist`, `ip link`, `wpa_supplicant`, `dhclient`, `killall` | DISABLED / ScanWifi / empty list / 0.0.0.0; Scan silently returned nothing; Disconnect would have run `killall -9 wpa_supplicant dhclient` on the host | Notice over the whole tab; `WifiManager` never spawns a command (logs `SimUnavailable: Wi-Fi command not run: ...`) |
| Ethernet | PREFS > Ethernet | `eth0` via `ifconfig`, `udhcpc`, `ip route`, `/sys/class/net/eth0` | Showed ENABLED / AUTOMATIC and 0.0.0.0 everywhere (off Linux `isInterfaceUp()` returns true) | Notice over the whole tab |
| CV / Gate | PREFS > CV, and its CV CALIBRATION screen | DAC over I2C (`/dev/i2c-3`, `mt::CV`) | Eight start-note selectors and a calibration screen whose +/-/Reset/Apply silently did nothing | Notice over the whole tab, so calibration cannot be opened |
| System resources (storage) | Resources button (chip icon) in a screen header, e.g. Preferences | `/proc/stat`, `sysinfo`, `df` on `/home/Kick`, `/mnt/sdcard`, `/mnt/flash` | CPU, RAM, internal memory "0.00 %" from structs that are never filled off Linux; SD / USB rows absent | Notice over the meters; the 1 Hz refresh is skipped; Purge / OK still work |
| SD card / USB drive in File Browser | BROWSE | Polls `/dev/mmcblk1`, `/dev/sda`; browses `/mnt/sdcard`, `/mnt/flash` | Only Internal / Factory Sample Library / Demos, no hint why | Small notice below the Location rows; the rest of the browser works |
| Date and time (Hardware tab) | PREFS > Hardware | Sets the system clock | `setDate` / `setTime` passed strings such as `2026-10-27` to `std::system()` on the host | `HwPref::setDate` / `setTime` log `SimUnavailable: system date/time not changed` and return. The rest of the tab (pad threshold, velocity curve, tap tempo) is a normal screen and works |
| LINK, LINK EDIT | LINK key; SHIFT + SAMPLE EDIT | None. These are the **Q-Link** knob controls, not Ableton Link (there is no Ableton Link code in the repository) | LINK EDIT opens "Q-Link Edit"; LINK (id 47) reaches `KnobEdit::knobLongPress()`, which picks the Q-Link page and starts a timer, with no visible change on the main screen | Unchanged: they are ordinary screens and work |
| Camera | - | None in software | The camera module is printed on the panel only; nothing in `Kick/` reads a camera | Nothing to stub |

Verified with `sim_ctl.sh` on a locked Mac (see 4.2): each screen was opened, captured, closed,
then MAIN and Pad 1 were pressed and their packets logged
(`Data[0] From DCL:8 ... type 1`, `Data[0] From DCL:0 ... type 3`). Touching a covered device
control (WiFi > ScanWifi, CV > CV CALIBRATION) lands on the notice
(`SimControl: touched Wi-Fi is not available in the simulator`). Before the change none of these
screens hung or blocked the UI. The Release build compiles `jassert` out, so none could assert
either; the problems were false or empty states and host shell commands.

Not covered: the `SWUpdateWindow` progress screen and the CV/Gate program editor
(`program_edit_ui/sources/cvgate`). The first is reachable only after an image validates. The
second needs a CV program, and the shipped projects have none.

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
