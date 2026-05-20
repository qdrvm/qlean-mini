#!/usr/bin/env python3
"""Run a Shadow simulation for qlean with a single command.

Usage:
  uv run scripts/run-shadow.py             # default: 64 nodes, 180s
  uv run scripts/run-shadow.py 4           # 4 nodes
  uv run scripts/run-shadow.py 128 300     # 128 nodes, 300s stop time
  uv run scripts/run-shadow.py 64 --no-build  # skip image check

Defaults are fixed — genesis dir, topology dir, ports, max bootnodes.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
IMAGE = "qlean-shadow:latest"
STOP_TIME = "180s"
UDP_PORT_BASE = 10000
METRICS_PORT_BASE = 9100
SHM_SIZE = "4g"


MAX_BOOTNODES = 50


def die(msg: str) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def docker_image_exists(name: str) -> bool:
    result = subprocess.run(
        ["docker", "images", "--format", "{{.Repository}}:{{.Tag}}", name],
        capture_output=True, text=True
    )
    return name in result.stdout


def build_image() -> None:
    print("==> Building Docker image (this may take a few minutes)...")
    subprocess.run(
        ["docker", "build", "-f", str(PROJECT / "simulation/Dockerfile"), "-t", IMAGE, str(PROJECT)],
        check=True
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Run qlean Shadow simulation")
    parser.add_argument("nodes", nargs="?", type=int, default=64,
                        help="Number of nodes (default: 64)")
    parser.add_argument("stop_time", nargs="?", default=STOP_TIME,
                        help=f"Simulation stop time (default: {STOP_TIME})")
    parser.add_argument("--no-build", action="store_true",
                        help="Skip Docker image existence check and build")
    parser.add_argument("--build", action="store_true",
                        help="Force rebuild Docker image")
    args = parser.parse_args()

    n = args.nodes
    stop_time = args.stop_time if args.stop_time.endswith("s") else f"{args.stop_time}s"
    max_bootnodes = MAX_BOOTNODES

    genesis_dir = PROJECT / "simulation" / "genesis_shadow" / str(n)
    topology_dir = PROJECT / "simulation" / "topology" / str(n)
    output_dir = Path(f"/tmp/qlean-sim-{n}/output")
    data_dir = Path(f"/tmp/qlean-sim-{n}/data")

    if not genesis_dir.is_dir():
        die(f"Genesis directory not found: {genesis_dir}")
    if not topology_dir.is_dir():
        die(f"Topology directory not found: {topology_dir}")

    print(f"==> Nodes:        {n}")
    print(f"==> Stop time:    {stop_time}")
    print(f"==> Max-bootnodes:{max_bootnodes}")
    print(f"==> Genesis:      {genesis_dir}")
    print(f"==> Topology:     {topology_dir}")
    print(f"==> Output:       {output_dir}")

    # Clean previous run data (stale blocks break finalization)
    for d in (output_dir, data_dir):
        if d.exists():
            print(f"==> Cleaning previous {d.name}...")
            shutil.rmtree(d)
        d.mkdir(parents=True, exist_ok=True)

    # Build image if needed
    if args.build or (not args.no_build and not docker_image_exists(IMAGE)):
        build_image()
    else:
        print(f"==> Using existing image: {IMAGE}")

    # Run simulation
    cmd = [
        "docker", "run", "--rm",
        "--platform", "linux/arm64",
        "--security-opt", "seccomp=unconfined",
        "--shm-size", SHM_SIZE,
        "-v", f"{genesis_dir}:/genesis:ro",
        "-v", f"{topology_dir}:/topology:ro",
        "-v", f"{output_dir}:/output",
        "-v", f"{data_dir}:/data",
        "-e", "TOPOLOGY_DIR=/topology",
        "-e", f"STOP_TIME={stop_time}",
        "-e", f"MAX_BOOTNODES={max_bootnodes}",
        "-e", f"UDP_PORT_BASE={UDP_PORT_BASE}",
        "-e", f"METRICS_PORT_BASE={METRICS_PORT_BASE}",
        IMAGE,
        "/genesis",
    ]

    print(f"==> {' '.join(cmd)}")
    print("=" * 60)
    subprocess.run(cmd, check=True)
    print(f"\n==> Done. Output in {output_dir}/shadow.data/")


if __name__ == "__main__":
    main()
