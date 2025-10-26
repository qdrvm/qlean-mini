SHELL := /bin/bash
PROJECT := $(shell pwd)
CI_DIR := $(PROJECT)/.ci

VENV ?= $(PROJECT)/.venv
BUILD ?= $(PROJECT)/.build
VCPKG ?= $(PROJECT)/.vcpkg
PATH = $(VENV)/bin:$(shell echo $$PATH)

ifneq (,$(wildcard $(CI_DIR)/.env))
    include $(CI_DIR)/.env
    export
endif

OS_TYPE := $(shell bash -c 'source $(CI_DIR)/scripts/detect_os.sh && detect_os')

# Docker image configuration
# Override these variables to customize image names and tags:
#   make docker_build_all DOCKER_IMAGE_NAME=my-project DOCKER_IMAGE_TAG=v1.0.0
DOCKER_IMAGE_NAME ?= qlean-mini
DOCKER_IMAGE_TAG ?= latest
DOCKER_PLATFORM ?= linux/arm64 #linux/amd64
DOCKER_REGISTRY ?= qdrvm
GIT_COMMIT := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")

# Derived image names for each stage:
#   qlean-mini-dependencies:latest
#   qlean-mini-builder:latest
#   qlean-mini:latest
DOCKER_IMAGE_DEPS := $(DOCKER_IMAGE_NAME)-dependencies:$(DOCKER_IMAGE_TAG)
DOCKER_IMAGE_BUILDER := $(DOCKER_IMAGE_NAME)-builder:$(DOCKER_IMAGE_TAG)
DOCKER_IMAGE_RUNTIME := $(DOCKER_IMAGE_NAME):$(DOCKER_IMAGE_TAG)


all: init_all configure build test

init_all: init init_py init_vcpkg

os:
	@echo "=== Detected OS: $(OS_TYPE)"

init:
	@echo "=== Initializing..."
	$(CI_DIR)/scripts/init.sh

init_py:
	@echo "=== Initializing Python..."
	source $(CI_DIR)/scripts/init_py.sh && init_py

init_vcpkg:
	@echo "=== Initializing Vcpkg..."
	source $(CI_DIR)/scripts/init_vcpkg.sh && init_vcpkg

configure:
	@echo "=== Configuring..."
	export PATH="$$HOME/.cargo/bin:$$PATH" && \
	source $$HOME/.cargo/env 2>/dev/null || true && \
	VCPKG_ROOT=$(VCPKG) cmake -G Ninja --preset=default -DPython3_EXECUTABLE="$(VENV)/bin/python3" -B $(BUILD) $(PROJECT)

build:
	@echo "=== Building..."
	cmake --build $(BUILD) --parallel

test:
	@echo "=== Testing..."
	ctest --test-dir $(BUILD)

clean_all:
	@echo "=== Cleaning..."
	rm -rf $(VENV) $(BUILD) $(VCPKG)	

# ==================== Docker Commands ====================

# Three-stage build

# Pull dependencies from registry (for CI/CD optimization)
docker_pull_dependencies:
	@echo "=== Pulling dependencies from registry ==="
	@echo "Registry: $(DOCKER_REGISTRY)"
	@echo "Image: $(DOCKER_REGISTRY)/$(DOCKER_IMAGE_DEPS)"
	@if docker pull $(DOCKER_REGISTRY)/$(DOCKER_IMAGE_DEPS) 2>/dev/null; then \
		docker tag $(DOCKER_REGISTRY)/$(DOCKER_IMAGE_DEPS) $(DOCKER_IMAGE_DEPS); \
		echo "✓ Dependencies pulled and tagged"; \
	else \
		echo "⚠ Dependencies not found in registry, will need to build"; \
	fi

