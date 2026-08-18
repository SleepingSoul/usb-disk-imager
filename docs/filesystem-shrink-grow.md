# Cross-platform filesystem shrink/grow — design notes

Goal: given a raw disk image whose last partition is an ext4 filesystem sized to fill a large card (the
common shape of a Raspberry Pi OS image after it has auto-expanded on first boot), shrink that filesystem
and the image file down to roughly what is actually used, then optionally grow it back out after writing
the shrunk image to a new, possibly larger, card. This is the same thing the community `PiShrink` script
does on Linux; the ask here is to make it work from this app on Windows, Linux and macOS.

**Status**: implemented and tested on Linux (both MBR and GPT, shrink and grow, verified end to end with
real `mkfs.ext4`/`sgdisk`/`e2fsck`/mount round-trips — see "How this was tested" below). Implemented on
macOS per the plan below but **not tested on real hardware** — there is no Mac available in the
environment this was built in; treat it as best-effort until someone verifies it on an actual Mac.
**Shrink** is also implemented on Windows, through WSL, when WSL and e2fsprogs are present; it is likewise
untested, for the same reason. **Grow** remains unavailable on Windows and its option stays greyed out
with an explanation. See "Windows, through WSL" below.

## This is not the same problem as trimming trailing zeros

The app already has a mechanism that looks superficially similar — `TrimMode` in `ImagingJob` drops
trailing zero bytes or everything past the last partition when *reading* a device into an image. That
works because those bytes carry no meaning: nothing addresses them.

Shrinking a filesystem is different in kind, not degree. A Raspberry Pi OS image's root partition is
usually already sized to fill the *nominal* card capacity — often because the OS itself auto-expands the
partition on first boot — so there is no long run of trailing zero bytes to detect; the used blocks are
scattered near the front of an otherwise mostly-empty but *fully allocated* filesystem. Shrinking it
requires understanding the ext4 on-disk format well enough to relocate any blocks that lie beyond the new
end point, shrink the free space bitmaps and block group descriptors, and rewrite the superblock — exactly
what `resize2fs` does. Byte-scanning cannot substitute for that.

## License shapes the architecture more than platform does

e2fsprogs (`resize2fs`, `e2fsck`, `libext2fs`) is GPLv2. This project is MIT. Statically linking
`libext2fs` into the binary would put the whole app under the GPL's terms.

Shelling out to the standalone `resize2fs`/`e2fsck` binaries as separate processes avoids that — mere
process invocation is the same "arm's length" pattern the FSF itself treats as outside the linking clause,
and it's the pattern this codebase already uses (`VolumeControl` on Linux shells out to `umount` via
`QProcess` rather than linking against anything that understands mount tables). This rules out vendoring
`libext2fs` via `FetchContent` the way xxHash and spdlog are vendored — that route is closed by the license,
not by engineering difficulty.

Decision: shell out to `resize2fs`/`e2fsck` as separate processes, never link against `libext2fs`.

As implemented, those are the *separately installed* system tools, not binaries this project ships: the
Linux backend resolves them on `PATH` (e2fsprogs is effectively always present there), and the macOS
backend checks `PATH` plus Homebrew's keg-only install prefixes. Nothing is bundled today, and both
backends report themselves unsupported — greying the option out with an install hint rather than failing
mid-run — when the tools are missing. Bundling them, which the packaging section below describes, is still
the right move before shipping to macOS users who cannot be assumed to have Homebrew, but it is future
work rather than something this implementation relies on.

## The actual hard problem: exposing "partition N of this .img" to the tool

`resize2fs`/`e2fsck` expect a device or file that *is* the filesystem starting at byte 0. Our ext4
filesystem starts at some offset inside a bigger raw image file. Bridging that gap is where the three
platforms genuinely diverge:

| Platform | Mechanism | Notes |
|---|---|---|
| Linux | `losetup -o <offset> --sizelimit <size> /dev/loopN` | Zero-copy — the loop device maps directly onto the underlying file's bytes. This is what PiShrink itself uses. e2fsprogs is virtually always already installed. |
| macOS | `hdiutil attach -nomount` on the whole image | macOS parses the image's own partition map and exposes each partition as its own `/dev/diskNsM` node, without root. Needs bundled static `resize2fs`/`e2fsck` binaries since macOS ships none. |
| Windows | No equivalent primitive — copy the partition out instead | Nothing in the platform exposes "byte range of a file" as a device node without a third-party driver (which is its own risk: kernel driver signing, install-time prompts). So: copy the partition's byte range out to a temp file, run the tools against that standalone file, copy the shrunk result back, rewrite the partition table entry, truncate. This is what is implemented, with WSL hosting the tools. |

