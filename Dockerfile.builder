# Stage 2: Build project code
# Uses pre-built dependencies from Stage 1

ARG DEPS_IMAGE=qlean-mini-dependencies:latest
FROM ${DEPS_IMAGE} AS dependencies

FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG GIT_COMMIT=unknown
ENV DEBIAN_FRONTEND=${DEBIAN_FRONTEND}
ENV VCPKG_FORCE_SYSTEM_BINARIES=1
ENV GIT_COMMIT=${GIT_COMMIT}

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
    mkdir -p /opt/artifacts/bin /opt/artifacts/modules /opt/artifacts/lib /opt/artifacts/vcpkg; \
    cp -v ${BUILD}/src/executable/qlean /opt/artifacts/bin/; \
    strip /opt/artifacts/bin/qlean; \
    find ${BUILD}/src/modules -type f -name "*_module.so" -exec cp -v {} /opt/artifacts/modules/ \; || true; \
    find /opt/artifacts/modules/ -name "*.so" -exec strip {} \; || true; \
    find ${BUILD}/src -type f -name "*.so" ! -name "*_module.so" -exec cp -v {} /opt/artifacts/lib/ \; || true; \
    find /opt/artifacts/lib/ -name "*.so" -exec strip {} \; || true; \
    if [ -d "${BUILD}/vcpkg_installed" ]; then \
      echo "Collecting only runtime .so libraries from vcpkg..."; \
      find ${BUILD}/vcpkg_installed -name "*.so*" -type f -exec cp -v {} /opt/artifacts/vcpkg/ \; 2>/dev/null || true; \
      find /opt/artifacts/vcpkg/ -name "*.so*" -exec strip {} \; 2>/dev/null || true; \
    fi; \
    echo "=== Artifacts ==="; \
    ls -lh /opt/artifacts/bin/; \
    ls -lh /opt/artifacts/modules/ || true; \
    ls -lh /opt/artifacts/lib/ || true; \
    echo "Vcpkg libraries: $(find /opt/artifacts/vcpkg/ -name '*.so*' | wc -l) files"

CMD ["/bin/bash"]

