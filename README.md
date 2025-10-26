# qlean-mini

Lean Ethereum consensus client for devnets, implemented in modern C++.

qlean-mini follows the Lean Ethereum devnet specification:
- Spec: https://github.com/leanEthereum/leanSpec/

This repository builds a small, modular node binary called `qlean` and a set of loadable modules. It uses CMake, Ninja, and vcpkg (manifest mode) for dependency management.


## Quick start

### 0) Clone the repository and set system dependencies

```bash
git clone https://github.com/qdrvm/qlean-mini.git
cd qlean-mini
```

Prerequisites:
- CMake ≥ 3.25
- Ninja
- A C++23-capable compiler (AppleClang/Clang/GCC)
- Python 3 (required by the CMake build)
- vcpkg (manifest mode)

Note: Dependencies are also listed in the `.ci/.env` file for both macOS and Linux. To install on Debian Linux:

```bash
source .ci/.env
sudo apt update && sudo apt install -y $(echo $LINUX_PACKAGES)
```

Make sure gcc-$GCC_VERSION is default compiler (where GCC_VERSION is defined in .ci/.env):

```bash
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-$GCC_VERSION 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-$GCC_VERSION 100
```

For macOS:

```bash
source .ci/.env
brew install $(echo $MACOS_PACKAGES)
```

### 1) Install and bootstrap vcpkg

```bash
# Clone vcpkg somewhere on your machine (e.g. in your home directory)
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"

# Bootstrap vcpkg
"$HOME/vcpkg"/bootstrap-vcpkg.sh

# Export VCPKG_ROOT for the current shell session (and add to ~/.zshrc if use zsh for convenience)
export VCPKG_ROOT="$HOME/vcpkg"
```

Notes:
- This project uses vcpkg in manifest mode with custom overlay ports from `vcpkg-overlay/` and the registry settings in `vcpkg-configuration.json`.
- CMake presets expect `CMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`.

### 2) Configure and build

Use the provided CMake preset (generator: Ninja):

```bash
# From the repository root
cmake --preset default
cmake --build build -j
```

You can also build the project's Docker images with **three-stage build**:

```bash
# First time (full build)
make docker_build_all          # ~25 min

# Daily development (fast rebuild)  
make docker_build              # ~3-5 min ⚡
```

**Three stages:**
- `qlean-mini-dependencies:build_test` - vcpkg libs (~2.5 GB, rebuild rarely)
- `qlean-mini-builder:build_test` - project code (~3 GB, rebuild often)
- `qlean-mini:build_test` - minimal image (~300 MB, production)

**Main commands:**
```bash
make docker_build_all          # Full build (all 3 stages)
make docker_build              # Fast rebuild (code only)
make docker_build_ci           # CI/CD: pull deps + build (~4 min)
```

**Push/Pull:**
```bash
make docker_push_dependencies  # Push dependencies to registry (once)
make docker_push               # Push all images
make docker_pull_dependencies  # Pull dependencies from registry
```

**Utility:**
```bash
make docker_run                # Run node
make docker_run ARGS='--version'
make docker_verify             # Test runtime image
make docker_clean              # Clean builder + runtime (keep deps)
make docker_clean_all          # Clean everything (including deps)
```

**For CI/CD:**
```bash
# One command - pulls dependencies from registry, builds code
export DOCKER_REGISTRY=your-registry
make docker_build_ci           # ~4 min
make docker_verify
make docker_push
```

See [DOCKER_BUILD.md](DOCKER_BUILD.md) for details. See the `Makefile` for all Docker targets.

This will:
- Configure the project into `./build/`
- Build the main node executable at `./build/src/executable/qlean`

### 3) Ensure build was successful
Print help:

```bash
./build/src/executable/qlean --help
```

## Run the node

For step-by-step instructions to run a local single-node devnet, see `example/0-single/README.md`. It includes the exact CLI command and an explanation of all flags.


## Generate a node key

The binary includes a helper subcommand to generate a node key and corresponding PeerId:

```bash
./build/src/executable/qlean key generate-node-key
```

This prints two lines:
1) The private key (hex)
2) The PeerId (base58)


## Tests

If `TESTING` is enabled (default ON in top-level CMakeLists), tests are built and can be run with CTest:

```bash
# After building
ctest --test-dir build --output-on-failure
```

Individual test binaries are placed under `build/test_bin/`.

## License

SPDX-License-Identifier: Apache-2.0 — Copyright Quadrivium LLC