The Windows fallback costs extra scratch-disk I/O (a copy-out of the *pre-shrink* partition size, which
could be tens of GB) and temporary disk space that Linux/macOS avoid via zero-copy loop/attach access. It
is still correct, just slower and more disk-hungry there specifically.

Note the copy-out/splice-back mechanics are not new to this codebase: `PartitionTable`'s GPT-rebuild logic
(used by the existing read-trim feature) already rewrites partition table structures by hand for a
resized image. The Windows path reuses that same kind of surgery.

## Considered: a container or VM instead of platform-specific tricks

Since this feature only ever operates on an image *file* — never a live device — it's tempting to sidestep
all three platform-specific mechanisms above by bind-mounting the file into a Linux container or VM and
running real `losetup` + `resize2fs` inside it, identically on every host OS. No raw-device passthrough is
needed, which is what would normally make containerizing a disk tool awkward.

Rejected as a general dependency. This app's audience is people flashing an SD card, not developers with a
container runtime already running. Requiring Docker (or bundling a hypervisor and a Linux rootfs
ourselves) trades a modest, self-contained problem — a couple of small static binaries plus an
OS-specific file-exposure trick — for a much heavier one: a multi-GB install, a background daemon,
BIOS/virtualization prerequisites, per-VM startup latency, and, for Docker Desktop specifically, its
commercial licensing terms. Building our own minimal VM instead of depending on Docker avoids the
licensing and daemon issues but is its own large, ongoing undertaking — bundling a kernel and rootfs,
picking a hypervisor per platform (WHPX/Hyper-V on Windows, `Virtualization.framework` on macOS, KVM on
Linux, where it's redundant anyway), and maintaining all of it — closer to building a small Docker
Desktop than to shipping a disk-imaging feature.

One targeted exception: **WSL on Windows**, which is what the Windows backend now uses. Unlike Docker,
it's a Microsoft-native feature many Windows installs already have enabled, not a separate multi-GB
download, and its default distribution ships e2fsprogs outright — no image to pull, no daemon, no network.

Docker was considered as a second option and deliberately left out. Docker Desktop for Windows runs *on*
WSL2 in its default configuration, so a machine with Docker almost always already has WSL — supporting WSL
covers nearly every machine Docker would have. On top of that Docker needs a running daemon and an image
that actually contains e2fsprogs, which means a network pull on first use. It is strictly more moving parts
for strictly fewer machines. If it is ever wanted anyway, it slots in beside `runInWsl()` as another way to
execute the same three commands.

macOS has no equivalent already-there primitive, so it doesn't get an analogous shortcut.

## Considered: writing our own ext4 shrink logic

Also considered, specifically to drop the Windows dependency on WSL2/bundled binaries entirely: parse and
manipulate ext4 ourselves, natively in this codebase, the way `PartitionTable` already does for MBR/GPT.

Rejected. Reading ext4's on-disk structures is tractable — the format is well documented, and permissively
licensed implementations exist (`lwext4`, MIT) for exactly that. But shrinking is not a parsing problem, it's
a *relocation* problem: any data block, inode, directory block, or extended-attribute block that lands
beyond the new boundary has to be moved and every extent-tree pointer to it rewritten; the resize inode and
possibly the journal have to be handled; and the block bitmaps, inode bitmaps, block group descriptor
table and superblock all have to be resized and rewritten — while keeping the `metadata_csum` checksums
that cover nearly every one of those structures correct, across whichever combination of ext4 feature
flags (64bit, flex_bg, meta_bg, bigalloc, uninit_bg, ...) the source image happens to use.

That's precisely what `resize2fs`/`libext2fs` encapsulate — on the order of two decades of hardening
against exactly those combinations. Reimplementing it is a multi-month-plus effort with a much worse
failure mode than "the feature doesn't ship": a bug in relocation or checksum logic doesn't fail loudly,
it silently corrupts the user's OS image. Existing permissive ext4 libraries like `lwext4` don't remove
this burden either — they're built for mounting/reading/writing files, not filesystem-level resize, so the
one operation that actually matters here still wouldn't be covered.

This is why the WSL2-or-bundled-binary plan above is the right tradeoff despite the extra dependency: it
reuses a battle-tested implementation instead of re-deriving its correctness, which matters much more here
than it would for a feature whose failure mode is merely "didn't work."

## Implemented shape

One header, three platform backends, matching the existing convention (`disk/deviceenumerator.hpp`,
`disk/rawdevice.hpp`, `disk/volumecontrol.hpp`, `utils/privileges.hpp`, each with implementations under
`src/disk/{windows,linux,macos}/`):

```
disk/filesystemresizer.hpp        — interface: shrinkFilesystem(path, partition) -> new sector count; growFilesystem(path, partition)
disk/linux/filesystemresizer.cpp  — losetup + system e2fsck/resize2fs/dumpe2fs (implemented, tested)
disk/macos/filesystemresizer.cpp  — hdiutil attach + Homebrew e2fsck/resize2fs/dumpe2fs (implemented, untested — no Mac available)
disk/windows/filesystemresizer.cpp — copy partition out + wsl.exe e2fsck/resize2fs/dumpe2fs; shrink only (implemented, untested — no Windows machine available)
```

### Windows, through WSL

The one assumption that does **not** survive contact with WSL is loop devices. A file under `/mnt/c` is
reached over the 9p protocol, and `losetup` cannot map it — so the plan of "run the Linux backend verbatim
inside WSL" does not work. What does work is that `e2fsck` and `resize2fs` operate perfectly well on a
*plain file* whose first byte is the filesystem's first byte, with no loop device involved at all. So the
Windows backend copies the partition's byte range out to a temporary file, runs the three tools against it
through `wsl.exe -e`, reads the new size from `dumpe2fs`, and copies just the shrunk result back before the
caller rewrites the partition table and truncates.

Consequences worth knowing:

- It costs a pass over the partition in each direction, plus temporary space for it. That is the price of
  Windows lacking the primitive the other two platforms have.
- **Grow is not offered on Windows.** Growing needs `resize2fs` to write group descriptors across the whole
  new extent, so the temporary file would have to be the size of the *destination device*, with a full copy
  back — a much worse trade than the shrink case, where the copy back is only as large as the shrunk
  result. `FilesystemResizer::isSupported()` is therefore asked per `Operation`, and the Write page's grow
  option stays greyed out on Windows with that explanation.
- Detection is one `wsl.exe -e sh -c "command -v e2fsck && …"` call, which answers "WSL present", "a
  distribution is installed and can start" and "e2fsprogs is in it" together, and is cached for the
  process lifetime. If it fails, the checkbox is greyed out with the reason rather than failing mid-run.
- `wsl.exe` reports its *own* failures in UTF-16 while Linux programs print UTF-8, so output decoding
  sniffs for interleaved NULs. Windows paths are translated with `wslpath` rather than by assembling
  `/mnt/<drive>/…` by hand, since that mapping is user-configurable.

`PartitionTable` gained `getLastPartition()`, `planShrinkLastPartition(newSectorCount)` and
`planGrowLastPartition(targetSizeBytes, alignmentBytes)`, sharing a `resizeLastPartitionTo()` implementation
that dispatches to a GPT or MBR-specific rewrite. Both directions reuse `TrimPlan`/`GptTrimTrailer` — the
same shape `planTrim()` already produced for the read-trim feature — with one addition:
`GptTrimTrailer::patchedPrimaryEntryArray`. A trim never changes any partition entry's contents, so the
*primary* copy of the entry array near the start of the disk stays valid on its own and only the backup
copy at the end needs to move. A resize changes one entry's extent, so both copies need rewriting with a
fresh CRC32 — missing this was the actual bug the testing below caught.

**Shrink** runs as an optional final phase of a read (`shrinkFilesystemAfterRead`), operating on the image
file after it has been written: parse the file's own partition table, identify the last partition,
confirm it looks like ext2/3/4 (superblock magic `0xEF53`), call `FilesystemResizer::shrinkFilesystem()`,
then rewrite the partition table and truncate the file to match. The digest reported to the user is
recomputed from the shrunk file afterward, so it still describes the file that was actually produced.

**Grow** runs as an optional phase *before* a write starts (`growFilesystemToFillDevice`), not after, and
operates on the image file rather than the live destination device. The original plan (grow directly on
the device after writing) turned out to conflict with this app's own exclusive hold on the device — Linux
`O_EXCL` on the write handle would make `losetup` unable to attach the same device — so instead the image
file itself is grown to the destination's size first (same file-based machinery as shrink, safe to reuse),
and the normal write loop that follows just copies a larger file, with no separate device-level code path
needed.

## How this was tested

No physical SD card or root access was available in the environment this was built in, so verification
used loop devices inside a privileged Docker container instead, exercising the *actual* `PartitionTable`
and `FilesystemResizer` source files (compiled directly, unmodified, against the project's real Qt 6.8.3
kit) rather than a reimplementation of the logic:

1. Built both an MBR image (boot + ext4 root, mirroring a real Raspberry Pi OS layout) and a GPT image,
   each with a real `mkfs.ext4` filesystem populated with test files, via `losetup --offset`.
2. Ran the real shrink code path against each, then independently verified the result with `sgdisk -v`
   (GPT) / `parted print` (MBR) and by re-mounting and `e2fsck -f` plus a SHA-256 comparison of the test
   files.
3. Repeated for grow, simulating writing the shrunk image onto a larger destination.

This caught one real bug before it shipped: the first version of `planGptResize()` patched the primary
header's metadata and CRC field, and wrote a correct *backup* entry array, but never rewrote the *primary*
entry array itself. `sgdisk -v` failed immediately with "Main partition table CRC mismatch!" and had to
fall back to the backup table — exactly the kind of failure that mount and `e2fsck` tolerate silently
(they accept the backup table without complaint) but a stricter GPT consumer might not. Fixed by adding
`GptTrimTrailer::patchedPrimaryEntryArray` (see above); re-running the same test afterward showed
`sgdisk -v` reporting "No problems found" on both the shrink and grow outputs.

All four combinations (MBR/GPT × shrink/grow) passed after the fix: valid partition tables per
`sgdisk`/`parted`, clean `e2fsck`, and byte-identical file contents before and after.

`PartitionTable` already identifies the last partition and its extent; this reuses that rather than
re-parsing.

### Shrink algorithm (mirrors PiShrink)

1. Identify the last partition and confirm it's ext-family (superblock magic `0xEF53` at
   `partition_offset + 1024`).
