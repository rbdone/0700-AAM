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
- `/goal` in Claude Code (`.claude/commands/goal.md`) is the phased plan that made every control
  behave as on the device (section 7 says how far it got). `/goal-serial` and `/goal-vm` are the
  follow-ons: the device's real serial path against the simulated panel, then the Linux build in
  an ARM virtual machine.
- Popups follow the screen. On the unit the Kick screen is the whole display, so the app puts
  its full-screen popups (dialogs, the loading spinner: `kick_utils_popup.cpp`) and notification
  bubbles (`kick_utils_bubble.cpp`) on the desktop at display coordinates. Under `SIM_BUILD` they
  are added to the screen's root component instead (`ui::simScreen()`, `kick_ControlBase.h`, set
  by `kick_Simulator.cpp`), so they appear inside the bezel, scaled with it, whichever app is in
  front. While such a dialog is modal, the panel's keys, pads and encoders still reach the app,
  as the hardware keys do. `Kick/tools/sim_overlays.sh` checks this (14 checks, about 25 s).

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
| `sim_ctl.sh press\|click\|tap\|shot\|list\|echo ...` | The same jobs **from inside the app**, with no accessibility and no window server, so it keeps working while the Mac is locked or asleep or the window is covered. `press NAME` = panel control by printed name (as `sim_press.sh`), `click NAME` = on-screen component by title (as `sim_click.sh`), `tap X Y` = Kick-screen pixel, `shot OUT.png` = renders the panel to `OUT.png`, the Kick screen at its native 1024x600 to `OUT-screen.png` and each popup window to `OUT-<n>.png`, `list [FILTER]` = titled components, `windows` = the desktop windows, the popups and bubbles over the screen and the screen, each with its bounds on the Mac's display, `echo TEXT` = marker in the log. Each command waits for the app's acknowledgement (`SimControl: ok <n>`), so a blocked message thread shows up as a timeout and exit status 1. `turn KNOB STEPS` turns encoder `P1`-`P4` or `JOG` by signed detents and `slider VALUE` sets the slider (0-100); see the next row. |
| `sim_turn.sh P1..P4\|JOG STEPS`, `sim_turn.sh SLIDER V` | Shorthand for `sim_ctl.sh turn` / `slider`: the encoders and the slider are not buttons, so `sim_press.sh` cannot reach them. Each detent is one `EN_PKT_TYPE_KNOBS` packet of magnitude 1 (cmd `EN_KNOB_QLINK` for P1-P4, `EN_KNOB_ROTARY` for the jog wheel), sent through the knob's own `onStep`, the same path as a mouse drag; positive is clockwise. The slider sends one `EN_PKT_TYPE_SLIDER` / `EN_SLIDER_TOUCH_INPUT` packet with the absolute value. (Arrow up/down in 4.3 also turn the jog wheel, but only while the window has keyboard focus.) |
| `sim_ctl.sh down NAME [VEL]`, `up NAME`, `hold NAME SECS [VEL]` | Hold a pad or key (section 4.9): `down` presses it and leaves it pressed, `up` releases it, `hold` does both with SECS in between and answers after the release. Pads take a velocity (1-127; the default is the pad's last one, 100 at start), e.g. `hold "Pad 1" 0.1 30` is one soft hit; `down ERASE`, `press "Pad 3"`, `up ERASE` is ERASE + pad. Packets are sent directly (no mouse state), so it works with the screen locked. |
| `sim_ctl.sh repeat MS N NAME`, `clickat NAME FX FY`, `status` | `repeat` presses a panel control N times MS ms apart on an absolute schedule timed inside the app (TAP TEMPO at a known tempo); `clickat` touches a titled component at a fraction of its size (the On half of the METRONOME dialog's ENABLE: `clickat ENABLE 0.75 0.6`); `status` prints the engine's transport state, BAR:BEAT:TICK (1-based, as on screen), master BPM, the MIDI clock's tempo, the record mode and the metronome switch, e.g. `playing bbt=1:03:068 bpm=120 clock=120.0 rec=off metronome=off` (section 4.7). |
| `sim_smoke.sh [--no-build] [OUTDIR]` | The smoke test (section 4.10): build, boot, load the Starter Kit with PUSH, play four bars, stop; fails if the engine stays silent, clips, or the LED feedback does not arrive. |
| `sim_overlays.sh [OUTDIR]` | Popups and bubbles appear inside the simulated screen, never as separate windows: the loading spinner, a dialog, nested dialogs, a notification bubble, and a panel key clicked while a dialog is modal (section 4.1). |
| `sim_pads.sh [OUTDIR]` | The phase 5 check for pads, modifiers, encoders and the slider (section 4.9). |
| `sim_transport.sh [OUTDIR]` | The phase 3 check (section 4.7): transport keys, BAR:BEAT:TICK, gain staging, RECORD / OVERDUB, TAP TEMPO and the metronome, measured on the audio probe. |
| `sim_sampler_looper.sh [OUTDIR]` | The phase 4 check (section 4.6): sampler record / preview / save to a pad / trim, and looper record / loop / overdub, measured on the audio probe. |
| `wav_measure.py FILE [--from S] [--to S] [--thr DB] [--onsets [--freqs]] [--channels]` | Length, peak, RMS, extent above a threshold and burst onsets of a WAV (the probe or a recorded sample); `--channels` adds per-channel peak / RMS, `--freqs` the dominant frequency and peak of the 40 ms after each onset. Needs numpy. |

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
| `KICK_SIM_AUDIO_INPUT=silence`, `tone`, `tone:<hz>`, `pulse`, `pulse:<ms>`, `<file>` or `device` | `sim_launch.sh` sets `silence` when the variable is unset (see 4.7, "Input and feedback"); `device` keeps the real input. `mt::AudioDevice` replaces the device input (every channel) with a test source: digital silence (use it on laptop speakers, see 4.4), a -12 dBFS sine (1 kHz by default), 50 ms 1 kHz bursts every 500 ms (or every `<ms>`) at -6 dBFS, or a looped audio file. The pulse train is counted in engine samples, so its onsets are exact in the probe. Use a period that does not divide the bar (e.g. `pulse:700`) to see a loop repeat: the pattern then repeats with the loop length, not the pulse period. Logged as `TestInput: ...`. |
| `KICK_SIM_FRESH=1` | `sim_launch.sh` first restores `Kick/build/demos` and `Kick/build/templates` from `Kick/demos` and `Kick/templates`. SAVE writes into those build copies, so without it one check's saved state (for example a pad level of 0) leaks into the next run. The check scripts set it. |
| `KICK_SIM_CONTROL=<file>` | The simulator polls `<file>` for `sim_ctl.sh` commands (see above). `sim_launch.sh` sets it to `Kick/build/.sim_ctl`; without it nothing is polled. |
| `KICK_SIM_LOG_NOTES=1` | The sequencer logs every note it plays: `SimNote: track 'Drums' tick 0 pos 0 note 0 velocity 94 id 1` (tick from the clock, position in the track, effective note and velocity, note id). `SIM_BUILD` only. |
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

`Sequence 02` (2 bars, its own 100 BPM, an off-grid "Drums Live" track and the drum loop) was
added in phase 5 for NEXT SEQ, MASTER and TC; see 4.8.

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
*Phase 3 (4.7):* the pad level and the clipping are explained and fixed; notes are not triggered
twice; and loading a project does **not** monitor the input (measured with a pulse test input and
with the real microphone), see 4.7.

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
| Load a project | `tap 283 150` (Demo 01, the second tile; `tap 110 150` is the first, Starter Kit) | A touch or PUSH. PUSH used to load an empty project with no directory (see 5.1, Phase 4 findings); fixed in phase 2 (4.4). Since phase 2 the first tile is Starter Kit, which already has programs and tracks. |
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

### 4.7 Transport, tempo and metronome (phase 3)

`Kick/tools/sim_transport.sh [OUTDIR]` (default `Kick/build/logs/phase3`) does all of this on a
fresh build copy of the Starter Kit in one session of about two minutes, prints the
measurements, leaves the app log, probe WAV, `events.txt` and screenshots in OUTDIR and exits 1
if a check fails. It runs with the screen locked (everything is `sim_ctl.sh`).

| Key | What it does now | Where |
|---|---|---|
| PLAY | Plays from the current position: after STOP it resumes where it stopped. | each screen's `handleCommand` -> `setSequencePlay( seq, false, true )` |
| PLAY START | Plays from the top (bar 1, tick 0), also after STOP. It used to resume like PLAY after STOP. | `MTAudioTransportManager::play( Seq, FromBeginning, Now )` -> `startPlayback( Seq, false )` |
| STOP | Stops (pauses: the position is kept for PLAY); voices are cut; ends a RECORD / OVERDUB take and disarms it; RECORD / OVERDUB LEDs off. | `setSequencePause()`, `KickHome::handleCommand` |
| RECORD | Arms (LED 42) while stopped; PLAY / PLAY START starts the take. While playing it punches in at once. Records pad hits into the **selected track** of the playing sequence. Replace: the track's old notes are not played during the take and those the playhead passed are erased when it ends (after one full pass the take simply goes on adding, like OVERDUB). Pressed again: disarm / punch out. | new, `KickHome::handleCommand` (every screen but LOOPER, which keeps RECORD for itself) -> `MTAudioInterface::setSequenceRecord` -> `MTAudioTransportManager::setRecordMode`, `MTTrack::startRecording / recordMidiMessage / stopRecording` |
| OVERDUB | Same, but only adds notes (LED 43). | same |
| TAP TEMPO | Two to four taps (a pause longer than one beat at 20 BPM restarts the count): the average interval of the last four becomes the selected sequence's BPM, or the master BPM when the sequence follows it. The clock takes it at the next bar. Logs `TapTempo: 4 taps, 600.5 ms apart -> 100 BPM`. | new, `tapTempo()` in `kick_AppHome.cpp` |

A take records each pad (or MIDI) note on / off pair that reaches the selected track through
`addLiveMidiMsg` (so pads already rewritten by FULL / HALF LEVEL, 16 LEVELS, NOTE REPEAT, 4.9),
at the track position of the last tick played, with the hit's velocity and the held length (1 tick
minimum). With Timing Correct on (ON/OFF, SHIFT + TC, 4.8) the start is snapped by
`TimingCorrect::correct`. Each note is logged: `Record: note 12 velocity 100 at tick 528 (played
at 529, timing corrected) length 20`, and each take: `Record: take on track 'Drums' ended after
3718 ticks, 65 old notes erased, 3 notes`.

**BAR : BEAT : TICK** (bottom right) runs while playing, holds on STOP, continues on PLAY and
restarts at 1:01:000 on PLAY START. **Metronome**: the top-bar Metronome button opens METRONOME;
ENABLE On clicks at RATE (1/4 default) with the 2 kHz accent on the first beat of every bar, at
LEVEL; MODE PLAY (default) clicks while playing and not recording, RECORD only during a take,
RECORD + PLAY always. MODE and LEVEL were stored but ignored before.

Measured with `sim_transport.sh` on 2026-10-27 (all checks passed; `sim_ctl.sh status` gives the
positions):

- STOP at `3:04:049`, still `3:04:049` a second later; PLAY: `4:03:022` 0.8 s later (+261 ticks);
  STOP at `2:03:083`, PLAY START: `1:03:068` 0.8 s later; the first note after PLAY START is
  `SimNote: track 'Drums' tick 0`. After STOP the probe reads -120 dBFS from 0.1 s on.
- Pad 1 (kick, sample peak -1.0 dBFS, velocity 100): -13.35 / -13.29 dBFS left / right (before:
  -9.2 / -24.2). Sequence playback, 10 s: master peak -3.0 dBFS (before: +2.6 / -3.8 dBFS).
- RECORD, three hits in pass 1 at 1.72 / 3.55 / 5.74 s after PLAY START, heard again in pass 2 at
  9.72 / 11.54 / 13.74 s: 8.000 / 7.990 / 8.000 s later (one loop is 4 bars at 120 BPM = 8.000 s;
  onsets are on 10 ms engine blocks). The take erased the track's 65 old notes and left 3. OVERDUB
  with Timing Correct on added 2 (snapped 529 -> 528 and 1279 -> 1272, the 1/16 grid), erased
  none, and the next pass played all 5.
- TAP TEMPO, four presses 600 ms apart (`sim_ctl.sh repeat 600 4 "TAP TEMPO"`): `600.5 ms apart ->
  100 BPM`, `status ... bpm=100`, and after the next bar the loop repeats every 9.59-9.61 s (4 bars
  at 100 BPM = 9.6 s).
- Metronome on, all tracks muted, 100 BPM: clicks 0.59-0.60 s apart at 1998 / 1201 / 1201 / 1201 Hz
  per bar, the first one included; ENABLE Off: -120 dBFS while playing.

**Gain staging.** A pad hit at velocity 100 on the starter kick (sample peak -1 dBFS) now comes out
at -13.3 dBFS in both channels, which is the sum of what the controls say: velocity 100/127
(-2.1 dB), track level 100/127 (-2.1 dB), program level 100/127 (-2.1 dB), the drum voice's
equal-power pan law (-3 dB in the centre) and the program's equal-power pan (-3 dB). Before, the
program gain and pan were applied twice, the velocity-to-pan default pushed every drum voice hard
left, and neither a loaded track's level nor any program level was applied; details in 5.1. Phase 4's
-20.2 dBFS for a -12 dBFS sampler take was the same chain.

**Input and feedback.** Loading a project does not monitor the input: with a 1 kHz pulse train as
input (`KICK_SIM_AUDIO_INPUT=pulse:1000`) and with the real default input (microphone), loading the
Starter Kit left the probe at exactly -120 dBFS. The input reaches the output only through the
SAMPLER and LOOPER MONITOR buttons (off by default; the device does the same). The Phase 2 howl was
therefore not a default of the reconstruction. To keep a fresh run safe on laptop speakers anyway,
`sim_launch.sh` now uses `KICK_SIM_AUDIO_INPUT=silence` unless the variable is set; pass `device`
for the real input. The app itself is unchanged (started by hand it opens the default input, as
the device opens its line input).

Not covered: count-in (the dialog only offers Off), the TAP TEMPO LED that flashes with the tempo
(the Hardware tab's "tap tempo LED" setting is stored, nothing drives it), RECORD in the step
sequencer / grid edit screens beyond what their pads send through `addLiveMidiMsg`, and recording
of Clip and Audio tracks. A take on a shorter track or with a non-zero loop start was not measured.

### 4.8 Mode keys (phase 5)

The function keys and their SHIFT functions (MAIN / GRID, BROWSE / SAVE, TRACK MIX / PAD MIX,
TRACK MUTE / PAD MUTE, NEXT SEQ / FX, TAP TEMPO's MASTER, MENU / PREFS, STEP SEQ / AUTO,
TC / ON/OFF, SAMPLER / LOOP, SAMPLE EDIT / LINK EDIT, PROG EDIT / MIDI CTRL, LINK) are checked
by `Kick/tools/sim_modes.sh [OUTDIR]` (default `Kick/build/logs/modes`). It resets the build
copies of "Starter Kit" and "Demo 01" from `Kick/demos`, starts the simulator with
`KICK_SIM_AUDIO_INPUT=silence`, the probe and surface logging, and drives everything through
`sim_ctl.sh`, so it runs with the screen locked:

1. Demo 01 (empty): P1-P4 in NEXT SEQ, TRACK MIX and PAD MIX, then TC, ON/OFF, MASTER, AUTO;
   the app must survive (these used to crash).
2. Starter Kit: every key in turn. For each: the packet (`Data[0] From DCL:<id>`), the LED packet
   (`SimSurface: LED cmd=0 data=<id>,<on>`) or, for keys without an LED, the log line of their
   handler, the app still alive, and a screenshot `<n>-<key>-screen.png`.
3. Follow-ups that show the screens working: P1 / P2 in TRACK MIX and PAD MIX, P1 in NEXT SEQ
   and FX, SAVE rewriting the project file, TRACK MUTE muting tracks while the sequence plays and
   PAD MUTE muting the kick (probe peaks), NEXT SEQ queuing Sequence 02, MASTER on Sequence 02,
   and TC on its off-grid drums (the saved notes must all be on the 1/16 grid).

It prints a table (`results.txt` in OUTDIR) and exits 1 on any FAIL. A `gap` row is a known
engine limitation, reported but not a failure. Last run (2026-10-27): 50 checks, 0 failed, one
gap; screenshots and the table are in `docs/sim_audit/phase5_modes/`. Measured: TRACK MUTE with
Keys and Loops muted -3 dBFS peak (drums alone), all three muted -120 dBFS, unmuted +2 dBFS;
PAD MUTE pad 1 then Pad 1 on MAIN -120 dBFS, unmuted -9.2 dBFS; TC moved 23 of 23 notes of
"Drums Live" onto the grid.

**Starter content.** `make_starter_kit.py` (`sequence_two()`) adds **Sequence 02**: 2 bars at its
own tempo of 100 BPM (`isMasterBPM="0"` with a `TEMPOS` child), a half-time "Drums Live" track
whose every hit is up to 9 ticks off the 16th grid (seeded, so deterministic), and the drum loop
on Loops. It gives NEXT SEQ a second sequence, MASTER a tempo to switch from, and TC something to
correct. Sequence 01 is unchanged.

**What the keys do now.**

| Key | Behaviour |
|---|---|
| TC | Opens TIMING CORRECT (`kick_ui/modes/kick_TimingCorrect.*`, new): a Timing Correct switch, NOTE VALUE 1/4 ... 1/32T (the selected sequence's quantization, the value the step sequencer's TIME DIV and the grid use) and STRENGTH 0-100 %. Apply stores the note value and, with Timing Correct on, moves the selected track's notes that far towards the grid. |
| ON/OFF (SHIFT + TC) | Switches Timing Correct on / off, lights the TC key while on (`LED 34`) and shows a notification. `TimingCorrect::correct( tick )` snaps a start to the note value while on, for live recording (phase 3) to call. |
| MASTER (SHIFT + TAP TEMPO) | Switches the selected sequence between its own tempo and the master tempo (the sequence's Global toggle, `setSequenceBPMToMaster`), shows "Master Tempo: 120 BPM" / "Sequence Tempo: 100 BPM", sends `LED 19` on / off (the panel has no LED under a caption). |
| AUTO (SHIFT + STEP SEQ) | Nothing to do: the release has no automation engine (the mode header's Automation button has an empty `onClick`, and nothing records or plays parameter changes). In the simulator it shows "Automation is not available in this firmware" and logs `SimUnavailable: Automation`. |
| LOOP | Leaving the looper now switches its LED off. |
| Others | Unchanged code; verified on the Starter Kit (`docs/SIMULATOR_STATUS.md`). |

What was inferred: the captions follow the MPC-style layout this panel copies, where TC opens the
Timing Correct settings, SHIFT + TC turns Timing Correct on or off, SHIFT + TAP TEMPO (MASTER)
makes the sequence follow the master tempo instead of its own, and AUTO arms automation. The code
supplied the pieces: the per-sequence quantization (`MTTempoSettings::quantize`), the per-sequence
master-tempo flag (`isMasterBPM`, shown as "Global" on the Main screen), the `EN_TC` / `EN_MASTER`
LED ids, and no automation code at all.

**Shared-code changes (the device runs them too).**

- Crash guards: `KickNextSequenceMode::knobCommand` returns for P1-P4 without a sequence;
  `ChannelMixer::knobCommand` null-checks its view casts and casts the Output (4) and Midi (5)
  tabs to the right class (they were swapped); the seven `kick_ui_channel_mixer` views,
  `PadMixerContent::knobCommand` and `ListEditMode::knobCommand` return when the table has no rows.
- `PadMixerContent::knobCommand` used `row - 1`, so P1-P4 addressed row -1 (nothing) and P5-P8 the
  first pad; it now uses the row as the track mixers do. Inferred from the identical maths in the
  other mixers and from the rows `getRowNumbers` returns; not checked on a unit.
- NEXT SEQ refreshes its list after P1-P4, which showed the old BPM / length before.
- TC, ON/OFF and MASTER handlers in `KickHome::handleCommand` and the new `TimingCorrect`.
- `kick_leds.cpp`: the looper's close case lacked `id =`, so its LED stayed on.

**Simulator-only changes.** `KickTableView::getRowNumbers` (`SIM_BUILD`): the Q-Link row maths
uses the canvas's desktop position, which on the unit is its position on the 1024x600 screen; in
the simulator the screen is scaled and offset inside the panel, so P1-P4 in TRACK MIX / PAD MIX /
LIST EDIT hit no row. It now measures from the transformed screen root. `kick_SimControl.cpp`:
`press NAME` skips printed labels that take no clicks, so `press MASTER` reaches the SHIFT + TAP
TEMPO caption instead of the label under the MASTER pot.

**Not like the device yet.** TRACK MIX's program LEVEL and PAD MIX's pad LEVEL change on screen
but not in the sound: nothing in the engine reads a program's `level`, and `DrumVoice` ignores a
pad's `gain` (as it ignores the pad's semi / fine, 4.4); gain staging belongs to phase 3, and the
`gap` row in `sim_modes.sh` turns into `ok` once Pad 1 gets quieter. FX (XY Effects) only updates
its labels; nothing connects it to audio in this release. LINK reaches
`KnobEdit::knobLongPress()` with no visible change on Main (Q-Link pages, phase 5b).

### 4.9 Pads, modifiers, encoders and slider (phase 5)

`Kick/tools/sim_pads.sh [OUTDIR]` (default `Kick/build/logs/sim_pads`) loads the Starter Kit,
plays every control below through `sim_ctl.sh`, measures the audio probe and the LED / pad packets
(`KICK_SIM_LOG_SURFACE=1`) and exits non-zero if a check fails. Screenshots, the log and the probe
stay in OUTDIR. It does not save the project. Last run (2026-10-27): all passed.

| Control | What it does now | Measured by `sim_pads.sh` |
|---|---|---|
| A-D, E-H | Pad bank; LED; pads recolour (`PAD cmd=6`) | A kit colours, B keys (6), C, D; E-H "no sample" (the kit has nothing above note 63) |
| FULL LEVEL | Toggle: every pad hit at velocity 127; HALF LEVEL off | vel 30 hit -21.2 dBFS → -7.0 dBFS = vel 127 |
| HALF LVL (SHIFT + FULL LEVEL) | Toggle: every pad hit at velocity 64; FULL LEVEL off | vel 30 hit → -13.7 dBFS = vel 64 |
| 16 LEVELS | Toggle: the last pad hit, on all 16 pads, velocity 7, 15, ... 127 (pad 1 → 16); the pads show its colour dim to bright (`PAD cmd=10`) | 16 hits from -40.5 to -12.7 dBFS, each louder |
| NOTE REPEAT (held) | Held pads retrigger at the sequence's time-correct division (TC; 1/16 by default) and tempo; on the grid while the sequencer plays | 1 s hold: 9-10 hits, median 120-130 ms (125 ms expected) |
| LATCH (SHIFT + NOTE REPEAT) | Toggle: note repeat without holding NOTE REPEAT | 8-9 hits, median 125-130 ms; without it 1 hit |
| NOTES (SHIFT + 16 LEVELS) | Pad Perform (Chromatic by default) | Keys track: pads 1 / 2 / 13 = 130.8 / 138.8 / 261.7 Hz |
| ERASE held + pad | Erases the pad's notes from the selected track (no sound) | Pad 3: 20 notes erased, playback onsets 13 → 12; all pads: 0 onsets |
| ERASE in GRID, COPY | Delete / duplicate the selected notes (unchanged; ERASE now acts on the press only) | screenshots `19`-`21` |
| BACK | The current screen's header back arrow (`KickHome::goBack`) | BROWSE and PROG EDIT → MAIN |
| Jog, PUSH | MAIN: selects tracks; MENU: moves / opens; a touched value: edits it | track 1 ↔ 2; MENU jog 2, +, PUSH → NEXT SEQ |
| + / -, slider | Edit the touched (focused) value | touched LEVEL: slider 30 → 38, + + → 40, - → 39, jog +3 → 42 |
| P1-P4 | MAIN Track tab: I/P MIDI CH, LENGTH, LEVEL, TRANSPOSE of the first row; TRACK MIX: OUT, LEVEL, PAN, mute/solo of strip 1; STEP SEQ: level of steps 1-4 of the page; PROG EDIT: nothing | P3 +5 LEVEL 42 → 47; P4 +2 TRANSPOSE → 2; TRACK MIX P2 -10 → 90, P3 -3 pan → -1.00; STEP SEQ P2 sets step 2 |

**What was missing and where it was looked for.** `KickHome::handleCommand` (the global switch),
the 62 `handleCommand` implementations, `DclContractsHelper`, `kick_leds.cpp` and every pad path
(`ae.addLiveMidiMsg` in MAIN, GRID, BROWSE, NOTES, the mixers, PROG EDIT; `MTTrack::processIncomingMidiMessage`)
were searched for NOTE REPEAT, LATCH, 16 LEVELS, BACK and any repeat or level logic: there is none
in the release. FULL / HALF LEVEL only lit an LED and set `halfLevelState`, which only STEP SEQ
reads (step velocity), and project load always lit FULL LEVEL. `modes/kick_Erase.cpp` and
`kick_Copy.cpp` are "Erase Notes" / copy dialogs that nothing opens. The pad screens send
`noteOn( pad + bank offset, packet velocity )`, so the natural place for pad modifiers is the
packet itself, before the screen sees it.

**Shared code added or changed** (the device build runs it too):

- `kick_ui/dialogs/kick_PadModifiers.{h,cpp}` (new), called from `KickHome::handleCommand` for every
  packet while a project is loaded (not in MIDI CONTROL, which learns keys). It consumes FULL LEVEL,
  HALF LEVEL, 16 LEVELS, NOTE REPEAT and LATCH (no screen used them), and rewrites pad packets:
  velocity for FULL / HALF LEVEL on every screen; pad number and velocity for 16 LEVELS, note
  repeat and ERASE + pad only on screens whose pads play the selected track (MAIN, GRID, TRACK MIX,
  PROG EDIT, NOTES), not where pads are steps, mutes or sequences. Note repeat retriggers from a
  2 ms `juce::Timer` on the message thread by handing the screen a release + press of the held
  pad; the interval is `60000 / BPM x ticks-per-quantization / 96` (the selected sequence's TC
  setting, `MTTempoMgr::getTicksPerQuantization`; master BPM unless the sequence has its own).
  ERASE + pad calls `deleteNotesInRange( selected track, note, note )` and logs the count.
- Inferences, not documented anywhere in the repository: FULL / HALF LEVEL are off by default
  (pads are velocity-sensitive; the surface gets threshold / sensitivity / curve from the hardware
  preferences), which is why `kick_leds.cpp` now sends their state at project load instead of
  lighting FULL LEVEL; 16 LEVELS maps by velocity (as on MPC-style units); LATCH latches the repeat,
  not the pads; ERASE + pad erases all of that pad's notes at once rather than only those under
  the playhead while held.
- `DclContractsHelper::setFullLevel / isFullLevel` (next to the existing half-level flag).
- `KickHome::goBack` for BACK.
- `kick_ProgramEdit.cpp`: Q-Link packets were handled as pad hits (P1 played note 1 at velocity 1
  and relabelled the selected pad); pads only now.
- `kick_AppGrid.cpp`: ERASE deletes the selection on the press only (the key now also reports its
  release).

**Key releases.** `AppAssignSample` handles ERASE's release, so the unit reports it; the panel now
sends releases for ERASE and NOTE REPEAT (the keys that are held). The other keys stay press-only:
`KickControlsProcessor` and GRID act on any button packet of their id, so a release would step
+ / - or paste twice. If the unit sends releases for every key, those handlers need a state check.

**Focus (simulator only, `SIM_BUILD`).** The jog wheel, + / -, PUSH and the slider go to the
touched control through JUCE keyboard focus (`ui::getComponentInFocus`). JUCE grants focus only
while the window is the key window, so with the screen locked (or the window in the background)
no control ever had focus and those keys reached nothing. `sim_ctl.sh tap` now does what a click
does (focus to the touched component or its nearest focusable parent) and records it in
`ui::simFocusTarget()` (`kick_ControlBase.h`), which `getComponentInFocus` uses while JUCE has no
focused component; `setFocus` / `clearFocusComponent` keep it in step. As on the device, a value
change can rebuild the row and drop the focus; touch again. While a control has focus, P1-P4 go to
it too (and do nothing): that is the app's routing (`KickHome::handleCommand`), unchanged.

**Panel** (`apps/kickem/simulator`): a SHIFT function's LED with no key of its own (HALF LEVEL 49,
LATCH 25, GRID 9, ...) lights the key it is printed under while either LED is on; the slider's LED
ladder shows the app's `SLIDER cmd=1/2` value (the touched control's value, or MAIN's track
meter); `PAD cmd=9/10` step colours are dimmed by their velocity, so 16 LEVELS shows its levels.

### 4.10 Smoke test (phase 7)

`Kick/tools/sim_smoke.sh` is the one command that says whether the simulator still plays:

```sh
Kick/tools/sim_smoke.sh                # builds KT_KickEM first
Kick/tools/sim_smoke.sh --no-build /tmp/smoke
```

It configures `Kick/build` if needed (honouring `CPM_SOURCE_CACHE`), builds `KT_KickEM`, moves
`Kick/build/prefs` aside so the jog wheel's selection starts on the first tile, and launches with
`KICK_SIM_FRESH=1`, silent input, the audio probe and the surface trace. Then it:

1. waits for the handshake (`Send Comm Live`);
2. presses PUSH and waits for `SimProject: loaded .../Starter Kit` (a `SIM_BUILD` log line in
   `MTAudioManager::loadProject`);
3. checks the pad colours arrived (`SimSurface: PAD cmd=6`) and the output is silent;
4. presses PLAY, waits four bars at the project's tempo, takes a screenshot, presses STOP.

It passes when: the PLAY and STOP LED packets were sent; every whole second of the four bars
peaks above -40 dBFS; nothing peaks above 0 dBFS; the output is silent after STOP; and the app is
still running. It prints one PASS / FAIL line per check, leaves `sim.log`, `probe.wav` and
screenshots in OUTDIR (default `Kick/build/logs/smoke`), puts the preferences back, and exits 1
on any failure. A run takes about 25 s after the build. Last result (2026-10-28): 10 / 10, peak
-3.2 to -3.3 dBFS while playing.

Screenshots from that run: `docs/sim_audit/phase7_smoke/loaded.png` and `playing.png`.

The deeper checks are `sim_transport.sh` (4.7), `sim_modes.sh` (4.8), `sim_pads.sh` (4.9),
`sim_sampler_looper.sh` (4.6) and `sim_overlays.sh` (4.1). Each takes a few minutes and waits for the simulator lock, so run
them one after another:

```sh
for t in smoke transport modes pads sampler_looper overlays; do Kick/tools/sim_$t.sh || echo "FAILED: $t"; done
```

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
      take at velocity 100): voice / track gain staging, not the recorder. Explained and fixed
      in phase 3 (below and 4.7).
  - Simulator tooling: `sim_ctl.sh turn P1..P4|JOG N` turns encoders (the sampler's Max Length
    and the sample editor's START need them; the knobs are told apart by geometry, the jog wheel
    being the largest); `sim_ctl.sh click` skips zero-size components (dialog contents such as
    CREATE PROGRAM carry unused 0x0 `Apply` / `Cancel` buttons that shadowed the real ones);
    `KICK_SIM_AUDIO_INPUT=pulse:<ms>`; `sim_sampler_looper.sh` and `wav_measure.py`.

  **Phase 3 findings** (transport, sequencer, tempo, metronome, gain staging; 4.7). The
  reconstructed glue in `AudIO/AudIO_juce/` turned out not to be the cause of the clipping, the
  pad deficit or the timing problems: `mt::AudioDevice` ran with a device buffer equal to the
  engine block (480), so no re-timing happened, and `mt::ADSR` / `mt::Filter` behave at the
  default (unattached) envelope values. No value in `AudIO_constants.h` was changed. The causes
  were in Kick's own engine code, which the device runs too, so **these change device behaviour**:
  - `audio_program/synth/drum_voice.cpp`: the drum voice multiplied its output by the program's
    master gain/pan, and `ISynthObject::postProcess` multiplied the sum of the voices by it again
    (the only voice that did; clip and keygroup voices rely on `postProcess`). The voice now adds
    at unity: -3 dB in the centre instead of -6 dB, and a program pan no longer counts twice.
  - `audio_program/source/mt_audio_iprogram_atomics.h`: the amplitude envelope's velocity-to-pan
    amount defaulted to 1, which `DrumVoice` maps through `jmap( 0..127 -> -1..1 )` to -0.98, so
    every drum hit was panned by -0.98 x velocity: left -9.2 dBFS, right -24.2 dBFS for Pad 1. The
    default is now 64 (the middle). The voice's envelope / LFO trees are never attached
    (`DrumVoice::attachToAndReset` has no caller), so the default is all a drum voice ever sees.
  - Same file: the program master now also scales by the program `level` (0..127, `/ 127` as
    `CommonUtils::midiToGain`). TRACK MIX's program strip LEVEL wrote it but nothing read it
    (phase 5's `sim_modes.sh` "MIX levels" row: level 100 -> 50 now takes Pad 1 from -13.3 to
    -19.3 dBFS).
  - Known issue, PAD MIX (UI, not the engine): since the pad gain is heard, `sim_modes.sh` shows
    that after P1 -50 (pad A01 1.00 -> 0.00) the P1 +50 that restores it updates the slider on
    screen but never calls `MTIProgram::setNoteGain` (logged: one call with 0, none with 1), so
    pad A01 stays silent and the script's later "PAD MUTE pad 1" check fails (unmuted -120
    dBFS); its "MIX levels" row reports `gap` because of the restore. Before the pad gain was
    applied this was inaudible. With the merge of phase 5's pads / encoders it was consistent
    (4 of 4 runs); before that merge the restore worked. Not investigated further (P1-P4 and
    PAD MIX are phase 5 areas).
  - `audio_program/synth/iSynthVoice.{h,cpp}`, `DrumProgram.cpp`, `drum_voice.cpp`: a drum pad's own
    `gain` and `pan` (PAD MIX) are applied to the voice (velocity gain x pad gain; pad pan added to
    the voice pan). They were stored per CLIP and ignored.
  - `audio_engine/engine/mt_audio_track.cpp`: `levelScaler` (track level / 127, applied to note
    velocities) was only set when the level property changed, so a loaded track played at x1.0
    until its level was touched (the Keys track at 70 played at 100 %). It is set on load now.
  - With these, the starter sequence peaks at -3.0 dBFS (it clipped at +2.6 dBFS). Notes were never
    triggered twice: with `KICK_SIM_LOG_NOTES=1` every note of a pass is played once.
  - `audio_file_manager/source/audio_sound_renderer.cpp`: a finished sample returns a shared static
    silent sample, which `DrumVoice` filters in place; the filter's output was then fed back as
    the next input for the rest of the block (a -32 dBFS, 900 Hz burst at the end of a drum
    hit, measured on the snare). It is cleared on every read now.
  - `audio_engine/engine/mt_audio_track.cpp`: the clock's first tick after play() is 1 or more,
    and notes were collected from `prevTick + 1`, so the notes on tick 0 (the first downbeat) were
    skipped until the sequence looped. The first tick now includes position 0. A tick counter
    that goes backwards (clock restarted while a tick was in flight) restarts from 0 instead of
    wrapping round `unsigned`.
  - `audio_engine/engine/mt_audio_sequence.cpp`: `play()` did not reset `prevTicks`, so a second
    play after a pause computed a huge negative difference and ran the bar counter away; it kept
    the paused beat and tick, so after PLAY START the BAR:BEAT:TICK display and the metronome
    counted from the old offset (the notes, counted by the tracks, were right). Bar, beat and
    1/n signals fired one tick late (`>` instead of `>=`); the bar-1 and beat-1 signals were
    never sent at the start (the clock never reports tick 0), so the metronome's first accent
    was missing. All fixed; the clock is stopped before it is restarted.
  - Threads (an intermittent SIGSEGV in `MTTrack::onTick`, 1 in 3 runs of `sim_modes.sh` at
    "NEXT SEQ pad 2"): the sequencer's ticks run on `MTAudioTransportManager::processTick`'s
    thread, but the MIDI clock's timer thread switched sequences itself (`play( next )` at the
    loop end for NEXT SEQ, song steps), stopping the old sequence's tracks (clearing their
    queued note-offs) while the tick thread was iterating them; the message thread's
    PLAY / STOP did the same. Now the clock thread only queues those changes
    (`MTAudioTransportManager::defer`) and the tick thread runs them between two ticks, dropping
    the tick of the old clock run; `MTTrack::play / pause / stop` and the whole of `onTick` hold
    the track's lock; a tick of the previous clock run arriving after `play()` is ignored by the
    sequence and the tracks instead of playing a bar of notes at once.
  - `audio_engine/engine/mt_audio_manager_transport.cpp`: PLAY START called the same `play( Seq )`
    as PLAY, which resumes when paused, so after STOP it did not go to the top
    (`startPlayback( Seq, Resume )` now). New: RECORD / OVERDUB arming and takes
    (`setRecordMode`, `beginTake`, `endTake`, `recordLiveMidi`); STOP ends a take and disarms.
  - `audio_engine/engine/mt_audio_track.{h,cpp}`: new live recording (`startRecording`,
    `recordMidiMessage`, `stopRecording`, `recordTimingCorrect` hook for TC), and the
    `KICK_SIM_LOG_NOTES` log (`SIM_BUILD` only). `mt_audio_interface.{h,cpp}`:
    `setSequenceRecord`, `isSequenceRecordArmed / Overdub`, `isSequenceRecording`,
    `setRecordTimingCorrect`; `addLiveMidiMsg` hands notes to the running take.
  - `audio_engine/engine/mt_audio_metronome.h`: MODE (RECORD / PLAY / RECORD + PLAY) and LEVEL
    from the METRONOME dialog are applied (they were stored and ignored).
  - `kick_ui/dialogs/kick_AppHome.cpp`: handlers for TAP TEMPO, RECORD and OVERDUB (none existed
    outside the LOOPER), RECORD / OVERDUB LEDs; `kick_leds/kick_leds.cpp`: RECORD / OVERDUB LEDs
    off when the transport stops.
  - Reconstruction (`AudIO/AudIO_juce/`): only `mt_AudioDeviceProbe.cpp` changed (the
    `KICK_SIM_AUDIO_INPUT=device` keyword).
  - Simulator only (`apps/kickem/`, `tools/`): the panel shows one of PLAY / STOP / PLAY START lit
    (the app's "media control" LED message never switches the other off; the surface firmware
    presumably does); `sim_ctl.sh repeat`, `clickat`, `status`; `click` also finds dialogs drawn
    inside the panel window; `sim_launch.sh` defaults to a silent input; `wav_measure.py
    --channels --freqs`; `sim_transport.sh`.

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
  `Kick/README.md`. The serial stub (5.2) must be replaced for a device build (`/goal-serial`).
  The build fixes in 5.1-5.3 leave the device code paths alone, but the `/goal` work changed shared
  app and engine code that the device runs too; section 7 lists it.

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