docker_build_dependencies:
	@echo "=== [Stage 1/3] Building DEPENDENCIES image ==="
	@echo "=== Using build args from .ci/.env ==="
	@echo "  - CMAKE_VERSION=$(CMAKE_VERSION)"
	@echo "  - GCC_VERSION=$(GCC_VERSION)"
	@echo "  - RUST_VERSION=$(RUST_VERSION)"
	@echo ""
	@echo "Image: $(DOCKER_IMAGE_DEPS)"
	@echo ""
	DOCKER_BUILDKIT=1 docker build \
		--build-arg CMAKE_VERSION=$(CMAKE_VERSION) \
		--build-arg GCC_VERSION=$(GCC_VERSION) \
		--build-arg RUST_VERSION=$(RUST_VERSION) \
		--build-arg DEBIAN_FRONTEND=$(DEBIAN_FRONTEND) \
		-f Dockerfile.dependencies \
		--target dependencies-final \
		--progress=plain \
		-t $(DOCKER_IMAGE_DEPS) .
	@echo ""
	@echo "✓ Dependencies image built: $(DOCKER_IMAGE_DEPS)"

docker_build_builder:
	@echo "=== [Stage 2/3] Building BUILDER image ==="
	@echo "=== Using dependencies: $(DOCKER_IMAGE_DEPS) ==="
	@echo ""
	@if docker image inspect $(DOCKER_IMAGE_DEPS) >/dev/null 2>&1; then \
		echo "Using dependencies image: $(DOCKER_IMAGE_DEPS)"; \
	else \
		echo "ERROR: Dependencies image not found!"; \
		echo "Image: $(DOCKER_IMAGE_DEPS)"; \
		echo "Run: make docker_build_dependencies"; \
		echo "Or:  make docker_pull_dependencies"; \
		exit 1; \
	fi
	@echo ""
	@echo "Image: $(DOCKER_IMAGE_BUILDER)"
	@echo ""
	DOCKER_BUILDKIT=1 docker build \
		--build-arg DEPS_IMAGE=$(DOCKER_IMAGE_DEPS) \
		--build-arg GIT_COMMIT=$(GIT_COMMIT) \
		-f Dockerfile.builder \
		--progress=plain \
		-t $(DOCKER_IMAGE_BUILDER) .
	@echo ""
	@echo "✓ Builder image built: $(DOCKER_IMAGE_BUILDER)"

docker_build_runtime:
	@echo "=== [Stage 3/3] Building RUNTIME image ==="
	@echo "=== Using builder: $(DOCKER_IMAGE_BUILDER) ==="
	@echo ""
	@if docker image inspect $(DOCKER_IMAGE_BUILDER) >/dev/null 2>&1; then \
		echo "Using builder image: $(DOCKER_IMAGE_BUILDER)"; \
	else \
		echo "ERROR: Builder image not found!"; \
		echo "Image: $(DOCKER_IMAGE_BUILDER)"; \
		echo "Run: make docker_build_builder"; \
		exit 1; \
	fi
	@echo ""
	@echo "Image: $(DOCKER_IMAGE_RUNTIME)"
	@echo ""
	DOCKER_BUILDKIT=1 docker build \
		--build-arg BUILDER_IMAGE=$(DOCKER_IMAGE_BUILDER) \
		-f Dockerfile.runtime \
		--progress=plain \
		-t $(DOCKER_IMAGE_RUNTIME) .
	@echo ""
	@echo "✓ Runtime image built: $(DOCKER_IMAGE_RUNTIME)"

# Build all stages from scratch (dependencies + code)
docker_build_all: docker_build_dependencies docker_build_builder docker_build_runtime
	@echo ""
	@echo "=== ✓ All Docker images built successfully (3 stages) ==="
	@echo "  - Dependencies: $(DOCKER_IMAGE_DEPS)"
	@echo "  - Builder: $(DOCKER_IMAGE_BUILDER)"
	@echo "  - Runtime: $(DOCKER_IMAGE_RUNTIME)"

# Fast rebuild: only code (assumes dependencies exist)
docker_build: docker_build_builder docker_build_runtime
	@echo ""
	@echo "=== ✓ Code rebuilt (using cached dependencies) ==="
	@echo "  - Builder: $(DOCKER_IMAGE_BUILDER)"
	@echo "  - Runtime: $(DOCKER_IMAGE_RUNTIME)"

# CI/CD optimized build: pull dependencies from registry, then build code
docker_build_ci:
	@echo "=== CI/CD Build ==="
	@echo "Step 1: Pull dependencies from registry..."
	@$(MAKE) docker_pull_dependencies || echo "Dependencies not in registry, will build"
	@echo ""
	@echo "Step 2: Check if dependencies exist..."
	@if ! docker image inspect $(DOCKER_IMAGE_DEPS) >/dev/null 2>&1; then \
		echo "Building dependencies (not found in registry)..."; \
		$(MAKE) docker_build_dependencies; \
	fi
	@echo ""
	@echo "Step 3: Build project code..."
	@$(MAKE) docker_build
	@echo ""
	@echo "=== ✓ CI/CD build completed ==="

