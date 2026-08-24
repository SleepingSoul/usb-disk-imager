# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Needs CMake ≥ 3.21, a C++17 compiler and Qt 6.8.3+. xxHash and spdlog are fetched at configure time.

The required Qt version is stated once, as `UDI_QT_MINIMUM_VERSION` in the root `CMakeLists.txt`, and
`find_package` rejects anything older. `CMakePresets.json` carries a debug and a release preset per
platform, each pointing `CMAKE_PREFIX_PATH` at that platform's 6.8.3 kit, and `scripts/run-windows.bat`
prefers the same kit over a newer one; all three have to move together when the version does.

```bash
cmake --preset windows-debug            # or linux-debug, macos-debug
cmake --build build/debug
```

Windows/MSVC needs the compiler environment first (`vcvars64.bat`), because the presets generate Ninja
build files. Without a preset, the kit is passed by hand:

```bash
cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=<Qt kit prefix>
cmake --build build/debug -j
```

Options: `-DUDI_WARNINGS_AS_ERRORS=ON` (on in CI), `-DUDI_USE_SYSTEM_XXHASH=ON`,
`-DUDI_USE_SYSTEM_SPDLOG=ON`. Translations are rescanned with
`cmake --build <dir> --target update_translations`; run it on every platform before a release, because
only the current platform's sources are compiled and therefore scanned.

**Platforms**: Windows 11, Ubuntu/Debian x86-64 and macOS. Anything the platforms disagree on lives behind
one of five headers — `disk/deviceenumerator.hpp`, `disk/rawdevice.hpp`, `disk/volumecontrol.hpp`,
`disk/filesystemresizer.hpp`, `utils/privileges.hpp` — implemented once per platform under
`src/disk/{windows,linux,macos,unix}/` and `src/utils/{windows,unix}/`. Nothing outside those directories
may contain a platform `#ifdef`. `disk/filesystemresizer.hpp` is asked per operation
(`isSupported(Operation)`), because Windows can shrink but not grow — see below.

## Architecture

The app is a `QGuiApplication` subclass (`Application`) owning a set of **managers**. Each manager is one
subsystem, is constructed on the main thread, and is moved to its own `QThread` before `postInit()` runs.

```
Application
├── LoggingManager       (main thread — installs the Qt message handler first)
├── LocalizationManager  (main thread — translators must be in place before the first qsTr())
├── DeviceManager        (device_scan_thread)
├── ImagingManager       (imaging_thread)
└── UiManager            (main thread — QML engine and view models)
```

**Manager template** (`src/app/manager.hpp`): `Manager<TManager, TConfig>` loads `TConfig` from
`configs/<name>.json` and `qFatal`s if any field is missing, so the app can never run on a half-loaded
config. `getManager<T>()` reaches a manager from anywhere; the registry behind it is mutex-guarded because
managers on worker threads deregister themselves as those threads unwind.

**Configs** are looked up next to the executable first (`configs/<file>.json`) and fall back to the copy
baked into the binary's resources, so a deployed build can be retuned without a rebuild.

**Device scanning** (`src/disk/`): `DeviceManager` re-enumerates on a timer on its own thread. Two rules
hold in every backend and are the whole reason this app coexists with VeraCrypt: a volume is never opened
before it has been identified as a partition of a physical disk (on Windows, via `QueryDosDevice` and a
`\Device\HarddiskVolume*` match — no handle involved), and enumeration only ever asks for metadata, never
for read access to media. Breaking either one reintroduces the hang this project exists to avoid.

**Imaging** (`src/imaging/`): `ImagingManager::start()` runs an `ImagingJob` synchronously and blocks
`imaging_thread` for the whole transfer. Cancellation is therefore an `std::atomic_bool` the loop checks
between chunks — `ImagingManager::requestCancel()` is deliberately *not* a slot, and is the one sanctioned
place where a manager method is called across threads. Progress is throttled
(`progress_interval_ms`) and queued to the GUI thread.

**Trimmed reads**: `PartitionTable::planTrim()` decides where an image may stop. For GPT it also rebuilds
the primary header, the entry-array copy and the secondary header for the shorter image, with fresh
CRC32s, and the primary header is patched into the first chunk *before* it is hashed and written so the
reported digest is a digest of the file that was produced.

**Filesystem shrink/grow**: unlike a trim, this changes an actual partition's extent, so
`PartitionTable::planShrinkLastPartition()`/`planGrowLastPartition()` additionally patch the one entry
describing it — for GPT that means both copies of the entry array (`GptTrimTrailer::
patchedPrimaryEntryArray` alongside the copy the trailer already carries) get rewritten with a fresh CRC32,
not just the header, since an unpatched primary copy fails GPT's own consistency check even though the
header still parses. `FilesystemResizer` decides the new size by shelling out to e2fsck/resize2fs, and a
resize is not cancellable once started — killing either mid-run risks a half-migrated filesystem, which is
worse than any wait, so those tools are waited on without a deadline and `ImagingViewModel::getCancellable()`
disables the UI's Cancel for their stages. How the tools reach the filesystem differs per platform: a loop
device over the byte range (Linux), an attached image (macOS), or — since Windows has neither e2fsprogs nor
any way to map a byte range of a file — a copy of the partition into a temporary file that e2fsck and
resize2fs can open directly, run through WSL when it is installed. That copy is why Windows offers shrink
but not grow, which would need a temporary the size of the whole destination. See
docs/filesystem-shrink-grow.md.

