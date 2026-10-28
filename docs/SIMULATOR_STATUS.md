# Simulator status: every panel control

This file records, control by control, what the KT_KickEM simulator does today compared with
the device. It started as the Phase 1 audit of `.claude/commands/goal.md` (2026-10-27, an empty
project); every later phase updated the rows it changed, and the "Open gaps" section near the end
is current as of 2026-10-28.

## Current status (2026-10-28)

On a fresh build with the synthesised Starter Kit project (`docs/DEVELOPMENT.md` 4.4), every
check script passes:

| Script | What it proves | Last run |
|---|---|---|
| `Kick/tools/sim_smoke.sh` | Boot, handshake, PUSH loads Starter Kit, four bars play and stop, LEDs, no clipping | 10 / 10 |
| `Kick/tools/sim_transport.sh` | PLAY / PLAY START / STOP, BAR:BEAT:TICK, RECORD, OVERDUB, TAP TEMPO, metronome, gain staging | 21 / 21 |
| `Kick/tools/sim_modes.sh` | Every function key and SHIFT function, mixers, mutes, NEXT SEQ, TC, MASTER, SAVE, no crash on an empty project | 50 / 50 |
| `Kick/tools/sim_pads.sh` | Banks, FULL / HALF LEVEL, 16 LEVELS, NOTE REPEAT, LATCH, NOTES, ERASE, COPY, BACK, jog, + / -, P1-P4, slider | 25 / 25 |
| `Kick/tools/sim_sampler_looper.sh` | Sampler record, sample edit trim, pad assignment, looper record / overdub / loop in time | pass |

Of the controls in the tables below, everything is **works** except: LINK (no visible Q-Link
page change), AUTO (no automation engine in this release), FX (the XY view is not connected to
audio in this release), P1-P4 in PROG EDIT (the screen assigns them nothing), and the rows marked
"not re-checked". The remaining differences from the unit are listed under "Open gaps".

How it was produced: `KT_KickEM` was built from this branch (`docs/DEVELOPMENT.md` section 3) and
run with `Kick/tools/sim_launch.sh` and `KICK_SIM_LOG_SURFACE=1` and `KICK_SIM_AUDIO_PROBE=<wav>`.
The Mac's screen was locked during the audit. Accessibility clicks (`sim_press.sh`) and
window-server captures (`sim_shot.sh`) do not work while the screen is locked, so every control
was driven through the in-app channel instead: `sim_ctl.sh press`, which uses the same
`Button::triggerClick()` path as an accessibility press, and `sim_ctl.sh turn` / `slider`.
Screens were captured with `sim_ctl.sh shot`, which renders the panel and any popups. Each
control was pressed in two states:

- **Boot**: the Projects screen right after start-up, with no project loaded.
- **Loaded**: after PUSH on Demo 01, with MAIN pressed before every control so each
  observation can be traced to that control.

A third pass pressed controls inside the screen that handles them, for example jog and PUSH in
MENU, pads in BROWSE, RECORD in LOOPER, and P1-P4 in each mode. Every "Observed" cell below
comes from the app log or a screenshot from these runs. Expected behaviour comes from reading
the handlers.

Demo 01 is an empty `<ROOT>` project: it has no sequences, tracks, programs or samples. Many
gaps below come from that.

Representative screenshots are in `docs/sim_audit/`:

| File | Shows |
|---|---|
| `01_boot_projects.png` | The boot screen |
| `02_loaded_main.png` | Demo 01 loaded |
| `03_mode_screens_1.png` | Browse, Track Mix, Track Mute, Next Seq, Step Seq, Sampler, Sample Edit, Prog Edit, Grid |
| `04_mode_screens_2.png` | Pad Mix, Pad Mute, FX, Prefs, Looper, Link Edit, MIDI Ctrl, Notes, Menu |
| `05_bank_leds_fixed.png` | The bank LEDs after the sim fix |
| `06_prefs_wifi_unavailable.png` | The WiFi tab |
| `07_looper_sampler.png` | Looper RECORD / PLAY, Sampler P1, FX P1 |

A last pass on the build that includes Phase 6 (`rbdone/goal-simulator-controls` at cd9511e7,
merged here) repeated a subset and confirmed the results below. It covered boot jog, PUSH,
banks C / F, the PREFS tabs, P1 / P2 / jog / slider on MAIN, TRACK MUTE, and the NEXT SEQ crash.

## Legend

**Packet.** Every key arrives as BUTTON (type 1), cmd `EN_BTN_STATUS`, `data = {id, state, velocity, aftertouch}`.
The app logs it as `Data[0] From DCL:<id> cmd From DCL : 0 type From DCL : 1`. Other packet types:

| Type | Packet |
|---|---|
| Pad | PAD (type 3), cmd `EN_PAD_STATE`, `data[0]` = pad 0-15; press and release are both sent |
| Jog and P1-P4 | KNOBS (type 4), cmd `EN_KNOB_ROTARY` (0) or `EN_KNOB_QLINK` (1), `data = {knob, direction, steps}` |
| Slider | SLIDER (type 7), `data[0]` = 0-100 |

A SHIFT function, such as a caption or banks E-H, arrives as three packets: SHIFT (31) pressed,
then the secondary id, then SHIFT (31) released.

**Log suffixes.** These are appended by the dispatcher in `KickHome::handleCommand`
(`kick_ui/dialogs/kick_AppHome.cpp`):

