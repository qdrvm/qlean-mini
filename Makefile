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

DOCKER_IMAGE ?= qlean-mini:latest
DOCKER_PLATFORM ?= linux/amd64


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

docker_build_builder:
	@echo "=== [Stage 1/2] Building Docker BUILDER image (init + configure + build) ==="
	@echo "=== Using build args from .ci/.env ==="
	@echo "  - CMAKE_VERSION=$(CMAKE_VERSION)"
	@echo "  - GCC_VERSION=$(GCC_VERSION)"
	@echo "  - RUST_VERSION=$(RUST_VERSION)"
	@echo "  - DEBIAN_FRONTEND=$(DEBIAN_FRONTEND)"
	@echo ""
	DOCKER_BUILDKIT=1 docker build \
		--build-arg CMAKE_VERSION=$(CMAKE_VERSION) \
		--build-arg GCC_VERSION=$(GCC_VERSION) \
		--build-arg RUST_VERSION=$(RUST_VERSION) \
		--build-arg DEBIAN_FRONTEND=$(DEBIAN_FRONTEND) \
		--target builder \
		--progress=plain \
		-t $(DOCKER_IMAGE)-builder .
	@echo ""
	@echo "✓ Builder image built: $(DOCKER_IMAGE)-builder"

docker_build_runtime:
	@echo "=== [Stage 2/2] Building Docker RUNTIME image (final) ==="
	@echo "=== Using existing builder image: $(DOCKER_IMAGE)-builder ==="
	@echo ""
	DOCKER_BUILDKIT=1 docker build \
		-f Dockerfile.runtime \
		--progress=plain \
		-t $(DOCKER_IMAGE) .
	@echo ""
	@echo "✓ Runtime image built: $(DOCKER_IMAGE)"

docker_build: docker_build_runtime

docker_build_all: docker_build_builder docker_build_runtime
	@echo ""
	@echo "=== ✓ All Docker images built successfully ==="
	@echo "  - Builder: $(DOCKER_IMAGE)-builder"
	@echo "  - Runtime: $(DOCKER_IMAGE)"

docker_run:
	@echo "=== Running Docker image $(DOCKER_IMAGE) ==="
	@echo "Note: --modules-dir is already set in ENTRYPOINT"
	@echo ""
	@echo "Usage examples:"
	@echo "  make docker_run                              # Show help"
	@echo "  make docker_run ARGS='--version'             # Show version"
	@echo "  make docker_run ARGS='--base-path /work ...' # Run with custom args"
	@echo ""
	docker run --rm -it $(DOCKER_IMAGE) $(ARGS)

docker_clean:
	@echo "=== Cleaning Docker images ==="
	docker rmi -f $(DOCKER_IMAGE) $(DOCKER_IMAGE)-builder 2>/dev/null || true
	@echo "✓ Docker images cleaned"

docker_inspect:
	@echo "=== Docker images info ==="
	@docker images | grep qlean-mini || echo "No qlean-mini images found"

.PHONY: all init_all os init init_py init_vcpkg configure build test clean_all \
	docker_build_builder docker_build_runtime docker_build docker_build_all \
	docker_run docker_clean docker_inspect
