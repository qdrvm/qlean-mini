#!/usr/bin/env bash
set -euo pipefail
# set -x

init_py() {
  echo "$VENV"
  # Verify python3 is available and check version
  if ! command -v python3 >/dev/null 2>&1; then
    echo "Error: python3 is not available"
    exit 1
  fi
  echo "Using Python: $(python3 --version)"
  
  # Create venv
  python3 -m venv "$VENV"
  
  # Verify venv was created successfully
  if [ ! -f "$VENV/bin/python3" ]; then
    echo "Error: venv created but python3 binary is missing"
    echo "This might indicate that python3-venv package is not installed"
    exit 1
  fi
  
  source $VENV/bin/activate
  pip3 install cmake==${CMAKE_VERSION}
  pip3 install --no-cache-dir asn1tools
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  echo "This script is intended to be sourced or used as a library."
  echo "Exported functions: init_py"
  exit 1
fi
