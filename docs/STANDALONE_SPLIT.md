# Standalone split: where every change on the work branch goes

This is the phase 0 inventory of `/goal-standalone` (`.claude/commands/goal-standalone.md`). It
classifies every change on `rbdone/goal-simulator-controls` against `KickEm-Tools/0700-AAM` `main`
and says where it goes. Later phases update the **Status** column.

- Base: `kickem/main` = `27194180` (equal to `MusicTribeOpenSource/0700-AAM` `main`).
- Work branch: `rbdone/goal-simulator-controls` at `53805d2a` (31 commits, 177 files,
  +11727 / -1040 lines).
- Outside the simulator's own directories (`Kick/apps/kickem/`, `Kick/tools/`, `docs/`, `.claude/`,
  `Kick/assets/StarterKit/`, `Kick/demos/Starter Kit/`): **89 files**, of which **24 hold the 44
  `SIM_BUILD` sites**. Upstream itself already has 2 of those sites (`input_events.cpp` lines 308
  and 358, the in-memory `SimulatorInterface`) and an older `Kick/apps/kickem/` that no longer
  compiles (DEVELOPMENT.md 5.3), behind the `KICK_SIMULATOR` option.

Classes: **BUILD** (the release builds), **FIX** (device bug fix or missing key handler; one row
per bug), **SEAM** (a `SIM_BUILD` site; decision **generic**, **runtime** or **emulator**), **SIM**
(simulator-only code, moves to the emulator), **CONTENT** (Starter Kit).

Status values: `planned`, `branch` (on its topic branch), `pr #n`, `merged`, `upstream #n`,
`dropped`.

## Decisions that shape the split

### 1. The automation channel

Scripted checks need two kinds of input: **panel** input (keys, pads, encoders, slider) and
**screen** input (a touch on the 1024x600 screen, find a component by title, render the screen,
read the transport state).

- **Panel input never needs the firmware.** The emulator's panel is its own process and speaks
  the microcontroller's side of `CPktMgr`. Its own control channel (a successor of
  `sim_ctl.sh press / down / up / hold / turn / slider / repeat`) sends the packets over the
  wire, so the app receives exactly what a unit's microcontroller would send.
