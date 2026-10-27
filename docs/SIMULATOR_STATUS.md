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
| MAIN | `EN_MAINMODE` (8) | Global → `HomeListView` | Main screen (Sequence / Track / Program tabs). | Main screen; `LED 8,1,0`. Tabs empty. | works | content (empty lists) |
| BROWSE | `EN_BROWSE` (10) | Global → `MenuFileBrowser` | File browser: Internal, Factory Sample Library, Demos, SD / USB. | File Browser opened; `LED 10,1,0`. Jog+2 then PUSH raised a popup window "WARNING! Selected file cannot be loaded". | partial | content / hardware (SD/USB) |
| TRACK MIX | `EN_TRACK_MIX` (12) | Global → `MenuChannelMixer` | Track mixer strips; Q-Links edit the strip. | Track Mixer with master strip only; `LED 12,1,0`. P1 here **crashed** (see encoders). | partial | content |
| TRACK MUTE | `EN_TRACK_MUTE` (14) | Global → `MenuTrackMute` | Track mute grid on the pads. | Track Mute screen (bank grid A-H, T01-T16); `LED 14,1,0`; pad colour state + pad colour mode sent. | partial | content |
| NEXT SEQ | `EN_NEXT_SEQ` (16) | Global → `MenuNextSequence` | Sequence grid; pads queue sequences. | Next Sequence screen (A01-A16, empty list); `LED 16,1,0`; pad colour mode + all-pads packets. P1 here **crashed**. | partial | content |
| STEP SEQ | `EN_STEP_SEQ` (32) | Global → `HomeSequencerView` | Step sequencer on the pads. | "Create any Track Before using StepSequencer"; `LED 32,1,0`, bank A LED, step pad colours (`PAD cmd=10`). | partial | content |
| TC | `EN_TC` (34) | none | Timing correct (quantise) settings. | Packet reached the global switch (`In screen 34`); nothing happened; no LED. | missing | code |
| SAMPLER | `EN_SAMPLER` (36) | Global → `MenuSampler` | Sampler: record input to a sample. | Sampler screen (input meters at -100 dB, Max Length 00:20); `LED 36,1,0`. RECORD did nothing visible. | partial | content / engine |
| SAMPLE EDIT | `EN_SAMPLE_EDIT` (38) | Global → `MenuSampleEdit` | Sample editor. | Sample Editor with "Samples list: None"; `LED 38,1,0`. | partial | content |
| PROG EDIT | `EN_PROG_EDIT` (40) | Global → `MenuProgramEdit` | Program editor for the selected program. | "No Programs Available"; `LED 40,1,0`. | partial | content |

## Loaded state: SHIFT functions (captions)