| Suffix | Meaning |
|---|---|
| `In Recent Prj` | A project is loaded. The name is misleading. |
| `In is button` | A button was pressed. |
| `In screen <id>` | The global mode-key switch ran. |
| `register handle command` | The packet was handed to the current screen's `handleCommand`. |
| `project is loading and center knob message` | Boot state only: the packet went to the Projects screen. |

**LED feedback.** These lines come from `KICK_SIM_LOG_SURFACE=1`.

| Line | Meaning |
|---|---|
| `SimSurface: LED cmd=0 data=<id>,<on>` | Set one key LED. |
| `SimSurface: PAD cmd=<n> ...` | A pad colour command (`PadCmd`). |

**Status.**

| Status | Meaning |
|---|---|
| works | Does what the device does, observed. |
| partial | The packet is handled and something visible happens, but not everything the device does. |
| missing | Nothing observable, while the device would do something. |
| crash | The app died (SIGSEGV; crash report in `~/Library/Logs/DiagnosticReports`). |

**Gap class.**

| Class | Meaning |
|---|---|
| content | Needs programs, samples, sequences or tracks (phase 2). |
| engine | Behaviour of the reconstructed `AudIO/AudIO_juce` or `AudIO_constants.h`. |
| hardware | The code path assumes `/dev`, `/mnt` or Linux devices. |
| sim | A bug in the simulator panel or the scripting tools. |
| code | The app has no handler for the key (or drops the packet) on the device too. This is an extra class beyond the four in the plan, for keys that need new UI code (phase 5) rather than content. |
| none | No gap. |

Audio: the probe logged `peak=-120.0dBFS` (silence) for every second of all audit runs: 911
seconds in total, with no exception.

## Boot state (Projects screen, no project)

`KickHome::handleCommand` only forwards jog rotate and jog push to the Projects screen
(`ui::RecentProject`). Every other packet is logged once (`Data[0] From DCL:<id> ...`) and goes
no further.

All 79 other panel controls were pressed: pads, banks, function keys, SHIFT functions,
transport, small keys and the encoders P1-P4. For every one:

- the log showed the packet;
- the screen was pixel-identical before and after (same screenshot hash);
- no LED packet was sent.

The device does the same, so these count as **works (ignored at boot)**. The rows below cover
only controls that do something at boot.

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| Jog wheel | KNOBS `EN_KNOB_CENTER` (0), cmd `EN_KNOB_ROTARY` | `RecentProject::handleCommand` | Moves the highlight through demos, then templates, then recent projects. | `... project is loading and center knob message` per detent. +2 moved the highlight Demo 01 to Demo 03; -1 moved it to Demo 02 (screenshots). | works | none |
| PUSH (jog click) | `EN_KNOB_ENTER` (26) | `RecentProject::handleCommand` → `loadProject` | Loads the highlighted demo or recent project. A template asks for a name first. | Demo 01 loaded: Main screen (Track tab). LEDs: STOP `44,1,1`, bank A `0,1`, FULL LEVEL `48,1,0`, MAIN `8,1,0`. Also pad colour mode / settings / aftertouch packets. | works | none |
| Slider | SLIDER 30 / 80 | none at boot | Ignored. | Packet logged; only the panel's own LED ladder moved. | works | none |
| SHIFT | `EN_SHIFT` (31) | none at boot | Ignored (modifier). | Packet logged; the panel SHIFT key lit and unlit. | works | none |

## Loaded state: pads

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| Pads 1-16 on MAIN | PAD `EN_PAD1`..`EN_PAD16` (0-15) | `KickHomeListView` / `kick_AppMainMode.cpp` → `ae.addLiveMidiMsg` note on/off on the selected track | Plays the pad's sound and lights it in the program's pad colour. | Phase 2 (Starter Kit): the probe goes from -120 dBFS to 16 distinct hits; pad colours `PAD cmd=6 data=12,13,14,9,...`. Since phase 3's gain fix Pad 1 is centred at -13.3 dBFS (velocity 100). Phase 1 (empty project): no sound, nothing to play. | works | none |
| Pad in BROWSE | PAD 0 | `fileBrowser::handleCommand` | Auditions the selected file on the pad and highlights the pad. | Phase 1: pad highlight sent, `PAD cmd=1 data=0,1,3`; there was nothing to audition. Not re-checked with the Starter Kit. | partial (not re-checked) | none known |
| Pads in NOTES (Pad Perform) | PAD 0 | `PadPerform::handleCommand` | Plays a scale or chord note per pad (Chromatic by default). | Phase 5: on the Starter Kit's Keys track, pads 1 / 2 / 13 play 130.8 / 138.8 / 261.7 Hz (C3, C#3, C4: chromatic, pad 13 an octave up), measured by `sim_pads.sh`. FULL / HALF LEVEL and NOTE REPEAT apply here too. | works | none |
| Pads in TRACK MUTE / PAD MUTE | PAD 0 | `KickPadButtonGroupComponent` (`kick_padButtonGroup.cpp`) → `triggerClick` | Toggles mute (or solo with SHIFT) of the track or pad. | Phase 5 (`sim_modes.sh`): TRACK MUTE pads mute tracks while playing (all three muted: -120 dBFS); PAD MUTE Pad 1 mutes the kick (-120 dBFS, unmuted -13.3 dBFS). SHIFT + pad (solo) not checked. | works | none |
| Pads in STEP SEQ | PAD 0 | `StepSeq::handleCommand` | Toggles the step. | 2026-10-28 (Starter Kit, Drums, pad A01): Pad 2 sets step 2: `PAD cmd=10 ... 120,127,0,...` (steps 1, 2 and 9 now set) and the screen shows the new bar. | works | none |
| Pads in SAMPLER | PAD 0 | `Sampler::handleCommand` | Starts recording or marks a slice. | Phase 4 records from the screen's record button with `KICK_SIM_AUDIO_INPUT` (DEVELOPMENT.md 4.6); starting a take from a pad was not checked. | partial (not re-checked) | none known |
| Pads in PROG EDIT, MIDI CTRL | PAD 0 | `ProgramEdit::handleCommand`, `MidiControl` | Selects the pad or plays it; MIDI-learns the control. | 2026-10-28, PROG EDIT on Starter Drums: Pad 2 plays (probe -12.9 dBFS) and selects the pad. MIDI CTRL learning not checked. | works (PROG EDIT) | none |
| SHIFT + pad on MAIN | PAD 0 with SHIFT on | Main | Modifier behaviour per screen. | 2026-10-28: SHIFT, Pad 1, SHIFT on MAIN plays the kick like a plain hit (-13.3 dBFS); no other effect was found in the handlers. | works | none |

