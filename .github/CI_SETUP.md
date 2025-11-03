# CI/CD Setup Complete ✅

GitHub Actions workflow has been configured for automated Docker multi-arch builds.

## What was created

### 1. GitHub Actions Workflow
**File:** `.github/workflows/docker-build.yml`

Multi-stage workflow with:
- **setup_matrix** - determines build configuration
- **build_dependencies** - builds vcpkg dependencies (only when needed)
- **build** - parallel native builds on ARM64/AMD64 runners
- **create_manifest** - creates unified multi-arch Docker manifest

### 2. Matrix Action
**File:** `.github/actions/docker-matrix/action.yml`

Generates build matrix dynamically based on selected architectures.

### 3. Documentation
**Files:**
- `.github/workflows/README.md` - Complete CI/CD guide
- `README.md` - Updated with CI/CD section

## Triggers

| Event | Action |
|-------|--------|
| Push to `ci/docker` branch | Auto-build + push to Docker Hub |
| Push git tag | Auto-build + push with tag (+ latest) |
| Pull request | Build only (validation) |
| Manual (UI) | Full control via GitHub Actions UI |

## Manual Workflow Parameters

Run workflow manually via GitHub UI with these options:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `build_amd64` | boolean | `true` | Build for AMD64 |
| `build_arm64` | boolean | `true` | Build for ARM64 |
| `build_dependencies` | boolean | `false` | Rebuild vcpkg dependencies |
| `push_to_registry` | boolean | `false` | Push to Docker Hub |
| `docker_push_tag` | string | `""` | Custom tag (e.g., `v1.0.0`) |
| `docker_push_latest` | boolean | `false` | Also push as `latest` |
| `docker_deps_tag` | string | `"latest"` | Dependencies tag |

## Required Secrets

Set these in GitHub repository settings (**Settings** → **Secrets** → **Actions**):

| Secret | Description |
|--------|-------------|
| `DOCKER_USERNAME` | Docker Hub username (from example CI: already set) |
| `DOCKER_TOKEN` | Docker Hub access token (from example CI: already set) |

## Runner Configuration

The workflow uses these runners (same as example CI):

| Architecture | Runner Label | Type |
|--------------|--------------|------|
| AMD64 | `actions-runner-controller` | GitHub-hosted or self-hosted |
| ARM64 | `["self-hosted", "qdrvm-arm64"]` | Self-hosted |

**Note:** These are already configured and match the example CI setup.

## Example Usage

### Scenario 1: Test build locally (no push)
1. Go to **Actions** → **Docker Build** → **Run workflow**
2. Settings:
   - Build linux/amd64: ✅
   - Build linux/arm64: ✅
   - Push to Docker Hub: ❌

### Scenario 2: Release v1.0.0
1. Create git tag:
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```
2. Workflow automatically:
   - Builds ARM64 + AMD64
   - Pushes images:
     - `qdrvm/qlean-mini:608f5cc` (commit)
     - `qdrvm/qlean-mini:v1.0.0` (tag)
     - `qdrvm/qlean-mini:latest`

### Scenario 3: Rebuild dependencies (vcpkg.json changed)
1. Go to **Actions** → **Docker Build** → **Run workflow**
2. Settings:
   - Build dependencies image: ✅
   - Push to Docker Hub: ✅
   - Dependencies image tag: `latest`
3. This updates `qdrvm/qlean-mini-dependencies:latest`

### Scenario 4: Staging deployment
1. Commit changes to `ci/docker` branch
2. Push: `git push origin ci/docker`
3. Workflow automatically builds and pushes:
   - `qdrvm/qlean-mini:608f5cc`

Or manually with custom tag:
1. Go to **Actions** → **Docker Build** → **Run workflow**
2. Settings:
   - Push to Docker Hub: ✅
   - Push additional custom tag: `staging`
3. Result: `qdrvm/qlean-mini:608f5cc` + `qdrvm/qlean-mini:staging`

## Workflow Process

**Automatic on push to `ci/docker`:**

```
┌─────────────────┐
│ Push to branch  │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────┐
│ setup_matrix                │
│ - Detect vcpkg.json changes │
│ - Generate build matrix     │
└────────┬────────────────────┘
         │
         ├─────────────────────────────┐
         │                             │
         ▼                             ▼
