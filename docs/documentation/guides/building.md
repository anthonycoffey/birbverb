# Building birbverb

## Requirements

- CMake ≥ 3.22
- A C++20 toolchain
  - **Windows** — Visual Studio 2022 (MSVC 19.3x) or clang-cl
  - **macOS** — Xcode 14 or newer
  - **Linux** — GCC 12+ or Clang 15+, plus the JUCE Linux dependencies
- Git (JUCE and Catch2 are fetched at configure time)

Nothing else to install. There is no package manager step, no SDK to download, and no
asset fetch.

## Configure and build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

The first configure clones JUCE (shallow, tag 8.0.14) and Catch2 (tag v3.7.1), so it takes
a few minutes. Later configures are fast.

Artifacts land in `build/birbverb_artefacts/`. `COPY_PLUGIN_AFTER_BUILD` is on, so the
VST3 — and the AU on macOS — are also installed to your user plugin folders.

### Formats per platform

| Platform | Formats |
| --- | --- |
| Windows | VST3, Standalone |
| macOS | VST3, Standalone, AU |
| Linux | VST3, Standalone |

AU is added only on Apple. Asking for it elsewhere fails the configure step, which is why
`CMakeLists.txt` builds the format list conditionally rather than hard-coding it.

### Build types

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug            # assertions on, no optimisation
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo   # the day-to-day default
cmake -B build -DCMAKE_BUILD_TYPE=Release          # for release builds
```

Use `RelWithDebInfo` for development. `Debug` builds of a JUCE plugin are slow enough that
real-time audio can glitch, which makes listening tests misleading.

### Options

| Option | Default | Effect |
| --- | --- | --- |
| `BIRBVERB_BUILD_TESTS` | `ON` | Builds `birbverb_tests` and registers CTest entries |
| `BIRBVERB_BUILD_TOOLS` | `ON` | Builds `bv_render`, the offline renderer |

```bash
cmake -B build -DBIRBVERB_BUILD_TESTS=OFF -DBIRBVERB_BUILD_TOOLS=OFF
```

### macOS architectures

Universal (arm64 + x86_64) by default. For a faster local build:

```bash
cmake -B build -DCMAKE_OSX_ARCHITECTURES=arm64
```

## Targets

```bash
cmake --build build --target birbverb_VST3 -j        # just the VST3
cmake --build build --target birbverb_Standalone -j  # just the standalone app
cmake --build build --target birbverb_tests -j       # the offline DSP suite
cmake --build build --target bv_render -j            # the offline renderer
```

`birbverb_tests` is the fast one and the one you want in a TDD loop — it does not link
JUCE at all, by design
([ADR-004](../../specs/adrs/ADR-004-catch2-for-dsp-tests.md)).

## Test

```bash
ctest --test-dir build --output-on-failure              # the implemented suite
ctest --test-dir build -L red --output-on-failure       # tests ahead of their code
./build/birbverb_tests "[measure]"                      # one tag
./build/birbverb_tests --list-tests                     # what exists
```

See [testing-dsp](testing-dsp.md) for the conventions.

## Validate

```bash
# macOS Audio Unit
auval -v aufx Bvrb Ynme

# Any platform, via pluginval (https://github.com/Tracktion/pluginval)
pluginval --strictness-level 10 --validate build/birbverb_artefacts/*/VST3/birbverb.vst3
```

Both are release checks, not part of the TDD loop.

## Sprite assets

`CMakeLists.txt` globs `Assets/sprites/*.png` and compiles whatever it finds into a
`BirbAssets` target. No PNGs are committed yet, so only `sprites.json` goes in and every
control falls back to procedural drawing.

A glob means **CMake does not notice a newly added PNG until you re-run configure**:

```bash
cmake -B build      # after adding or removing a sprite sheet
cmake --build build -j
```

## Troubleshooting

**Configure fails cloning JUCE or Catch2** — network or proxy. Both are plain
`git clone --depth 1`; if your environment needs a proxy, set `https_proxy` before
configuring.

**`BinaryData.h: No such file`** — the `BirbAssets` target did not get created, which
means the glob found nothing. `Assets/sprites/sprites.json` must exist. The
`BIRBVERB_HAS_ASSETS` define guards every include of that header, so this should not
happen; if it does, re-run configure.

**Plugin not appearing in a host** — check the host rescanned, and check
`build/birbverb_artefacts/` actually contains a bundle. On macOS an unsigned plugin may
need to be opened once from Finder.

**Audio glitching in a `Debug` build** — expected. Use `RelWithDebInfo`.

**Tests fail with wildly wrong measurements** — check you did not enable fast-math. The
test target sets `/fp:precise` (MSVC) or `-fno-fast-math` deliberately, because
measurements have to be deterministic
([SPEC-006](../../specs/active/SPEC-006-offline-dsp-test-harness.md) requirement 4).
