# Simulator status: every panel control

This file records, control by control, what the KT_KickEM simulator does today compared with
the device. It is the Phase 1 audit of `.claude/commands/goal.md`. Date: 2026-10-27.

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
| Pads 1-16 on MAIN | PAD `EN_PAD1`..`EN_PAD16` (0-15) | `KickHomeListView` / `kick_AppMainMode.cpp` → `ae.addLiveMidiMsg` note on/off on the selected track | Plays the pad's sound and lights it in the program's pad colour. | Press and release each logged `In Recent Prj register handle command`. No pad LED, no audio (probe silent), screen unchanged. Not observed: `addLiveMidiMsg` returns early without a selected track. | missing | content |
| Pad in BROWSE | PAD 0 | `fileBrowser::handleCommand` | Auditions the selected file on the pad and highlights the pad. | Pad highlight sent: `PAD cmd=1 data=0,1,3`. Nothing to audition. | partial | content |
| Pads in NOTES (Pad Perform) | PAD 0 | `PadPerform::handleCommand` | Plays a scale or chord note per pad. | Entering NOTES sent per-pad colours (`PAD cmd=1 data=n,1,colour` for all 16, root notes highlighted). A pad press gave no sound. | partial | content |
| Pads in TRACK MUTE / PAD MUTE | PAD 0 | `KickPadButtonGroupComponent` (`kick_padButtonGroup.cpp`) → `triggerClick` | Toggles mute (or solo with SHIFT) of the track or pad. | Packet logged. No change; the screens say "PAD MUTE IS NOT AVAILABLE FOR SELECTED TRA..." and the track grid is empty. | missing | content |
| Pads in STEP SEQ | PAD 0 | `StepSeq::handleCommand` | Toggles the step. | Packet logged. Screen: "Create any Track Before using StepSequencer". | missing | content |
| Pads in SAMPLER | PAD 0 | `Sampler::handleCommand` | Starts recording or marks a slice. | Packet logged. No visible change. Input is silent unless `KICK_SIM_AUDIO_INPUT` is set (phase 4). | missing | content / engine |
| Pads in PROG EDIT, MIDI CTRL | PAD 0 | `ProgramEdit::handleCommand`, `MidiControl` | Selects the pad or plays it; MIDI-learns the control. | Packet logged; "No Programs Available"; no change. | missing | content |
| SHIFT + pad on MAIN | PAD 0 with SHIFT on | Main | Modifier behaviour per screen. | Same as a plain pad. | missing | content |

## Loaded state: banks

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| A, B, C, D | `EN_BANK_A`..`EN_BANK_D` (0-3) | Global switch → `ae.setBank`; `kick_leds.cpp` `setBankMessage` + `refreshPadLeds`; StepSeq / Pad Mute / Track Mute page their own grids | Switches pad bank (pad note offset); bank LED; pads recolour for the program on the new bank. | `In screen N ... register handle command`, then `LED cmd=0 data=N,1`; the bank key lights. In TRACK MUTE, bank B also re-sent the pad colour state (`PAD cmd=6`). No program colours: nothing is assigned. | partial | content |
| E, F, G, H (SHIFT + A-D) | `EN_BANK_E`..`EN_BANK_H` (4-7) | Same | Banks E-H. | SHIFT 31, then id 4-7 `In screen 4..7`, then `LED cmd=0 data=4..7,1`. After the sim fix, the A-D key under the caption lights. | partial | content |
| Bank LEDs (sim fix) | n/a | `Simulator::handleLedMessage` | One bank lit at a time. | Before the fix, LEDs piled up (A, then A+B, then A+B+C...), because the app only ever sends "bank N on". Fixed in the panel; verified: C, then E lights A, H lights D, then A. | works (fixed) | sim |

## Loaded state: function keys (row 1 and row 2)

