---
description: Run the Linux build of Kick in an ARM virtual machine on the Mac, with the simulated panel attached over a virtual serial port, so the device's Linux code paths, files, scripts and services can be tested and heard. Optional argument: a phase number to work on only that phase.
---

# Goal: the Linux build runs in an ARM VM, driven by the simulated panel

Read `docs/DEVELOPMENT.md` first, and `Linux/README.md`. This goal needs `/goal-serial` done:
the wire-mode panel (`KT_KickEM --surface`) and a real `juce_serialport`.

## Why

The simulator compiles the macOS desktop and `SIM_BUILD` branches. The unit runs the
`JUCE_LINUX` branches: `/app`, `/data/sounds` and `/home/Kick` paths, the serial transport, the
Wi-Fi, Ethernet, date, storage and update code that Phase 6 of `/goal` stubbed out, systemd
start-up (`ktkick.service` → `kt_kick.sh` → `/usr/bin/KT_Kick`), Weston, the overlay mounts on
`/media/kickstorage`, and the eMMC layout. A VM with the same CPU architecture (64-bit ARM) runs
all of that on a Mac at near-native speed (QEMU `virt` with `-accel hvf`, or UTM), and makes it
testable in CI.

What a VM cannot tell you, and must not be used for: pad-to-sound latency, CPU headroom on the
unit's Cortex-A53 cores, audio dropouts under load, the codec and its ALSA driver, the GPU
driver, and anything the panel microcontroller does itself. Say so wherever results are reported.

## The target in the repository

- `Linux/buildroot/configs/KTKick_defconfig`: Buildroot for the Variscite i.MX 8M Mini module.
- `Linux/buildroot/board/freescale/imx8mm_var_som/`: board files, kernel config, U-Boot,
  `genimage` layouts (eMMC: `rootfs` plus an auto-resized `usr` partition), and
  `rootfs_overlay/` with `ktkick.service`, `kt_kick.sh`, Weston, Wi-Fi scripts, udev rules,
  `asound.conf`, and the overlay mounts.
- `Kick/how-to/Kick_Compilation.txt`: the Linux desktop package list and the SDK route
  (`make KTKick_defconfig && make sdk`, then the SDK's `toolchainfile.cmake`).
- QEMU has no i.MX 8M Mini machine. The VM therefore uses QEMU's generic `virt` board. That is
  the same userland and app, but a different kernel, device tree and set of drivers.

## Definition of done

1. **The VM boots and runs Kick.** One script, `Linux/vm/run.sh`, starts an arm64 VM with a
   1024x600 display, touch through a tablet-style pointer, audio, networking with SSH forwarded
   to the host, extra disk images standing in for the SD card and USB stick, and its serial port
   connected to the host. Inside, `KT_Kick` built from this tree (non-`SIM_BUILD`, `JUCE_LINUX`)
   starts on boot and shows its screen.
2. **The panel drives it.** `KT_KickEM --surface` on the Mac connects to the VM's serial port
   (QEMU passes the guest's UART to a socket or pseudo-terminal on the host). In the guest the
   port is `/dev/ttyAMA0`, so `KICK_SERIAL_DEVICE` or a udev symlink to `/dev/ttymxc0` is used.
   The handshake completes and every panel control reaches the app.
3. **You can hear it and measure it.** The guest plays through QEMU's virtual sound card to the
   Mac's default output. `KICK_SIM_AUDIO_PROBE` and `KICK_SIM_AUDIO_INPUT` (in `mt::AudioDevice`,
   not behind `SIM_BUILD`) work in the guest, and the probe WAV is copied back to the host so
   scripts assert sound without relying on the VM's audio.
4. **The device's Linux paths run for real.**
   - Projects save and load under the unit's paths.
   - The SD and USB disk images mount through the device's own udev rules and scripts.
   - Wi-Fi scan and connect run against the kernel's fake Wi-Fi driver (`mac80211_hwsim`).
   - The date, Ethernet and update screens run their real code against the VM.
   - CV/gate (I2C DAC) and the camera are absent: the app must cope, and any crash or hang is
     fixed.
5. **It is scripted.** `Linux/vm/smoke.sh` boots the VM headless, waits for SSH, deploys the
   current build, attaches the panel, runs the `sim_smoke.sh` flow over the serial link, copies
   back the app log and probe WAV, and exits non-zero on failure. It is documented, with the
   expected run time, in `docs/DEVELOPMENT.md`.

## How to work

Work in phases, in order unless a phase number was given as the argument, in which case do only
that phase. Commit at the end of each phase with a message that says what now works. Keep
`docs/DEVELOPMENT.md` current as you go, with a section on the VM: how to build it, how to run
it, what it emulates, and what it does not. Verification is not optional: boot logs, the app
log, the panel's `SimSerial:` log, screenshots of the VM display, and probe measurements. Never
report "works" from reading code alone.

### Phase 1: stock Linux VM (3a)

Start from a stock Debian or Ubuntu arm64 VM, not Buildroot: install the packages from
`Kick/how-to/Kick_Compilation.txt` and build `Kick` natively in the guest. Write a setup script
that creates the device's directories (`/app/{assets,demos,templates}`, `/data/sounds`,
`/home/Kick/{projects,logs,prefs}`) and copies the assets, demos (including Starter Kit) and
samples into place. Get the app running under a Wayland or X session at 1024x600, with ALSA
audio through QEMU's sound card.

### Phase 2: the serial link

Connect the guest UART to the host, connect `KT_KickEM --surface`, and get the handshake and
every control through. Script start-up of both ends.

### Phase 3: device paths and services

Install the relevant parts of `rootfs_overlay/` into the guest: the systemd unit and start
script, udev rules, Wi-Fi scripts and `asound.conf`. Make SD and USB disk images appear as the
device expects, load `mac80211_hwsim`, and run the Wi-Fi, Ethernet, storage and update flows.
Fix crashes and hangs in the shared code. Fixes that change device behaviour are kept minimal
and explained in the commit.

### Phase 4: smoke test and CI

Write `Linux/vm/smoke.sh` (headless boot, SSH deploy, panel attach, play four bars, assert the
probe and LED feedback). Document how to run it on a macOS ARM machine (hardware-accelerated)
and on Linux x86 (emulated, slow), with measured boot and run times.

### Phase 5 (optional): Buildroot image for the VM (3b)

Only if phases 1-4 show that the stock-distribution VM misses device behaviour that matters.
Derive a `virt` configuration from `KTKick_defconfig`: same packages, overlay and services, with
a `virt` kernel, and NXP's GPU and video packages replaced by Mesa's software renderer or a
virtual GPU. Cross-compile `KT_Kick` with the Buildroot SDK, which also proves the SDK route in
`Kick/README.md` works. Buildroot takes hours on the first build and asks you to accept NXP's
license terms for some downloads; say what was accepted.

## Constraints

- Do not redistribute anything you lack the rights to: NXP firmware and driver blobs stay
  downloads, never committed; the VM disk images are build outputs, never committed.
- Keep the device build intact. VM-only files live under `Linux/vm/` (and `Kick/tools/` for the
  host side); shared-code fixes are minimal and explained.
- The image is a development build (root without a password, Dropbear SSH enabled, see
  `Linux/README.md`). Keep the VM's SSH forwarded to localhost only.
- Do not push. Commit per phase.
