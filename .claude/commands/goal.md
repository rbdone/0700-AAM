---
description: Make the KT_KickEM simulator behave like the real Kick: every panel control does what it does on the device, including making and controlling music. Optional argument: a phase number to work on only that phase.
---

# Goal: the simulator plays like the device

You are working on branch `simulator-macos` of the Behringer KICK (BE 0700-AAM) repository. Read
`docs/DEVELOPMENT.md` first; it explains how the code is put together, how to build and run, and
every change already made to get the app and the simulator running on macOS.

## Where things stand

- `Kick/build/KT_KickEM.app` is a to-scale front panel around the real Kick screen. Every pad,
  key, encoder and the slider already sends the exact `CPktMgr::HWMsg_t` packet the hardware
  sends, and the app's LED / pad-colour packets are shown. Transport of input is **not** the
  problem: `Kick/tools/sim_press.sh "Pad 1" MAIN PUSH PLAY GRID` presses controls by name and the
  app logs `Data[0] From DCL:...` for each one.
- A caption printed under a key is that key's **SHIFT function**: on the unit you hold SHIFT and
  press the key, and the control-surface firmware reports the secondary id (SHIFT + MAIN arrives
  as `EN_GRID`). The simulator reproduces this: the SHIFT key is a toggle, keys pressed while it
  is on send their secondary id, and clicking a caption performs the whole hold-press-release.
  `sim_press.sh GRID` does the same combo. The app also tracks the SHIFT state itself
  (`DclContractsHelper::isShiftPressed`) for modifier behaviour in grids and pad groups.
- The app is a hollow shell on this branch because the public release stripped all content:
  the demo and template projects are empty `<ROOT>` documents with no programs, sequences or
  samples; the factory samples, metronome sounds and the General MIDI SoundFont are gone; the
  Myriad Pro font is gone. Pads therefore make no sound and most screens have nothing to show.
- Several engine pieces are **reconstructions**, not original code, and nobody has heard them
  yet: `mt::AudioDevice`, `mt::ADSR`, `mt::Filter`, `mt::Recorder` / `SampleRecorder` /
  `Looper` (all under `AudIO/AudIO_juce/`), and the constants in
  `AudIO/AudIO_constants/AudIO_constants.h`. Treat their values and behaviour as hypotheses to
  verify by ear and by measurement.
- The screens that react to hardware packets are the 62 `handleCommand( CPktMgr::HWMsg_t& )`
  implementations under `Kick/modules/kick_ui`, `kick_ui_settings`, `kick_ui_channel_mixer`
  and `program_edit_ui`. `contracts::DclContractsHelper` decodes packets;
  `contracts::DeviceControlContracts` names every key, pad and knob.

## Definition of done

The simulator, started from `Kick/build` with no hardware attached, does all of this, and a
script proves it:

1. **Sound out of the box.** A fresh checkout boots into a starter project where every pad in
   bank A plays a drum sound and banks B-D have at least one playable program each. Pressing a
   pad is audible through the default audio device and shows in the app's meters.
2. **Transport works.** PLAY starts the sequencer from the current position, PLAY START from the
   top, STOP stops, RECORD and OVERDUB arm and capture pad hits into the current sequence, the
   bar:beat:tick display runs, TAP TEMPO sets tempo, and the metronome clicks when enabled.
3. **Every panel control does its device job.** Each function key opens its mode, each SHIFT
   function (SHIFT held plus the key under whose caption it is printed) does its secondary job,
   bank keys (and SHIFT + bank for E-H) switch pad banks and
   the pad colours follow, P1-P4 edit the parameters the current screen assigns to them, the jog
   wheel scrolls and PUSH selects, + / - step values, BACK / COPY / ERASE / FULL LEVEL /
   16 LEVELS / NOTE REPEAT / LATCH / NOTES behave as their names say, and the slider controls
   what the current screen assigns to it.
4. **Sampler and looper record and play back.** With a test signal on the input (see phase 4),
   SAMPLER records a sample, SAMPLE EDIT trims it, it can be assigned to a pad and played; the
   LOOPER records a loop and plays it back in time.
5. **Nothing hardware-only crashes.** CV/Gate, Wi-Fi, Ethernet, LINK, firmware update, camera and
   SD / USB storage screens open and close cleanly in the simulator, showing an honest
   "not available in the simulator" state rather than hanging or asserting.
6. **It is all documented and scripted.** `docs/SIMULATOR_STATUS.md` lists every control, the
   screen(s) that handle it, what it does on the device, and whether it works here;
   `Kick/tools/sim_smoke.sh` boots the simulator, loads the starter project, plays it, and fails
   if the engine output stays silent or a key's LED feedback does not arrive.

## How to work

Work in phases, in order unless a phase number was given as the argument, in which case do only
that phase. Commit at the end of each phase with a message that says what now works. Keep
`docs/DEVELOPMENT.md` and `docs/SIMULATOR_STATUS.md` current as you go. Independent phases
(for example 2 and 4) may be handed to sub-agents, but each sub-agent must build and run the
simulator itself before reporting.

