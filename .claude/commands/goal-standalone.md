---
description: Turn the in-tree KT_KickEM simulator into a separate project, KickEm-Tools/KickStandaloneEmulator, that builds and runs any fork of the 0700-AAM firmware without simulator code inside it. Lands the firmware changes in KickEm-Tools/0700-AAM, then proposes them to MusicTribeOpenSource. Optional argument: a phase number to work on only that phase.
---

# Goal: a standalone emulator for any 0700-AAM fork

Read `docs/DEVELOPMENT.md` (all of it, especially sections 3, 4, 5 and 7) and
`docs/SIMULATOR_STATUS.md` first, then `.claude/commands/goal-serial.md` and
`.claude/commands/goal-vm.md`: this goal absorbs both of them.

## Why

People who change the Kick firmware (the app under `Kick/`, the audio engine under `AudIO/`, the
Linux image under `Linux/`) should be able to test their change without a unit. Today the
simulator only exists inside one branch of one fork, compiled into the firmware tree with
`SIM_BUILD`, so it cannot be pointed at somebody else's fork. The goal is an emulator that takes
**a firmware repository URL and a branch** and runs it, with no emulator code inside the firmware.

## Where things stand

- **Repositories.**
  - `MusicTribeOpenSource/0700-AAM` is the upstream: the public release, AGPL-3.0-or-later.
  - `KickEm-Tools/0700-AAM` is the base fork for this goal (the user is an org admin). Its `main`
    equals upstream `main` (`27194180` when this plan was written). It has no CI workflows and
    no pull requests. Two branches of other work exist: `codeowners-team` (adds
    `.github/CODEOWNERS`) and `pin-buildroot-ci-image`. Respect them, and expect code-owner
    review on pull requests once CODEOWNERS lands.
  - `rbdone/0700-AAM`, branch `rbdone/goal-simulator-controls`, is all the work so far: the macOS
    build, the simulator, `/goal` phases 1-7 and the later fixes. It is the source material for
    this goal. Keep it as the record and do not rewrite or delete it.
  - `KickEm-Tools/KickStandaloneEmulator` does not exist yet. Phase 4 creates it.
- **Upstream does not build as published.** The release is missing `AudIO_juce` (`mt::AudioDevice`,
  `ADSR`, `Filter`, `Recorder` / `SampleRecorder` / `Looper`) and `AudIO_constants.h`. The work
  branch reconstructs them, which accounts for 12 new files. It also has the build fixes in
  DEVELOPMENT.md section 5. Anyone building a fork needs these, for the device or the desktop.
- **`juce_serialport` is a stub** (DEVELOPMENT.md 5.2), so a release build cannot talk to the
  panel microcontroller even on a real unit.
- **The simulator is woven into the firmware tree.** The work branch changes 88 shared files
  outside the simulator's own directories.
  - 24 of them hold 44 `SIM_BUILD` sites. These cover: popups and bubbles attached to the scaled
    screen, list hit-testing, focus tracking for scripted touches, "not available in the
    simulator" panels for hardware screens, project-load logging, and similar.
  - The others are build fixes and real device bug fixes found with the simulator (section 7 of
    DEVELOPMENT.md), for example the PAD MIX row off-by-one, P1-P16 knob routing, and the
    `MTTrack::onTick` crash on NEXT SEQ.
- **Firmware has three layers.**
  - The Kick app, where most custom changes happen.
  - The Linux image: Buildroot, systemd units, scripts.
  - The control-surface microcontroller's firmware. This is not in any repository, so the
    emulator plays its side of the `PktMgr` protocol.

## The design