## 7. What works in the simulator, and what is still different (2026-10-28)

### Works

Started from `Kick/build` with no hardware, on a fresh checkout:

- **Sound out of the box.** The Projects screen offers the synthesised Starter Kit first (4.4).
  PUSH loads it. Pads 1-16 in bank A play 16 drum sounds, banks B-D play keys, bass and loops.
  The engine plays to the default output device (logged as `AudioDevice: using '...'`), and the
  probe measures what it plays.
- **Transport.**
  - PLAY resumes and PLAY START plays from the top; STOP stops.
  - RECORD and OVERDUB capture pad hits into the selected track, with Timing Correct applied.
  - BAR:BEAT:TICK runs, TAP TEMPO sets the tempo, and the metronome clicks with an accent on
    the bar (4.7).
- **Every panel key and SHIFT function** does its job:
  - Every mode key, mixer and mute, NEXT SEQ, TC / ON/OFF, MASTER, SAVE and BACK.
  - The bank keys with pad colours, FULL / HALF LEVEL, 16 LEVELS, NOTE REPEAT / LATCH, NOTES,
    ERASE and COPY.
  - The jog wheel, PUSH, + / -, P1-P4 where the screen assigns them, and the slider on the touched
    control (4.8, 4.9).
- **Sampler, sample edit and looper.** Recording works from a test input, as do trimming,
  assigning the take to a pad and playing it. The looper records, overdubs and loops in time
  (4.6).
