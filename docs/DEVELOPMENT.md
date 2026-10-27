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
(STOP, bank A, MAIN, FULL LEVEL). The factory samples are not part of the release; since phase 2
the first project on the Projects screen is the synthesised Starter Kit (4.4), whose pads light
and play.

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
| `sim_sampler_looper.sh [OUTDIR]` | The phase 4 check (section 4.6): sampler record / preview / save to a pad / trim, and looper record / loop / overdub, measured on the audio probe. |
| `wav_measure.py FILE [--from S] [--to S] [--thr DB] [--onsets]` | Length, peak, RMS, extent above a threshold and burst onsets of a WAV (the probe or a recorded sample). Needs numpy. |

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
| `KICK_SIM_AUDIO_INPUT=tone`, `tone:<hz>`, `pulse`, `pulse:<ms>` or `<file>` | `mt::AudioDevice` replaces the device input (every channel) with a test source: a -12 dBFS sine (1 kHz by default), 50 ms 1 kHz bursts every 500 ms (or every `<ms>`) at -6 dBFS, or a looped audio file. The pulse train is counted in engine samples, so its onsets are exact in the probe. Use a period that does not divide the bar (e.g. `pulse:700`) to see a loop repeat: the pattern then repeats with the loop length, not the pulse period. Logged as `TestInput: ...`. |
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

### 4.4 Starter content (phase 2)

The public release ships empty demo and template projects and no samples, so a fresh checkout
now carries a small kit and a project that are ours to ship (CC0, `Kick/assets/StarterKit/LICENSE`).

**The kit.** `Kick/tools/make_starter_kit.py` synthesises everything with the Python standard
library from fixed seeds (no recordings, no numpy): running it again rewrites byte-identical
files. All WAVs are 48 kHz, 16-bit mono (the engine loads mono as two equal channels), about
1.4 MB in all, and they are committed so nothing needs Python at build time.

| Folder (`Kick/assets/StarterKit/`) | Contents |
|---|---|
| `Kit/` | 16 drums in pad order: 01 Kick, 02 Snare, 03 Closed Hat, 04 Open Hat, 05 Clap, 06 Rim, 07-09 Low / Mid / High Tom, 10 Crash, 11 Ride, 12 Shaker, 13 Cowbell, 14 Conga, 15 Clave, 16 Zap |
| `Instruments/` | `Keys C4.wav` (additive e-piano tone), `Bass C2.wav` (filtered saw / square) |
| `Loops/` | `Drum Loop 120.wav`, `Bass Loop 120.wav` (one bar at 120 BPM), `Chord Stab.wav`, `Riser.wav` |
| `Metronome/` | `Click.wav` (1.2 kHz, the beat) and `Click Accent.wav` (2 kHz, the bar) |

Regenerate with `python3 Kick/tools/make_starter_kit.py`, then configure again (the copies into
`Kick/build` happen at configure time, see below).

