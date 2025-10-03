#!/usr/bin/env bash
set -euo pipefail
# set -x
trap 'echo "=== Error on line $LINENO"; exit 1' ERR

SCRIPT_DIR="$(dirname "${BASH_SOURCE[0]}")"
OS_SELECT=$(source ${SCRIPT_DIR}/detect_os.sh && detect_os)

set -o allexport && . ${SCRIPT_DIR}/../.env && set +o allexport

main() {
  case "$OS_SELECT" in
    linux_deb)
      echo "=== Detected Linux system with apt"
      apt update && apt install -y  $LINUX_PACKAGES
      update-alternatives --install /usr/bin/gcc          gcc          /usr/bin/gcc-$GCC_VERSION 90
      update-alternatives --install /usr/bin/g++          g++          /usr/bin/g++-$GCC_VERSION 90
      # Install Rust via rustup
      if ! command -v rustup >/dev/null 2>&1; then
        echo "=== Installing Rust ${RUST_VERSION} via rustup..."
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain ${RUST_VERSION} --profile minimal
      fi
      if [ -f "$HOME/.cargo/env" ]; then
        source "$HOME/.cargo/env"
      fi
      export PATH="$HOME/.cargo/bin:$PATH"
      ;;
    linux_other)
      echo "=== Detected Linux system without apt"
      echo "=== Support for other package managers is not added"
      ;;
    macos)
      echo "=== Detected macOS system"
      if command -v brew >/dev/null 2>&1; then
        echo "=== Homebrew found. Installing packages..."
        brew update && brew install $MACOS_PACKAGES
      else
        echo "=== Homebrew is not installed. Install it before proceeding: https://brew.sh"
      fi
      ;;
    *)
      echo "=== Unknown system"
      ;;
  esac
  
  if command -v cargo >/dev/null 2>&1; then
    echo "=== Cargo is available: $(cargo --version)"
  else
    echo "=== Warning: Cargo is not available in PATH"
  fi
}

main

exit 0