- **Hardware-only screens** open, say "Not available in the simulator", and close (4.5).
- **Scripted.** `sim_smoke.sh` and the four deeper checks pass (4.10).
  `docs/SIMULATOR_STATUS.md` has the control-by-control status.

### Still different from the unit

- **Content.** Only the synthesised Starter Kit has sounds. The factory demos and templates are
  empty, banks E-H are empty, the Myriad Pro font is missing, and keygroup programs are silent
  (`KeygroupProgram::noteOn` is a stub in this release).
- **Missing in the release's own code.** These are absent on the device as well:
  - AUTO: there is no automation engine.
  - FX: the XY view is not connected to audio.
  - PROG EDIT assigns nothing to the Q-Links.
  - LINK shows no visible page change.
  - There is no count-in, no tempo-synced TAP TEMPO LED, and no audio-track recording.
- **Inferred, not checked on a unit:**
  - ERASE + pad erases all of that pad's notes.
  - LATCH latches the repeat.
  - FULL / HALF LEVEL start off.
  - Only ERASE and NOTE REPEAT report releases.
  - The PAD MIX row fix (`docs/SIMULATOR_STATUS.md`, "Open gaps").
- **Hardware.**
  - **No hardware behind these screens:** Wi-Fi, Ethernet, CV / Gate, firmware update, SD / USB
    storage and the camera have nothing to drive in the simulator.
  - **Analogue controls:** the MASTER and REC pots do nothing.
  - **Audio device:** audio goes through the Mac's default device at 48 kHz in 480-sample
    blocks, not the unit's codec, so latency and CPU load say nothing about the unit.
  - **Microcontroller path:** the panel hands packets to the app in memory. The serial framing,
    CRC and `juce_serialport` (a stub) are not exercised; `/goal-serial` covers them.
  - **Linux paths:** the device's Linux code paths (`/app`, systemd, Weston, overlay mounts) are
    not compiled on macOS; `/goal-vm` covers them.