All mode keys go through the global switch in `KickHome::handleCommand` (`sigScreenClose(<mode>)`).
On close the previous mode's LED is turned off (`LED <id>,0,0`); on load the new mode's LED is
turned on.

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| MAIN | `EN_MAINMODE` (8) | Global → `HomeListView` | Main screen (Sequence / Track / Program tabs). | Phase 5 (`sim_modes.sh`, Starter Kit): Main screen with the kit's tracks / programs; `LED 8,1,0`, previous mode LED off. | works | none |
| BROWSE | `EN_BROWSE` (10) | Global → `MenuFileBrowser` | File browser: Internal, Factory Sample Library, Demos, SD / USB. | Phase 5 (`sim_modes.sh`, Starter Kit): File Browser; `LED 10,1,0`. SD / USB rows are hardware (see 4.5). | works | hardware (SD/USB) |
| TRACK MIX | `EN_TRACK_MIX` (12) | Global → `MenuChannelMixer` | Track mixer strips; Q-Links edit the strip. | Phase 5 (`sim_modes.sh`, Starter Kit): Track Mixer with the three program strips; `LED 12,1,0`. P1 = row 1 OUT, P2 = LEVEL (100 → 50), P3 = PAN; no crash, also with an empty project (fixed). The program LEVEL is not heard: nothing in the engine reads a program's level. | partial | engine (program level unused) |
| TRACK MUTE | `EN_TRACK_MUTE` (14) | Global → `MenuTrackMute` | Track mute grid on the pads. | Phase 5 (`sim_modes.sh`, Starter Kit): T01 Drums, T02 Keys, T03 Loops; `LED 14,1,0`. Pads mute tracks while playing: drums alone peak -3 dBFS, all three muted -120 dBFS, unmuted +2 dBFS (probe). | works | none |
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
| PAD MIX (SHIFT+TRACK MIX) | `EN_PAD_MIX` (13) | Global → `MenuPadMixer` | Per-pad mixer. | Phase 5 (`sim_modes.sh`, Starter Kit): Pad Mixer of Starter Drums (01 Kick ...); `LED 13,1,0`. P1 = pad A01 LEVEL (1.00 → 0.00; was off by one row and addressed nothing, fixed), P2 PAN, P3 mute / solo; no crash with an empty project (fixed). The pad gain is not heard: the drum voice ignores a pad's gain. | partial | engine (pad gain unused) |
| PAD MUTE (SHIFT+TRACK MUTE) | `EN_PAD_MUTE` (15) | Global → `MenuPadMute` | Pad mute grid. | Phase 5 (`sim_modes.sh`, Starter Kit): Pads A01-A16 named after the kit; `LED 15,1,0`. Pad 1 mutes the kick: Pad 1 on MAIN peaks -120 dBFS muted, -9.2 dBFS unmuted (probe). | works | none |
| FX (SHIFT+NEXT SEQ) | `EN_FX` (17) | Global → `HomeXYView` | XY effects; Q-Links: preset, attack, release, wet/dry. | Phase 5 (`sim_modes.sh`, Starter Kit): XY Effects; `LED 17,1,0`; P1-P4 edit the Setup panel's preset / attack / release / wet-dry. No sound change: `XyFxView` only updates its labels, nothing connects it to the engine in this release (no XY effect code). | partial | code (XY FX not connected to audio) |
| MASTER (SHIFT+TAP TEMPO) | `EN_MASTER` (19) | Global → `KickHome::toggleMasterTempo` (new) | Switches the selected sequence between its own tempo and the master tempo. | Phase 5 (`sim_modes.sh`, Starter Kit): Sequence 02 (own tempo 100 BPM): `Tempo source: Sequence 02 Master Tempo: 120 BPM`, again: `Sequence Tempo: 100 BPM`; the Main screen's Global toggle follows; notification bubble; `LED 19,1` / `19,0` (the panel has no LED under the caption). | works | none (new code) |
| LATCH (SHIFT+NOTE REPEAT) | `EN_LATCH` (25) | none | Latches note repeat. | `In screen 25`, nothing. | missing | code |
| PREFS (SHIFT+MENU) | `EN_PREF` (23) | Global (second switch) → `HomePreferencesView` | Preferences (Info, Project Defaults, Hardware, MIDI, CV, Ethernet, WiFi). | Phase 5 (`sim_modes.sh`, Starter Kit): Preferences / Info; `LED 23,1,0`. WiFi / Ethernet / CV show "Not available in the simulator" (4.5). | works | hardware (handled by phase 6) |
| AUTO (SHIFT+STEP SEQ) | `EN_AUTOMATION` (33) | Global → notice (`SIM_BUILD`) | Automation read / write. | Phase 5 (`sim_modes.sh`, Starter Kit): Notification "Automation is not available in this firmware", `SimUnavailable: Automation`. There is no automation engine in the release (the mode header's Automation button has an empty `onClick`; nothing records or plays parameter changes), so there is nothing to switch. | missing | code (no automation engine) |
| ON/OFF (SHIFT+TC) | `EN_ON_OFF` (35) | Global → `TimingCorrect::setEnabled` (new) | Timing Correct on / off. | Phase 5 (`sim_modes.sh`, Starter Kit): Toggles Timing Correct: `LED 34,1` (TC lit) / `LED 34,0`, notification "Timing Correct ON 1/16". With it on, TC's Apply moves notes and `TimingCorrect::correct()` snaps a recorded start (for phase 3 recording). | works | none (new code) |
| LOOP (SHIFT+SAMPLER) | `EN_LOOPER` (37) | Global → `MenuLooper` | Looper. | Phase 5 (`sim_modes.sh`, Starter Kit): Looper screen; `LED 37,1,0`, and leaving it now sends `LED 37,0,0` (fixed in `kick_leds.cpp`). Recording: phase 4 (4.6). | works | none |
| LINK EDIT (SHIFT+SAMPLE EDIT) | `EN_LINK_EDIT` (39) | Global → `MenuKnobEdit` | Q-Link assignment editor. | Phase 5 (`sim_modes.sh`, Starter Kit): Q-Link Edit opens (`In screen 39`); no LED (none defined). Assignments: phase 5b. | works | none |
| MIDI CTRL (SHIFT+PROG EDIT) | `EN_MIDI_CONTROL` (41) | Global → `MenuMidiControl` | MIDI control mapping; global keys disabled while open. | Phase 5 (`sim_modes.sh`, Starter Kit): MIDI Control; `LED 41,1,0`; MAIN leaves it. | works | none |
| HALF LVL (SHIFT+FULL LEVEL) | `EN_HALF_LEVEL` (49) | Global → `setHalfLevel(true)` + `LED 49,1,0` | Pads play at half velocity (only StepSeq reads it). | `LED 49,1,0` sent. The panel has no LED 49 (HALF LVL is a caption), and the app never switches 48 off, so nothing changes on the panel. | partial | sim / code |
| NOTES (SHIFT+16 LEVELS) | `EN_NOTES` (51) | Global → `MenuPadPerform` | Pad Perform: scales, chords, progressions on the pads. | Pad Notes (Major, root C, octave -2); per-pad colours sent; no LED. | partial | content |

## Loaded state: transport

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| PLAY | `EN_PLAY` (45) | Current screen (Main: `ae.setSequencePlay`) | Plays the current sequence; PLAY LED; counter runs. | `register handle command`; no PLAY LED; BAR:BEAT:TICK stayed `0:01:000`; silence. Same in NEXT SEQ. | missing | content |
| STOP | `EN_STOP` (44) | Current screen → `setSequencePause` | Stops; STOP LED. | `LED 44,1,1` (STOP lit). | works | none |
| PLAY START | `EN_PLAYSTART` (46) | Current screen | Plays from bar 1. | Handled; nothing. | missing | content |
| RECORD | `EN_RECORD` (42) | Looper only (`record(false)`) | Records pad hits into the sequence; arms the looper. | On MAIN: nothing; no main-screen handler exists. In LOOPER: the record button lit. | partial | code (main) / content |
| OVERDUB | `EN_OVERDUB` (43) | Looper only (`record(true)`) | Overdubs into the sequence or loop. | On MAIN: nothing. In LOOPER: handled. | partial | code / content |

## Loaded state: other keys

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| TAP TEMPO | `EN_TAP_TEMPO` (18) | none | Taps the tempo. | Three taps: `In screen 18` each; nothing. | missing | code |
| NOTE REPEAT | `EN_NOTE_REPEAT` (24) | none | Repeats held pads at the grid rate. | `In screen 24`, nothing. | missing | code |
| MENU | `EN_MENU` (22) | Global (second switch) → `HomeOptions` | Menu of all modes. | Phase 5 (`sim_modes.sh`, Starter Kit): Menu grid; `LED 22,1,0`. Audit: jog +2, + and PUSH opened Next Sequence (`LED 22,0,0`, `LED 16,1,0`). | works | none |
| SHIFT | `EN_SHIFT` (31) | Dispatcher only | Modifier: secondary functions, SHIFT + pad. | Press / release logged; panel toggle lit; SHIFT functions arrive as their own ids (above). | works | none |
| FULL LEVEL | `EN_FULL_LEVEL` (48) | Global → `setHalfLevel(false)` + `LED 48,1,0` | Pads play at full velocity. | `LED 48,1,0`; key lit. | works | none |
| 16 LEVELS | `EN_16LEVELS` (50) | none | Spreads one sound over 16 pads by level. | `In screen 50`, nothing. | missing | code |
| ERASE | `EN_ERASE` (52) | Focused control; Grid deletes notes; AssignSample clears a pad | Erases the selection. | On MAIN and in GRID: handled, nothing (no notes). | missing | content |
| LINK | `EN_LINK` (47) | Global → `KnobEdit::knobLongPress()` | Q-Link page select (long press). | Phase 5 (`sim_modes.sh`, Starter Kit): `In screen 47`, reaches `knobLongPress`; no visible change on Main (Q-Link pages: phase 5b). No crash. | partial | none (phase 5b) |
| + (PLUS) | `EN_PLUS` (21) | Focused control `knobRotated(+)`; StepSeq nudge | Increments the focused value. | On MAIN / in STEP SEQ: handled, no change (no focus, no steps). In MENU it seemed to advance the selection (see MENU). | partial | content |
| - (MINUS) | `EN_MINUS` (27) | Same | Decrements. | On MAIN: handled, no change. | missing | content |
| BACK | `EN_BACK` (29) | Navigation key → focused control; no screen uses it | Goes back one screen. | `In screen 29`, nothing, also in MENU. | missing | code |
| COPY | `EN_COPY` (20) | Grid only (`pasteSelectedNotesDefault`) | Copies the selection. | Handled, nothing (no notes). | missing | content |
| PUSH (jog click) | `EN_KNOB_ENTER` (26) | Focused control `knobClicked()`; Browser / menus | Enter / select. | MENU: opens the item. BROWSE: tried to load and raised the "Selected file cannot be loaded" popup. MAIN / PREFS: nothing. | partial | content |

## Loaded state: jog wheel, encoders and slider

Driven with `sim_ctl.sh turn` / `sim_turn.sh` (one KNOBS packet per detent) and
`sim_ctl.sh slider`.

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| Jog wheel | KNOBS knob 0, cmd 0 | Focused control `knobRotated`; else the screen (Main rows, Grid note edit, Browser list) | Scrolls / edits. | MAIN, GRID, PREFS, SAMPLE EDIT, TRACK MIX, BROWSE: `register handle command`, no visible change. | missing | content |
| P1-P4 on MAIN | KNOBS knob 1-4, cmd 1 | Main tab `knobCommand` → `linkRotated` | Edits the row's parameters. | +3 / -2 on each: handled, no change (no rows). | missing | content |
| P1 in NEXT SEQ | KNOBS knob 1 | `KickNextSequenceMode::knobCommand` | Sequence BPM. | Fixed (phase 5): null sequence guarded, no crash with an empty project. Starter Kit: BPM 120 → 122, the list now refreshes. | works | none |
| P1 in TRACK MIX | KNOBS knob 1 | `ChannelMixer::knobCommand` | Row 1 OUT; P2 level, P3 pan. | Fixed (phase 5): empty table and the Output / Midi tab casts guarded; the row maths now measures from the Kick screen in the simulator. Starter Kit: P2 level 100 → 50. | works | none |
| P1 in PAD MIX | KNOBS knob 1 | `PadMixer::handleCommand` → `PadMixerContent::knobCommand` | Pad A01 level. | Fixed (phase 5): empty table guarded, row off-by-one fixed. Starter Kit: A01 level 1.00 → 0.00. | works | none |
| P1 in SAMPLER | KNOBS knob 1 | Sampler `knobCommand` | Max record length. | Max Length 00:20 → 00:21; slider LED `SLIDER cmd=1 data=35`. | works | none |
| P1-P4 in NOTES, FX, GRID, LOOPER, PREFS, LINK EDIT, MIDI CTRL, PROG EDIT, SAMPLE EDIT | KNOBS | Screen `knobCommand` | Screen-specific parameters. | Handled; no crash; no visible change. | missing | content |
| Slider | SLIDER 0-100 | Focused control `sliderMoved` only (`kick_ControlBase.h`) | Sets the value the screen assigns (focused control). | On MAIN, FX, TRACK MIX, PAD MIX, PREFS: `register handle command`; nothing. No control had focus. | missing | code / content |
| MASTER, REC pots | none | none | Analogue pots on the unit. | Turn in the UI; no packet (by design). | works | none |

## Gaps by class

### content (phase 2, starter project)

- Pads make no sound in any mode; the audio probe was silent for the whole audit.
- PLAY / PLAY START do not start the counter or light PLAY.
- Pad colours never show a program.
- Every mode screen is empty: Track Mix, Pad Mix, Pad / Track Mute, Next Seq, Step Seq,
  Prog Edit, Sample Edit, Grid.
- ERASE, COPY and the jog / Q-Links have nothing to act on.

A starter project with a selected track and program on bank A unblocks most rows.

**Three crashes** happen with an empty project: P1 in NEXT SEQ, P1 in TRACK MIX, and P1 in
PAD MIX. All are null dereferences when there is no sequence, strip or program. Starter content
hides them. Phase 5 should still guard `KickNextSequenceMode::knobCommand`,
`ChannelMixer::knobCommand` and `PadMixer::handleCommand`, because a user project can be empty
too.

### engine (phases 3 and 4)

Not reachable yet. Nothing plays, so the reconstructed `mt::ADSR`, `Filter`, `Looper` and
`Recorder` were not exercised:

- Looper RECORD / PLAY changed the screen, but the input was silent.
- Sampler RECORD showed no change.

Phase 4 needs `KICK_SIM_AUDIO_INPUT` plus content. Phase 3 should re-test transport once a
sequence exists.

### hardware (phase 6)

- BROWSE lists SD / USB locations, which read `/dev/mmcblk1`, `/dev/sda` and `/mnt/...`.
- PREFS has Ethernet, WiFi, CV and Firmware Update.

With Phase 6 merged, the WiFi, Ethernet and CV tabs show "Not available in the simulator"
(verified above), and nothing hung. The SD / USB area of BROWSE and Firmware Update were not
re-checked in this audit.

### sim (fixed in this phase)

- **Bank LEDs piled up.** The app sends only "bank N on", so the panel lit every bank ever
  selected. Now one bank is lit at a time, and E-H light the A-D key they are printed under.
  This is in `Simulator::handleLedMessage`.
- **Encoders and the slider could not be scripted** (no accessibility names, not buttons).
  Fix: `sim_ctl.sh turn P1..P4|JOG STEPS` and `sim_ctl.sh slider V`, with `sim_turn.sh` as
  shorthand. The knobs are named `P1`-`P4` / `JOG` and the slider `SLIDER` so
  `kick_SimControl.cpp` can find them.

Still open:

- HALF LEVEL's LED (id 49) has no key on the panel.
- `sim_launch.sh` can lose its lock. Between `mkdir` of the lock and writing the app pid,
  another waiting launcher may treat the empty lock as stale and remove it. Seen once:
  `sim_launch.sh: line 60: /tmp/kt_kickem.lock/pid: No such file or directory` while the app
  was already running.

### code (phase 5, new UI code)

No handler exists for:

- TAP TEMPO, NOTE REPEAT, LATCH, 16 LEVELS, TC, ON/OFF, AUTO, MASTER and BACK;
- RECORD and OVERDUB outside the Looper.

Other code gaps:

- The slider only reaches a focused control.
- The LOOPER LED is never switched off.
- FULL LEVEL and HALF LEVEL never switch each other's LED off.

These need new handlers, not content.

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
- **Still different from the device**: program LEVEL (TRACK MIX) and pad LEVEL (PAD MIX) change on
  screen but not in the sound (engine, see 4.8); FX (XY) is not connected to audio in this
  release; there is no automation (AUTO).
