# Docker Build - Three-Stage Strategy

## Overview

Three-stage build separates dependencies from project code for faster development:

```
Stage 1: Dependencies (vcpkg libs) → ~20 min, rebuild rarely
Stage 2: Builder (project code)    → ~3-5 min, rebuild often  
Stage 3: Runtime (minimal image)   → ~1 min, automatic
```

## Quick Start

### First Time (Full Build)

```bash
make docker_build_all
# Builds: dependencies → builder → runtime (~25 min)
```

### Daily Development (Fast Rebuild)

```bash
# After code changes
make docker_build
# Builds: builder → runtime using cached dependencies (~4 min)
```

### CI/CD Optimized

```bash
make docker_build_ci
# Pull deps from registry + build code (~4 min)
```

## All Commands

### Build

```bash
make docker_build_dependencies  # Stage 1: vcpkg libs (~20 min)
make docker_build_builder       # Stage 2: project code (~3-5 min)
make docker_build_runtime       # Stage 3: final image (~1 min)
make docker_build               # Fast: builder + runtime (~4 min)
make docker_build_all           # Full: all 3 stages (~25 min)
make docker_build_ci            # CI/CD optimized (~4 min)
```

**Platform selection:**
```bash
# ARM64 (default)
make docker_build_all

# AMD64 / x86_64
make docker_build_all DOCKER_PLATFORM=linux/amd64

# Build for specific architecture
DOCKER_PLATFORM=linux/amd64 make docker_build_dependencies
DOCKER_PLATFORM=linux/amd64 make docker_build

# Multi-arch: see "Multi-platform Builds" section for CI/CD workflow
```

### Push to Registry

**Dependencies (push once per version):**
```bash
# Push dependencies with default tag (latest)
make docker_push_dependencies   # Push: qlean-mini-dependencies:latest

# Push dependencies with custom tag
DOCKER_DEPS_TAG=v2 make docker_build_dependencies
make docker_push_dependencies   # Push: qlean-mini-dependencies:v2
```

**Builder & Runtime (push per commit):**
```bash
# Local dev (default) - only commit tag
make docker_push                # Push: qlean-mini:608f5cc

# Push with custom tag (e.g., version)
DOCKER_PUSH_TAG=true DOCKER_IMAGE_TAG=v1.0.0 make docker_push  # Push: commit + v1.0.0

# Push with latest tag
DOCKER_PUSH_LATEST=true make docker_push  # Push: commit + latest

# Production release - push all 3 tags
DOCKER_PUSH_TAG=true DOCKER_PUSH_LATEST=true DOCKER_IMAGE_TAG=v1.0.0 make docker_push
# Push: qlean-mini:608f5cc + qlean-mini:v1.0.0 + qlean-mini:latest

# Staging environment
DOCKER_PUSH_TAG=true DOCKER_IMAGE_TAG=staging make docker_push  # Push: commit + staging

# Individual stages
make docker_push_builder        # Push builder only
make docker_push_runtime        # Push runtime only
```

### Pull from Registry

```bash
# Pull dependencies with default tag (latest)
make docker_pull_dependencies   # Pull: qlean-mini-dependencies:latest

# Pull dependencies with custom tag
DOCKER_DEPS_TAG=v2 make docker_pull_dependencies  # Pull: qlean-mini-dependencies:v2
```

### Run & Verify

```bash
make docker_run                 # Run with --help
make docker_run ARGS='--version'
make docker_verify              # Test runtime image

# Run on specific platform
make docker_run DOCKER_PLATFORM=linux/amd64 ARGS='--version'
make docker_verify DOCKER_PLATFORM=linux/amd64
```

### Clean

```bash
make docker_clean               # Remove builder + runtime (keep dependencies)
make docker_clean_all           # Remove all images (including dependencies)
make docker_inspect             # Show image info
```

## Images

- `qlean-mini-dependencies:latest` (~18 GB) - vcpkg libraries, build tools
- `qlean-mini-builder:latest` (~19 GB) - compiled project code  
- `qlean-mini:latest` (~240 MB) - **optimized** runtime image for production

**Image tagging:**

