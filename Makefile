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
DOCKER_REGISTRY ?= qdrvm
GIT_COMMIT := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")


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

docker_verify:
	@echo "=== Verifying Docker image: $(DOCKER_IMAGE) ==="
	@echo ""
	@echo "[1/6] Testing help command..."
	@docker run --rm $(DOCKER_IMAGE) --help > /dev/null && echo "  ✓ Help works" || (echo "  ✗ Help failed" && exit 1)
	@echo ""
	@echo "[2/6] Testing version command..."
	@docker run --rm $(DOCKER_IMAGE) --version && echo "  ✓ Version works" || (echo "  ✗ Version failed" && exit 1)
	@echo ""
	@echo "[3/6] Checking binary dependencies..."
	@docker run --rm --entrypoint /bin/bash $(DOCKER_IMAGE) -c '\
		apt-get update -qq && apt-get install -y -qq file > /dev/null 2>&1 && \
		echo "Binary info:" && file /usr/local/bin/qlean && \
		echo "" && echo "Checking for missing libraries..." && \
		ldd /usr/local/bin/qlean | grep "not found" && exit 1 || echo "  ✓ All binary dependencies OK"'
	@echo ""
	@echo "[4/6] Checking modules..."
	@docker run --rm --entrypoint /bin/bash $(DOCKER_IMAGE) -c '\
		echo "Modules:" && ls -lh /opt/qlean/modules/ && \
		echo "" && echo "Checking module dependencies..." && \
		for mod in /opt/qlean/modules/*.so; do \
			echo "Checking $$(basename $$mod)..."; \
			ldd $$mod | grep "not found" && exit 1 || echo "  ✓ OK"; \
		done'
	@echo ""
	@echo "[5/6] Checking environment variables..."
	@docker run --rm --entrypoint /bin/bash $(DOCKER_IMAGE) -c '\
		echo "LD_LIBRARY_PATH=$$LD_LIBRARY_PATH" && \
		echo "QLEAN_MODULES_DIR=$$QLEAN_MODULES_DIR" && \
		echo "" && echo "Verifying paths exist:" && \
		ls -ld $$QLEAN_MODULES_DIR > /dev/null && echo "  ✓ Modules dir exists" || (echo "  ✗ Modules dir missing" && exit 1) && \
		ls -d /opt/qlean/lib > /dev/null && echo "  ✓ Lib dir exists" || (echo "  ✗ Lib dir missing" && exit 1)'
	@echo ""
	@echo "[6/6] Checking project libraries..."
	@docker run --rm --entrypoint /bin/bash $(DOCKER_IMAGE) -c '\
		apt-get update -qq && apt-get install -y -qq file > /dev/null 2>&1 && \
		echo "Project libraries:" && ls /opt/qlean/lib/ && \
		echo "" && echo "Checking libapplication.so dependencies..." && \
		ldd /opt/qlean/lib/libapplication.so | grep "not found" && exit 1 || echo "  ✓ All project libraries OK"'
	@echo ""
	@echo "=== ✓ All verification checks passed! ==="

docker_verify_all: docker_verify
	@echo ""
	@echo "=== Verifying builder image: $(DOCKER_IMAGE)-builder ==="
	@echo ""
	@echo "Checking builder image exists..."
	@docker image inspect $(DOCKER_IMAGE)-builder > /dev/null 2>&1 && echo "  ✓ Builder image found" || (echo "  ✗ Builder image not found" && exit 1)
	@echo "Checking builder image size..."
	@docker images $(DOCKER_IMAGE)-builder --format "  Size: {{.Size}}"
	@echo ""
	@echo "=== ✓ All images verified! ==="

docker_tag:
	@echo "=== Tagging Docker images for push ==="
	@echo "Registry: $(DOCKER_REGISTRY)"
	@echo "Commit: $(GIT_COMMIT)"
	@echo ""
	docker tag $(DOCKER_IMAGE)-builder $(DOCKER_REGISTRY)/qlean-mini-builder:$(GIT_COMMIT)
	docker tag $(DOCKER_IMAGE)-builder $(DOCKER_REGISTRY)/qlean-mini-builder:latest
	docker tag $(DOCKER_IMAGE) $(DOCKER_REGISTRY)/qlean-mini:$(GIT_COMMIT)
	docker tag $(DOCKER_IMAGE) $(DOCKER_REGISTRY)/qlean-mini:latest
	@echo ""
	@echo "✓ Images tagged:"
	@echo "  - $(DOCKER_REGISTRY)/qlean-mini-builder:$(GIT_COMMIT)"
	@echo "  - $(DOCKER_REGISTRY)/qlean-mini-builder:latest"
	@echo "  - $(DOCKER_REGISTRY)/qlean-mini:$(GIT_COMMIT)"
	@echo "  - $(DOCKER_REGISTRY)/qlean-mini:latest"

docker_push: docker_tag
	@echo ""
	@echo "=== Pushing Docker images to $(DOCKER_REGISTRY) ==="
	@echo ""
	@echo "[1/4] Pushing builder with commit tag..."
	docker push $(DOCKER_REGISTRY)/qlean-mini-builder:$(GIT_COMMIT)
	@echo ""
	@echo "[2/4] Pushing builder:latest..."
	docker push $(DOCKER_REGISTRY)/qlean-mini-builder:latest
	@echo ""
	@echo "[3/4] Pushing runtime with commit tag..."
	docker push $(DOCKER_REGISTRY)/qlean-mini:$(GIT_COMMIT)
	@echo ""
	@echo "[4/4] Pushing runtime:latest..."
	docker push $(DOCKER_REGISTRY)/qlean-mini:latest
	@echo ""
	@echo "✓ All images pushed successfully!"

docker_build_push: docker_build_all docker_push
	@echo ""
	@echo "=== ✓ Build and push completed ==="
	@echo "Images available at:"
	@echo "  docker pull $(DOCKER_REGISTRY)/qlean-mini:latest"
	@echo "  docker pull $(DOCKER_REGISTRY)/qlean-mini:$(GIT_COMMIT)"
	@echo "  docker pull $(DOCKER_REGISTRY)/qlean-mini-builder:latest"
	@echo "  docker pull $(DOCKER_REGISTRY)/qlean-mini-builder:$(GIT_COMMIT)"

.PHONY: all init_all os init init_py init_vcpkg configure build test clean_all \
	docker_build_builder docker_build_runtime docker_build docker_build_all \
	docker_run docker_clean docker_inspect docker_verify docker_verify_all \
	docker_tag docker_push docker_build_push