**QML layer**: `UiManager` registers the C++ singletons imperatively into the `usbdiskimager.qml` module
(`App`, `Devices`, `Imager`, `Localization`) and loads `qrc:/content/App.qml`. QML files live in their own
module (`content/`, URI `content`, resource prefix `/`). View models expose pre-formatted strings and
primitives — never enums — so a binding never computes and the boundary stays cheap. `m_engine` is
declared *last* in `UiManager` so it is destroyed first: the QML objects it owns hold bindings to the view
models above it.

**Design Studio**: `usb-disk-imager.qmlproject` loads `content/` from disk, which is why `content/qmldir`
(the `Style` and `Icons` singleton declarations, generated into the build tree for the compiled module) and
the QML stand-ins under `designer/usbdiskimager/qml/` exist. A view model that gains a property, a method
or a model role needs the same one on its stand-in, or the designer stops rendering whatever binds to it.
Neither directory is referenced by any `CMakeLists.txt`.

**Localization**: sources are English. `LocalizationManager` installs the catalogues, and a language
change re-runs `QQmlApplicationEngine::retranslate()` plus `IViewModel::onLanguageChanged()` on every view
model, because a string a view model translated itself has to be rebuilt.

## Code Conventions

**Namespace**: all production code lives in `UDI`.

**Files**: lowercase, no separators (`devicelistmodel.cpp`, `imagingtypes.hpp`). Headers are `.hpp`.
Includes are angle-bracket style relative to `src/` (`#include <disk/rawdevice.hpp>`), never quote style.

**Naming**: classes `PascalCase`; members `m_` prefixed; methods `camelCase`; getters `get`-prefixed; Qt
slot handlers `on` + signal name; protocol constants scoped in a `namespace` or `struct` (`Gpt::MyLbaOffset`).
Prefer long descriptive names over abbreviations, and normal casing for acronyms (`Usb`, `Crc`, `Gpt`).

**Class design**: `Q_DISABLE_COPY_MOVE` on managers and other heavy Qt classes; `std::unique_ptr` for
owned heap objects, `QPointer` for non-owning Qt references. `QT_NO_KEYWORDS` is on, so use `Q_SIGNALS`,
`Q_SLOTS` and `Q_EMIT`.

**Config classes — no default member initializers.** Every field is loaded from JSON and the manager
`qFatal`s if one fails, so a default would mask a missing key behind a plausible value.

**Indentation**: 4 spaces, never a tab byte — not in C++, QML or JSON.

**Braces**: Allman, always on their own line, including lambdas. A split signature indents continuations
by exactly 4 spaces; aligning to the opening parenthesis is forbidden. A lambda passed to a call keeps its
introducer on the call line and its brace at the statement's indent.

**Comments**: only where the *why* is non-obvious (hardware quirks, on-disk format edge cases, threading
and lifetime subtleties). Never restate what the code does, never label a branch, never add section
banners. If a competent reader of the language would understand the line without it, the comment is
forbidden.

**Modern C++17**: structured bindings, `std::optional`, `std::array`, `if constexpr`. Never a C-style
cast — `static_cast` for numeric/hierarchy conversions, `reinterpret_cast` for memory reinterpretation.
Always qualify standard names with `std::`.

**Containers**: prefer STL over Qt containers unless the Qt one integrates meaningfully — `QByteArray` for
raw byte I/O, `QStringList` where a signal or QML property wants one.

**Strings and buffers**: `std::uint8_t` for raw byte buffers, never `char`. `QByteArray` for non-localized
identifiers (device paths, digests); `QString` only for user-visible text. `QLatin1StringView` for ASCII
literals — never for one containing a multi-byte character, which Latin-1 would render as mojibake.

**`auto`**, **`const`**, **`std::move`**: `auto` for almost every deduction; everything that can be `const`
is; `std::move` wherever a copy would otherwise happen.

**No exceptions.** Failures are returned (`std::optional`, `bool` + an out-parameter message) and logged.
`qFatal` only where continuing would corrupt state.

**Logging**: `qDebug` diagnostics, `qInfo` production-worthy runtime facts, `qWarning` unexpected but
recoverable, `qCritical` errors survived, `qFatal` unrecoverable. Everything reaches a rotating file
through `LoggingManager`.

**C++ ↔ QML**: minimise the rate at which data crosses. Batch related property changes behind one signal
(`progressChanged`, `resultChanged`) rather than emitting per field.

**Post-change self-review (mandatory)**: after finishing a change and before calling it done, re-read every
file touched and audit it against this document — comments, naming, `m_`, Allman, `std::` qualification,
container choice, buffer typing, and any subsystem rule involved. Fix what the audit finds in the same
turn.

## Dependencies

| Library | Purpose |
|---------|---------|
| Qt 6.8.3+ (Core, Gui, Qml, Quick, QuickControls2, Svg) | Application framework, QML UI, translations |
| xxHash (v0.8.3, FetchContent) | XXH3 digests of transfers |
| spdlog (v1.15.1, FetchContent) | Rotating log files behind the Qt message handler |