docker_run:
	@echo "=== Running Docker image $(DOCKER_IMAGE_RUNTIME) ==="
	@echo "Note: --modules-dir is already set in ENTRYPOINT"
	@echo ""
	@echo "Usage examples:"
	@echo "  make docker_run                              # Show help"
	@echo "  make docker_run ARGS='--version'             # Show version"
	@echo "  make docker_run ARGS='--base-path /work ...' # Run with custom args"
	@echo ""
	docker run --rm -it $(DOCKER_IMAGE_RUNTIME) $(ARGS)

docker_clean:
	@echo "=== Cleaning Docker images (builder + runtime) ==="
	@echo "Note: Dependencies will be kept for fast rebuilds"
	docker rmi -f $(DOCKER_IMAGE_RUNTIME) $(DOCKER_IMAGE_BUILDER) 2>/dev/null || true
	@echo "✓ Builder and runtime images cleaned"
	@echo "Tip: Use 'make docker_clean_all' to also remove dependencies"

docker_clean_all:
	@echo "=== Cleaning ALL Docker images (including dependencies) ==="
	docker rmi -f $(DOCKER_IMAGE_RUNTIME) $(DOCKER_IMAGE_BUILDER) $(DOCKER_IMAGE_DEPS) 2>/dev/null || true
	@echo "✓ All Docker images cleaned"

docker_inspect:
	@echo "=== Docker images info ==="
	@docker images | grep qlean-mini || echo "No qlean-mini images found"

docker_verify:
	@echo "=== Verifying Docker runtime image ==="
	@echo "Image: $(DOCKER_IMAGE_RUNTIME)"
	@echo ""
	@echo "[1/6] Testing help command..."
	@docker run --rm $(DOCKER_IMAGE_RUNTIME) --help > /dev/null && echo "  ✓ Help works" || (echo "  ✗ Help failed" && exit 1)
	@echo ""
	@echo "[2/6] Testing version command..."
	@docker run --rm $(DOCKER_IMAGE_RUNTIME) --version && echo "  ✓ Version works" || (echo "  ✗ Version failed" && exit 1)
	@echo ""
	@echo "[3/6] Checking binary dependencies..."
	@docker run --rm --entrypoint /bin/bash $(DOCKER_IMAGE_RUNTIME) -c '\
		apt-get update -qq && apt-get install -y -qq file > /dev/null 2>&1 && \
		echo "Binary info:" && file /usr/local/bin/qlean && \
		echo "" && echo "Checking for missing libraries..." && \
		ldd /usr/local/bin/qlean | grep "not found" && exit 1 || echo "  ✓ All binary dependencies OK"'
	@echo ""
	@echo "[4/6] Checking modules..."
	@docker run --rm --entrypoint /bin/bash $(DOCKER_IMAGE_RUNTIME) -c '\
		echo "Modules:" && ls -lh /opt/qlean/modules/ && \
		echo "" && echo "Checking module dependencies..." && \
		for mod in /opt/qlean/modules/*.so; do \
			echo "Checking $$(basename $$mod)..."; \
			ldd $$mod | grep "not found" && exit 1 || echo "  ✓ OK"; \
		done'
	@echo ""
	@echo "[5/6] Checking environment variables..."
	@docker run --rm --entrypoint /bin/bash $(DOCKER_IMAGE_RUNTIME) -c '\
		echo "LD_LIBRARY_PATH=$$LD_LIBRARY_PATH" && \
		echo "QLEAN_MODULES_DIR=$$QLEAN_MODULES_DIR" && \
		echo "" && echo "Verifying paths exist:" && \
		ls -ld $$QLEAN_MODULES_DIR > /dev/null && echo "  ✓ Modules dir exists" || (echo "  ✗ Modules dir missing" && exit 1) && \
		ls -d /opt/qlean/lib > /dev/null && echo "  ✓ Lib dir exists" || (echo "  ✗ Lib dir missing" && exit 1)'
	@echo ""
	@echo "[6/6] Checking project libraries..."
	@docker run --rm --entrypoint /bin/bash $(DOCKER_IMAGE_RUNTIME) -c '\
		apt-get update -qq && apt-get install -y -qq file > /dev/null 2>&1 && \
		echo "Project libraries:" && ls /opt/qlean/lib/ && \
		echo "" && echo "Checking libapplication.so dependencies..." && \
		ldd /opt/qlean/lib/libapplication.so | grep "not found" && exit 1 || echo "  ✓ All project libraries OK"'
	@echo ""
	@echo "=== ✓ Runtime image verified successfully! ==="

