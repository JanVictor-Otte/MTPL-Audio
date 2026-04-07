# MTPL-Audio Canonical

This folder is a clean MTPL-Audio rebuild on top of the latest upstream MTPL C++ core.

It combines:

- the current upstream MTPL core from GitHub
- the local `mtpl_audio` layer from your newer audio work
- the upstream MTPL core tests
- the current audio tests and sound assets used by the clickscape and soundscape examples

## Layout

- `src/mtpl/` — latest upstream MTPL C++ headers
- `src/mtpl_audio/` — audio domain types, generators, mixer, scheduler, and platform backends
- `tests/` — upstream MTPL tests plus current audio tests
- `soundfiles/` — assets used by the audio demos/tests

## Targets

- `mtpl` — upstream MTPL header-only core
- `mtpl_audio` — header-only audio layer built on top of `mtpl`

## Entry Points

- `#include "mtpl/mtpl.hpp"` for core MTPL only
- `#include "mtpl_audio/mtpl_audio.hpp"` for the full audio package

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

## Current Coverage

- Core MTPL tests: `test_mtpl_core`, `test_signal_transform`, `test_immutability`, `test_lane_signal_sink`
- Audio tests: `test_audio`, `test_import`
- Apple-only automated runtime test: `test_core_audio`
- Apple-only manual demos: `test_soundscape`, `test_clickscape`

The Linux, Windows, and Android platform headers are preserved in `src/mtpl_audio/platform/`, but there are not yet dedicated platform test binaries for them in this folder.
