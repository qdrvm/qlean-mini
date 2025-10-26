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

### Push to Registry

```bash
make docker_push_dependencies   # Push dependencies image
make docker_push_builder        # Push builder image  
make docker_push_runtime        # Push runtime image
make docker_push                # Push all built images
```

### Pull from Registry

```bash
make docker_pull_dependencies   # Pull dependencies from registry
```

### Run & Verify

```bash
make docker_run                 # Run with --help
make docker_run ARGS='--version'
make docker_verify              # Test runtime image
```

### Clean

```bash
make docker_clean               # Remove builder + runtime (keep dependencies)
make docker_clean_all           # Remove all images (including dependencies)
make docker_inspect             # Show image info
```

## Images

- `qlean-mini-dependencies:build_test` (~2.5 GB) - vcpkg libraries, build tools
- `qlean-mini-builder:build_test` (~3 GB) - compiled project code
- `qlean-mini:build_test` (~300 MB) - minimal runtime image for production

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

# Test
make docker_run ARGS='--version'
make docker_verify
```

### CI/CD Pipeline

```bash
# Set registry
export DOCKER_REGISTRY=your-registry

# Build (pulls dependencies from registry)
make docker_build_ci            # ~4 min

# Test
make docker_verify

# Push
make docker_push                # Push all built images
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

## Benefits

- **4-6x faster** builds during development
- **75-80% time savings** (3-5 min vs 20 min)
- **10x smaller** production images (300 MB vs 3 GB)
- Better caching and layer reuse

## Tracking Dependency Changes

Dependencies rebuild when these files change:
- `vcpkg.json`
- `vcpkg-configuration.json`
- `vcpkg-overlay/`
- `.ci/.env` (CMAKE_VERSION, GCC_VERSION, RUST_VERSION)

### Auto-detect in CI

```bash
# Check if dependencies changed
if git diff HEAD~1 HEAD -- vcpkg.json vcpkg-configuration.json vcpkg-overlay/ | grep .; then
  make docker_build_dependencies
else
  docker pull registry/deps:latest
fi
make docker_build_fast
```

## Troubleshooting

**Error: dependencies image not found**
```bash
make docker_build_dependencies
# or
docker pull registry/deps:latest
docker tag registry/deps:latest qlean-mini:latest-dependencies
```

**Changes not reflected**
```bash
docker builder prune -a
make docker_build_fast
```

**Library not found in runtime**
```bash
docker run --rm qlean-mini:latest ldd /usr/local/bin/qlean
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
      
      - name: Push
        if: github.ref == 'refs/heads/master'
        run: make docker_push
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

push:
  stage: push
  script:
    - make docker_push
  only:
    - master
```

### Key Points for CI/CD

1. **Use `docker_build_ci`** - automatically pulls dependencies from registry
2. **Set `DOCKER_REGISTRY`** environment variable
3. **Dependencies only rebuild** when vcpkg.json changes
4. **Fast builds** - ~4 min instead of 20 min
5. **Push dependencies** to registry when they change (once)
6. **Git required** - for version detection in build