**The project.** The same script writes `Kick/demos/Starter Kit/Starter Kit.kickPrj`, its 120x70
`Starter Kit.png`, and the 180x130 tile `assets/Default/recent_project/Starter Kit.png` (the
Projects screen draws a demo's tile from the theme, named after the PNG in the project folder).
Demos are sorted by the number in their name and "Starter Kit" has none, so it is the first tile
on the Projects screen and selected on start; PUSH (or a touch) loads it.

| Program | Type | Pads |
|---|---|---|
| Starter Drums | Drum (0) | bank A: the 16 kit sounds; B: keys, C: bass, chromatic from C (pad n = n-1 semitones up); D: pads 1-4 the loops / stab / riser, 5-16 kit sounds 1-12 a fifth lower |
| Starter Keys | Drum (0) | keys C3-B6 over banks A-C, bass C2-D#3 on D. A drum program because the keygroup engine is a stub in this release (`KeygroupProgram::noteOn` returns before playing anything; the SoundFont is not involved) |
| Starter Loops | Clip (4) | pads 1-4 the loops, stab and riser, one-shot (`padPlay="1"`) |

`Sequence 01` (4 bars, 120 BPM, looping) has three tracks: **Drums** (Starter Drums, a pattern
that plays every one of the 16 kit pads), **Keys** (Starter Keys, a chord on beats 1 and 3,
level 70) and **Loops** (Starter Loops, the bass loop every bar, level 80). Pads play the
selected track's program (track 1 after loading). Bank keys move the pads by 16 notes within that
program, so banks B-D of Starter Drums are what a pad reaches after pressing B, C or D; to play
Starter Keys or Starter Loops from the pads, select their track.

**Project schema** (learned from `modules/engine/` and from what the app writes back on SAVE;
the `IDs::` names are in `interface/source/mt_audio_ids.h`; properties left out take the engine
defaults):

```xml
<ROOT name=".." manufacturer="Musictribe" version="0.1" ...>
  <BUSES/>
  <FILEMANAGER id="..">                        <!-- MTAudioAssetManager -->
    <ASSETS>
      <FILE id=".." path="StarterKit/Kit/01 Kick.wav">   <!-- one per sample file -->
        <SLICE id=".."/>                        <!-- slice 0; more slices = regions or tunings -->
        <SLICE id=".." semi="7"/>               <!-- ASoundSlice: semi, fine, gain, pan, loopStart/End, reverse, loop -->
      </FILE>
    </ASSETS>
    <AUDIO/><EXPORTS/>
  </FILEMANAGER>
  <TEMPOS><TEMPO bpm="120.0" num="4" den="4"/></TEMPOS>
  <PROGRAMS>
    <PROGRAM id="skPrg001" name="Starter Drums" type="0">   <!-- MTProgramType: 0 Drum, 1 Keygroup, 2 Midi, 3 Audio, 4 Clip, 6 CV -->
      <COLOURS clr16="6" .../>                  <!-- pad colour ids (DeviceControlContracts::PadColourIds); the default table colours pads 1-16 only -->
      <CLIPS>
        <CLIP keyNote="0" minNote="0" maxNote="0">         <!-- one per pad; keyNote = pad - 1 + 16 x bank (0-127) -->
          <LAYERS>                              <!-- always 4 LAYERs -->
            <LAYER source="StarterKit/Kit/01 Kick.wav" slice="0"/>  <!-- source = FILE path string, slice = SLICE index -->
            <LAYER/><LAYER/><LAYER/>
          </LAYERS>
        </CLIP>
      </CLIPS>
    </PROGRAM>
  </PROGRAMS>
  <SEQUENCES>
    <SEQUENCE id=".." name="Sequence 01" length="4" loop="1" loopStart="0" loopEnd="4" isMasterBPM="1">
      <TRACK id=".." name="Drums" program="skPrg001" usingSequenceLength="1" length="4" level="100">
        <NOTELIST>
          <NOTE id="1" no="0" velocity="120" start="0" length="24"/>  <!-- ticks at 96 PPQN: 16th = 24, bar = 384 -->
        </NOTELIST>
      </TRACK>
    </SEQUENCE>
  </SEQUENCES>
</ROOT>
```

Things that matter: a track's `length` defaults to 0, so set `usingSequenceLength="1"` (or a
length). The drum voice ignores a CLIP's own `semi` / `fine` (only the program's pitch reaches
`DrumVoice`), so a tuned pad is a tuned SLICE of the file; that is how the starter kit does its
chromatic pads. The sound cache and the layers key samples by the full path string, so FILE
`path` and LAYER `source` must be the same string. On load the app adds `Project`, `LOOPER`,
`SAMPLER`, `SONGS`, per-slice loop points and `COLOURS mode`, and rewrites relative paths as
absolute (below); SAVE (SHIFT + BROWSE) then writes back the same programs, pads, slices, tracks
and notes. A demo is loaded and saved in place, i.e. in `Kick/build/demos/`.