2. Expose it to the tools per the table above.
3. `e2fsck -f -y` (a resize refuses to run against a filesystem it hasn't just checked cleanly).
4. `resize2fs -M` (shrink to the tool's own computed minimum).
5. Rewrite the partition table entry to the new, smaller partition size (extending `PartitionTable`'s
   existing rebuild logic, which today only ever grows the *image*, not the partition record itself, to
   also patch a partition's size field in place).
6. Truncate the image file to the new total size.

### Grow, and whether it's even needed

Growing to fill a bigger card is the easy direction — `resize2fs` with no target size fills whatever
partition currently contains it. But most stock Raspberry Pi OS images already auto-expand the root
filesystem on first boot for exactly this reason: PiShrink-style shrinking is designed to be paired with
that auto-expand, not with the imaging tool doing the growing itself. So this stayed an *optional* toggle
rather than automatic — useful for images that don't auto-expand, or for someone who wants a fully-ready
card without a first-boot resize/reboot cycle, but not needed for a stock Pi OS image.

The toggle grows the image *before* the write rather than the device after it — see "Implemented shape"
above for why the device-side ordering this section originally assumed does not work here.

### Packaging impact

Bundling third-party binaries per platform touches `packaging/` on all three platforms (Windows and macOS
need e2fsprogs binaries added to the installer payload that don't exist there today; Linux can likely
depend on the system package instead of bundling, since e2fsprogs is almost always already present there).
This should be scoped as part of the estimate, not discovered during implementation.

## Suggested phasing

This is substantially bigger than a single change — new platform-specific backends, bundled third-party
binaries per platform, and packaging changes on all three. Recommend building the Linux path first (the
`losetup` + `resize2fs`/`e2fsck` case) as a working vertical slice, since it's the simplest mechanism, the
dev machine is already Linux, and it validates the `PartitionTable` extensions and the shrink algorithm
before taking on macOS's `hdiutil` path and Windows' copy-out fallback.
