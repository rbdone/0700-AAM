---
description: Run the device's real control-surface path on the desktop: a real juce_serialport, and a simulator panel that speaks framed PktMgr bytes over a pseudo-terminal to the plain (non-SIM_BUILD) Kick app. Optional argument: a phase number to work on only that phase.
---

# Goal: the device's serial path runs against the simulated panel

Read `docs/DEVELOPMENT.md` first (sections 3, 4.1, 4.2, 5.2) and `docs/SIMULATOR_STATUS.md`. This
goal follows `/goal` (the simulator plays like the device) and is the first step towards
`/goal-vm` (the Linux build in an ARM virtual machine).

## Where things stand

- On the unit, `KT_Kick` talks to the control-surface microcontroller over `/dev/ttymxc0` at
  230400 baud, 8N1, no flow control. `Kick/modules/input_events/input_events.cpp` (`HwSerialData`)
  opens the port through `juce_serialport`, frames every `CPktMgr::HWMsg_t` as bytes with
  `ComProtocol/PktMgr` (start bytes, header, payload, CRC), runs a reader and a send queue, and
  performs the handshake (`EN_SYS_COMM_INIT`, then `CONNECTED`, then `LIVE`). The packet types
  include BUTTON, LED, PAD, KNOBS, SLIDER, SYSTEM, and also MIDI and CV, so DIN MIDI and CV/gate
  appear to go through the microcontroller as well. No audio crosses this link.
- That code is compiled only on Linux (`#if JUCE_LINUX`, `#ifndef __APPLE__`) and only connects
  when `/dev/ttymxc0` exists.
- `Kick/modules/3rdParty/juce_serialport` is an API-compatible **stub** (DEVELOPMENT.md 5.2): the
  original had no license and was removed from the release. A device build therefore cannot talk
  to the control surface at all today.
- The simulator (`KT_KickEM`, `SIM_BUILD`) bypasses all of it: `kick_SimulatorInterfaceImpl`
  hands whole `HWMsg_t` structures to the app in memory, so byte framing, CRC, partial reads,
  the reader thread, the send queue and `reconnect()` never run anywhere.
- The microcontroller's own firmware is not in the repository. Only its side of the protocol can
  be reproduced, from `PktMgr` and from what `input_events.cpp` expects.

## Definition of done

1. **A real serial port.** `juce_serialport` is a working POSIX implementation (termios) of the
   API `input_events.cpp` uses: `SerialPort`, `SerialPortConfig`, `SerialPortInputStream` (with
   change notifications), `SerialPortOutputStream`, `getSerialPortPaths()`. It is written from
   scratch under the repository's license, builds on Linux and macOS, and keeps a stub only where
   there is no POSIX serial API (Windows). The compiler warning added in 5.2 goes away.
2. **A configurable port.** `KICK_SERIAL_DEVICE=<path>` overrides `/dev/ttymxc0`. With the
   variable unset, the device behaves exactly as before: same path, same baud rate, same
   handshake. The serial connect path also compiles and runs in the macOS desktop `Kick` build.
3. **The panel speaks bytes.** `KT_KickEM --surface` (or a separate small target, if that is
   cleaner) shows only the front panel, opens a pseudo-terminal (`posix_openpt`), prints and
   logs its slave path (`SimSerial: listening on /dev/ttys012`), and plays the microcontroller's
   part of the protocol: it encodes panel events with `CPktMgr`, decodes the app's packets into
   LEDs and pad colours, and answers the handshake. In-memory mode stays the default.
4. **The plain app runs against it.** `Kick` built without `SIM_BUILD`, started with
   `KICK_SERIAL_DEVICE=<that path>`, completes the handshake. Every panel control reaches it, and
   the LED and pad-colour feedback comes back over the wire.
5. **Robustness is tested.** A script injects, through the surface, a bad CRC, a packet split
   across two reads, two packets in one read, garbage between frames, and closing and reopening
   the port. The app rejects or recovers from each without crashing, and recovers through
   `reconnect()` when the port comes back.
6. **It is all scripted and documented.** `Kick/tools/sim_smoke.sh --wire` runs the existing smoke
   test (load Starter Kit, play four bars, stop, assert sound and LEDs) over the serial path.
   `Kick/tools/sim_serial_faults.sh` runs item 5. `docs/DEVELOPMENT.md` gains a section on the
   wire mode and the serial implementation, and 5.2 is rewritten.

## How to work

Work in phases, in order unless a phase number was given as the argument, in which case do only
that phase. Commit at the end of each phase with a message that says what now works. Keep
`docs/DEVELOPMENT.md` current as you go. Verification is not optional: show the log lines (both
ends of the link), the bytes where framing matters (hex dump), and the smoke-test output. Never
report "works" from reading code alone. Use the tooling in DEVELOPMENT.md 4.2 (`sim_launch.sh`
takes a machine-wide lock, so only one simulator runs at a time; always `sim_quit.sh`).

### Phase 1: protocol notes

Read `ComProtocol/PktMgr` and `input_events.cpp` and write down, in `docs/DEVELOPMENT.md`, the
exact frame layout (start bytes, length, direction, type, command, payload, CRC algorithm and
coverage), the handshake sequence with timings and retries, what the app does on a bad frame,
and when `reconnect()` is called. Add a unit test (a small console target or a test in
`Kick/tools/`) that round-trips every packet type through `CPktMgr` encode and decode, including
the largest payloads (pad colour sets of 16 or 32 values).

### Phase 2: juce_serialport

Implement the POSIX serial port: open with `O_NOCTTY`, raw mode, the configured baud rate and 8N1,
a reader thread that fills the input stream and notifies listeners, blocking or queued writes,
clean close. Make `KICK_SERIAL_DEVICE` the only behaviour change on the device path. Test it
against a pseudo-terminal pair, and against a USB serial adapter in loopback if one is to hand.

### Phase 3: the panel on the wire

Add the surface mode to the simulator. Factor the encoding and decoding the panel already does
(`kick_Simulator.cpp`) so the in-memory and wire modes share it. Optionally throttle the link
to 230400 baud's real throughput (about 23 KB/s), behind a flag, so floods of LED and pad
packets show up as they would on the unit.

### Phase 4: the plain app over the wire

Enable the serial connect path on macOS desktop builds. Run `Kick` against the surface, get the
handshake through, and walk every control (reuse `sim_modes.sh` and `sim_pads.sh` through the
surface's control channel). Write `sim_smoke.sh --wire`.

### Phase 5: faults

Write `sim_serial_faults.sh`. Fix any crash, hang or lost-sync problem it finds in
`input_events.cpp` or `PktMgr`. Record each fix and the observation behind it.

## Constraints

- No code you lack the rights to ship: write `juce_serialport` from scratch, do not look for the
  removed original.
- The device path must behave as before when `KICK_SERIAL_DEVICE` is unset: same device, same
  settings, same handshake.
- Simulator-only code lives in `Kick/apps/kickem/` and `Kick/tools/`, or behind `SIM_BUILD`.
- Build with the command in `docs/DEVELOPMENT.md` section 3 (`MACOSX_DEPLOYMENT_TARGET=12.0`).
- Do not push. Commit per phase.