# Internal function to push a single Docker image
define push_image
	@if ! docker image inspect $(1) >/dev/null 2>&1; then \
		echo "ERROR: $(2) image not found: $(1)"; \
		echo "Run: make docker_build_$(3)"; \
		exit 1; \
	fi
	@docker tag $(1) $(DOCKER_REGISTRY)/$(1)
	@docker push $(DOCKER_REGISTRY)/$(1)
	@echo "✓ $(2) pushed: $(DOCKER_REGISTRY)/$(1)"
endef

# Push individual images to registry
docker_push_dependencies:
	@echo "=== Pushing dependencies image ==="
	@echo "Registry: $(DOCKER_REGISTRY)"
	$(call push_image,$(DOCKER_IMAGE_DEPS),Dependencies,dependencies)

docker_push_builder:
	@echo "=== Pushing builder image ==="
	@echo "Registry: $(DOCKER_REGISTRY)"
	$(call push_image,$(DOCKER_IMAGE_BUILDER),Builder,builder)

docker_push_runtime:
	@echo "=== Pushing runtime image ==="
	@echo "Registry: $(DOCKER_REGISTRY)"
	$(call push_image,$(DOCKER_IMAGE_RUNTIME),Runtime,runtime)

# Push all built images to registry
docker_push:
	@echo "=== Pushing all Docker images ==="
	@echo "Registry: $(DOCKER_REGISTRY)"
	@echo "Commit: $(GIT_COMMIT)"
	@echo ""
	@if docker image inspect $(DOCKER_IMAGE_DEPS) >/dev/null 2>&1; then \
		echo "[1/3] Pushing dependencies..."; \
		docker tag $(DOCKER_IMAGE_DEPS) $(DOCKER_REGISTRY)/$(DOCKER_IMAGE_DEPS); \
		docker push $(DOCKER_REGISTRY)/$(DOCKER_IMAGE_DEPS); \
		echo "✓ Dependencies pushed"; \
	else \
		echo "[1/3] Skipping dependencies (not built)"; \
	fi
	@echo ""
	@if docker image inspect $(DOCKER_IMAGE_BUILDER) >/dev/null 2>&1; then \
		echo "[2/3] Pushing builder..."; \
		docker tag $(DOCKER_IMAGE_BUILDER) $(DOCKER_REGISTRY)/$(DOCKER_IMAGE_BUILDER); \
		docker push $(DOCKER_REGISTRY)/$(DOCKER_IMAGE_BUILDER); \
		echo "✓ Builder pushed"; \
	else \
		echo "[2/3] Skipping builder (not built)"; \
	fi
	@echo ""
	@if docker image inspect $(DOCKER_IMAGE_RUNTIME) >/dev/null 2>&1; then \
		echo "[3/3] Pushing runtime..."; \
		docker tag $(DOCKER_IMAGE_RUNTIME) $(DOCKER_REGISTRY)/$(DOCKER_IMAGE_RUNTIME); \
		docker push $(DOCKER_REGISTRY)/$(DOCKER_IMAGE_RUNTIME); \
		echo "✓ Runtime pushed"; \
	else \
		echo "[3/3] Skipping runtime (not built)"; \
	fi
	@echo ""
	@echo "✓ All images pushed to $(DOCKER_REGISTRY)!"

.PHONY: all init_all os init init_py init_vcpkg configure build test clean_all \
	docker_pull_dependencies \
	docker_build_dependencies docker_build_builder docker_build_runtime docker_build docker_build_all docker_build_ci \
	docker_push_dependencies docker_push_builder docker_push_runtime docker_push \
	docker_run docker_clean docker_clean_all docker_inspect docker_verify
