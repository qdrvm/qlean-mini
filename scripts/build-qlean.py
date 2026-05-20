#!/usr/bin/env python3
"""Rebuild the qlean binary inside the Shadow Docker image.

Usage:
  uv run scripts/build-qlean.py
"""

import subprocess
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
SIM_DOCKERFILE = PROJECT / "simulation" / "Dockerfile"
IMAGE = "qlean-shadow:latest"
DEPS_IMAGE = "qlean-mini-dependencies:latest"


def die(msg: str) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def image_exists(name: str) -> bool:
    result = subprocess.run(
        ["docker", "images", "--format", "{{.Repository}}:{{.Tag}}", name],
        capture_output=True, text=True,
    )
    return name in result.stdout


def main() -> None:
    if not SIM_DOCKERFILE.exists():
        die(f"Dockerfile not found: {SIM_DOCKERFILE}")

    if not image_exists(DEPS_IMAGE):
        die(
            f"Dependencies image '{DEPS_IMAGE}' not found.\n"
            f"Build it first: make docker_build_dependencies"
        )

    print(f"==> Building {IMAGE}...")
    subprocess.run(
        ["docker", "build", "-f", str(SIM_DOCKERFILE), "-t", IMAGE, str(PROJECT)],
        check=True,
    )
    print(f"==> Done: {IMAGE}")


if __name__ == "__main__":
    main()