The serial protocol is the only coupling between emulator and firmware. The panel becomes its own
program, speaking the microcontroller's side of `CPktMgr` over a transport: a pseudo-terminal, a
TCP socket (QEMU's serial backend, and Windows), or a real serial port. The firmware needs no
emulator code for input, and the protocol headers come from the firmware ref being tested, so a
fork that extends the protocol still works.

The emulator repository offers two modes, driven by the same panel and the same checks:

| | Fast mode | Faithful mode |
|---|---|---|
| Runs | The fork's `Kick` app built for the desktop (macOS, Linux) from the fork's source, connected to the panel over a pty | The fork's Linux arm64 `KT_Kick` build in a QEMU arm64 VM, with the panel on the VM's serial port |
| For | Quick edits and rebuilds, a debugger, scripted checks | The device's Linux code paths and services; CI on firmware pull requests |
| Needs in the firmware | Builds on desktop, a working serial port, a few generic extension points (phase 3) | A working serial port |
| Cannot tell you | Anything Linux-specific | Latency, CPU headroom on the A53 cores, the codec, the GPU driver, the microcontroller's own behaviour |

Say what each mode cannot tell you wherever results are reported.

## Definition of done

1. **The firmware fork builds, and needs no simulator code.**
   - `KickEm-Tools/0700-AAM` `main` builds `Kick` on macOS and Linux desktop, and `KT_Kick` for
     Linux arm64, in CI.
   - It contains no `SIM_BUILD`, no `Kick/apps/kickem` and no `Kick/tools/sim_*`. Each of the 44
     sites became generic code that is correct on the device as well, moved behind a small
     runtime extension point, or moved out to the emulator.
   - The serial port is real.
   - Device behaviour changes only through deliberate fix pull requests, each with the
     observation that motivated it.
2. **The emulator repository exists and works.**
   - `KickEm-Tools/KickStandaloneEmulator` is AGPL-3.0-or-later.
   - One command (for example `./kickem run --firmware <repo url> --ref <branch> [--mode fast|vm]`)
     fetches that ref, builds it, starts the panel and the firmware, and connects them.
   - On an Apple Silicon Mac, the default fork, fast mode: sound out of the Starter Kit within
     the first-build time plus 30 s.
3. **Parity with the in-tree simulator.** The checks from the work branch are ported to drive the
   panel over the wire, and pass in fast mode against `KickEm-Tools/0700-AAM` `main`:
   - `sim_smoke`, `sim_transport`, `sim_modes`, `sim_pads`, `sim_sampler_looper`, `sim_overlays`;
   - plus `/goal-serial`'s fault test.

   `smoke` passes in faithful mode.
4. **CI for forks.**
   - The emulator publishes a reusable GitHub workflow that any fork calls with a few lines. It
     builds the fork's firmware, boots it headless, runs the smoke test, and uploads the log,
     probe WAV and screenshots.
   - `KickEm-Tools/0700-AAM` uses it on every pull request.
   - The panel is released as downloadable builds (macOS arm64, Linux x86_64 / arm64; Windows
     for faithful mode over TCP if it comes cheap).
5. **Proposed upstream.**
   - Every firmware change that belongs upstream is open as a pull request from
     `KickEm-Tools/0700-AAM` to `MusicTribeOpenSource/0700-AAM`, split by topic.
   - Each pull request says what it fixes and how it was verified.
   - The user approves each one before it is opened.
6. **Documented.**
   - The emulator README covers: quick start, modes, what each mode cannot tell you, how to
     point it at your fork, how to add it to your fork's CI, and the licence of the Starter Kit
     content (CC0).
   - The fork's `docs/DEVELOPMENT.md` loses its simulator sections. They are replaced with a
     pointer to the emulator.

## How to work

Work in phases, in order unless a phase number was given as the argument; then do only that phase.

- **Phase 0 comes first.** Its inventory drives everything after it.
- **Parallel work.** Phases 1, 2 and 4 can run in parallel with sub-agents once phase 0 is
  committed. Give each sub-agent its own worktree and its own branch.
- **Committing.** Commit at the end of each phase with a message that says what now works.
- **Keeping records.** Keep the inventory, `docs/DEVELOPMENT.md` (fork) and the emulator README
  current as you go.
- **Verification is not optional.** Show build logs, CI run links, check output, screenshots,
  probe measurements, and the `SimSerial:` logs from both ends. Never report "works" from
  reading code alone.
- **Sub-agent briefs.** Every sub-agent must build and run what it changed before reporting.

Git and GitHub, for this goal:

- **Remotes.** Add `KickEm-Tools/0700-AAM` as remote `kickem`; `origin` stays `rbdone/0700-AAM`.
- **Topic branches.** Firmware changes go to topic branches on `kickem`, cut from `kickem/main`.
  Each has a pull request into `kickem/main` with CI green.
- **Merging.** Merge when the user has said to: list the pull requests that are ready and ask
  once per batch. Never push to `kickem/main` directly. Never force-push a shared branch.
- **Upstream.** Never push to `MusicTribeOpenSource`. Upstream pull requests are opened from
  `kickem` topic branches, only after the user approves each one (phase 8).
- **Emulator repository.** Push to `KickEm-Tools/KickStandaloneEmulator` freely once it exists.
  Use pull requests there too once CI exists.
- **The other goals.** When `/goal-serial` or `/goal-vm` work is done as part of this goal,
  their "Do not push" line is superseded by these rules.

### Phase 0: inventory and split plan

1. Add the `kickem` remote and fetch. Diff `kickem/main..rbdone/goal-simulator-controls`.
2. Classify every hunk outside `Kick/apps/kickem/`, `Kick/tools/`, `docs/`, `.claude/`,
   `Kick/assets/StarterKit/` and `Kick/demos/Starter Kit/` as one of the following:
   - **BUILD**: the release builds (reconstructed AudIO, build-system and compile fixes).
   - **FIX**: device bug fixes (DEVELOPMENT.md section 7), one entry per bug.
   - **SEAM**: a `SIM_BUILD` site. For each, decide:
     - **generic**: correct on the device too. For example, list rows hit-tested with
       `getEventRelativeTo`, and popups parented to the app's root component rather than the
       desktop (the unit's window covers the display, so behaviour is unchanged).
     - **runtime**: a small extension point that is inactive on the device. For example, hardware
       screens detecting a missing `/dev` node at run time instead of at compile time, the
       opt-in audio probe and test input, and an opt-in automation channel.
     - **emulator**: moves out of the firmware entirely.
   - **SIM**: simulator-only code that moves to the emulator.
   - **CONTENT**: Starter Kit samples, project and metronome clicks.
3. Write the result to `docs/STANDALONE_SPLIT.md` on the work branch. It is a table of file,
   hunk, class, decision, target branch or repository, and status. List the topic branches it
   implies, the order they depend on each other, and which are upstreamable.
4. Decide the automation channel, and record the decision. Scripted screen touches need either
   OS-level events, which do not work with the Mac locked, or an in-app channel like
   `KICK_SIM_CONTROL`. Prefer a generic, opt-in, environment-variable-gated test-control module
   in the firmware (inactive and inert unless set), proposed upstream like any other change. In
   faithful mode use QEMU's input injection (absolute pointer events through the monitor or QMP),
   which works headless and needs nothing in the firmware.

### Phase 1: the fork builds (BUILD)

- **Release build.** Branch `build/release-builds` from `kickem/main`. Apply the BUILD hunks:
  reconstructed `AudIO_juce` and `AudIO_constants.h` (keep the observation notes next to changed
  constants), the CMake and source fixes from DEVELOPMENT.md section 5, and a short section of
  DEVELOPMENT.md on building.
- **Verify** on macOS (`MACOSX_DEPLOYMENT_TARGET=12.0`) and Linux arm64. Use an arm64 Debian
  container on the Mac: Docker on Apple Silicon runs arm64 natively. Cover the package list from
  `Kick/how-to/Kick_Compilation.txt`.
- **CI.** Add `.github/workflows/build.yml`: `Kick` on `macos-14` and `ubuntu-24.04`, `KT_Kick`
  on `ubuntu-24.04-arm`. Upload a `KT_Kick-linux-arm64` artifact: binary, assets, and a manifest
  with the git ref. Cache CPM downloads. Coordinate with `pin-buildroot-ci-image`, and do not
  undo it.
- **Pull request** into `kickem/main`.

### Phase 2: device fixes (FIX)

One topic branch and pull request per fix, or per closely related group, from `kickem/main` (after
phase 1 if it needs the build). Each pull request states:

- the bug on the device;
- the observation that found it (log lines, measurement);
- the fix;
- the check that proves it.

The checks still live in the simulator at this point. Reference the work-branch script and its
output.

### Phase 3: serial port and seams (/goal-serial phases 1-2, SEAM)

- **Serial port.** Do `/goal-serial` phases 1 and 2 on a branch `feature/serialport` from
  `kickem/main`:
  - protocol notes and the round-trip test;
  - a real POSIX `juce_serialport`, written from scratch;
  - `KICK_SERIAL_DEVICE`;
  - the serial connect path enabled on desktop builds.
- **Seams.** On `feature/desktop-seams`, apply every SEAM decision from phase 0 that stays in the
  firmware. The target is zero `SIM_BUILD` and a short list of runtime extension points, each
  documented in DEVELOPMENT.md with what it does on the device: nothing, unless an environment
  variable is set or the hardware is missing.
- **Verify** the device path is unchanged with `KICK_SERIAL_DEVICE` and the extension points
  unset. Show this in the pull request.
- **Pull requests** into `kickem/main`.

### Phase 4: the emulator repository

1. Confirm with the user, then create `KickEm-Tools/KickStandaloneEmulator`: public unless the
   user says otherwise, with a description, AGPL-3.0-or-later `LICENSE`, and a README stub.
2. Lay it out, roughly:
   - `panel/`: the front panel, from `Kick/apps/kickem/simulator`;
   - `transport/`: pty, tcp, serial;
   - `cmake/`: firmware fetch, by URL and ref through CPM or FetchContent;
   - `modes/fast/`;
   - `modes/vm/`;
   - `checks/`: the ported `sim_*` scripts;
   - `content/StarterKit/`: CC0, with `make_starter_kit.py`;
   - `kickem`: the launcher script;
   - `.github/workflows/`.
3. The panel takes protocol and control ids (`ComProtocol/PktMgr`, `DeviceControlContracts`) from
   the fetched firmware ref, not from a copy.
4. Import history where it helps (`git filter-repo` of the simulator paths from the work branch,
   for example), or start clean with a commit that names the source branch and commit. Say which
   was done.

### Phase 5: fast mode (/goal-serial phases 3-5, in the emulator)

- **Panel on the wire.** Do `/goal-serial` phase 3 (the panel speaking `CPktMgr` over a pty,
  optional 230400-baud throttling) in the emulator's `panel/`.
- **Running a fork.** `kickem run --mode fast` fetches the firmware ref, builds `Kick` for the
  desktop with the phase 3 extension points, installs the Starter Kit into its data paths, starts
  the panel, and connects them.
- **Port the checks** to drive the panel over the wire and the screen through the automation
  channel chosen in phase 0:
  - `sim_smoke`, `sim_transport`, `sim_modes`, `sim_pads`, `sim_sampler_looper`, `sim_overlays`;
  - `/goal-serial` phase 5's fault test.

  All must pass against `KickEm-Tools/0700-AAM` `main`.
- **Drift.** Where a check fails because the fork's `main` lacks a phase 2 fix that is still in
  review, say so. Run against the fix branch to show the check itself is right.

### Phase 6: faithful mode (/goal-vm phases 1-4, in the emulator)

- **The VM.** Do `/goal-vm` phases 1 to 4 under `modes/vm/`, with these changes:
  - The guest runs the **built artifact** of the fork (`KT_Kick-linux-arm64` from the fork's CI,
    or built locally in the arm64 container), not a build inside the guest.
  - Start from a stock Debian arm64 cloud image.
  - The panel attaches through QEMU's serial backend (pty on macOS and Linux, TCP for Windows).
  - Audio is asserted with the probe, or with ALSA's loopback driver (`snd-aloop`) in the
    guest, so no firmware change is needed.
  - Screen touches go through QEMU input injection.
- **Timing.** `kickem run --mode vm` and `checks/smoke --mode vm` pass. Document boot and run
  times measured on Apple Silicon (hvf) and on an x86 Linux runner (TCG, emulated).
- **Buildroot image.** `/goal-vm` phase 5 stays optional. Do it only if the stock-image VM misses
  device behaviour that matters, and say what.

### Phase 7: CI and distribution

- **Reusable workflow.** In the emulator, add
  `.github/workflows/test-firmware.yml` (`workflow_call`), with inputs for the firmware
  repository, the ref, and the mode. It builds, boots headless, runs smoke, and uploads the
  evidence. Tag `v1`.
- **The fork's pull requests.** In `KickEm-Tools/0700-AAM`, add a workflow that calls it on pull
  requests: fast mode on `macos-14`, and faithful mode where a runner allows it. Record run times
  and any runner limits, such as no KVM on hosted arm64 runners, in the README.
- **Panel releases.** Build the panel on tags, for macOS arm64 (signed only if the user provides
  an identity; otherwise document the Gatekeeper step) and for Linux.
- **Sizing.** Measure how long a fork's CI run takes, and keep fast mode under 15 minutes cold.

### Phase 8: propose upstream

- **List** the merged `kickem` pull requests that belong upstream: BUILD, FIX, `feature/serialport`,
  and the generic and runtime seams. Not the CI wiring that calls the KickEm-Tools emulator,
  unless the user wants it.
- **Rebase** each onto `MusicTribeOpenSource/0700-AAM` `main`.
- **Ask the user to approve each one**, then open it with what it fixes, how it was verified, and
  a link to the emulator for reproducing it.
- **Follow up** on review comments only when asked.

### Phase 9: retire the in-tree simulator

Once phase 5 parity holds:

- On `rbdone/goal-simulator-controls`, add a final commit that points `docs/DEVELOPMENT.md` and
  the `/goal*` plans at the emulator.
- In `KickEm-Tools/0700-AAM`, make sure no simulator code was ever merged, and that the README
  links the emulator.
- Update the user's memory notes (`kick-simulator-goal-status`) with where everything now lives.

## Constraints

- **Licence.** The emulator repository is AGPL-3.0-or-later, like the firmware it builds against.
  The Starter Kit content stays CC0, with its `LICENSE` beside it.
- **Content.** Do not add content you lack the rights to ship. Synthesise it. NXP firmware and
  driver blobs, Buildroot downloads, VM disk images and firmware build outputs are never
  committed.
- **Device behaviour.** No firmware change alters device behaviour unless it is a FIX pull
  request that says so. Extension points are inert on the device unless an environment variable
  is set or the hardware is missing.
- **Destructive actions.** No force-pushes to shared branches, no deleting branches others
  created, and no rewriting `rbdone/goal-simulator-controls`.
- **Outward-facing actions need the user's go-ahead:**
  - creating the emulator repository;
  - merging into `kickem/main` (ask per batch);
  - every upstream pull request;
  - publishing releases.
- **Building and running.** Build with `MACOSX_DEPLOYMENT_TARGET=12.0` on macOS. Only one
  `KT_KickEM` or emulator instance runs per machine: keep a machine-wide lock as `sim_launch.sh`
  does.
- **Commits.** Commit per phase.
