# Global ARGs (must be before FROM to be available in FROM instructions)
ARG BASE_IMAGE=ubuntu:24.04

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
ARG GIT_COMMIT=unknown
ARG GIT_BRANCH=unknown
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
  # Clean up any existing venv that might have incompatible Python version
  rm -rf ${VENV}; \
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
  # Clean build directory to avoid CMake cache path mismatch
  rm -rf ${BUILD}; \
  make configure; \
  make build; \
  # Collect artifacts
  # Copy statically linked executable
  mkdir -p /opt/artifacts/out/bin; \
  cp -r -v ${BUILD}/out/bin/qlean /opt/artifacts/out/bin/qlean; \
  # List collected artifacts
  echo "=== Collected artifacts ==="; \
  ls -lh /opt/artifacts/out/bin/qlean

# ==================== Stage 2: Runtime ====================
FROM ${BASE_IMAGE} AS runtime

ARG BASE_IMAGE
ARG GIT_COMMIT=unknown
ARG GIT_BRANCH=unknown
ARG BUILD_DATE=unknown
ARG VERSION=unknown

# Install minimal runtime dependencies
RUN apt-get update && \
  apt-get install -y --no-install-recommends \
  libstdc++6 \
  ca-certificates && \
  rm -rf /var/lib/apt/lists/*

WORKDIR /work

# Copy artifacts from builder
COPY --from=builder /opt/artifacts/out/bin/qlean /opt/qlean/bin/qlean

# Verify artifacts
RUN echo "=== Runtime image contents ===" && \
  ls -lh /opt/qlean/bin/qlean

# OCI Image Spec annotations
# https://github.com/opencontainers/image-spec/blob/main/annotations.md
LABEL org.opencontainers.image.title="qlean-mini"
LABEL org.opencontainers.image.description="Qlean-mini: lean Ethereum consensus client for devnets — minimal optimized runtime"
LABEL org.opencontainers.image.source="https://github.com/qdrvm/qlean-mini"
LABEL org.opencontainers.image.documentation="https://github.com/qdrvm/qlean-mini#readme"
LABEL org.opencontainers.image.vendor="QDRVM"
LABEL org.opencontainers.image.licenses="Apache-2.0"
LABEL org.opencontainers.image.version=$VERSION
LABEL org.opencontainers.image.created=$BUILD_DATE
LABEL org.opencontainers.image.revision=$GIT_COMMIT
LABEL org.opencontainers.image.ref.name=$GIT_BRANCH
LABEL org.opencontainers.image.base.name=$BASE_IMAGE

ENTRYPOINT ["/opt/qlean/bin/qlean"]
CMD ["--help"]