| Control | Packet id | Handler(s) | Device behaviour | Observed in simulator | Status | Gap class |
|---|---|---|---|---|---|---|
| GRID (SHIFT+MAIN) | `EN_GRID` (9) | Global (second switch) → `HomeGridView` | Piano-roll grid of the track. | Grid View (A1-A16 rows, bars 1-1.4); `LED 9,1,0`. COPY / ERASE / jog / P3 did nothing (no notes). | partial | content |
| SAVE (SHIFT+BROWSE) | `EN_SAVE` (11) | Global → `ae.saveProject()` → `KickXML::save` | Saves the project; writes only if the XML changed. | Packet handled (`In screen 11`). No screen or LED change. `Demo 01.kickPrj` mtime unchanged (nothing to save). | partial | content |
| PAD MIX (SHIFT+TRACK MIX) | `EN_PAD_MIX` (13) | Global → `MenuPadMixer` | Per-pad mixer. | "Pad Mixer Is Not Applicable For Unknown Program"; `LED 13,1,0`. P1 here **crashed**. | partial | content |
| PAD MUTE (SHIFT+TRACK MUTE) | `EN_PAD_MUTE` (15) | Global → `MenuPadMute` | Pad mute grid. | "PAD MUTE IS NOT AVAILABLE FOR SELECTED TRA..."; `LED 15,1,0`; pad colour packets. | partial | content |
| FX (SHIFT+NEXT SEQ) | `EN_FX` (17) | Global → `HomeXYView` | XY effects; Q-Links: preset, attack, release, wet/dry. | XY Effects screen (Output 1, Latch, Setup, Off); `LED 17,1,0`. P1 +2 and slider 50 caused no visible change. | partial | content |
| MASTER (SHIFT+TAP TEMPO) | `EN_MASTER` (19) | none | Master (tempo / level) settings. | `In screen 19`, nothing. | missing | code |
| LATCH (SHIFT+NOTE REPEAT) | `EN_LATCH` (25) | none | Latches note repeat. | `In screen 25`, nothing. | missing | code |
| PREFS (SHIFT+MENU) | `EN_PREF` (23) | Global (second switch) → `HomePreferencesView` | Preferences (Info, Project Defaults, Hardware, MIDI, CV, Ethernet, WiFi). | Preferences / Info tab (serial 000000000, firmware 0.0.0); `LED 23,1,0`. Jog / PUSH / P1 / slider did nothing here. Tabs touched with `sim_ctl.sh click`: WiFi, Ethernet, CV each logged `SimUnavailable: <name> shown` / `hidden` and showed "Not available in the simulator"; no hang. Hardware opened normally (`SLIDER cmd=1 data=6` sent). | partial | hardware (handled by phase 6) |
| AUTO (SHIFT+STEP SEQ) | `EN_AUTOMATION` (33) | none | Automation mode. | `In screen 33`, nothing. | missing | code |
| ON/OFF (SHIFT+TC) | `EN_ON_OFF` (35) | none | Timing correct on/off. | `In screen 35`, nothing. | missing | code |
| LOOP (SHIFT+SAMPLER) | `EN_LOOPER` (37) | Global → `MenuLooper` | Looper. | Looper screen; `LED 37,1,0`. RECORD lit the looper's record button and drew the loop region; PLAY changed it to STOP; input silent. On leaving, LED 37 is never switched off (`kick_leds.cpp` close case lacks `id =`). | partial | content / engine; code (LED) |
| LINK EDIT (SHIFT+SAMPLE EDIT) | `EN_LINK_EDIT` (39) | Global → `MenuKnobEdit` | Q-Link assignment editor. | Q-Link Edit (16 knobs OFF, Custom / Screen); no LED (none defined). | partial | content |
| MIDI CTRL (SHIFT+PROG EDIT) | `EN_MIDI_CONTROL` (41) | Global → `MenuMidiControl` | MIDI control mapping; global keys disabled while open. | MIDI Control (Hardware tab, Step Seq pad parameters); `LED 41,1,0`. MAIN still leaves it (logged without `In screen`). | works | none |
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
| MENU | `EN_MENU` (22) | Global (second switch) → `HomeOptions` | Menu of all modes. | Menu grid (Browser ... MIDI Control); `LED 22,1,0`. Jog +2 then + then PUSH opened Next Sequence (`LED 22,0,0`, `LED 16,1,0`). The highlight did not visibly move. | works | none |
| SHIFT | `EN_SHIFT` (31) | Dispatcher only | Modifier: secondary functions, SHIFT + pad. | Press / release logged; panel toggle lit; SHIFT functions arrive as their own ids (above). | works | none |
| FULL LEVEL | `EN_FULL_LEVEL` (48) | Global → `setHalfLevel(false)` + `LED 48,1,0` | Pads play at full velocity. | `LED 48,1,0`; key lit. | works | none |
| 16 LEVELS | `EN_16LEVELS` (50) | none | Spreads one sound over 16 pads by level. | `In screen 50`, nothing. | missing | code |
| ERASE | `EN_ERASE` (52) | Focused control; Grid deletes notes; AssignSample clears a pad | Erases the selection. | On MAIN and in GRID: handled, nothing (no notes). | missing | content |
| LINK | `EN_LINK` (47) | Global → `KnobEdit::knobLongPress()` | Q-Link link / long-press. | `In screen 47`, nothing visible. | missing | content |
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
| P1 in NEXT SEQ | KNOBS knob 1 | `KickNextSequenceMode::knobCommand` | Sequence BPM. | **Crash**: SIGSEGV in `KickNextSequenceMode::knobCommand` (null `seqData`, no sequence). Reproduced twice, including on the merged build. P2-P4 dereference the same pointer. | crash | content |
| P1 in TRACK MIX | KNOBS knob 1 | `ChannelMixer::knobCommand` | Strip level. | **Crash**: SIGSEGV in `ChannelMixer::knobCommand` (no strip / program). | crash | content |
| P1 in PAD MIX | KNOBS knob 1 | `PadMixer::handleCommand` | Pad level. | **Crash**: SIGSEGV in `PadMixer::handleCommand` ("Unknown Program"). | crash | content |
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