- **Screen input, fast mode: an opt-in test-control module in the firmware.**
  `Kick/modules/test_control` (new, generic). It is inert unless `KICK_TEST_CONTROL=<file>` is set:
  without it nothing is created, polled or logged.
  - Commands, the screen half of today's `kick_SimControl.cpp`: `tap X Y`, `click NAME`,
    `clickat NAME FX FY`, `list [FILTER]`, `shot OUT.png` (the screen and its popups, rendered
    in-process), `windows`, `status` (transport, BAR:BEAT:TICK, BPM, record mode, metronome),
    `echo TEXT`.
  - Acknowledged in the log as `TestControl: ok <n>`.
  - It also owns the focus fallback (today's `ui::simFocusTarget()`), active only while the module
    is enabled.
  - Why in-app: OS-level events (accessibility, CGEvent) do not work while the Mac is locked or
    the window is covered. Today's checks were all run with the screen locked. The module is
    proposed upstream like any other change: it is a generic test hook, with no simulator
    knowledge in it.
- **Screen input, faithful mode: QEMU input injection.** Absolute pointer events go through QMP
  (`input-send-event` with `abs` axes on a `usb-tablet` / `virtio-tablet`). This works headless
  and needs nothing in the firmware. The test-control module also works in the guest (same file
  protocol over the shared directory or SSH) for `status` and `click NAME`. The faithful-mode
  smoke test only needs panel keys and the probe, so it does not depend on it.

### 2. Runtime extension points in the firmware (all inert on the device)

| Variable | Replaces | What it does | On the device |
|---|---|---|---|
| `KICK_SERIAL_DEVICE=<path>` | (new, `/goal-serial`) | Opens that port instead of `/dev/ttymxc0` | unset: `/dev/ttymxc0`, 230400 8N1, same handshake |
| `KICK_TEST_CONTROL=<file>` | `KICK_SIM_CONTROL` | Test-control module (above) | unset: not created |
| `KICK_AUDIO_PROBE=<file.wav>` | `KICK_SIM_AUDIO_PROBE` | `mt::AudioDevice` writes the master output to a WAV and logs `AudioProbe:` once per second | unset: no probe |
| `KICK_AUDIO_INPUT=silence\|tone[:hz]\|pulse[:ms]\|<file>` | `KICK_SIM_AUDIO_INPUT` | Replaces the device input with a test source | unset: the real input |
| `KICK_LOG_NOTES=1` | `KICK_SIM_LOG_NOTES` | The sequencer logs every note it plays | unset: nothing |
| `KICK_HARDWARE=none\|<list>` | (new) | Overrides the hardware probe (below) | unset: every feature whose device node exists is enabled, which on a unit is all of them |

**The hardware probe** (`mt::hw::isAvailable( Feature )`, new, in `modules/hardware`):

| Feature | Available when (Linux only; never on macOS) |
|---|---|
| `wifi` | `/sys/class/net/wlan0` exists |
| `ethernet` | `/sys/class/net/eth0` exists |
| `cv` | `/dev/i2c-3` exists |
| `update` | `swupdate` is on `PATH` |
| `clock` | always on Linux |
| `resources` | always on Linux (`/proc/stat`, `sysinfo`, `df`) |

`KICK_HARDWARE=none` disables all of them, and `KICK_HARDWARE=wifi,clock` enables only those
listed. The emulator sets `none` for desktop Linux runs, so the app never runs `killall -9
wpa_supplicant` or `ifconfig` on a developer's computer. In faithful mode the variable is left
unset, so the VM runs the real code.

A screen whose feature is unavailable covers itself with a generic `ui::FeatureUnavailable`
panel, which replaces `ui::SimUnavailable`. The panel reads "<feature>: not available on this
system" and logs `FeatureUnavailable: <feature> shown / hidden`. On a unit every feature is
present, so the panel is never created.

### 3. Overlays and lists: generic code, correct on the device

- **Overlays.** Full-screen popups, the loading spinner and notification bubbles are added to the
  app's root component (`ui::overlayRoot()`, registered by `apps/kick/Main.cpp`), not to the
  desktop at display coordinates.
  - On the unit the window covers the display, so where they appear does not change.
  - On a desktop, the app's own window can sit anywhere on a larger display. Without this change,
    a popup covers the top-left 1024x600 of the Mac's display instead of the Kick window. The
    in-tree simulator's `ui::simScreen()` existed for the same reason.
- **List hit-testing.** List rows are hit-tested with `getEventRelativeTo( &canvas )`. The Q-Link
  row maths measures from the top-level (or transformed) component instead of the desktop. On
  the unit both give the same numbers.

### 4. Content and branding

- **Content.** The Starter Kit (CC0) goes to the emulator, `content/StarterKit/` with
  `make_starter_kit.py`, and the emulator installs it into the fork's data paths:
  - `assets/StarterKit` and `data/samplesKick/StarterKit`;
  - `demos/Starter Kit`;
  - `assets/Default/recent_project/Starter Kit.png`.

  The firmware keeps only generic code that content needs: relative sample paths, and the
  metronome's fallback click with a bar accent.
- **Branding** (decided by the user, 2026-10-28). The emulator draws no logo or other trademark.
  - `Kick/assets/Default/Sim Behringer Logo.png` (artwork the user supplied for the panel) is not
    committed, and there is no `--logo` option.
  - The area above the screen, where the unit carries its logo, is blank. Before, the panel fell
    back to printing the word "behringer" there.
  - Done in KickEmulator `e0cd137`.
- **Generator licence** (decided by the user, 2026-10-28). `make_starter_kit.py` synthesises
  everything from fixed seeds with the Python standard library and reads no input files, so the
  script is CC0 like its output.

### 5. What leaves the firmware entirely

The fork's `main` ends up with no `SIM_BUILD`, no `Kick/apps/kickem/` and no `Kick/tools/sim_*`.
The following are removed or never merged:

- The upstream `SimulatorInterface` path, which is the only consumer of `SIM_BUILD` upstream:
  `input_events.cpp`, `kick_SimulatorInterface.h`, and `Factory<SimulatorInterface>`.
- The old upstream `Kick/apps/kickem/` and the `KICK_SIMULATOR` option.

The panel replaces all of it over the wire.

## Inventory

### BUILD (branch `build/release-builds`, phase 1, upstreamable)

| File | Hunk | Decision / note | Status |
|---|---|---|---|
| `.gitignore` | new: `.3rdPartySourceCache/` | as is | pr #1 |
| `AudIO/AudIO/Includes/Platform.h` | `<thread>`, `<chrono>` | as is | pr #1 |
| `AudIO/juce_modules.cmake` | new: registers AudIO, AudIO_juce, AudIO_constants | as is | pr #1 |
| `AudIO/AudIO_constants/AudIO_constants.h` | new: reconstructed constants | keep the reasoning table (DEVELOPMENT.md 5.1) as comments beside each value | pr #1 |
| `AudIO/AudIO_juce/AudIO_juce.{h,cpp}` | new module | as is | pr #1 |
| `AudIO/AudIO_juce/source/mt_ADSR.h`, `mt_Filter.h` | new: reconstructed | as is | pr #1 |
| `AudIO/AudIO_juce/source/mt_AudioDevice.{h,cpp}` | new: reconstructed device, FIFO re-blocking | **without** the probe hook (moves to `feature/desktop-seams`) | pr #1 |
| `AudIO/AudIO_juce/source/mt_Recorder.{h,cpp}` | new: Recorder / SampleRecorder / Looper | as is, with the phase 4 rework (routing id, length callback, loop mode). Needs `fix/audio-object-ids` to be heard: without it the sampler is never processed | pr #1 |
| `Kick/modules/3rdParty/juce_serialport/*` | new: API-compatible stub | Stub only, to make the release build. Drop the `SIM_BUILD` test in its `#warning` (warn on every Linux build). `feature/serialport` replaces it | pr #1 |
| `Kick/modules/common/source/kick_Constants.h` | `EMPTY::String` | as is | pr #1 |
| `Kick/modules/engine/interface/interface.h` | `EMPTY::SampleBuffer` | as is | pr #1 |
| `Kick/modules/hardware/source/kick_WifiFirmware.cpp` | `#include <sys/stat.h>` | as is (the `SIM_BUILD` hunks are SEAM, below) | pr #1 |
| `Kick/modules/contracts/utils/keyboard/keyboard_detector.{h,cpp}` | keyboard map on `__APPLE__` | as is (macOS desktop input without a panel) | pr #1 |
| `Kick/apps/kick/CMakeLists.txt` | `MICROPHONE_PERMISSION_ENABLED TRUE` | as is | pr #1 |
| `Kick/apps/kick/CMakeLists.txt` | `if(APPLE)` copy `assets/StarterKit` to `data/samplesKick` | **CONTENT, moves to the emulator installer** | not in pr #1 (emulator installs it) |
| `Kick/3rdParty.cmake` / configure | (new) | Default `CPM_AudIO_SOURCE` / `CPM_COM_PROTOCOL_SOURCE` to the in-tree copies when the internal git server is unreachable, or document the flags; CI passes them | pr #1 |
| `.github/workflows/build.yml` | (new) | `Kick` on `macos-14` and `ubuntu-24.04`, `KT_Kick` on `ubuntu-24.04-arm`, CPM cache, `KT_Kick-linux-arm64` artifact with a manifest; coordinate with `pin-buildroot-ci-image` | pr #1 |
| `docs/DEVELOPMENT.md` (fork) | (new, short) | building on macOS / Linux / arm64, what 5.1-5.3 fixed | pr #1 |

### FIX (phase 2: one topic branch per fix or close group, from `kickem/main` after phase 1, all upstreamable)

Each row is one device bug or one missing handler. The "Check" column names the work-branch
script whose output goes into the pull request.

| Branch | File(s) | Bug on the device / observation | Check | Status |
|---|---|---|---|---|
| `fix/audio-object-ids` | `AudIO/AudIO/Includes/AudioObject.h` | `static` id counter per TU: objects from different JUCE module TUs got the same id, so `Design::insertObject` refused them. The sampler's recorder got id 2, already an engine bus, and was never processed | `sim_sampler_looper.sh` (sampler ids 33 / 34, take 2.000 s at -12 dBFS) | planned |
| `fix/audio-object-ids` | `Kick/modules/engine/audio_engine/engine/mt_audio_recorder.cpp` | a failed `connectInputTo` was silent; now logged | same | planned |
| `fix/engine-gain-staging` | `audio_program/synth/drum_voice.cpp` | program gain / pan applied twice (voice and `postProcess`): -6 dB in the centre, pan law twice | `sim_transport.sh` gain staging (Pad 1 -13.35 / -13.29 dBFS L / R, was -9.2 / -24.2) | planned |
| `fix/engine-gain-staging` | `audio_program/source/mt_audio_iprogram_atomics.h` | velocity-to-pan default 1 maps to -0.98: every drum hit hard left | same | planned |
| `fix/engine-gain-staging` | `mt_audio_iprogram_atomics.h` | program `level` stored, never heard | `sim_modes.sh` "MIX levels" (100 → 50: -13.3 → -19.3 dBFS) | planned |
| `fix/engine-gain-staging` | `synth/iSynthVoice.{h,cpp}`, `synth/DrumProgram.cpp`, `drum_voice.cpp` | pad `gain` / `pan` (PAD MIX) stored per CLIP, ignored | `sim_modes.sh` PAD MIX / PAD MUTE rows | planned |
| `fix/engine-gain-staging` | `audio_engine/engine/mt_audio_track.cpp` (`levelScaler` on load) | a loaded track's level ignored until touched | `sim_transport.sh` (sequence peak -3.0 dBFS, was +2.6) | planned |
| `fix/engine-gain-staging` | `audio_file_manager/source/audio_sound_renderer.cpp` | the shared silent sample was filtered in place, giving a -32 dBFS 900 Hz burst at the end of drum hits | probe on the snare tail | planned |
| `fix/sequencer-timing` | `mt_audio_sequence.{h,cpp}` | `play()` kept `prevTicks` and beat / tick: the bar counter ran away and BAR:BEAT:TICK counted from the old offset; bar / beat / 1/n signals one tick late (`>`); bar 1 and beat 1 signals never sent | `sim_transport.sh` (PLAY START → `1:03:068`, metronome accent on bar 1) | planned |
| `fix/sequencer-timing` | `mt_audio_track.cpp` (`playFromStart`, `prevTick` reset, stale tick) | notes on tick 0 skipped until the loop wrapped; tick counter wrapped `unsigned` on clock restart | `KICK_LOG_NOTES` first note `tick 0` | planned |
| `fix/sequencer-timing` | `mt_audio_manager_transport.{h,cpp}` (`startPlayback`) | PLAY START after STOP resumed instead of playing from the top | `sim_transport.sh` | planned |
| `fix/next-seq-crash` | `mt_audio_manager_transport.{h,cpp}` (`defer`, `processTick`), `mt_audio_track.cpp` (locks in `play / pause / stop / onTick`) | intermittent SIGSEGV in `MTTrack::onTick` (1 in 3 `sim_modes.sh` runs at "NEXT SEQ pad 2"): the clock thread stopped tracks the tick thread was iterating | `sim_modes.sh` x4, no crash report | planned |
| `feature/timing-correct` | `kick_ui/modes/kick_TimingCorrect.{h,cpp}` (new), `kick_ui.{h,cpp}` includes, `kick_AppHome.cpp` TC / ON_OFF cases | TC and ON/OFF had no handler | `sim_modes.sh` (23 of 23 notes onto the 1/16 grid) | planned |
| `feature/note-recording` | `mt_audio_manager_transport.{h,cpp}` (record mode, takes), `mt_audio_track.{h,cpp}` (`startRecording` ...), `mt_audio_interface.{h,cpp}`, `kick_AppHome.cpp` (RECORD / OVERDUB / STOP cases, `showRecordLeds`, `setRecordTimingCorrect`), `kick_leds.cpp` (RECORD / OVERDUB LEDs off on stop) | RECORD and OVERDUB did nothing outside the LOOPER | `sim_transport.sh` (3 hits replayed 8.000 / 7.990 / 8.000 s later; 65 old notes erased) | planned |
| `feature/metronome` | `mt_audio_metronome.h` (MODE, LEVEL; `isRecording` hook), `mt_audio_manager.cpp` (hook) | MODE and LEVEL stored, ignored | `sim_transport.sh` metronome rows | planned |
| `feature/metronome` | `synth/metronome.{h,cpp}` | missing click file: silence. Now: fall back to `assets/StarterKit/Metronome/Click.wav` with a bar accent if present; unchanged when the factory click exists | `sim_transport.sh` (2 kHz / 1.2 kHz clicks) | planned |
| `feature/key-handlers` | `kick_AppHome.{h,cpp}` (`tapTempo`, `toggleMasterTempo`, `goBack`, TAP TEMPO / MASTER / BACK cases) | TAP TEMPO, MASTER and BACK had no handler | `sim_transport.sh` (`600.5 ms apart -> 100 BPM`), `sim_modes.sh`, `sim_pads.sh` BACK | planned |
| `feature/pad-modifiers` | `kick_ui/dialogs/kick_PadModifiers.{h,cpp}` (new), `kick_ui.{h,cpp}`, `kick_AppHome.{h,cpp}` (construct, `handle`, FULL / HALF cases removed), `contracts/dcl/dcl_contracts_helper.{h,cpp}` (`setFullLevel`), `kick_leds.cpp` (FULL / HALF at load), `gridview/kick_AppGrid.cpp` (ERASE on press only) | FULL / HALF LEVEL lit LEDs only; no 16 LEVELS, NOTE REPEAT, LATCH, ERASE + pad. **Inferred behaviour** (DEVELOPMENT.md 4.9), say so in the PR | `sim_pads.sh` (25 / 25) | planned |
| `fix/knob-handlers` | `kick_nextSequence.{h,cpp}` | P1-P4 with no sequence: crash; list not refreshed after an edit | `sim_modes.sh` empty-project rows | planned |
| `fix/knob-handlers` | `kick_ui_channel_mixer.cpp` | Output (4) / Midi (5) tabs cast to each other's class; unchecked casts | same | planned |
| `fix/knob-handlers` | `kick_ui_channel_mixer/source/*_mix.cpp` (7 files), `modes/kick_ListEdit.cpp`, `mixers/kick_PadMixer.cpp` (guard) | P1-P4 on an empty table: null dereference | same | planned |
| `fix/knob-handlers` | `mixers/kick_PadMixer.cpp` (`rowIndex = rowList[i]`) | PAD MIX P1-P4 addressed row -1, P5-P8 the first pad. **Inferred** from the track mixers, not checked on a unit | `sim_modes.sh` PAD MIX | planned |
| `fix/knob-handlers` | `kick_AppHome.cpp` (link knobs to the screen unless a modal is open) | P1-P16 went to the focused control, which ignores them: in PAD MIX P1 stopped working after a pad hit | `sim_modes.sh` "PAD MUTE pad 1" | planned |
| `fix/knob-handlers` | `program_edit_ui/sources/kick_ProgramEdit.cpp` | PROG EDIT read Q-Link packets as pad hits (P1 played note 1 at velocity 1, relabelled the pad) | `sim_pads.sh` / log | planned |
| `fix/ui-small` | `kick_leds/kick_leds.cpp` (`id = EN_LOOPER`) | LOOPER LED never switched off | `sim_modes.sh` (`LED 37,0,0`) | planned |
| `fix/ui-small` | `recent_project/kick_RecentProject.cpp` | PUSH on the Projects screen loaded an empty project with no directory (the sampler then saved to `/New Sample.wav`) | `sim_smoke.sh` (PUSH loads Starter Kit) | planned |
| `fix/relative-sample-paths` | `mt_audio_manager.cpp` (`resolveRelativeSamplePaths`) | was SEAM `SIM_BUILD \|\| JUCE_MAC` → **generic, unconditional**. A project may name samples relative to its folder or the samples root; absolute paths (all the device writes) untouched. Needed on Linux too (VM, Linux desktop) | `sim_smoke.sh` | planned |
| `fix/hwpref-date-time` *(candidate)* | `hardware/source/kick_Hwpref.cpp` | Found by reading, **not observed yet**: `setDate` / `setTime` run `std::system( "2024-5-15" )` (the comment says `date -s`), so the Hardware tab cannot set the clock on the unit either. Verify in faithful mode (phase 6) before opening | phase 6 VM | planned |

| `fix/shift-state` | `input_events/input_events.cpp`, `contracts/dcl/dcl_contracts_helper.cpp` | Found by reading in phase 4, **observed over the wire 2026-10-28** (`feature/serialport`, `fake_mcu.py`: SHIFT down then pad 1 logs `Controls::shiftBtnState=1 DclContractsHelper::isShiftPressed()=0`): the serial reader stores SHIFT in `Controls::shiftBtnState` (`input_events.cpp:248`), but `DclContractsHelper::isShiftPressed()` reads a different atomic, `contracts::shiftBtnState`, which only the in-tree simulator and the keyboard map set. On the unit SHIFT-dependent behaviour (`kick_table.cpp`, `kick_padButtonGroup.cpp`: SHIFT + pad = solo) may never trigger. Fixed: one state, written by the serial reader and the keyboard map (`DclContractsHelper::setShiftPressed`), read by `isShiftPressed()`. After: `isShiftPressed()=1` while SHIFT is held and the pad handler takes its SHIFT branch. In this tree only Assign Sample uses SHIFT + pad (no SHIFT + pad solo exists) | `fake_mcu.py` SHIFT + pad, before / after logs | pr #16 (stacked on #15) |
| `fix/keyboard-shift` *(candidate, low priority)* | `contracts/utils/keyboard/keyboard_detector.cpp` | Found by reading in the SHIFT work: the desktop keyboard map's SHIFT button message carries the inverted state (RELEASED when Shift goes down), and its `shiftBtnState` member is never initialised. Desktop keyboard only; nothing on the unit | none yet | planned |

### SEAM: the 44 `SIM_BUILD` sites

| # | File | Site | Decision | Becomes | Branch | Status |
|---|---|---|---|---|---|---|
| 1-2 | `3rdParty/juce_serialport/juce_serialport.h` | stub's `#warning` unless `SIM_BUILD` | generic | stub warns on Linux (phase 1); real POSIX port replaces it | `build/release-builds`, then `feature/serialport` | planned |
| 3-4 | `engine/.../mt_audio_manager.cpp` | `resolveRelativeSamplePaths` (definition, call) under `SIM_BUILD \|\| JUCE_MAC` | generic | unconditional | `fix/relative-sample-paths` | planned |
| 5 | `engine/.../mt_audio_manager.cpp` | `SimProject: loaded <path>` | generic | `Project: loaded <path>`, always logged | `feature/desktop-seams` | pr #2 |
| 6 | `engine/.../mt_audio_track.cpp` | `KICK_SIM_LOG_NOTES` note log | runtime | `KICK_LOG_NOTES=1`, read once | `feature/desktop-seams` | pr #2 |
| 7-8 | `hardware/source/kick_Hwpref.cpp` | `setDate` / `setTime` return early | runtime | `mt::hw::isAvailable( clock )`, otherwise log `FeatureUnavailable: clock` | `feature/desktop-seams` | pr #2 |
| 9-10 | `hardware/source/kick_WifiFirmware.cpp` | `executeCommand` / scan return early | runtime | `mt::hw::isAvailable( wifi )` | `feature/desktop-seams` | pr #2 |
| 11 | `input_events/input_events.cpp` | `#include` for `Factory<SimulatorInterface>` | emulator | removed | `feature/desktop-seams` | pr #2 |
| 12-13 | `input_events/input_events.cpp` | `SimulatorInterface` connect / send (**upstream's own two sites**, reworked) | emulator | removed; the serial path runs on every platform, and the panel is on the other end of the port | `feature/desktop-seams` (after `feature/serialport`) | pr #2 |
| 14 | `input_events/kick_SimulatorInterface.h` | interface (doc comment names `SIM_BUILD`) | emulator | file deleted | `feature/desktop-seams` | pr #2 |
| 15 | `kick_ui/dialogs/kick_AppHome.cpp` | AUTO: notification + `SimUnavailable: Automation` | generic | log line only (`AUTO: no automation engine in this firmware`); no UI change on the device | `feature/key-handlers` | planned |
| 16-17 | `modes/filebrowser/kick_fileBrowser.{h,cpp}` | SD / USB notice below the Location rows | emulator | dropped: an absent SD card is normal on a unit, so a notice would change the device. The emulator README documents it | `feature/desktop-seams` | dropped |
| 18-20 | `swupdate/kick_SWUpdateView.{h,cpp}` | notice member, add, skip the image scan | runtime | `FeatureUnavailable` when `!isAvailable( update )`; the scan is skipped only then | `feature/desktop-seams` | pr #2 |
| 21 | `kick_ui_controls/kick_ui_controls.h` | include `kick_SimUnavailable.h` | runtime | include `kick_FeatureUnavailable.h` | `feature/desktop-seams` | pr #2 |
| 22 | `kick_ui_controls/source/kick_ControlBase.h` | `simFocusTarget()` | runtime | focus fallback owned by the test-control module, active only with `KICK_TEST_CONTROL` | `feature/desktop-seams` | pr #2 |
| 23 | `kick_ControlBase.h` | `simScreen()` | generic | `ui::overlayRoot()`, registered by `apps/kick/Main.cpp` | `feature/desktop-seams` | pr #2 |
| 24 | `kick_ControlBase.h` | `hasSelectionFocus` uses the fallback | runtime | as 22 | `feature/desktop-seams` | pr #2 |
| 25 | `kick_ControlBase.h` | `setFocus` records the fallback | runtime | as 22 | `feature/desktop-seams` | pr #2 |
| 26 | `kick_ControlBase.h` | `getComponentInFocus` / `clearFocusComponent` use the fallback | runtime | as 22 | `feature/desktop-seams` | pr #2 |
| 27-28 | `kick_ui_controls/source/kick_SimUnavailable.h` | whole file under `SIM_BUILD` | runtime | `kick_FeatureUnavailable.h`, always compiled, created only for an unavailable feature | `feature/desktop-seams` | pr #2 |
| 29 | `kick_ui_controls/source/kick_table.cpp` | `mouseDown`: touch in canvas coordinates, `SimTable:` log | generic | `getEventRelativeTo( &canvas )` always; log line dropped (or `KICK_TEST_CONTROL`-gated) | `feature/desktop-seams` | pr #2 |
| 30 | `kick_table.cpp` | `getRowNumbers`: measure from the transformed / top-level root | generic | always | `feature/desktop-seams` | pr #2 |
| 31 | `kick_table.cpp` | `mouseDoubleClick` as 29 | generic | always | `feature/desktop-seams` | pr #2 |
| 32-33 | `kick_ui_controls/source/kick_utils_bubble.cpp` | bubble added to `simScreen()`, deleted after fading | generic | added to `overlayRoot()` | `feature/desktop-seams` | pr #2 |
| 34 | `kick_ui_controls/source/kick_utils_popup.cpp` | popup bounds = `layout::screen`, no `addToDesktop` | generic | child of `overlayRoot()` | `feature/desktop-seams` | pr #2 |
| 35 | `kick_utils_popup.cpp` | blurred backdrop from `simScreen()` | generic | snapshot of `overlayRoot()` | `feature/desktop-seams` | pr #2 |
| 36 | `kick_utils_popup.cpp` | `canModalEventBeSentToComponent` (panel keys beside the screen) | emulator | dropped: the panel is another process | `feature/desktop-seams` | dropped |
| 37 | `kick_utils_popup.cpp` | `getFullScreenModalPopUP` adds to `simScreen()` | generic | adds to `overlayRoot()` (was: the active top-level window) | `feature/desktop-seams` | pr #2 |
| 38 | `kick_ui_settings/kick_ui_settings.h` | `SettingsInterface::simUnavailable` + `resized` | runtime | `FeatureUnavailable` member, same re-attach on layout | `feature/desktop-seams` | pr #2 |
| 39 | `source/kick_ui_settings_cv.cpp` | CV notice | runtime | `!isAvailable( cv )` | `feature/desktop-seams` | pr #2 |
| 40 | `source/kick_ui_settings_etnernet.cpp` | Ethernet notice | runtime | `!isAvailable( ethernet )` | `feature/desktop-seams` | pr #2 |
| 41 | `source/kick_ui_settings_wifi.cpp` | Wi-Fi notice | runtime | `!isAvailable( wifi )` | `feature/desktop-seams` | pr #2 |
| 42-44 | `kick_ui_systemResources/kick_ui_systemResources.{h,cpp}` | meters notice member, add, skip the 1 Hz refresh | runtime | `!isAvailable( resources )` (false off Linux) | `feature/desktop-seams` | pr #2 |

Totals: **generic 15**, **runtime 22**, **emulator 7** (44).

- Generic: 1-5, 15, 23, 29-35, 37.
- Runtime: 6-10, 18-22, 24-28, 38-44.
- Emulator: 11-14, 16-17, 36.

Two lines added outside these sites come with them:
- `apps/kick/Main.cpp` registers `ui::overlayRoot()` (generic).
- `Kick/CMakeLists.txt` / `Kick/apps/CMakeLists.txt` drop `KICK_SIMULATOR` and `add_subdirectory( kickem )` (emulator).

### SIM: moves to `KickEm-Tools/KickEmulator`

| Path on the work branch | Goes to | Note |
|---|---|---|
| `Kick/apps/kickem/simulator/kick_Simulator.{h,cpp}` | `panel/` | Panel geometry, drawing, SHIFT semantics, LED / pad-colour decoding. The packet encoding is factored so that the in-memory path becomes the wire path (`/goal-serial` phase 3) |
| `Kick/apps/kickem/simulator/kick_SimControl.{h,cpp}` | split: panel commands → `panel/` control channel; screen commands → firmware `test_control` (above) | |
| `Kick/apps/kickem/simulator/kick_SimulatorInterfaceImpl.{h,cpp}`, `Main.cpp`, `CMakeLists.txt` | dropped / rewritten as the panel's own `main` | the panel no longer links the app |
| `Kick/assets/Default/Sim Behringer Logo.png` | not committed (trademark, see decision 4); the area is blank | decided |
| `Kick/tools/sim_launch.sh`, `sim_quit.sh` (lock), `sim_ctl.sh`, `sim_turn.sh` | `kickem` launcher and `panel` control client | machine-wide lock kept |
| `Kick/tools/sim_press.sh`, `sim_click.sh`, `sim_shot.sh`, `sim_ax.swift` | dropped (OS accessibility; the wire and test-control replace them) | |
| `Kick/tools/sim_smoke.sh`, `sim_transport.sh`, `sim_modes.sh`, `sim_pads.sh`, `sim_sampler_looper.sh`, `sim_overlays.sh` | `checks/` (ported to the panel channel + test-control) | phase 5 |
| `Kick/tools/wav_measure.py` | `checks/lib/` | |
| `docs/sim_audit/` | stays on the work branch as the record; representative images may be copied to the emulator `docs/` | |

### CONTENT: moves to `KickEm-Tools/KickEmulator/content/StarterKit/` (CC0)

| Path | Note |
|---|---|
| `Kick/tools/make_starter_kit.py` | generator, byte-identical output |
| `Kick/assets/StarterKit/**` (Kit, Instruments, Loops, Metronome, `LICENSE`) | about 1.4 MB, committed |
| `Kick/demos/Starter Kit/Starter Kit.{kickPrj,png}` | demo project |
| `Kick/assets/Default/recent_project/Starter Kit.png` | Projects-screen tile |
| `Kick/apps/kick/CMakeLists.txt` `if(APPLE)` copy | replaced by the emulator's installer |

## Topic branches and their order

All of them are cut from `kickem/main`. Each has a pull request into `kickem/main` with CI green.
The ones that touch the same regions of `kick_AppHome.cpp` and the engine are stacked: each is
cut from `kickem/main` after the previous one merged, or opened against the previous branch and
retargeted.

```
build/release-builds                        (phase 1)  BUILD + build.yml
 ├─ fix/audio-object-ids                    (phase 2)
 ├─ fix/engine-gain-staging                 (phase 2)
 ├─ fix/knob-handlers                       (phase 2)
 ├─ fix/ui-small                            (phase 2)
 ├─ fix/relative-sample-paths               (phase 2)
 ├─ fix/sequencer-timing                    (phase 2)
 │   └─ fix/next-seq-crash
 │       └─ feature/timing-correct
 │           └─ feature/note-recording
 │               ├─ feature/metronome
 │               └─ feature/key-handlers    (AUTO log, TAP TEMPO, MASTER, BACK)
 │                   └─ feature/pad-modifiers
 ├─ feature/serialport                      (phase 3)  /goal-serial 1-2, KICK_SERIAL_DEVICE
 │   └─ feature/desktop-seams               (phase 3)  every remaining SEAM; zero SIM_BUILD
 ├─ fix/hwpref-date-time                    (phase 6, only if observed in the VM)
 └─ ci/emulator                             (phase 7)  calls the emulator's reusable workflow
```

Upstreamable (phase 8, each only with the user's approval):

- `build/release-builds`, without the CI that calls KickEm-Tools.
- Every `fix/*` and `feature/*` above.
- `feature/serialport`.
- `feature/desktop-seams`. Its removal of the dead upstream `SimulatorInterface` and the old
  `apps/kickem` is part of it and is explained in the PR.

Not upstreamable: `ci/emulator`, unless the user wants it.

The fast-mode checks need, in the fork: the build, `feature/serialport`, `feature/desktop-seams`
and `fix/relative-sample-paths` (Starter Kit loads). Each other check also needs the fixes it
measures. Until a fix is merged, phase 5 runs that check against the fix branch and reports the
drift.

## Progress

| Phase | State | Where |
|---|---|---|
| 1 release builds | done (2026-10-28), CI green (runs 40771852889, 40773282546): `Kick` on macOS, Linux x86_64, `KT_Kick` (the `Kick` target, renamed as the device installs it) on Linux arm64, artifact `KT_Kick-linux-arm64` (tarball + manifest; native Ubuntu 24.04 build, needs glibc >= 2.39). Deviations: the stub notice is `#pragma message` (the project's `-w` hides `#warning`); `mt_AudioDevice` from `da3a94f9` (before the probe); `Kick/README.md` Building section points to `docs/DEVELOPMENT.md`; the in-tree AudIO / ComProtocol default applies only while `MT_PLATFORM_GIT_PATH` is the placeholder | PR #1, `build/release-builds` at `6f69e7b1` |
| 0 inventory | done (2026-10-28) | this file, `6b6024ce` |
| 3 serial port | done (2026-10-28), CI queued: POSIX `juce_serialport` written from scratch, `KICK_SERIAL_DEVICE`, serial link on macOS, round-trip test (95 checks) and pty test (29), handshake LIVE over a pty on macOS (0.47 s) and Linux arm64 (0.11 s), device default `/dev/ttymxc0` unchanged | PR #15, `feature/serialport` at `da824faa` |
| 3 desktop seams | done (2026-10-28), CI green on `e997af3f`: `ui::overlayRoot()`, list hit-testing, `mt::hw::isAvailable` + `KICK_HARDWARE` + `ui::FeatureUnavailable`, `Kick/modules/test_control` (`KICK_TEST_CONTROL`), `KICK_AUDIO_PROBE` / `KICK_AUDIO_INPUT`, `KICK_LOG_NOTES`, `Project: loaded`; `SimulatorInterface`, old `apps/kickem` and `KICK_SIMULATOR` removed. **`git grep SIM_BUILD` on the branch: 0.** Popups shown inside the app's 1024x600 window on macOS; device path unchanged with the variables unset (macOS and Linux arm64 logs) | PR #2 (draft), `feature/desktop-seams` at `e997af3f` |
| 5a fast mode | done (2026-10-28): `kickem-panel` is its own program (only `juce_gui_basics` + the fetched ref's protocol), pty / tcp / serial transports, `kickem run --mode fast`, `checks/smoke` **passes over the wire against `integration/standalone`** (10 / 10, max -3.1 dBFS). DoD 2: sound 7.3 s after a 128 s first build (target build + 30 s). Against `kickem/main` it fails as expected (no `AudIO_juce`, no serial port) | KickEmulator `c2077d3`; firmware `integration/standalone` at `cc5fa831` |
| 4 emulator repository | done (2026-10-28): AGPL `LICENSE`, layout, clean import naming `6b6024ce` (no `filter-repo`: the simulator commits also changed firmware), firmware fetch by URL / ref with `kickem::protocol` from the fetched tree, `protocol_roundtrip` passes against `kickem/main`, Starter Kit regenerates byte-identical, `kickem fetch` and the machine lock work; logo not committed | `KickEm-Tools/KickEmulator` `main` at `7395d4e` |

Wire-format notes from phase 4, for the phase 5 panel:
- The app's reader expects one byte after the CRC: it reads `size + 7` bytes per frame. The panel must send a trailing `\n`.
- The app sends its own frames without that byte.
- A frame with no data never completes in `PushByteToRxFifo`.
- The partial-frame buffer is a function-level `static`, shared by every `CPktMgr` in the process.
- Added in phase 3 (`docs/DEVELOPMENT.md` section 8 on `feature/serialport`):
  - An empty payload also spoils the next frame.
  - A read that doesn't start with `MT`, or holds an incomplete frame, is discarded whole. A frame
    split across two reads therefore loses itself and the frame after it.
  - The app's receive buffer is 20 bytes with no bounds check, so a surface payload can be at most
    20 bytes.
  - `reconnect()` runs only on `EN_SYS_RECONNECT`. It leaves the send thread stopped (from reading
    the code, not yet exercised), and nothing calls it on a hang-up.
  - These are the targets for phase 5's fault test.
- `Kick` is single-instance (`moreThanOneInstanceAllowed()` returns false): a second one exits at
  once with no log. The emulator's machine lock must cover the plain `Kick` too.
- New log names from phase 3, which the checks must be ported to:
  - `TestControl: ...` (was `SimControl:`)
  - `FeatureUnavailable: <feature> shown|hidden` (was `SimUnavailable:`)
  - `FeatureUnavailable: <feature>: command not run: <cmd>`
  - `Hardware: KICK_HARDWARE=...`
  - `Note: track ...` (was `SimNote:`)
  - `Project: loaded <path>` (was `SimProject:`)
- `TestControl` `tap` / `click` deliver to the touched component only, so a parent's mouse
  listener (`KickTableView` canvas) does not hear them. Selecting list rows by touch still needs a
  check in phase 5.
- On Linux desktop the plain app runs `ifconfig eth0 192.168.0.1` and `timedatectl set-timezone`
  at startup: the emulator must set `KICK_HARDWARE=none` there.
- `integration/standalone` (scratch, never merged) = `build/release-builds` + `desktop-seams` + `fix/shift-state` (brings all of `feature/serialport`) + `relative-sample-paths` + `audio-object-ids` + `engine-gain-staging` + `knob-handlers` + `ui-small` + `pad-modifiers` (the whole stacked chain) + `metronome`. Two conflicts:
  - `docs/DEVELOPMENT.md` sections 8 and 9, between serialport and seams.
  - `loadProject`, between relative paths and the `Project: loaded` log.

  Expect both again when the PRs merge into `main`.
- Keys that arrive while a project is loading are ignored (`RecentProject::isProjectLoading`). Checks wait for `Project: loaded` plus a settle time.
- Phase 1 docs gap: on bookworm without webkit, `libxi-dev` is needed, because the JUCE multitouch
  patch includes `XInput2.h`.