- **Simulator limits.** Only one simulator runs per machine.

### Shared code that changed device behaviour

These changes are in code the device runs (not behind `SIM_BUILD`). Each was made because the
control did nothing, crashed or sounded wrong in the simulator, and the device build compiles
the same code. Review them before a device release:

- **Engine gain** (phase 3):
  - The drum voice no longer applies the program's gain and pan twice.
  - The velocity-to-pan default is now centre (64), where 1 put every drum hit hard left.
  - Program level, track level and pad gain / pan are applied.
  - The shared silent sample is no longer filtered in place, which caused a burst at the end of
    drum hits.
- **Sequencer** (phase 3):
  - PLAY START after STOP plays from the top.
  - Notes at tick 0 and the bar-1 accent play.
  - Bar and beat signals are no longer one tick late.
  - `play()` resets its counters.
  - Sequence changes run on the tick thread, which fixes the NEXT SEQ crash.
  - The metronome applies MODE and LEVEL.
- **New key handlers** (phases 3 and 5):
  - TAP TEMPO, RECORD, OVERDUB, TC, ON/OFF, MASTER, BACK (`KickHome`), and `TimingCorrect`.
  - The pad modifiers in `kick_ui/dialogs/kick_PadModifiers`: FULL / HALF LEVEL, 16 LEVELS,
    NOTE REPEAT, LATCH, ERASE + pad.
- **Fixes in existing handlers** (phases 2 and 5):
  - Crash guards for P1 in NEXT SEQ, TRACK MIX, PAD MIX and LIST EDIT.
  - The PAD MIX row off-by-one.
  - The LOOPER LED is switched off.
  - The FULL LEVEL LED follows its state at project load.
  - PROG EDIT no longer treats Q-Link packets as pad hits.
  - GRID ERASE acts on the press only.
  - PUSH on the Projects screen loads the selected demo or template (it loaded an empty project).
  - The metronome falls back to the starter click set when the factory click is missing.
- **Knob routing** (2026-10-28): P1-P16 go to the screen, not to a focused control, unless a
  modal popup is open.
- **AudIO** (phase 4): one program-wide `AudioObject` id counter, where each module had its own
  and ids collided. `mt::Recorder` and `mt::Looper` (reconstructed) set their routing id and were
  reworked (5.1).