## Loaded state: banks

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| A, B, C, D | `EN_BANK_A`..`EN_BANK_D` (0-3) | Global switch → `ae.setBank`; `kick_leds.cpp` `setBankMessage` + `refreshPadLeds`; StepSeq / Pad Mute / Track Mute page their own grids | Switches pad bank (pad note offset); bank LED; pads recolour for the program on the new bank. | Phase 5, Starter Kit (Drums track): `LED cmd=0 data=N,1` and one `PAD cmd=6` per bank: A the kit colours `12,13,14,9,16,...`, B all 6 (keys), C green, D pink / orange; pads play the bank's notes (phase 2). | works | none |
| E, F, G, H (SHIFT + A-D) | `EN_BANK_E`..`EN_BANK_H` (4-7) | Same | Banks E-H (notes 64-127). | Phase 5: `LED cmd=0 data=4..7,1` (the A-D key under the caption lights), `PAD cmd=6 data=0,...,0,3,...,3`: every pad "no sample", because Starter Drums has nothing above note 63. The pads follow; there is just nothing to play there. | works | content (nothing on E-H) |
| Bank LEDs (sim fix) | n/a | `Simulator::handleLedMessage` | One bank lit at a time. | Before the fix, LEDs piled up (A, then A+B, then A+B+C...), because the app only ever sends "bank N on". Fixed in the panel; verified: C, then E lights A, H lights D, then A. | works (fixed) | sim |

## Loaded state: function keys (row 1 and row 2)