**Dependencies** (single version, shared across all commits):
- Tag: `qlean-mini-dependencies:latest` (default, configurable via `DOCKER_DEPS_TAG`)
- Example: `DOCKER_DEPS_TAG=v1 make docker_build_dependencies`
- Changes only when `vcpkg.json`, `vcpkg-configuration.json`, or system deps change
- Always uses the same tag across all code commits

**Builder & Runtime** (per-commit versioning):
- Every build creates **2 local tags**:
  - **Commit tag**: `qlean-mini:608f5cc` (always, based on git commit)
  - **Additional tag**: `qlean-mini:localBuild` (default, configurable via `DOCKER_IMAGE_TAG`)

Push behavior for Builder & Runtime (**up to 3 tags** can be pushed):

| Tag Type | Variable | Always Pushed? | Example |
|----------|----------|----------------|---------|
| **Commit** | `GIT_COMMIT` | ✅ Yes | `qlean-mini:608f5cc` |
| **Custom** | `DOCKER_IMAGE_TAG` | Only if `DOCKER_PUSH_TAG=true` | `qlean-mini:v1.0.0` |
| **Latest** | (hardcoded) | Only if `DOCKER_PUSH_LATEST=true` | `qlean-mini:latest` |

**Examples:**

| Scenario | Command | Local Tags | Pushed Tags |
|----------|---------|------------|-------------|
| Local dev (default) | `make docker_build` | Builder/Runtime: `608f5cc`, `localBuild`<br>Deps: `latest` | None (manual push) |
| Push to registry | `make docker_push` | Builder/Runtime: `608f5cc`, `localBuild`<br>Deps: `latest` | `608f5cc` only |
| Master branch | `DOCKER_PUSH_LATEST=true` | Builder/Runtime: `608f5cc`, `localBuild`<br>Deps: `latest` | `608f5cc`, `latest` |
| Production release | `DOCKER_PUSH_TAG=true`<br>`DOCKER_PUSH_LATEST=true`<br>`DOCKER_IMAGE_TAG=v1.0.0` | Builder/Runtime: `608f5cc`, `v1.0.0`<br>Deps: `latest` | `608f5cc`, `v1.0.0`, `latest` |
| Staging | `DOCKER_PUSH_TAG=true`<br>`DOCKER_IMAGE_TAG=staging` | Builder/Runtime: `608f5cc`, `staging`<br>Deps: `latest` | `608f5cc`, `staging` |
| New deps version | `DOCKER_DEPS_TAG=v2`<br>`make docker_build_dependencies` | Deps: `v2` | Manual: `make docker_push_dependencies` |

**Optimization applied:**
- Strip debug symbols from binaries (~30-50% size reduction)
- Copy only `.so` files from vcpkg (not static libs, headers, cmake files)
- Result: **14x smaller** runtime image (240 MB vs 3.4 GB)

## When to Rebuild

**Dependencies** - rebuild when:
- `vcpkg.json` changes
- System dependencies update (cmake, gcc, rust versions)
- Rarely - maybe once per month or when adding new libraries

**Builder** - rebuild when:
- Code changes in `src/`
- `CMakeLists.txt` changes
- Every commit (fast - only 3-5 min!)

**Runtime** - automatically rebuilt after builder

## Workflow Examples

### Team Setup (First Time)

```bash
# Lead/DevOps builds and pushes dependencies (once)
make docker_build_dependencies
make docker_push_dependencies

# Or push everything
make docker_build_all
make docker_push
```

### Developer Daily Work

```bash
# Pull dependencies (once per setup)
make docker_pull_dependencies

# Daily development cycle
# 1. Edit code
# 2. Rebuild (fast!)
make docker_build               # ~4 min
# Creates: qlean-mini:abc1234 + qlean-mini:localBuild (local only)

# Test
make docker_run ARGS='--version'
make docker_verify

# Push to registry (only commit tag)
make docker_push                # Push: qlean-mini:abc1234
```

### CI/CD Pipeline

