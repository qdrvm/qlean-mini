# GitHub Actions CI/CD

This directory contains GitHub Actions workflows for automated Docker multi-arch builds.

## Workflows

### `docker-build.yml` - Docker Multi-arch Build

Builds Docker images for multiple CPU architectures (ARM64, AMD64) and pushes them to Docker Hub.

**Triggers:**

1. **Manual (workflow_dispatch)** - Full control via GitHub UI
2. **Push to `ci/docker` branch** - Auto-build and push
3. **Git tags** - Auto-build and push with tag
4. **Pull requests** - Build only (no push)

**Architecture:**

The workflow consists of 4 stages:

1. **setup_matrix** - Determines what to build based on inputs/triggers
2. **build_dependencies** - Builds dependencies image (only when needed)
3. **build** - Builds builder and runtime images on native runners (ARM64 + AMD64)
4. **create_manifest** - Creates unified multi-arch manifest

**Key Features:**

- ✅ Native builds on ARM64 and AMD64 runners (fast, no emulation)
- ✅ Parallel builds on different architectures
- ✅ Smart dependency caching (rebuild only when `vcpkg.json` changes)
- ✅ Multi-arch manifest (one tag works on both architectures)
- ✅ Flexible tagging (commit hash + optional custom tag + optional latest)
- ✅ Auto-push on branch/tag pushes

## Manual Build via GitHub UI

1. Go to **Actions** → **Docker Build** → **Run workflow**
2. Select parameters:
   - **Build linux/amd64** - Build for AMD64 (default: yes)
   - **Build linux/arm64** - Build for ARM64 (default: yes)
   - **Build dependencies image** - Rebuild vcpkg dependencies (default: no)
   - **Push to Docker Hub** - Push images to registry (default: no)
   - **Push additional custom tag** - Optional tag (e.g., `v1.0.0`, `staging`)
   - **Push 'latest' tag** - Also tag as `latest` (default: no)
   - **Dependencies image tag** - Which deps tag to use/create (default: `latest`)

3. Click **Run workflow**

**Example scenarios:**

```yaml
# Scenario 1: Build both architectures, don't push (testing)
build_amd64: true
build_arm64: true
push_to_registry: false

# Scenario 2: Build and push with version tag
build_amd64: true
build_arm64: true
push_to_registry: true
docker_push_tag: "v1.0.0"
docker_push_latest: true  # Also tag as 'latest'

# Scenario 3: Rebuild dependencies (vcpkg.json changed)
build_dependencies: true
push_to_registry: true
docker_deps_tag: "latest"

# Scenario 4: Build only ARM64 for testing
build_amd64: false
build_arm64: true
push_to_registry: false
```

## Automatic Builds

**Push to `ci/docker` branch:**
```bash
git push origin ci/docker
```
- Builds both ARM64 and AMD64
- Automatically pushes to Docker Hub
- Tags: `qlean-mini:608f5cc` (commit hash)
- If `vcpkg.json` changed → rebuilds dependencies

**Create git tag:**
```bash
git tag v1.0.0
git push origin v1.0.0
```
- Same as branch push
- Tags: `qlean-mini:608f5cc` + `qlean-mini:v1.0.0` + `qlean-mini:latest`

**Pull request:**
```bash
git push origin feature-branch
# Create PR to ci/docker
```
- Builds images but doesn't push
- Validates that the build works

## Runners

The workflow uses two types of runners:

- **AMD64**: `actions-runner-controller` (GitHub-hosted or self-hosted)
- **ARM64**: `["self-hosted", "qdrvm-arm64"]` (self-hosted)

These are configured in `.github/actions/docker-matrix/action.yml`.

## Secrets

Required secrets in GitHub repository settings:

- `DOCKER_USERNAME` - Docker Hub username
- `DOCKER_TOKEN` - Docker Hub access token

**Setting up secrets:**

1. Go to **Settings** → **Secrets and variables** → **Actions**
2. Click **New repository secret**
3. Add both secrets

## Dependencies Image

The dependencies image (`qlean-mini-dependencies:latest`) contains:
- vcpkg packages
- Python venv
- Rust toolchain
- System dependencies

**When to rebuild:**

- ❌ **Don't rebuild** for normal code changes
- ✅ **Rebuild** when `vcpkg.json` changes
- ✅ **Rebuild** when system dependencies change (`.ci/scripts/init.sh`)

The workflow automatically detects `vcpkg.json` changes on push events.