All mode keys go through the global switch in `KickHome::handleCommand` (`sigScreenClose(<mode>)`).
On close the previous mode's LED is turned off (`LED <id>,0,0`); on load the new mode's LED is
turned on.

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| MAIN | `EN_MAINMODE` (8) | Global → `HomeListView` | Main screen (Sequence / Track / Program tabs). | Phase 5 (`sim_modes.sh`, Starter Kit): Main screen with the kit's tracks / programs; `LED 8,1,0`, previous mode LED off. | works | none |
| BROWSE | `EN_BROWSE` (10) | Global → `MenuFileBrowser` | File browser: Internal, Factory Sample Library, Demos, SD / USB. | Phase 5 (`sim_modes.sh`, Starter Kit): File Browser; `LED 10,1,0`. SD / USB rows are hardware (see 4.5). | works | hardware (SD/USB) |
| TRACK MIX | `EN_TRACK_MIX` (12) | Global → `MenuChannelMixer` | Track mixer strips; Q-Links edit the strip. | Phase 5 (`sim_modes.sh`, Starter Kit): Track Mixer with the three program strips; `LED 12,1,0`. P1 = row 1 OUT, P2 = LEVEL (100 → 50), P3 = PAN; no crash, also with an empty project (fixed). Since phase 3 the level is heard: 100 → 50 takes Pad 1 from -13.3 to -19.3 dBFS. | works | none |
| TRACK MUTE | `EN_TRACK_MUTE` (14) | Global → `MenuTrackMute` | Track mute grid on the pads. | Phase 5 (`sim_modes.sh`, Starter Kit): T01 Drums, T02 Keys, T03 Loops; `LED 14,1,0`. Pads mute tracks while playing: drums alone peak -7.2 dBFS, all three muted -120 dBFS, unmuted -3.2 dBFS (probe, 2026-10-28; before phase 3's gain fix the unmuted mix clipped at +2 dBFS). | works | none |
| NEXT SEQ | `EN_NEXT_SEQ` (16) | Global → `MenuNextSequence` | Sequence grid; pads queue sequences. | Phase 5 (`sim_modes.sh`, Starter Kit): S01 / S02 (starter Sequence 02 added); `LED 16,1,0`. While playing, Pad 2 queues Sequence 02, which takes over at the end of Sequence 01 (status bar Sequence 02 / Drums Live). P1 = BPM (120 → 122, list now refreshes), P2 length, P3 / P4 loop points; no crash with an empty project (fixed). | works | none |
| STEP SEQ | `EN_STEP_SEQ` (32) | Global → `HomeSequencerView` | Step sequencer on the pads. | Phase 5 (`sim_modes.sh`, Starter Kit): Step Sequencer on Drums, pad A01 steps 1 and 9 set, TIME DIV 16; `LED 32,1,0`. | works | none |
| TC | `EN_TC` (34) | Global → `TimingCorrect::showWindow` (new) | Timing Correct window (quantise). | Phase 5 (`sim_modes.sh`, Starter Kit): TIMING CORRECT window: on/off, NOTE VALUE (the sequence's quantization, shared with Step Seq / Grid), STRENGTH. Apply with TC on moved 23 of 23 off-grid notes of Sequence 02 "Drums Live" onto the 1/16 grid (checked in the saved project). No LED of its own; the TC LED shows ON/OFF. | works | none (new code) |
| SAMPLER | `EN_SAMPLER` (36) | Global → `MenuSampler` | Sampler: record input to a sample. | Phase 5 (`sim_modes.sh`, Starter Kit): Sampler screen; `LED 36,1,0`. Recording: phase 4 (4.6). | works | none |
| SAMPLE EDIT | `EN_SAMPLE_EDIT` (38) | Global → `MenuSampleEdit` | Sample editor. | Phase 5 (`sim_modes.sh`, Starter Kit): Sample Editor; `LED 38,1,0`. Editing: phase 4 (4.6). | works | none |
| PROG EDIT | `EN_PROG_EDIT` (40) | Global → `MenuProgramEdit` | Program editor for the selected program. | Phase 5 (`sim_modes.sh`, Starter Kit): Program Edit on Starter Drums (Main / Samples / Layer Mix / Envelopes / LFO); `LED 40,1,0`. | works | none |

## Loaded state: SHIFT functions (captions)

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| GRID (SHIFT+MAIN) | `EN_GRID` (9) | Global (second switch) → `HomeGridView` | Piano-roll grid of the track. | Phase 5 (`sim_modes.sh`, Starter Kit): Grid View of Drums with its notes; `LED 9,1,0`. | works | none |
| SAVE (SHIFT+BROWSE) | `EN_SAVE` (11) | Global → `ae.saveProject()` → `KickXML::save` | Saves the project; writes only if the XML changed. | Phase 5 (`sim_modes.sh`, Starter Kit): `Starter Kit.kickPrj` rewritten (mtime), and after TC the saved file holds the moved notes. No LED (none defined). | works | none |
| PAD MIX (SHIFT+TRACK MIX) | `EN_PAD_MIX` (13) | Global → `MenuPadMixer` | Per-pad mixer. | Phase 5 (`sim_modes.sh`, Starter Kit): Pad Mixer of Starter Drums (01 Kick ...); `LED 13,1,0`. P1 = pad A01 LEVEL (was off by one row and addressed nothing, fixed), P2 PAN, P3 mute / solo; no crash with an empty project (fixed). Heard since phase 3: P1 -50 takes Pad 1 to -120 dBFS, P1 +50 restores -13.3 dBFS (after the 2026-10-28 knob-routing fix, see Open gaps). | works | none |
| PAD MUTE (SHIFT+TRACK MUTE) | `EN_PAD_MUTE` (15) | Global → `MenuPadMute` | Pad mute grid. | Phase 5 (`sim_modes.sh`, Starter Kit): Pads A01-A16 named after the kit; `LED 15,1,0`. Pad 1 mutes the kick: Pad 1 on MAIN peaks -120 dBFS muted, -13.3 dBFS unmuted (probe; -9.2 before phase 3's gain fix). | works | none |
| FX (SHIFT+NEXT SEQ) | `EN_FX` (17) | Global → `HomeXYView` | XY effects; Q-Links: preset, attack, release, wet/dry. | Phase 5 (`sim_modes.sh`, Starter Kit): XY Effects; `LED 17,1,0`; P1-P4 edit the Setup panel's preset / attack / release / wet-dry. No sound change: `XyFxView` only updates its labels, nothing connects it to the engine in this release (no XY effect code). | partial | code (XY FX not connected to audio) |
| MASTER (SHIFT+TAP TEMPO) | `EN_MASTER` (19) | Global → `KickHome::toggleMasterTempo` (new) | Switches the selected sequence between its own tempo and the master tempo. | Phase 5 (`sim_modes.sh`, Starter Kit): Sequence 02 (own tempo 100 BPM): `Tempo source: Sequence 02 Master Tempo: 120 BPM`, again: `Sequence Tempo: 100 BPM`; the Main screen's Global toggle follows; notification bubble; `LED 19,1` / `19,0` (the panel has no LED under the caption). | works | none (new code) |
| LATCH (SHIFT+NOTE REPEAT) | `EN_LATCH` (25) | `PadModifiers` (`kick_ui/dialogs/kick_PadModifiers.cpp`, new) | Latches note repeat on, so held pads repeat without holding NOTE REPEAT; again (or NOTE REPEAT) unlatches. | Phase 5: `LED 24,1` + `LED 25,1` (the panel lights NOTE REPEAT for both); a 1 s held clave retriggers 8-9 times, median 125-130 ms (1/16 at 120 BPM); unlatched, the same hold gives 1 onset. | works | none |
| PREFS (SHIFT+MENU) | `EN_PREF` (23) | Global (second switch) → `HomePreferencesView` | Preferences (Info, Project Defaults, Hardware, MIDI, CV, Ethernet, WiFi). | Phase 5 (`sim_modes.sh`, Starter Kit): Preferences / Info; `LED 23,1,0`. WiFi / Ethernet / CV show "Not available in the simulator" (4.5). | works | hardware (handled by phase 6) |
| AUTO (SHIFT+STEP SEQ) | `EN_AUTOMATION` (33) | Global → notice (`SIM_BUILD`) | Automation read / write. | Phase 5 (`sim_modes.sh`, Starter Kit): Notification "Automation is not available in this firmware", `SimUnavailable: Automation`. There is no automation engine in the release (the mode header's Automation button has an empty `onClick`; nothing records or plays parameter changes), so there is nothing to switch. | missing | code (no automation engine) |
| ON/OFF (SHIFT+TC) | `EN_ON_OFF` (35) | Global → `TimingCorrect::setEnabled` (new) | Timing Correct on / off. | Phase 5 (`sim_modes.sh`, Starter Kit): Toggles Timing Correct: `LED 34,1` (TC lit) / `LED 34,0`, notification "Timing Correct ON 1/16". With it on, TC's Apply moves notes and `TimingCorrect::correct()` snaps a recorded start (for phase 3 recording). | works | none (new code) |
| LOOP (SHIFT+SAMPLER) | `EN_LOOPER` (37) | Global → `MenuLooper` | Looper. | Phase 5 (`sim_modes.sh`, Starter Kit): Looper screen; `LED 37,1,0`, and leaving it now sends `LED 37,0,0` (fixed in `kick_leds.cpp`). Recording: phase 4 (4.6). | works | none |
| LINK EDIT (SHIFT+SAMPLE EDIT) | `EN_LINK_EDIT` (39) | Global → `MenuKnobEdit` | Q-Link assignment editor. | Phase 5 (`sim_modes.sh`, Starter Kit): Q-Link Edit opens (`In screen 39`); no LED (none defined). Assignments: phase 5b. | works | none |
| MIDI CTRL (SHIFT+PROG EDIT) | `EN_MIDI_CONTROL` (41) | Global → `MenuMidiControl` | MIDI control mapping; global keys disabled while open. | Phase 5 (`sim_modes.sh`, Starter Kit): MIDI Control; `LED 41,1,0`; MAIN leaves it. | works | none |
| HALF LVL (SHIFT+FULL LEVEL) | `EN_HALF_LEVEL` (49) | `PadModifiers` (replaces the global case) | Toggle: every pad hit plays at velocity 64; switches FULL LEVEL off. | Phase 5: `LED 48,0` then `LED 49,1`; a velocity-30 kick hit peaks at -13.7 dBFS, the same as a velocity-64 hit (-13.7) and 7 dB under velocity 127. The panel shows LED 49 on the FULL LEVEL key it is printed under. | works | none |
| NOTES (SHIFT+16 LEVELS) | `EN_NOTES` (51) | Global → `MenuPadPerform` | Pad Perform: chromatic (default), scales, chords, progressions on the pads. | Pad Notes (Chromatic, root C, octave -2); per-pad colours sent. Pads play chromatic notes on the selected track (see Pads in NOTES). No LED (none defined). | works | none |

## Loaded state: transport

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| PLAY | `EN_PLAY` (45) | Current screen (Main: `ae.setSequencePlay`) | Plays the current sequence from the current position; PLAY LED; counter runs. | Phase 3 (`sim_transport.sh`, Starter Kit): STOP at `3:04:049`, PLAY: `4:03:022` 0.8 s later (resumes); `LED 45,1,1`, STOP unlit on the panel; sequence peaks -3.0 dBFS (was +2.6, clipping). | works | none (engine fixes, see Phase 3) |
| STOP | `EN_STOP` (44) | Current screen → `setSequencePause`; `KickHome` (ends a take) | Stops (position kept); STOP LED; ends recording. | `LED 44,1,1`; BAR:BEAT:TICK holds; probe -120 dBFS from 0.1 s after; RECORD / OVERDUB LEDs off (`42,0`, `43,0`). | works | none |
| PLAY START | `EN_PLAYSTART` (46) | Current screen → `MTAudioTransportManager::play` (`startPlayback( Seq, false )`) | Plays from bar 1. | STOP at `2:03:083`, PLAY START: `1:03:068` 0.8 s later; first note `SimNote: track 'Drums' tick 0` (tick 0 used to be skipped; after STOP it used to resume). | works | none (engine fix) |
| RECORD | `EN_RECORD` (42) | `KickHome` (new; LOOPER keeps its own `record(false)`) → `setSequenceRecord` | Records pad hits into the sequence; arms the looper. | Arms while stopped (`LED 42,1`), PLAY START starts the take on the selected track; three hits at 1.72 / 3.55 / 5.74 s play back 8.000 / 7.990 / 8.000 s later; the take replaced the track's 65 old notes (`Record: ... 65 old notes erased, 3 notes`). | works | none (new code) |
| OVERDUB | `EN_OVERDUB` (43) | `KickHome` (new; LOOPER keeps `record(true)`) → `setSequenceRecord` | Overdubs into the sequence or loop. | `LED 43,1`; two hits added, none erased (`5 notes`), all 5 play on the next pass; with Timing Correct on the starts snap to 1/16 (529 -> 528, 1279 -> 1272). | works | none (new code) |

## Loaded state: other keys

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| TAP TEMPO | `EN_TAP_TEMPO` (18) | `KickHome` → `tapTempo()` (new) | Taps the tempo. | Phase 3: four taps 600 ms apart (`sim_ctl.sh repeat 600 4 "TAP TEMPO"`): `TapTempo: 4 taps, 600.5 ms apart -> 100 BPM`, `bpm=100`; from the next bar the loop repeats every 9.59-9.61 s (4 bars at 100 BPM). No LED flashing with the tempo. | works | none (new code) |
| NOTE REPEAT | `EN_NOTE_REPEAT` (24) | `PadModifiers` (new); the panel now also sends its release | While held, held pads retrigger at the sequence's time-correct division (TC, 1/16 by default) at the sequence tempo, on the grid while the sequencer plays. | Phase 5: `LED 24,1` while held, `24,0` on release; holding the clave 1 s gives 9-10 onsets, median 120-130 ms (expected 125 ms; the engine applies live notes per 10 ms block). Works on MAIN, GRID, TRACK MIX, PROG EDIT, NOTES. | works | none |
| MENU | `EN_MENU` (22) | Global (second switch) → `HomeOptions` | Menu of all modes. | Phase 5 (`sim_modes.sh`, Starter Kit): Menu grid; `LED 22,1,0`. Audit: jog +2, + and PUSH opened Next Sequence (`LED 22,0,0`, `LED 16,1,0`). | works | none |
| SHIFT | `EN_SHIFT` (31) | Dispatcher only | Modifier: secondary functions, SHIFT + pad. | Press / release logged; panel toggle lit; SHIFT functions arrive as their own ids (above). | works | none |
| FULL LEVEL | `EN_FULL_LEVEL` (48) | `PadModifiers` (replaces the global case) | Toggle: every pad hit plays at velocity 127; switches HALF LEVEL off. | Phase 5: `LED 49,0` then `LED 48,1` (and `48,0` when toggled off); a velocity-30 kick hit (-21.2 dBFS) peaks at -7.0 dBFS, the same as a velocity-127 hit. Off by default: project load now sends both LEDs' state instead of always lighting FULL LEVEL. | works | none |
| 16 LEVELS | `EN_16LEVELS` (50) | `PadModifiers` (new) | Toggle: the last pad hit is spread over the 16 pads, pad 1 softest to pad 16 loudest. | Phase 5: `LED 50,1`; `PAD cmd=10 data=1,14 x16,7,15,23,...,127` (the hat's colour, dim to bright); pads 1-16 then play the closed hat at -40.5, -33.5, ... -12.7 dBFS, each louder than the last. | works | none |
| ERASE | `EN_ERASE` (52) | `PadModifiers` (held + pad); Grid deletes the selected notes (now on the press only); AssignSample clears a pad; the panel now sends its release | Held with a pad: erases that pad's notes from the selected track; alone: erases the screen's selection. | Phase 5: ERASE + Pad 3 logs `ERASE pad 3 note 2: 20 notes erased from track 1` and the 2-bar playback of the Drums track loses the hats (13 → 12 onsets); ERASE + all pads: 0 onsets. GRID: a selected note is deleted. | works | none |
| LINK | `EN_LINK` (47) | Global → `KnobEdit::knobLongPress()` | Q-Link page select (long press). | Phase 5 (`sim_modes.sh`, Starter Kit): `In screen 47`, reaches `knobLongPress`; no visible change on Main (Q-Link pages: phase 5b). No crash. | partial | none (phase 5b) |
| + (PLUS) | `EN_PLUS` (21) | Focused control `knobRotated(+)`; StepSeq nudge; MENU next item | Increments the touched (focused) value. | Phase 5: touch LEVEL of track 1, + + → 38 → 40 (`is Navigation message`). Needed the simulator focus fix (below). In MENU it moves the selection. | works | none |
| - (MINUS) | `EN_MINUS` (27) | Same | Decrements. | Phase 5: touched LEVEL 40 → 39. | works | none |
| BACK | `EN_BACK` (29) | `KickHome::goBack` (new): the current screen's header back arrow | Goes back one screen. | Phase 5: from BROWSE and PROG EDIT back to MAIN (`BACK: header back of screen 1019 / 1008`, MAIN LED). MAIN and the Projects screen have no back arrow and stay. | works | none |
| COPY | `EN_COPY` (20) | Grid only (`pasteSelectedNotesDefault`) | Copies the selection (pastes it right after itself). | Phase 5: GRID, arrow tool, touch the first hat, COPY: a copy appears on the next step (screenshot). Elsewhere the app gives COPY no job; `KickCopy` / `KickErase` dialogs exist in `modes/` but nothing opens them. | works (GRID) | none |
| PUSH (jog click) | `EN_KNOB_ENTER` (26) | Focused control `knobClicked()`; Projects, MENU, BROWSE | Enter / select. | Projects: loads the highlighted project. MENU: jog 2, + 1, PUSH opens the 4th item (NEXT SEQ, `LED 16,1`). A touched value control opens its options. | works | none |

## Loaded state: jog wheel, encoders and slider

Driven with `sim_ctl.sh turn` / `sim_turn.sh` (one KNOBS packet per detent) and
`sim_ctl.sh slider`.

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| Jog wheel | KNOBS knob 0, cmd 0 | Focused control `knobRotated`; else the screen (Main rows, Grid note edit, Browser list, MENU) | Scrolls / edits. | Phase 5: MAIN Track tab: +1 / -1 selects the next / previous track (the pads recolour for its program); a touched LEVEL: +3 → 39 → 42; MENU: moves the selection. | works | none |
| P1-P4 on MAIN | KNOBS knob 1-4, cmd 1 | Main tab `knobCommand` → `linkRotated` | Track tab: the first row's I/P MIDI CH, LENGTH, LEVEL, TRANSPOSE (P5-P16 the next rows, via the Q-Link pages). | Phase 5: P3 +5 LEVEL 42 → 47, P4 +2 TRANSPOSE 0 → 2 (screenshots). TRACK MIX: P2 -10 level 100 → 90, P3 -3 pan 0 → -1.00. STEP SEQ: P1-P4 = the level of steps 1-4 of the page (P2 -4 sets step 2, `PAD cmd=10`). | works | none |
| P1 in NEXT SEQ | KNOBS knob 1 | `KickNextSequenceMode::knobCommand` | Sequence BPM. | Fixed (phase 5): null sequence guarded, no crash with an empty project. Starter Kit: BPM 120 → 122, the list now refreshes. | works | none |
| P1 in TRACK MIX | KNOBS knob 1 | `ChannelMixer::knobCommand` | Row 1 OUT; P2 level, P3 pan. | Fixed (phase 5): empty table and the Output / Midi tab casts guarded; the row maths now measures from the Kick screen in the simulator. Starter Kit: P2 level 100 → 50. | works | none |
| P1 in PAD MIX | KNOBS knob 1 | `PadMixer::handleCommand` → `PadMixerContent::knobCommand` | Pad A01 level. | Fixed (phase 5): empty table guarded, row off-by-one fixed. Starter Kit: A01 level 1.00 → 0.00 → 1.00, heard (-120 / -13.3 dBFS). | works | none |
| P1 in SAMPLER | KNOBS knob 1 | Sampler `knobCommand` | Max record length. | Max Length 00:20 → 00:21; slider LED `SLIDER cmd=1 data=35`. | works | none |
| P1-P4 in NOTES, FX, GRID, LOOPER, PREFS, LINK EDIT, MIDI CTRL, PROG EDIT, SAMPLE EDIT | KNOBS | Screen `knobCommand` | Screen-specific parameters. | PROG EDIT (drum, keygroup, clip) assigns nothing to the Q-Links (only the audio program grid reads them). It used to treat each Q-Link packet as a pad hit: P1 played note 1 at velocity 1 and relabelled the pad "A02"; fixed in phase 5 (`kick_ProgramEdit.cpp`). Others not re-checked in phase 5 (pads and encoders). | partial | code (no assignment) |
| Slider | SLIDER 0-100 | Focused control `sliderMoved` (`kick_ControlBase.h`) | The touch strip sets the touched (focused) control, as the jog and + / - do; the app shows that control's value, or on MAIN's Track tab the track meter, on the strip's LEDs. | Phase 5: touch LEVEL of track 1, slider 30 → LEVEL 100 → 38 (30 % of 0-127). Before, the simulator could not give a control focus (below). The panel's LED ladder now shows the app's `SLIDER cmd=1/2` values. | works | none |
| MASTER, REC pots | none | none | Analogue pots on the unit. | Turn in the UI; no packet (by design). | works | none |

## Open gaps (2026-10-28)

The Phase 1 audit's "gaps by class" (content, engine, hardware, sim, code) are closed except as
listed here. The original list is in commit `ed11a60a`.

**Content.** Nothing proprietary can come back, so:

- Banks E-H have no sounds: the Starter Kit's programs stop at note 63.
- The factory projects (Demo 01-05, Template 01-05) are still empty `<ROOT>` documents.
- Keygroup programs are silent, because `KeygroupProgram::noteOn` is a stub in this release.
  Starter Keys is a chromatic drum program instead.

**Missing from the release's code.** These are absent on the device too:

- AUTO: there is no automation engine. The simulator says so; the device build does nothing.
- FX: the XY view is not connected to any audio processing.
- PROG EDIT assigns nothing to P1-P4.
- LINK (Q-Link page select) shows no visible change on MAIN.
- COPY only has a job in GRID. The `KickCopy` / `KickErase` dialogs are never opened.
- Transport: no count-in, the TAP TEMPO LED does not flash, and RECORD captures notes only (no
  audio-track recording).

**Engine.**

- After a SAMPLE EDIT trim, a pad that already holds the file plays the old audio until the
  project is reloaded.
- The looper takes its tempo from the selected sequence. With no sequence the loop is 1 s long.
- A looper take starts at the key press, because the input threshold defaults to -100 dB.

**Inferred device behaviour.** These were built from the code, captions and screens, not
checked on a unit:

- ERASE + pad erases all of that pad's notes.
- LATCH latches the note repeat, not the pads.
- FULL / HALF LEVEL are off at project load.
- Only ERASE and NOTE REPEAT report key releases.
- PAD MIX P1-P4 address the row they sit on (the off-by-one fix).

**Fixed on 2026-10-28.** The P1-P16 knobs went to whichever control had keyboard focus, and those
controls ignore them. In PAD MIX a pad hit focuses its on-screen button, so P1 stopped changing
the level after any pad press. `KickHome::handleCommand` now sends link knobs to the screen
unless a modal popup is open. This is shared code, so it changes the device too.

**Hardware.** Wi-Fi, Ethernet, CV / Gate, System Resources, Firmware Update, and the SD / USB
locations in BROWSE show "Not available in the simulator" (DEVELOPMENT.md 4.5). The camera has no
software. Not reached: the firmware progress screen (needs a valid image) and the CV program
editor (needs a CV program).

**Simulator limits.**

- The app's full-screen popups open as separate desktop windows at native size, not inside the
  scaled bezel (DEVELOPMENT.md 4.1).
- The MASTER and REC pots are analogue on the unit and do nothing here.
- Only one `KT_KickEM` runs per machine.

**Not re-checked since the empty-project audit:**

- Pads in BROWSE (audition), in SAMPLER and in MIDI CTRL.
- P1-P4 in NOTES, GRID, LOOPER, PREFS, LINK EDIT, MIDI CTRL and SAMPLE EDIT (SAMPLE EDIT's P4
  trim is exercised by `sim_sampler_looper.sh`).

## Phase 5 (mode keys)

Date: 2026-10-27. `Kick/tools/sim_modes.sh` (see `docs/DEVELOPMENT.md` 4.8) walks every function
key and SHIFT function above on the Starter Kit, first turning P1-P4 in NEXT SEQ, TRACK MIX and
PAD MIX on the empty Demo 01. Last run: 50 checks, 0 failed, 1 known gap. Representative
screenshots and the result table are in `docs/sim_audit/phase5_modes/`.

- **Crashes fixed** (shared code, guards only): `KickNextSequenceMode::knobCommand` (no sequence),
  `ChannelMixer::knobCommand` and the seven mixer views' `knobCommand` (no rows; the Output and
  Midi tabs also cast to the wrong view class), `PadMixerContent::knobCommand` (no rows),
  and the same pattern in `ListEditMode::knobCommand`.
- **New handlers** (shared code): TC opens a Timing Correct window, ON/OFF switches Timing
  Correct and the TC LED, MASTER switches the selected sequence between its own and the master
  tempo. AUTO has nothing to switch (no automation engine) and says so in the simulator.
- **LOOPER LED** is switched off when the looper is left (`kick_leds.cpp`).
- **Q-Links in table screens** (TRACK MIX, PAD MIX, LIST EDIT) now reach the right row in the
  simulator: the row maths used desktop coordinates, which the scaled panel offsets
  (`SIM_BUILD` only). PAD MIX also addressed one row too high (fixed in shared code).
- **Content**: the Starter Kit has a second sequence (Sequence 02, 100 BPM, an off-grid "Drums
  Live" track and the loops) for NEXT SEQ, MASTER and TC.
- **Still different from the device**: FX (XY) is not connected to audio in this release; there
  is no automation (AUTO). (Program LEVEL and pad LEVEL were not heard at the time; phase 3 fixed
  that.)


## Phase 5 (pads and encoders)

Date: 2026-10-27. `Kick/tools/sim_pads.sh` (see `docs/DEVELOPMENT.md` 4.9) plays and measures
every control in the rows above on the Starter Kit: last run 8 audio measurements and 18 packet /
LED checks, all passed. Results and screenshots: `docs/sim_audit/phase5_pads/`.

- **New handlers** (shared code, `kick_ui/dialogs/kick_PadModifiers.cpp`): FULL / HALF LEVEL
  are exclusive toggles that set the velocity of every pad hit (127 / 64); 16 LEVELS; NOTE REPEAT
  (held) and LATCH; ERASE held + pad erases the pad's notes. They rewrite pad packets before the
  screen sees them, so MAIN, GRID, TRACK MIX, PROG EDIT and NOTES all get them. BACK runs the
  current screen's header back arrow (`KickHome::goBack`).
- **Fixed** (shared code): FULL LEVEL was lit at every project load but never applied; the LEDs now
  follow the state. PROG EDIT read Q-Link packets as pad hits. GRID's ERASE ran on the key's
  release too.
- **Simulator**: a touched control now takes focus with the screen locked (it did not, so the jog,
  + / -, PUSH and the slider never reached a control); ERASE and NOTE REPEAT send their release;
  `sim_ctl.sh down / up / hold` hold pads and keys with a velocity; a SHIFT function's LED (HALF
  LEVEL, LATCH, ...) shows on the key it is printed under; the slider's LED ladder shows the app's
  value; step and 16 LEVELS pad colours are dimmed by velocity.
- **Still different**: banks E-H have no sounds in the Starter Kit; PROG EDIT gives the Q-Links
  nothing to edit; key releases are assumed for ERASE and NOTE REPEAT only (AppAssignSample already
  expects ERASE's release; the other keys stay press-only because several screens act on any
  button packet).

## Phase 3 (transport, tempo, metronome, recording)

Date: 2026-10-27. `Kick/tools/sim_transport.sh` (see `docs/DEVELOPMENT.md` 4.7) checks the
transport rows above, TAP TEMPO, BAR:BEAT:TICK, gain staging, RECORD / OVERDUB and the metronome
on the Starter Kit and measures them on the audio probe: last run all 21 checks passed. The NEXT SEQ crash fix was checked with four `sim_modes.sh` runs (NEXT SEQ pad 2 ok in all, no crash report).

- **Metronome** (top-bar Metronome button, ENABLE On): clicks 0.59-0.60 s apart at 100 BPM, the
  2 kHz accent on beat 1 of every bar including the first, 1.2 kHz on the others; ENABLE Off:
  silent. MODE and LEVEL are applied now.
- **Gain staging** (shared engine code, changes the device): a pad hit is now centred and at the
  level its controls say (Pad 1 -13.3 dBFS: velocity, track level, program level, two pan laws);
  TRACK MIX program LEVEL and PAD MIX pad LEVEL / PAN are heard (phase 5's "MIX levels" row now
  passes: level 100 -> 50 takes Pad 1 from -13.3 to -19.3 dBFS). The playing sequence no longer
  clips. Notes were not triggered twice.
- **Crash fixed**: the intermittent SIGSEGV in `MTTrack::onTick` when NEXT SEQ switched sequences
  at the loop end (the clock thread stopped the old tracks while the tick thread played them);
  sequence changes now run on the tick thread and the tracks lock (DEVELOPMENT.md 5.1).
- **Input**: loading a project does not monitor the input (measured); `sim_launch.sh` uses a
  silent test input unless `KICK_SIM_AUDIO_INPUT` is set (`device` for the real one).
- **Known issue, fixed 2026-10-28**: PAD MIX P1 changed the pad gain once, but the turn that
  restores it (P1 +50 after -50) never reached the engine, so `sim_modes.sh` failed "PAD MUTE pad
  1". The cause was knob routing, not the engine: see "Open gaps", fixed on 2026-10-28.
- **Still different**: no count-in; no TAP TEMPO LED flashing; RECORD records only notes that
  reach the selected track through the pads / MIDI (no audio-track recording).