**Where samples resolve.** Sample paths are stored as strings and opened with `juce::File`, so a
project in the repository cannot hold absolute paths. On desktop builds only (`SIM_BUILD` or
`JUCE_MAC`), `MTAudioManager::loadProject` now resolves every relative FILE `path` and SLICE /
LAYER `source` first against the project's folder, then against the samples root
(`getAppDirs( MTSamples )`, `./data/samplesKick/` on desktop), and replaces it with the absolute
path before anything reads it. Absolute paths, which is all the device ever writes, are left
alone, and the device build does not compile the code. CMake (`apps/kickem`, and `apps/kick` on
macOS) copies `assets/StarterKit` to `build/data/samplesKick/StarterKit` at configure time, so the
kit also shows in the sample browser, and the project names its samples `StarterKit/...`.

**Metronome.** The factory click files named by `KickMetronome` (`met-1-bar.wav`,
`Perc_*_hi.wav`, ...) are not in the release. `mt::Metronome::setSource`
(`audio_program/synth/metronome.cpp`) now falls back to `assets/StarterKit/Metronome/Click.wav`
when the configured file does not exist, and loads `Click Accent.wav` beside it for the bar
(note 0 from `triggerBar`; note 1 from `triggerBeat` keeps the plain click). With the factory
files present nothing changes. Turn it on with the Metronome button in the top bar (ENABLE On);
it clicks while the sequencer plays, at the selected RATE (1/4 by default).

**Measured** (2026-10-27, `KICK_SIM_AUDIO_INPUT=<silent wav>`, probe on, everything through
`sim_ctl.sh` with the screen locked): Pads 1-16 in bank A give 16 separate hits at -9.2 to -19.0
dBFS peak from a -120 dBFS floor, each with its own brightness (from 140 zero crossings per second
for the kick to 11300/s for the open hat); bank B pads 1 / 8 / 16 play the keys at C4 / G4 / D#5
(260 / 390 / 620 zero crossings per second), bank C the bass at C2 / G2 / D#3, bank D the drum
loop, the bass loop and the shaker a fifth down; PLAY runs the sequence with all three track
meters moving; with the metronome on, PLAY START puts a 2 kHz accent on the bar and a 1.2 kHz
click on each following quarter (0.5 s apart), 20-30 dB above the neighbouring band. Bank A pads
light in the kit's colours (`SimSurface: PAD cmd=6 data=12,13,14,9,...`), B sky blue, C green,
D pink / orange.

Known differences, left to later phases: a pad plays about 8 dB under the sample's level (-9.2
dBFS for the kick normalised to -1 dBFS, velocity 100; the gain staging Phase 4 also measured).
While the sequence plays, the master peaks at about +3 dBFS although the keys track is at level 70
and velocity 64, so track level / note velocity may not be applied in sequencer playback, or
notes are triggered more than once (Phase 3). With the default input device, loading any project
with programs starts input monitoring through the looper / recorder path: on a laptop the built-in
microphone picks up the speakers and the output howls to +30 dBFS within seconds. Use
`KICK_SIM_AUDIO_INPUT` (a silent WAV, or `tone`) or headphones.

Fixed on the way: PUSH on the Projects screen loaded an empty project (`onMouseDown` was handed
`juce::File()`); for demos and templates it now takes the item's own folder
(`kick_RecentProject.cpp`).

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

### 4.6 Sampler, sample edit and looper (phase 4)

All of this runs through `sim_ctl.sh`, so it works with the screen locked;
`Kick/tools/sim_sampler_looper.sh` performs it end to end and prints the measurements. Start the
simulator with a test input and the probe, e.g.
`KICK_SIM_AUDIO_INPUT=tone KICK_SIM_AUDIO_PROBE=/tmp/p.wav Kick/tools/sim_launch.sh`.