For manual builds, check **Build dependencies image** in the UI.

## Image Naming

**Dependencies:**
```
qdrvm/qlean-mini-dependencies:latest
qdrvm/qlean-mini-dependencies:v1       (custom tag)
```

**Builder:**
```
qdrvm/qlean-mini-builder:608f5cc       (commit hash, always)
qdrvm/qlean-mini-builder:v1.0.0        (custom tag, optional)
```

**Runtime:**
```
qdrvm/qlean-mini:608f5cc               (commit hash, always)
qdrvm/qlean-mini:v1.0.0                (custom tag, optional)
qdrvm/qlean-mini:latest                (latest tag, optional)
```

## Workflow Process

**1. Setup Matrix**
```yaml
outputs:
  matrix: {"include":[{"platform":"linux/amd64","arch_suffix":"amd64","runs_on":"actions-runner-controller"},{"platform":"linux/arm64","arch_suffix":"arm64","runs_on":["self-hosted","qdrvm-arm64"]}]}
  should_build_deps: false
  should_push: true
```

**2. Build Dependencies (if needed)**
```bash
# Job 1: ARM64 runner
DOCKER_PLATFORM=linux/arm64 make docker_build_dependencies
make docker_push_platform_dependencies  # Push with -arm64 suffix

# Job 2: AMD64 runner (parallel)
DOCKER_PLATFORM=linux/amd64 make docker_build_dependencies
make docker_push_platform_dependencies  # Push with -amd64 suffix
```

**3. Build Application**
```bash
# Job 1: ARM64 runner
make docker_pull_dependencies           # Pull deps from registry
DOCKER_PLATFORM=linux/arm64 make docker_build_builder
DOCKER_PLATFORM=linux/arm64 make docker_build_runtime
make docker_push_platform               # Push with -arm64 suffix

# Job 2: AMD64 runner (parallel)
make docker_pull_dependencies
DOCKER_PLATFORM=linux/amd64 make docker_build_builder
DOCKER_PLATFORM=linux/amd64 make docker_build_runtime
make docker_push_platform               # Push with -amd64 suffix
```

**4. Create Manifest**
```bash
# On any runner (no build, no pull)
make docker_manifest_dependencies       # If deps were built
make docker_manifest_create             # Builder + runtime
```

**Result:** Single tag works on both architectures:
```bash
docker pull qdrvm/qlean-mini:608f5cc
# Docker automatically pulls correct architecture (ARM64 or AMD64)
```

## Troubleshooting

**Error: "Dependencies image not found in registry"**

Solution:
```bash
# Option 1: Run workflow with "Build dependencies image" = true
# Option 2: Build and push manually
make docker_build_dependencies DOCKER_PLATFORM=linux/arm64
make docker_push_platform_dependencies

make docker_build_dependencies DOCKER_PLATFORM=linux/amd64
make docker_push_platform_dependencies

make docker_manifest_dependencies
```

**Error: "Runner not found"**

Check that you have configured self-hosted runners with labels:
- `actions-runner-controller` (for AMD64)
- `self-hosted, qdrvm-arm64` (for ARM64)

**Build timeout**

Default timeout is 180 minutes (3 hours). If builds take longer, increase in workflow:
```yaml
timeout-minutes: 240  # 4 hours
```

## Local Development

The workflow uses standard Makefile commands, so you can test locally:

```bash
# Build locally
export DOCKER_PLATFORM=linux/arm64
make docker_build_all

# Push to test
export DOCKER_REGISTRY=qdrvm
export DOCKER_PUSH_TAG=true
export DOCKER_IMAGE_TAG=test-build
make docker_push
```

## CI/CD Best Practices

1. **Use commit tags** - Always pushed, provides full traceability
2. **Use custom tags for releases** - `v1.0.0`, `staging`, etc.
3. **Use `latest` sparingly** - Only for stable releases
4. **Rebuild dependencies rarely** - Only when `vcpkg.json` changes
5. **Test PRs before merging** - Auto-builds verify changes work

## Migration Plan

Current configuration builds on `ci/docker` branch for testing.

**After validation:**

1. Update workflow triggers to `master`:
   ```yaml
   on:
     push:
       branches:
         - master  # Change from ci/docker
   ```

2. Update PR target:
   ```yaml
   pull_request:
     branches:
       - master
   ```

3. Push workflows to master
4. Delete `ci/docker` branch

