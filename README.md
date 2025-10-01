# qlean-mini

Lean Ethereum consensus client for devnets, implemented in modern C++.

qlean-mini follows the Lean Ethereum devnet specification:
- Spec: https://github.com/leanEthereum/leanSpec/

This repository builds a small, modular node binary called `qlean` and a set of loadable modules. It uses CMake, Ninja, and vcpkg (manifest mode) for dependency management.


## Quick start (macOS, zsh)

Prerequisites:
- macOS (Apple Silicon is supported; the build sets `CMAKE_OSX_ARCHITECTURES=arm64` for executables)
- CMake ≥ 3.25
- Ninja
- A C++23-capable compiler (AppleClang/Clang)
- Python 3 (required by the CMake build)
- vcpkg (manifest mode)

Install prerequisites using Homebrew (optional):

```zsh
brew install cmake ninja python
```

### 1) Install and bootstrap vcpkg

```zsh
# Clone vcpkg somewhere on your machine (e.g. in your home directory)
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"

# Bootstrap vcpkg
"$HOME/vcpkg"/bootstrap-vcpkg.sh

# Export VCPKG_ROOT for the current shell session (and add to ~/.zshrc for convenience)
export VCPKG_ROOT="$HOME/vcpkg"
```

Notes:
- This project uses vcpkg in manifest mode with custom overlay ports from `vcpkg-overlay/` and the registry settings in `vcpkg-configuration.json`.
- CMake presets expect `CMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`.

### 2) Configure and build

Use the provided CMake preset (generator: Ninja):

```zsh
# From the repository root
cmake --preset default
cmake --build --preset default -j
```

This will:
- Configure the project into `./build/`
- Build the main node executable at `./build/src/executable/qlean`

If you prefer manual configuration without presets:

```zsh
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_OVERLAY_PORTS="$(pwd)/vcpkg-overlay"
cmake --build build -j
```


## Run the node

The node reads its settings from a YAML config. A sample config is provided in `example/config.yaml`.

Important paths and rules derived from the code:
- `base_path` must be an absolute path and must exist.
- The process changes its working directory to `base_path` at startup.
- `modules_dir` and `spec_file` are interpreted relative to `base_path` when given as relative paths.

A convenient setup is to use the repository root as `base_path`. For a quick local run:

1) Copy and adjust the example config (or use it directly by passing an absolute path). For repository-root `base_path`, set these fields:
   - `general.base_path`: Absolute path to this repo, e.g. `/Users/you/dev/qlean-mini`
   - `general.modules_dir`: `modules_dir` (this repository contains prebuilt modules in that folder)
   - `general.spec_file`: `data/jamduna-spec.json` (or `example/jamduna-spec.json`)

2) Run the binary with your config:

```zsh
# Absolute path to the config file is recommended
./build/src/executable/qlean --config "$(pwd)/example/config.yaml"
```

You can also inspect CLI help and version:

```zsh
./build/src/executable/qlean --help
./build/src/executable/qlean --version
```


## Generate a node key

The binary includes a helper subcommand to generate a node key and corresponding PeerId:

```zsh
./build/src/executable/qlean key generate-node-key
```

This prints two lines:
1) The private key (hex)
2) The PeerId (base58)


## Tests

If `TESTING` is enabled (default ON in top-level CMakeLists), tests are built and can be run with CTest:

```zsh
# After building
ctest --test-dir build --output-on-failure
```

Individual test binaries are placed under `build/test_bin/`.


## Project layout (selected)

- `src/` — main sources (application, injector, modules, storage, etc.)
- `src/executable/` — entry points; the main target is `qlean`
- `modules_dir/` — sample dynamic modules used by the node
- `data/` — network/spec data (e.g. `jamduna-spec.json`)
- `example/config.yaml` — example node config
- `vcpkg.json` — vcpkg manifest with dependencies
- `vcpkg-configuration.json` — vcpkg registry config
- `vcpkg-overlay/` — overlay ports for custom packages


## Dependencies (via vcpkg)

Key libraries are pulled via vcpkg manifest mode, including (non-exhaustive):
- Boost (algorithm, filesystem, program_options, property_tree, random, DI)
- fmt
- soralog
- prometheus-cpp
- libp2p
- RocksDB + Snappy
- sszpp
- qtils
- cppcodec

The exact list is declared in `vcpkg.json` and overlayed via `vcpkg-overlay/`.


## Troubleshooting

- CMake cannot find the vcpkg toolchain file: Ensure `VCPKG_ROOT` is exported and points to your vcpkg clone, and rerun configure.
- Ninja not found: `brew install ninja`.
- On macOS, the build system may produce a loader executable to work around dyld limits; the output name remains `qlean` in `build/src/executable/`.
- `base_path` must be absolute and exist, otherwise the program will exit with an error.
- `modules_dir` must exist and contain the expected modules; when using the repo as `base_path`, set `modules_dir: modules_dir`.


## License

SPDX-License-Identifier: Apache-2.0 — Copyright Quadrivium LLC