| Step | Commands | What happens |
|---|---|---|
| Load a project | `tap 110 150` (the first tile) | A touch or PUSH. PUSH used to load an empty project with no directory (see 5.1, Phase 4 findings); fixed in phase 2 (4.4). Since phase 2 the first tile is Starter Kit, which already has programs and tracks. |
| Make a pad playable | `tap 76 108` (the Sequence tab: its name is " Sequence "), `tap 993 108`; `click Program`, `tap 993 108`, `click Drum`, `click Apply`; `click Track`, `tap 993 108`, `tap 33 222`, `click "Program 01"`, `click Apply` | MAIN screen tabs and their "+": a sequence (it gives the looper its tempo), a drum program, and a track that plays it. Pads play the selected track's program. |
| Record | `press SAMPLER`, `turn P1 -18` (Max Length 20 s -> 2 s; P1 = seconds, P2 = minutes), `click recordButton` | "Waiting for Signal" until the input crosses the threshold, then records and stops by itself at Max Length; touching the button again stops early. |
| Preview | `click Play` | Plays the take once through the master. |
| Save to a pad | `click Save`, `click Apply` | KEEP OR DISCARD SAMPLES: name, PROGRAM (Program 01), ASSIGN PAD (A01). Writes `<project dir>/../<name>.wav` (`New Sample.wav` beside the project folder), adds it to the project's samples and assigns it to the pad. |
| Play it | `press MAIN`, `press "Pad 1"` | |
| Trim | `press "SAMPLE EDIT"`, `click Chop`, `turn P4 24` (START + 24000 samples: P1/P2/P3/P4 = 1/10/100/1000 samples per detent), `click Tune`, `click Process`, `click Discard`, `click Apply` | Discard keeps START..END and rewrites the file. A program that already holds the file keeps playing the old audio until the project is reloaded (see 5.1). |
| Save the project | `press SAVE` (SHIFT + BROWSE) | |
| Looper | `press LOOP` (SHIFT + SAMPLER), `press "key 42"` (RECORD), `press "key 43"` (OVERDUB), `press "key 45"` (PLAY), `press "key 44"` (STOP) | LENGTH is in bars of the selected sequence's tempo (4 bars at 120 BPM = 8 s). RECORD records one pass from the top (or punches in at the play position), then plays it round; OVERDUB adds to the loop while it plays, until PLAY (punch out, keep looping in time) or STOP. |

