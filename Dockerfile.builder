# Stage 2: Build project code
# Uses pre-built dependencies from Stage 1

ARG DEPS_IMAGE=qlean-mini-dependencies:latest
FROM ${DEPS_IMAGE} AS dependencies

FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG GIT_COMMIT=unknown
ARG GIT_BRANCH=unknown
ARG BUILD_DATE=unknown
ARG VERSION=unknown
ENV DEBIAN_FRONTEND=${DEBIAN_FRONTEND}
ENV VCPKG_FORCE_SYSTEM_BINARIES=1
ENV GIT_COMMIT=${GIT_COMMIT}
ENV GIT_BRANCH=${GIT_BRANCH}

ENV PROJECT=/qlean-mini
ENV VENV=${PROJECT}/.venv
ENV BUILD=${PROJECT}/.build
ENV PATH=${VENV}/bin:/root/.cargo/bin:${PATH}
ENV CARGO_HOME=/root/.cargo
ENV RUSTUP_HOME=/root/.rustup

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    ca-certificates \
    pkg-config \
    python3 \
    python3-venv \
    libstdc++6 \
    zip \
    unzip && \
    rm -rf /var/lib/apt/lists/*

WORKDIR ${PROJECT}

# Copy dependencies from dependencies image
COPY --from=dependencies ${VENV} ${VENV}
COPY --from=dependencies ${PROJECT}/.vcpkg ${PROJECT}/.vcpkg
COPY --from=dependencies ${BUILD}/vcpkg_installed ${BUILD}/vcpkg_installed
COPY --from=dependencies /root/.cargo /root/.cargo
COPY --from=dependencies /root/.rustup /root/.rustup

# Copy project source code
COPY . ${PROJECT}

# Create minimal .git structure for build version generation
# (actual .git is excluded by .dockerignore for smaller image)
RUN mkdir -p ${PROJECT}/.git && \
    echo "${GIT_COMMIT}" > ${PROJECT}/.git/HEAD

# Build project
RUN set -eux; \
    export PATH="${HOME}/.cargo/bin:${PATH}"; \
    source ${HOME}/.cargo/env 2>/dev/null || true; \
    echo "=== Checking vcpkg_installed ==="; \
    ls -la ${BUILD}/vcpkg_installed/ || echo "No vcpkg_installed found!"; \
    VCPKG_ROOT=${PROJECT}/.vcpkg cmake -G Ninja --preset=default \
    -DPython3_EXECUTABLE="${VENV}/bin/python3" \
    -DTESTING=OFF \
    -DVCPKG_INSTALLED_DIR=${BUILD}/vcpkg_installed \
    -DVCPKG_MANIFEST_MODE=OFF \
    -B ${BUILD} \
    ${PROJECT}; \
    cmake --build ${BUILD} --parallel; \
    cp -r -v ${BUILD}/out/bin/qlean /opt/artifacts/out/bin/qlean; \
    strip /opt/artifacts/out/bin/qlean; \
    echo "=== Artifacts ==="; \
    ls -lh /opt/artifacts/out/bin/qlean

# OCI Image Spec annotations
# https://github.com/opencontainers/image-spec/blob/main/annotations.md
LABEL org.opencontainers.image.title="qlean-mini-builder"
LABEL org.opencontainers.image.description="Qlean-mini: builder image with compiled artifacts"
LABEL org.opencontainers.image.source="https://github.com/qdrvm/qlean-mini"
LABEL org.opencontainers.image.vendor="QDRVM"
LABEL org.opencontainers.image.licenses="Apache-2.0"
LABEL org.opencontainers.image.version=$VERSION
LABEL org.opencontainers.image.created=$BUILD_DATE
LABEL org.opencontainers.image.revision=$GIT_COMMIT
LABEL org.opencontainers.image.ref.name=$GIT_BRANCH

CMD ["/bin/bash"]

