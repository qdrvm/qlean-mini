# ==================== Stage 1: Builder (init + configure + build) ====================
FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG CMAKE_VERSION=3.31.1
ARG GCC_VERSION=14
ARG RUST_VERSION=stable

ENV DEBIAN_FRONTEND=${DEBIAN_FRONTEND}
ENV CMAKE_VERSION=${CMAKE_VERSION}
ENV GCC_VERSION=${GCC_VERSION}
ENV RUST_VERSION=${RUST_VERSION}
ENV VCPKG_FORCE_SYSTEM_BINARIES=1

ENV PROJECT=/qlean-mini
ENV VENV=${PROJECT}/.venv
ENV BUILD=${PROJECT}/.build
ENV PATH=${VENV}/bin:/root/.cargo/bin:${PATH}
ENV CARGO_HOME=/root/.cargo
ENV RUSTUP_HOME=/root/.rustup

WORKDIR ${PROJECT}

# Copy project files
COPY . ${PROJECT}

# Run all inits and build with vcpkg cache
RUN set -eux; \
    chmod +x .ci/scripts/*.sh; \
    # System dependencies and Rust via init.sh
    ./.ci/scripts/init.sh; \
    # Python venv and cmake via init_py
    make init_py

# Configure and build with full vcpkg cache
RUN --mount=type=cache,target=/qlean-mini/.vcpkg,id=vcpkg-full \
    set -eux; \
    export PATH="${HOME}/.cargo/bin:${PATH}"; \
    source ${HOME}/.cargo/env 2>/dev/null || true; \
    # Init vcpkg inside cache mount if needed
    if [ ! -f "/qlean-mini/.vcpkg/vcpkg" ]; then \
      make init_vcpkg; \
    fi; \
    make configure; \
    make build; \
    # Collect artifacts
    mkdir -p /opt/artifacts/bin /opt/artifacts/modules /opt/artifacts/lib /opt/artifacts/vcpkg; \
    # Copy executable
    cp -v ${BUILD}/src/executable/qlean /opt/artifacts/bin/; \
    # Copy all module .so files
    find ${BUILD}/src/modules -type f -name "*_module.so" -exec cp -v {} /opt/artifacts/modules/ \; || true; \
    # Copy all other project .so libraries (app, utils, etc)
    find ${BUILD}/src -type f -name "*.so" ! -name "*_module.so" -exec cp -v {} /opt/artifacts/lib/ \; || true; \
    # Copy vcpkg installed libraries
    if [ -d "${BUILD}/vcpkg_installed" ]; then \
      cp -R ${BUILD}/vcpkg_installed /opt/artifacts/vcpkg/installed; \
    fi; \
    # List collected artifacts
    echo "=== Collected artifacts ==="; \
    ls -lh /opt/artifacts/bin/; \
    ls -lh /opt/artifacts/modules/ || true; \
    ls -lh /opt/artifacts/lib/ || true

# ==================== Stage 2: Runtime ====================
FROM ubuntu:24.04 AS runtime

# Install minimal runtime dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    libstdc++6 \
    ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# Environment variables for runtime
ENV LD_LIBRARY_PATH=/opt/qlean/lib:/opt/qlean/vcpkg/installed/x64-linux/lib:/opt/qlean/vcpkg/installed/x64-linux-dynamic/lib:/opt/qlean/vcpkg/installed/lib:/usr/local/lib
ENV QLEAN_MODULES_DIR=/opt/qlean/modules

WORKDIR /work

# Copy artifacts from builder
COPY --from=builder /opt/artifacts/bin/qlean /usr/local/bin/qlean
COPY --from=builder /opt/artifacts/lib/ /opt/qlean/lib/
COPY --from=builder /opt/artifacts/modules/ /opt/qlean/modules/
COPY --from=builder /opt/artifacts/vcpkg/installed/ /opt/qlean/vcpkg/installed/

# Verify artifacts
RUN echo "=== Runtime image contents ===" && \
    ls -lh /usr/local/bin/qlean && \
    echo "=== Project libraries ===" && \
    ls -lh /opt/qlean/lib/ || true && \
    echo "=== Modules ===" && \
    ls -lh /opt/qlean/modules/ || true

ENTRYPOINT ["qlean", "--modules-dir", "/opt/qlean/modules"]
CMD ["--help"]
