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
- `qlean-mini-dependencies:latest` - vcpkg libs (~18 GB, rebuild rarely)
- `qlean-mini-builder:latest` - project code (~19 GB, rebuild often)
- `qlean-mini:latest` - runtime image (~240 MB, production)

**When to rebuild dependencies:**
- `vcpkg.json` or `vcpkg-configuration.json` changes (new libraries)
- System dependencies update (`.ci/.env`: cmake, gcc, rust versions)
- Typically: once per month or when adding new dependencies
- **Tip:** Push dependencies to registry after rebuild for team reuse

**Main commands:**
```bash
make docker_build_all          # Full build (all 3 stages)
make docker_build              # Fast rebuild (code only)
make docker_build_ci           # CI/CD: pull deps + build (~4 min)
```

**Platform selection:**
```bash
# ARM64 (default)
make docker_build_all

# AMD64 / x86_64
make docker_build_all DOCKER_PLATFORM=linux/amd64

# Multi-arch: CI/CD on native runners
# Job 1 (ARM64 runner):
DOCKER_PLATFORM=linux/arm64 make docker_build_all
make docker_push_platform              # Push with -arm64 tag

# Job 2 (AMD64 runner):
DOCKER_PLATFORM=linux/amd64 make docker_build_all
make docker_push_platform              # Push with -amd64 tag

# Job 3 (any machine):
make docker_manifest_create            # Create unified manifest

# Run on specific platform
make docker_run DOCKER_PLATFORM=linux/amd64 ARGS='--version'
```

**Push/Pull:**
```bash
make docker_push_dependencies  # Push dependencies to registry (once)
make docker_push               # Push all images (commit tag only)
make docker_pull_dependencies  # Pull dependencies from registry

# Push with custom tag
DOCKER_PUSH_TAG=true DOCKER_IMAGE_TAG=v1.0.0 make docker_push  # commit + v1.0.0

# Push with latest tag
DOCKER_PUSH_LATEST=true make docker_push  # commit + latest

# Push all 3 tags (commit + custom + latest)
DOCKER_PUSH_TAG=true DOCKER_PUSH_LATEST=true DOCKER_IMAGE_TAG=v1.0.0 make docker_push
```

**Image tagging:**

Dependencies (single version, shared across commits):
- Tag: `qlean-mini-dependencies:latest` (configurable via `DOCKER_DEPS_TAG`)
- Changes only when `vcpkg.json` or system dependencies change

Builder & Runtime (per commit):
- Each build creates 2 local tags: `qlean-mini:608f5cc` (commit) + `qlean-mini:localBuild` (default)
- Push behavior:
  - **Commit tag** (`608f5cc`): always pushed to registry
  - **Custom tag** (`DOCKER_IMAGE_TAG`): pushed if `DOCKER_PUSH_TAG=true` (default: `localBuild`, not pushed)
  - **Latest tag**: pushed if `DOCKER_PUSH_LATEST=true`
- Push up to 3 tags: commit + custom (v1.0.0) + latest

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
make docker_push               # Push commit tag only

# Production release with version and latest
DOCKER_PUSH_TAG=true DOCKER_PUSH_LATEST=true DOCKER_IMAGE_TAG=v1.0.0 make docker_push
# Pushes: qlean-mini:608f5cc + qlean-mini:v1.0.0 + qlean-mini:latest

# Only latest
DOCKER_PUSH_LATEST=true make docker_push  # qlean-mini:608f5cc + qlean-mini:latest

# Staging environment
DOCKER_PUSH_TAG=true DOCKER_IMAGE_TAG=staging make docker_push  # commit + staging
```

See [DOCKER_BUILD.md](DOCKER_BUILD.md) for details. See the `Makefile` for all Docker targets.

### Automated CI/CD (GitHub Actions)

This project includes GitHub Actions workflow for automated multi-arch Docker builds:

- ✅ **Auto-build on push** to `ci/docker` branch (or tags)
- ✅ **Manual builds** via GitHub UI with flexible parameters
- ✅ **Native multi-arch** (ARM64 + AMD64) on free GitHub-hosted runners
- ✅ **Fast builds** (~20-30 min per architecture, native compilation)
- ✅ **Smart caching** (rebuilds dependencies only when `vcpkg.json` changes)
- ✅ **Flexible tagging** (commit hash + custom tag + latest)
- ✅ **Zero setup** - works out of the box, 100% free for public repos

**Quick actions:**

```bash
# Push to ci/docker branch → auto-build and push
git push origin ci/docker

# Create tag → auto-build and push with tag
git tag v1.0.0 && git push origin v1.0.0

# Manual build via GitHub UI:
# Actions → Docker Build → Run workflow
```

See [.github/workflows/README.md](.github/workflows/README.md) for CI/CD documentation.

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