```bash
# Set registry
export DOCKER_REGISTRY=your-registry

# Build (pulls dependencies from registry)
make docker_build_ci            # ~4 min

# Test
make docker_verify

# Push scenarios:

# 1. Feature branch / PR - only commit tag
make docker_push                # Push: qlean-mini:608f5cc

# 2. Master branch - commit + latest
DOCKER_PUSH_LATEST=true make docker_push  # Push: qlean-mini:608f5cc + latest

# 3. Release tag - commit + version + latest
DOCKER_PUSH_TAG=true DOCKER_PUSH_LATEST=true DOCKER_IMAGE_TAG=v1.0.0 make docker_push
# Push: qlean-mini:608f5cc + v1.0.0 + latest

# 4. Staging environment - commit + staging
DOCKER_PUSH_TAG=true DOCKER_IMAGE_TAG=staging make docker_push  # Push: commit + staging
```

## CI/CD Integration

### Automatic (Recommended)

```bash
# One command: pull dependencies from registry, then build
export DOCKER_REGISTRY=your-registry
make docker_build_ci            # ~4 min (if deps cached)
make docker_verify
make docker_push
```

### Manual Control

```bash
# 1. Pull dependencies from registry
export DOCKER_REGISTRY=your-registry
make docker_pull_dependencies

# 2. Build code only
make docker_build               # ~4 min

# 3. Test and push
make docker_verify
make docker_push
```

## Tracking Dependency Changes

Dependencies rebuild when these files change:
- `vcpkg.json`
- `vcpkg-configuration.json`
- `vcpkg-overlay/`
- `.ci/.env` (CMAKE_VERSION, GCC_VERSION, RUST_VERSION)

### Auto-detect in CI

```bash
# Check if dependencies changed
if git diff HEAD~1 HEAD -- vcpkg.json vcpkg-configuration.json vcpkg-overlay/ .ci/.env | grep .; then
  make docker_build_dependencies
  make docker_push_dependencies
else
  docker pull qdrvm/qlean-mini-dependencies:latest
  docker tag qdrvm/qlean-mini-dependencies:latest qlean-mini-dependencies:latest
fi
make docker_build
```

## Troubleshooting

**Error: dependencies image not found**
```bash
make docker_build_dependencies
# or
docker pull qdrvm/qlean-mini-dependencies:latest
docker tag qdrvm/qlean-mini-dependencies:latest qlean-mini-dependencies:latest
```

**Changes not reflected**
```bash
docker builder prune -a
make docker_build
```

**Library not found in runtime**
```bash
docker run --rm qlean-mini:latest ldd /usr/local/bin/qlean
```

**Check image sizes**
```bash
make docker_inspect
```

## CI/CD Examples

### GitHub Actions

```yaml
name: Docker Build

on:
  push:
    branches: [ master, develop ]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v2
      
      - name: Login to Registry
        uses: docker/login-action@v2
        with:
          registry: ${{ secrets.DOCKER_REGISTRY_URL }}
          username: ${{ secrets.DOCKER_USERNAME }}
          password: ${{ secrets.DOCKER_PASSWORD }}
      
      - name: Build with cached dependencies
        run: |
          export DOCKER_REGISTRY=${{ secrets.DOCKER_REGISTRY }}
          make docker_build_ci
      
      - name: Verify
        run: make docker_verify
      
      - name: Push (commit tag only)
        if: github.ref != 'refs/heads/master' && !startsWith(github.ref, 'refs/tags/v')
        run: make docker_push
      
      - name: Push with latest tag (master)
        if: github.ref == 'refs/heads/master'
        run: DOCKER_PUSH_LATEST=true make docker_push
      
      - name: Push with version and latest tags (releases)
        if: startsWith(github.ref, 'refs/tags/v')
        run: |
          VERSION=${GITHUB_REF#refs/tags/}
          DOCKER_PUSH_TAG=true DOCKER_PUSH_LATEST=true DOCKER_IMAGE_TAG=$VERSION make docker_push
```

### GitLab CI

```yaml
build:
  stage: build
  image: docker:24-dind
  services:
    - docker:24-dind
  before_script:
    - docker login -u $CI_REGISTRY_USER -p $CI_REGISTRY_PASSWORD $CI_REGISTRY
    - apk add --no-cache make bash git
  script:
    - export DOCKER_REGISTRY=$CI_REGISTRY
    - make docker_build_ci
    - make docker_verify
  only:
    - master
    - develop

push_commit:
  stage: push
  script:
    - make docker_push
  only:
    - develop

push_latest:
  stage: push
  script:
    - export DOCKER_PUSH_LATEST=true
    - make docker_push
  only:
    - master

push_version:
  stage: push
  script:
    - export DOCKER_PUSH_TAG=true
    - export DOCKER_PUSH_LATEST=true
    - export DOCKER_IMAGE_TAG=$CI_COMMIT_TAG
    - make docker_push
  only:
    - tags
```

