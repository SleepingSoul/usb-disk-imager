# Publishing to the Snap Store

`snap/snapcraft.yaml` builds a strictly confined snap for `core24`. This document explains the one design
decision that shapes the whole package, then the commands to build and publish it.

## Why udisks2, and not root

A snap runs as the invoking user. It cannot call `pkexec` to restart itself as root, and it cannot ship
the udev rule in `packaging/linux/` — nothing outside the snap ever reads it. So the model this app used
on Linux until now, "be root or refuse", has no way to work inside a snap.

Raspberry Pi Imager, the closest published equivalent, is strictly confined on `core24` and solves this by
plugging **`udisks2`** rather than `block-devices`: it never opens `/dev/sdX` itself. It asks the udisks2
system service, over D-Bus, to open the device and hand back an already-open file descriptor. polkit
decides whether to allow it, prompting the user once. This app now does the same, in
`src/disk/linux/udisks2.cpp`:

- `RawDevice::open()` still opens the device directly when the process *is* root — that path is
  unchanged, and is what a `sudo`-launched or packaged-from-source build keeps using. Only when the
  direct `open()` is refused with `EACCES`/`EPERM` does it fall back to udisks2.
- `O_DIRECT` cannot be passed through udisks2's own `open()`, so it is turned on afterwards with
  `fcntl(F_SETFL)`, which Linux permits for exactly that flag. The transfer still bypasses the page cache.
- `O_EXCL` *is* passed through, in the `flags` option udisks2 accepts, so a write is still refused while a
  filesystem on the device is mounted.
- `VolumeControl` unmounts through `org.freedesktop.UDisks2.Filesystem.Unmount` when not root, which
  unmounts as the owning desktop session rather than as root.

This is a genuine improvement outside the snap too: on any desktop with udisks2, an ordinary user can now
image a card after answering one polkit prompt, instead of restarting the whole GUI under `sudo`. The
`ElevationAlert` and the "restart with rights" button now appear only where that is actually necessary,
because `App.deviceAccessAvailable` asks whether a device can be opened at all rather than whether the
process is root.

### What this costs

Filesystem shrink/grow does **not** work inside the snap. It needs `losetup`, which cannot map a loop
device under confinement. Nothing special is done about this: `FilesystemResizer::isSupported()` already
looks for the tools and reports why they are missing, so the option greys itself out with an explanation,
exactly as it does on a system without e2fsprogs. The snap therefore does not ship e2fsprogs at all.

## Interfaces

The `kde-neon-qt6` extension adds `desktop`, `desktop-legacy`, `opengl`, `wayland`, `x11`,
`audio-playback`, `unity7`, `network` and `network-bind` on its own. `snapcraft.yaml` adds only what the
extension does not:

| Interface | Why |
|---|---|
| `udisks2` | Opens the block device and unmounts its filesystems. The whole privilege model. |
| `hardware-observe` | Reading `/sys/block` to enumerate devices. |
| `mount-observe` | Reading the mount table to tell which volumes are mounted. |
| `home`, `removable-media` | Reading and writing the image file itself. |

`block-devices` is deliberately **not** plugged: nothing opens `/dev/sdX` directly inside the snap.

Several of these do not auto-connect on install. After installing, check:

```bash
snap connections usb-disk-imager
```

and connect anything left disconnected:

```bash
sudo snap connect usb-disk-imager:udisks2
sudo snap connect usb-disk-imager:removable-media
sudo snap connect usb-disk-imager:hardware-observe
sudo snap connect usb-disk-imager:mount-observe
```

For published releases, auto-connection is requested from the reviewers on
<https://forum.snapcraft.io/c/store-requests/19>, citing the same justification rpi-imager uses: a disk
imager is useless if it cannot reach the device the user selected. Until such a request is granted, the
connect commands above are what users need, and they belong in the store listing description.

## Building

Snapcraft needs snapd, which does not run inside an ordinary container, so this is built on the host:

```bash
sudo snap install snapcraft --classic
sudo snap install lxd && sudo lxd init --auto && sudo adduser "$USER" lxd   # log out and back in once
cd usb-disk-imager
snapcraft
```

That produces `usb-disk-imager_<version>_amd64.snap`. Install it locally to test:

```bash
sudo snap install --dangerous ./usb-disk-imager_*_amd64.snap
snap connections usb-disk-imager     # connect anything missing, see above
usb-disk-imager
```

`snapcraft clean` between attempts if a part misbehaves.

## Publishing

```bash
snapcraft login
snapcraft register usb-disk-imager          # once; the name was unregistered as of writing
snapcraft upload --release=edge ./usb-disk-imager_*_amd64.snap
```

Promote when it has been tested:

```bash
snapcraft release usb-disk-imager <revision> beta
snapcraft release usb-disk-imager <revision> stable
```

## Notes on the build

- `-DUDI_DEPLOY_QT_RUNTIME=OFF` is passed because `cmake/Deploy.cmake` otherwise installs a private copy
  of the whole Qt runtime beside the binary. That is right for a self-contained tarball and wrong here,
  where Qt comes from the `kde-qt6-core24` content snap — a second copy would add well over a hundred
  megabytes and shadow the platform's.
- The extension builds against `kde-qt6-core24-sdk`, which tracks a Qt newer than this project's 6.8.3
  floor, so no version pinning is needed.
- `override-prime` deletes `usr/etc/udev`, since a udev rule inside a snap is never read.

## What has and has not been verified

Verified here: the app compiles on a `core24` (Ubuntu 24.04, gcc 13.3) toolchain with
`-DUDI_WARNINGS_AS_ERRORS=ON`; the install tree contains exactly the binary, desktop file, icon and
metainfo once the private Qt copy is turned off; `snapcraft.yaml` validates against snapcraft's own
project schema; and udisks2 really does answer `OpenDevice` for an unprivileged caller on this machine —
it returned `NotAuthorizedCanObtain`, which is polkit saying the user may authorise it, rather than a
refusal.

Not verified here: the `snapcraft` run itself and the resulting snap, because snapd will not start inside
a container (`snapd-apparmor` reports "inside container environment without internal policy") and
installing it on the host needs root. The first `snapcraft` run on a machine that has it is the remaining
step.