Verification is not optional. For every behaviour you claim, show the evidence: the packet in the
app log, a screenshot of the simulator window (capture it by window id, see
`docs/DEVELOPMENT.md`), and for anything audible a measurement. For audio measurements add an
opt-in probe to `mt::AudioDevice` (environment variable `KICK_SIM_AUDIO_PROBE=<file>`) that
writes the master output to a WAV and logs peak / RMS per second, so a script can assert that
sound happened. Never report "works" from reading code alone.

### Phase 1: audit

Run the simulator and press every control with `sim_press.sh` while reading the log and the
screen. For each control record: packet seen, handler(s) reached, expected device behaviour (from
the handler code and the captions on the unit), observed behaviour, gap. Put the matrix in
`docs/SIMULATOR_STATUS.md`. Note which gaps are content, which are reconstructed-engine
behaviour, and which are code paths that assume hardware.

### Phase 2: starter content

Nothing proprietary can come back, so make content that is clearly ours to ship:

- `Kick/tools/make_starter_kit.py` synthesises a 16-sound drum kit (kick, snare, hats, toms,
  claps, percussion; 48 kHz WAV) plus a short metronome click set, deterministic, no external
  assets. Put the output under `Kick/assets/StarterKit/` and license it CC0 in a `LICENSE` file
  next to it.
- A starter project (`Kick/demos/Starter Kit/`) with a drum program on bank A using those
  sounds, a four-bar sequence that plays them, a keygroup program on bank B if the keygroup
  engine can run without the SoundFont (otherwise a second drum program), and a clip or audio
  program on bank C. The app must load it automatically on first start, or it must be the first
  item in the Projects list with a thumbnail.
- Wire the metronome to the click set (`kick_settings.cpp`, `mt_audio_metronome.h`).
- Sample roots: on desktop the engine resolves samples under `./data/samplesKick/`
  (`kick_paths.cpp`); make the starter kit resolve there too, or fix the path logic so a
  project's samples can live beside it.

Done when pressing Pad 1-16 in bank A is audible and measurable, and the Projects screen offers
the starter project.

### Phase 3: transport, sequencer, tempo, metronome

Make the five transport keys, TAP TEMPO, the bar:beat:tick counter, the metronome and
recording of pad hits work end to end on the starter project. Expect to find real gaps in the
reconstructed engine glue here (voice lifecycle after release, envelope idle levels, block
re-timing); fix them in `AudIO/AudIO_juce/` and record what you changed and why in
`docs/DEVELOPMENT.md` section 5.1.

### Phase 4: sampler and looper

The simulator's audio input is whatever the default device's input is, which may be silent or
denied. Add an opt-in test source to `mt::AudioDevice` (`KICK_SIM_AUDIO_INPUT=<wav or
"tone">`) that feeds a file or a generated tone into the engine inputs. Then make SAMPLER record,
SAMPLE EDIT trim, assignment to a pad, and the LOOPER record / overdub / play work, verified
with the probe from "How to work".

### Phase 5: every remaining control

Go through `docs/SIMULATOR_STATUS.md` and close each remaining gap: SHIFT + key functions, Q-Link
assignments per screen, jog navigation in lists and grids, + / -, BACK, COPY, ERASE, FULL and
HALF LEVEL, 16 LEVELS, NOTE REPEAT, LATCH, NOTES, PAD MIX / PAD MUTE / TRACK MIX / TRACK MUTE,
NEXT SEQ, FX, GRID, SAVE, MENU / PREFS, the slider. Where a screen is unreachable or broken
because of missing content, extend the starter project rather than stubbing the screen.

### Phase 6: hardware-only screens

Wi-Fi, Ethernet, CV/Gate, LINK, firmware update, camera, SD card and USB storage all talk to
Linux device nodes under `/dev` and `/mnt`. Under `SIM_BUILD`, make each of them open, show an
explicit unavailable state, and close without blocking the UI or asserting. Keep the device code
paths untouched outside `#ifdef SIM_BUILD`.

### Phase 7: smoke test and wrap-up

Write `Kick/tools/sim_smoke.sh`: build, launch, wait for the handshake, press PUSH on the
starter project, PLAY, wait four bars, STOP; assert the audio probe saw sound, assert the
expected LED packets were logged, capture a screenshot, exit non-zero on any failure. Document
it. Finish by updating `docs/DEVELOPMENT.md` with a short "what works in the simulator" section
and a list of what is still different from the hardware.

## Constraints

- Do not add any content you do not have the rights to ship. Synthesise it.
- Keep the device build intact: anything simulator-specific lives behind `SIM_BUILD` or in
  `Kick/apps/kickem/` and `Kick/tools/`.
- Reconstructed values in `AudIO_constants.h` may be changed, but every change needs the
  observation that motivated it written next to it.
- Build with the command in `docs/DEVELOPMENT.md` section 3 (remember
  `MACOSX_DEPLOYMENT_TARGET=12.0`). Run from `Kick/build`.
- Do not push. Commit per phase on `simulator-macos`.