### Key Points for CI/CD

1. **Use `docker_build_ci`** - automatically pulls dependencies from registry
2. **Set `DOCKER_REGISTRY`** environment variable
3. **Dependencies tag** - use `DOCKER_DEPS_TAG` to specify which dependencies version to use (default: `latest`)
4. **Dependencies only rebuild** when vcpkg.json changes
5. **Fast builds** - ~4 min instead of 25 min
6. **Automatic tagging** - images always tagged by git commit hash
7. **Push up to 3 tags**:
   - Commit tag (always): `qlean-mini:608f5cc`
   - Custom tag (optional): set `DOCKER_PUSH_TAG=true` + `DOCKER_IMAGE_TAG=v1.0.0`
   - Latest tag (optional): set `DOCKER_PUSH_LATEST=true`
8. **Flexible tagging** - use any custom tag: latest, v1.0.0, staging, production, etc.
9. **Git required** - for version detection and commit-based tagging
10. **Optimized runtime** - 240 MB production image (stripped binaries)

## Working with Dependencies Tag

The dependencies image is **shared across all code commits** and only needs to be rebuilt when dependencies change.

### Understanding `DOCKER_DEPS_TAG`

- **Variable**: `DOCKER_DEPS_TAG` (default: `latest`)
- **Purpose**: Specify which version of dependencies to use
- **When to change**: After updating `vcpkg.json`, `vcpkg-configuration.json`, or system dependencies

### Typical Workflow

**1. Team uses default (latest):**
```bash
# Everyone pulls the same dependencies
make docker_pull_dependencies  # Pulls: qlean-mini-dependencies:latest
make docker_build              # Builds code using latest dependencies
```

**2. Developer updates dependencies:**
```bash
# Edit vcpkg.json to add new library
vim vcpkg.json

# Build new dependencies version
DOCKER_DEPS_TAG=v2 make docker_build_dependencies

# Push for team
DOCKER_DEPS_TAG=v2 make docker_push_dependencies

# Update CI/CD to use v2
# Set DOCKER_DEPS_TAG=v2 in CI environment variables
```

**3. Using specific dependency version:**
```bash
# Pull specific version
DOCKER_DEPS_TAG=v2 make docker_pull_dependencies

# Build code with specific dependencies
DOCKER_DEPS_TAG=v2 make docker_build

# Or set as default in your shell
export DOCKER_DEPS_TAG=v2
make docker_pull_dependencies
make docker_build
```

### Best Practices

1. **Use `latest` for active development** - simplest for most developers
2. **Version dependencies for releases** - e.g., `v1`, `v2`, `v3` when making breaking changes
3. **Push after building** - always push dependencies to registry after rebuilding:
   ```bash
   DOCKER_DEPS_TAG=v2 make docker_build_dependencies
   DOCKER_DEPS_TAG=v2 make docker_push_dependencies
   ```
4. **Document in team** - notify team when dependencies version changes
5. **CI/CD pinning** - for stable builds, pin `DOCKER_DEPS_TAG` in CI config instead of using `latest`

## Platform Support

The build system supports multiple architectures via the `DOCKER_PLATFORM` variable.

### Supported Platforms

- **`linux/arm64`** (default) - ARM 64-bit (Apple Silicon, AWS Graviton, etc.)
- **`linux/amd64`** - x86_64 / Intel/AMD 64-bit

### Usage

**Build for specific platform:**
```bash
# ARM64 (default)
make docker_build_all

# AMD64
DOCKER_PLATFORM=linux/amd64 make docker_build_all

# Only dependencies for AMD64
DOCKER_PLATFORM=linux/amd64 make docker_build_dependencies
```

**Pull from registry:**
```bash
# Pull ARM64 (default)
make docker_pull_dependencies

# Pull AMD64
DOCKER_PLATFORM=linux/amd64 make docker_pull_dependencies
```

