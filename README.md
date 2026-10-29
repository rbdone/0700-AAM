<p align="center">
  <img src="docs/images/social-preview.png" alt="Behringer KICK (BE 0700-AAM)" width="100%">
</p>

# Behringer KICK (BE 0700-AAM)

Source code for the Behringer KICK.

## This fork's emulator work has moved

The Kick emulator developed in this fork is now its own project,
[KickEm-Tools/KickEmulator](https://github.com/KickEm-Tools/KickEmulator) (AGPL-3.0-or-later). It
takes a firmware repository URL and a ref, builds that firmware, and runs it against an emulated
front panel that speaks the control-surface microcontroller's serial protocol. Fast mode runs the
desktop build over a pseudo-terminal; faithful mode runs the Linux arm64 build in a QEMU VM. Its
checks cover smoke, transport, modes, pads, sampler/looper, overlays and serial faults.

**Why:** with the emulator separate from the firmware, it can test any new firmware release or fork
that builds and keeps the control-surface protocol, by URL and ref. No simulator code goes into the
firmware, and the work is not tied to one branch of MusicTribeOpenSource's repository.

The firmware changes it needs (building the public release, a real serial port, device bug fixes)
land in [KickEm-Tools/0700-AAM](https://github.com/KickEm-Tools/0700-AAM) and are to be proposed
upstream to [MusicTribeOpenSource/0700-AAM](https://github.com/MusicTribeOpenSource/0700-AAM). The
earlier in-tree simulator work is kept on the branch
[`rbdone/goal-simulator-controls`](https://github.com/rbdone/0700-AAM/tree/rbdone/goal-simulator-controls).

<p align="center">
  <img src="docs/images/kick-front.webp" alt="Behringer KICK" width="720">
</p>

## Key features

- AI camera for gesture control
- Touchscreen
- 16 RGB pads, 8 banks
- Sampler with sample editing, step sequencer and looper
- Over 300 built-in effects
- 4 assignable knobs and a jog wheel
- MIDI in/out, USB and 4 CV / Gate jacks

## Connections

<p align="center">
  <img src="docs/images/kick-rear.webp" alt="Behringer KICK rear panel" width="100%">
</p>

| Rear | Front |
|---|---|
| 12 V DC power input, power switch | SD card slot |
| LINK (RJ45) | Headphones output |
| USB-C, USB-A | |
| MIDI IN, MIDI OUT (5-pin DIN) | |
| Stereo output, stereo input | |
| CV / Gate (4 jacks) | |

## Repository

| Folder | Contents | License |
|---|---|---|
| [`Kick/`](Kick) | Application | AGPL-3.0-or-later |
| [`AudIO/`](AudIO) | Audio engine library | AGPL-3.0-or-later |
| [`ComProtocol/`](ComProtocol) | Hardware communication protocol | AGPL-3.0-or-later |
| [`Linux/`](Linux) | Buildroot Linux image | GPL-2.0 (Buildroot) |

The root [`LICENSE`](LICENSE) is the AGPL-3.0 text that covers the Music Tribe code (Kick, AudIO, ComProtocol).
Third-party components keep their original licenses. See the README in each folder.

## Community

- **Chat with us:** [Discord](https://discord.gg/gS6qtuShvw)
- **Contribute:** fork the repository and open a pull request. No membership needed. See the
  [contributing guide](https://github.com/MusicTribeOpenSource/.github/blob/main/CONTRIBUTING.md).
- **Report a security problem** privately: [Report a vulnerability](https://github.com/MusicTribeOpenSource/0700-AAM/security/advisories/new).

<details>
<summary>Product renders</summary>

<p align="center">
  <img src="docs/images/views.png" alt="KICK top, front, side and rear views" width="100%">
</p>
</details>

---

Behringer is a brand of MUSIC Tribe. The Behringer name and logo, and the KICK name and logo, are
trademarks and are not covered by the software licenses in this repository.

Copyright (c) MUSIC Tribe.
