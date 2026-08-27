# Opening a device on Linux without being root

Raw disk access needs privileges no desktop app is given by default. This document explains how the Linux
backend gets them, and why it does not simply demand root.

## The problem with demanding root

The obvious model — refuse unless `geteuid() == 0`, and offer to relaunch through `pkexec` — is what this
app did originally, and it has two flaws.

The first is that it contradicted the app's own packaging. `packaging/linux/99-usb-disk-imager.rules`
grants members of the `disk` group `0660` access to removable block devices, and
`Privileges::getElevationHint()` tells the user to "add your user to the `disk` group and log in again".
Both were dead ends: a user who did exactly that still had `geteuid() != 0`, so the operation was refused
anyway. The rule handed out access the app then declined to use.

The second is that relaunching a GUI through `pkexec` is a poor experience even when it works. The new
process starts with a stripped environment, has to be handed `DISPLAY`/`WAYLAND_DISPLAY`/`XAUTHORITY` by
hand, and replaces the window the user was already looking at.

## What it does instead

`RawDevice::open()` tries the direct `open()` first. When the process really is root — a `sudo` run, or a
system where the udev rule plus `disk` group membership applies — nothing has changed; that path is
untouched and is still the fast one, with no D-Bus round trip.

Only when the direct open is refused with `EACCES`/`EPERM` does it fall back to **udisks2**
(`src/disk/linux/udisks2.cpp`). udisks2 is the system service every desktop already runs to mount removable
media. Its `org.freedesktop.UDisks2.Block.OpenDevice` method opens a block device and passes the
already-open file descriptor back over D-Bus, with polkit deciding whether to allow it — which for an
interactive desktop session means one authentication prompt.

Details that matter:

- **`O_EXCL` survives.** It is passed through the `flags` option `OpenDevice` accepts, so a write is still
  refused while a filesystem on the device is mounted. That guard is the one that holds even when
  unmounting silently failed, so losing it would have been unacceptable.
- **`O_DIRECT` cannot be passed that way**, so it is set afterwards with `fcntl(F_SETFL)`, which Linux
  permits for exactly that flag. A transfer through udisks2 still bypasses the page cache, so it neither
  evicts the user's working set nor reports itself finished while data is still draining to the card.
- **Unmounting goes the same way.** `VolumeControl` calls `org.freedesktop.UDisks2.Filesystem.Unmount`
  when not root, which unmounts as the owning desktop session rather than as root and keeps that session's
  bookkeeping intact. `umount2()` and the `umount` helper are still used when root.
- `DeviceInfo::mountedDevicePaths` exists for this: udisks2 unmounts a named partition object, not a mount
  point, so the enumerator records which partition each mount belongs to.

## What the UI asks

The pages gate on `App.deviceAccessAvailable` rather than on elevation. It answers "can a device be opened
at all" — either this process is root, or `RawDevice::canOpenWithoutElevation()` reports a broker that can
open one for us. The rights banner and the "restart with rights" button therefore appear only where they
would actually help; on an ordinary desktop with udisks2 there is nothing to restart for.

`Privileges::isElevated()` still means exactly what it says and still drives the header badge, which reads
ADMINISTRATOR when root and DEVICE ACCESS when udisks2 is doing the opening.

## macOS and Windows

Neither has an equivalent broker, so `RawDevice::canOpenWithoutElevation()` returns false on both and the
elevation path is unchanged there. On macOS `/dev/rdiskN` needs the process itself to be root; Windows
needs an elevated Administrators token.

## How this was checked

udisks2 on a normal Ubuntu desktop was asked to open a loop device as an unprivileged user, through a Qt
D-Bus client. Directly `open()`ing the same device failed with `Permission denied`, while `OpenDevice`
returned `org.freedesktop.UDisks2.Error.NotAuthorizedCanObtain` — polkit stating that the caller *may*
authorise the operation rather than refusing it. With a polkit agent present (every desktop session has
one) that becomes a prompt and then a usable descriptor.

The call blocks while that prompt is on screen, which is why `udisks2.cpp` raises the D-Bus reply timeout
well above the default 25 seconds, and why it must not run on the GUI thread — it runs on
`imaging_thread`, like the rest of the transfer.