**Run and verify:**
```bash
# Run on ARM64 (default)
make docker_run ARGS='--version'

# Run on AMD64
DOCKER_PLATFORM=linux/amd64 make docker_run ARGS='--version'

# Verify AMD64 image
DOCKER_PLATFORM=linux/amd64 make docker_verify
```

### Important Notes

1. **Platform consistency** - Use the same platform for all stages (dependencies, builder, runtime)
2. **Registry separation** - Different platforms use the same image tags, stored separately by Docker
3. **Native builds** - Build on native platform when possible for best performance
4. **Cross-compilation** - Docker supports cross-platform builds via QEMU (slower)
5. **CI/CD** - Set `DOCKER_PLATFORM` in CI environment variables for consistent builds

### CI/CD Example

```yaml
# GitHub Actions
- name: Build for AMD64
  run: |
    export DOCKER_PLATFORM=linux/amd64
    make docker_build_ci
    make docker_verify
    make docker_push

# GitLab CI
variables:
  DOCKER_PLATFORM: linux/amd64

build:
  script:
    - make docker_build_ci
    - make docker_verify
```

### Multi-platform Builds (Unified Manifest)

**CI/CD with native runners (RECOMMENDED)**

Best practice - build on native machines in parallel, then create manifest:

```bash
# === Job 1: Build on ARM64 runner ===
export DOCKER_PLATFORM=linux/arm64
make docker_build_all                  # Build on ARM64 machine
make docker_push_platform              # Push with -arm64 tag

# === Job 2: Build on AMD64 runner ===
export DOCKER_PLATFORM=linux/amd64
make docker_build_all                  # Build on AMD64 machine
make docker_push_platform              # Push with -amd64 tag

# === Job 3: Create manifest (any machine) ===
make docker_manifest_create            # Create unified manifest (no pull!)
# Result: qlean-mini:608f5cc points to both architectures
```

**Verify multi-arch image:**

```bash
docker manifest inspect qdrvm/qlean-mini:608f5cc
```

**How it works:**

1. **Build stage**: Creates platform-specific images locally with `-arm64` and `-amd64` suffixes
2. **Push stage**: Pushes both platform images to registry
3. **Manifest creation**: Creates Docker manifest that points to both images
4. **Result**: Single tag (`qlean-mini:608f5cc`) works on both ARM64 and AMD64
5. **Auto-selection**: Docker automatically pulls correct architecture

**Commands:**

| Command | Description |
|---------|-------------|
| `make docker_push_platform` | Push builder + runtime with arch suffix (`-arm64` / `-amd64`) |
| `make docker_push_platform_dependencies` | Push dependencies with arch suffix |
| `make docker_manifest_create` | Create unified manifest from pushed images (builder + runtime) |
| `make docker_manifest_dependencies` | Create unified manifest for dependencies |

**CI/CD Integration (RECOMMENDED):**

```yaml
# GitHub Actions - Build on native runners
jobs:
  build-arm64:
    runs-on: ubuntu-latest-arm64  # ARM64 runner
    steps:
      - uses: actions/checkout@v4
      - name: Build ARM64
        run: |
          export DOCKER_PLATFORM=linux/arm64
          make docker_build_all
          make docker_push_platform  # Push with -arm64 suffix

  build-amd64:
    runs-on: ubuntu-latest  # AMD64 runner
    steps:
      - uses: actions/checkout@v4
      - name: Build AMD64
        run: |
          export DOCKER_PLATFORM=linux/amd64
          make docker_build_all
          make docker_push_platform  # Push with -amd64 suffix

  create-manifest:
    needs: [build-arm64, build-amd64]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Create multi-arch manifest
        run: make docker_manifest_create  # No build, no pull!

# GitLab CI - Build on native runners
build:arm64:
  tags: [arm64]
  script:
    - export DOCKER_PLATFORM=linux/arm64
    - make docker_build_all
    - make docker_push_platform

build:amd64:
  tags: [amd64]
  script:
    - export DOCKER_PLATFORM=linux/amd64
    - make docker_build_all
    - make docker_push_platform

manifest:
  needs: [build:arm64, build:amd64]
  script:
    - make docker_manifest_create
```
