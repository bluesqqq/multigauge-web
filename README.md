<p align="center">
  <a href="https://www.multi-gauge.com/">
    <img src="./external/multigauge-core/assets/multigauge.png" alt="Multigauge" width="250" />
  </a>
</p>

<h1 align="center">Multigauge Web</h1>

## About

`multigauge-web` is the WebAssembly/browser target for [`multigauge-core`](https://github.com/bluesqqq/multigauge-core).

This repo builds the Multigauge runtime for the web and outputs the generated browser artifacts directly into the root [`wasm/`](./wasm/) folder.

## Quick Start

Get a web build running in a few minutes.

### 1. Clone the repo
```bash
git clone https://github.com/bluesqqq/multigauge-web.git
cd multigauge-web
git submodule update --init --recursive
```

### 2. Install prerequisites

You will need:

- [CMake](https://cmake.org/download/)
- [Ninja](https://ninja-build.org/)
- [Emscripten SDK (emsdk)](https://emscripten.org/docs/getting_started/downloads.html)

### 3. Make `emsdk` available

The build scripts look for `emsdk` in this order:

1. `EMSDK` environment variable
2. `./emsdk` inside this repo
3. `../emsdk` next to this repo

If your `emsdk` install lives somewhere else, set `EMSDK` first.

### 4. Build

#### Windows
```powershell
.\build.ps1
```

Release build:
```powershell
.\build.ps1 -Configuration Release
```

#### Linux
```bash
chmod +x ./build.sh
./build.sh
```

Release build:
```bash
./build.sh Release
```

### 5. Done

The generated artifacts will be written to [`wasm/`](./wasm/):

- [`wasm/multigauge.js`](./wasm/multigauge.js)
- [`wasm/multigauge.wasm`](./wasm/multigauge.wasm)
- [`wasm/multigauge.wasm.map`](./wasm/multigauge.wasm.map)

## Manual Build

If you prefer to build without the helper scripts, you can use the CMake presets directly:

```bash
cmake --preset wasm-debug
cmake --build --preset wasm-debug
```

Release:

```bash
cmake --preset wasm-release
cmake --build --preset wasm-release
```

## Project Layout

- [`src/`](./src/) contains the web target sources and Emscripten-specific platform bindings
- [`external/multigauge-core/`](./external/multigauge-core/) is the shared core renderer/editor library
- [`wasm/`](./wasm/) contains the generated browser build outputs
- [`CMakeLists.txt`](./CMakeLists.txt) defines the wasm target and output paths
- [`CMakePresets.json`](./CMakePresets.json) defines the standard debug and release configure/build presets

## Notes

- The build output is intentionally written into the repo root [`wasm/`](./wasm/) folder rather than the CMake build directory.
- The helper scripts are convenience wrappers around the same preset-based CMake flow used for manual builds.
- This repo currently targets the browser/WebAssembly build. Native desktop or server targets would need separate platform wiring.

## Status

`multigauge-web` is actively under development.

## License

This project is free to use for personal, educational, and non-commercial purposes.

You may not use this code in any product or service that is sold or monetized
without permission.

If you're unsure whether your use case is allowed, feel free to reach out.