Measured on 2026-10-27 (`sim_sampler_looper.sh`, numbers in the phase 4 commit message):
the tone take is 96000 frames = 2.000 s at -12.00 dBFS peak / -15.01 dBFS RMS, exactly the
input; the preview and Pad 1 each play 2.000 s with silence before; Discard with START = 24000
leaves 72000 frames = 1.500 s, which is what Pad 1 plays after the project is reloaded; the
looper's playback repeats with a period of 384000 samples = 8.000 s (4 bars at 120 BPM),
successive periods sample-identical, and an overdub pass adds a second, offset pulse set.

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

  **Phase 4 findings** (sampler and looper made to work in `KT_KickEM`, see 4.6):
  - `AudIO/AudIO/Includes/AudioObject.h`: the object-id counter was `static uint32_t
    audioObjectId` at namespace scope, i.e. one counter per translation unit. Kick builds AudIO,
    AudIO_juce and its own engine as separate JUCE module TUs, so objects created in different
    TUs got the same ids, and `Design::insertObject` refuses an id it already has: the sampler's
    `mt::SampleRecorder` got id 2, already an engine bus, and was never processed (its
    `connectInputTo` returned false). Now one `inline std::atomic<uint32_t>` counter for the
    program (sampler and looper then get ids 33 / 34 and patch). This can affect any object Kick
    creates in its own TU (`ISynthObject`, `AudioPreview`), not only the recorder.
  - `mt::Recorder` never called `DawRouting::setID( getId() )` (the original `AudIO::DRecorder`,
    Kick's `ISynthObject` and `AudioPreview` all do), so `initAudioRouting()` /
    `connectInputTo()` used an uninitialised routing id: no input, no output.
  - `onRecorderLengthChanged` now fires as in the public `AudIO::AudioRecorder`: with the set
    length, and with the recorded length when recording stops; it used to fire every block with
    the recorded length, which made the sampler's timer show progress x recorded length.
  - A sampler take stopped early is truncated to what was recorded (as `AudioRecorder::stop()`
    did), so the progress reads 1.0, and the waveform and saved file have no silent tail. The
    "OUT MONO" modes are saved as one channel. Recording always runs forward; `Direction` is the
    playback direction (only the looper's reverse button sets it).
  - Playback gain: constant-power pan normalised to unity in the centre, so a take plays back at
    the level it was recorded (it was -3 dB).
  - Looper (`loopMode`): the buffer is the loop (LENGTH bars of the selected sequence's tempo),
    one position is shared by playback and recording and wraps at the loop length, so the
    period is exactly the loop length. RECORD from a stopped looper records one pass from the top
    and then plays it; while playing it punches in at the play position. OVERDUB adds to the loop
    while it plays. `setRecord( true )` clears the overdub flag (the screen only sets it for
    OVERDUB, so a later RECORD used to overdub). `setPlay()` / `play()` (the screen's PLAY /
    PLAY START keys and buttons) punch out of recording and keep a running loop in time instead of
    restarting it; a stopped loop starts from the top. `looping` is on by default for the
    looper. The screen calls `startRecording` twice per key press (`setRecord`, then `start`):
    only the first places the take.
  - `saveToFile` logs where it wrote and why it could not.
  - Not changed, original Kick behaviour found on the way:
    - PUSH on the Projects screen calls `onMouseDown( selectedIndex, juce::File() )`
      (`kick_RecentProject.cpp`), i.e. loads an empty project with no directory; the sampler then
      tries to save to `/New Sample.wav`. A touch on the tile loads the project properly.
      Fixed in phase 2: for demos and templates the handler now uses the item's own folder.
    - A pad's sample source is a `CachedValue` path: after SAMPLE EDIT rewrites the file,
      assigning the same file again does not reload it, so the pad plays the old audio until
      the project is reloaded.
    - The looper's length comes from the selected sequence's tempo; with no sequence it is 1 s
      whatever LENGTH says (`LoopRecorder::getRecordingLength`).
    - With the threshold at its default (-100 dB) a looper take starts at the key press.
    - A pad plays a full-scale take about 8 dB lower than recorded (-20.2 dBFS for a -12 dBFS
      take at velocity 100): voice / track gain staging, not the recorder.
  - Simulator tooling: `sim_ctl.sh turn P1..P4|JOG N` turns encoders (the sampler's Max Length
    and the sample editor's START need them; the knobs are told apart by geometry, the jog wheel
    being the largest); `sim_ctl.sh click` skips zero-size components (dialog contents such as
    CREATE PROGRAM carry unused 0x0 `Apply` / `Cancel` buttons that shadowed the real ones);
    `KICK_SIM_AUDIO_INPUT=pulse:<ms>`; `sim_sampler_looper.sh` and `wav_measure.py`.

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
- Phase 2 (details in 4.4): `engine/audio_engine/engine/mt_audio_manager.cpp` resolves relative
  sample paths on load (desktop only, `SIM_BUILD` / `JUCE_MAC`); `engine/audio_program/synth/
  metronome.{h,cpp}` fall back to the starter click set with a bar accent when the configured click
  file is missing; `kick_ui/recent_project/kick_RecentProject.cpp` makes PUSH load the selected
  demo / template instead of an empty project; `apps/kickem/CMakeLists.txt` (and
  `apps/kick/CMakeLists.txt` on macOS) copy the kit to `build/data/samplesKick/StarterKit`.

### 5.4 Still missing from the release

- The Myriad Pro font: `assets/Default/theme.xml` asks for `Myriad Pro Regular.ttf`. If the file is
  absent the app silently uses JUCE's default sans-serif. Drop any `.ttf` you have rights to into
  `Kick/assets/Default/` and name it in `theme.xml`.
- Factory samples, metronome sounds and the General MIDI SoundFont. Phase 2 replaces the samples
  and clicks with the synthesised starter kit (4.4). The factory demo / template projects are still
  empty `<ROOT>` documents, and keygroup programs stay silent: `KeygroupProgram::noteOn` is a stub
  in this release.
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