┌────────────────────┐      ┌────────────────────┐
│ build (ARM64)      │      │ build (AMD64)      │
│ - Pull deps        │      │ - Pull deps        │
│ - Build builder    │      │ - Build builder    │
│ - Build runtime    │      │ - Build runtime    │
│ - Push -arm64      │      │ - Push -amd64      │
└────────┬───────────┘      └────────┬───────────┘
         │                           │
         └───────────┬───────────────┘
                     │
                     ▼
         ┌─────────────────────┐
         │ create_manifest     │
         │ - Create multi-arch │
         │ - Push unified tags │
         └─────────────────────┘
```

**If vcpkg.json changed:**

```
┌─────────────────┐
│ Detect changes  │
└────────┬────────┘
         │
         ├─────────────────────────────┐
         │                             │
         ▼                             ▼
┌────────────────────┐      ┌────────────────────┐
│ build_dependencies │      │ build_dependencies │
│ (ARM64)            │      │ (AMD64)            │
│ - Build deps       │      │ - Build deps       │
│ - Push -arm64      │      │ - Push -amd64      │
└────────┬───────────┘      └────────┬───────────┘
         │                           │
         └───────────┬───────────────┘
                     │
                     ▼
         ┌─────────────────────┐
         │ deps manifest       │
         │ - Create multi-arch │
         └─────────────────────┘
                     │
                     ▼
         (continue with build stage)
```

## Image Tags

**Always pushed (commit hash):**
- `qdrvm/qlean-mini:608f5cc`
- `qdrvm/qlean-mini-builder:608f5cc`

**Optional (custom tag):**
- `qdrvm/qlean-mini:v1.0.0` (if tag created)
- `qdrvm/qlean-mini:staging` (if manually specified)

**Optional (latest):**
- `qdrvm/qlean-mini:latest` (for releases)

**Dependencies:**
- `qdrvm/qlean-mini-dependencies:latest` (default)
- `qdrvm/qlean-mini-dependencies:v1` (custom)

## Migrating to Production

Currently configured for `ci/docker` branch (testing).

**To enable for `master`:**

1. Edit `.github/workflows/docker-build.yml`
2. Change:
   ```yaml
   on:
     push:
       branches:
         - ci/docker  # Change to: master
   ```
3. Update PR trigger:
   ```yaml
   pull_request:
     branches:
       - ci/docker  # Change to: master
   ```
4. Commit and push to master
5. Delete `ci/docker` branch after validation

## Verification

After first workflow run, verify:

```bash
# Check multi-arch manifest
docker manifest inspect qdrvm/qlean-mini:608f5cc

# Output should show:
# - linux/arm64
# - linux/amd64

# Pull and run
docker pull qdrvm/qlean-mini:608f5cc
docker run --rm qdrvm/qlean-mini:608f5cc --help
```

## Benefits vs Manual Builds

| Feature | Manual | CI/CD |
|---------|--------|-------|
| Build time | ~50 min (emulated) | ~25 min (native parallel) |
| Consistency | Depends on dev | Always reproducible |
| Tag management | Manual | Automatic |
| Multi-arch | Complex setup | Built-in |
| Dependency caching | Manual | Automatic detection |
| Team collaboration | Requires coordination | Automatic |

## Troubleshooting

**Q: Workflow fails with "Dependencies image not found"**

A: Run workflow with "Build dependencies image" = ✅ first time

**Q: ARM64 runner not available**

A: Check runner status in **Settings** → **Actions** → **Runners**

**Q: Secrets not found**

A: Verify `DOCKER_USERNAME` and `DOCKER_TOKEN` in repository secrets

**Q: Build timeout**

A: Default is 180 min. Increase in workflow if needed:
```yaml
timeout-minutes: 240
```

## Next Steps

1. ✅ **Verify secrets** - Check Docker Hub credentials are set
2. ✅ **Test workflow** - Run manual build via UI
3. ✅ **Push to ci/docker** - Test automatic build
4. ✅ **Create test tag** - Verify tag workflow
5. ✅ **Migrate to master** - After validation

## Support

- **Workflow docs:** `.github/workflows/README.md`
- **Docker build docs:** `DOCKER_BUILD.md`
- **Example CI:** `.ci/example-ci/` (reference)

---

**Status:** ✅ CI/CD infrastructure ready for use

**Next action:** Test workflow by pushing to `ci/docker` branch or running manually via GitHub UI

