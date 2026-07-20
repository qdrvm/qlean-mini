#!/usr/bin/env python3
"""Run a Shadow simulation for qlean with a single command.

Usage:
  uv run scripts/run-shadow.py                    # default: 64 nodes, 180s
  uv run scripts/run-shadow.py 4                  # 4 nodes
  uv run scripts/run-shadow.py 128 300            # 128 nodes, 300s stop time
  uv run scripts/run-shadow.py 64 --no-build      # skip image check
  uv run scripts/run-shadow.py 64 --generate-genesis --subnet-count 4
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
IMAGE = "qlean-shadow:latest"
STOP_TIME = "180s"
UDP_PORT_BASE = 10000
METRICS_PORT_BASE = 9100
SHM_SIZE = "4g"

MAX_BOOTNODES = 50
SUBNET_COUNT = 1


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
        ["docker", "build", "-f", str(PROJECT / "Dockerfile.shadow"),
         "--build-arg", "QLEAN_ENABLE_SHADOW=ON",
         "-t", IMAGE, str(PROJECT)],
        check=True
    )


def generate_genesis(n: int, subnet_count: int, genesis_dir: Path) -> None:
    """Generate a shadow genesis directory with the given subnet count."""
    print(f"==> Generating shadow genesis for {n} nodes (subnet_count={subnet_count})...")

    with tempfile.TemporaryDirectory(prefix="qlean-shadow-gen-") as tmp_dir_str:
        tmp_dir = Path(tmp_dir_str)
        subprocess.run([
            "docker", "run", "--rm",
            "--platform", "linux/arm64",
            "--entrypoint", "/opt/qlean/bin/qlean",
            "-v", f"{tmp_dir}:/genesis",
            IMAGE,
            "generate-genesis", "/genesis", str(n), str(subnet_count), "shadow",
        ], check=True)

        if genesis_dir.exists():
            shutil.rmtree(genesis_dir)
        shutil.copytree(tmp_dir, genesis_dir)

    print(f"==> Genesis ready at {genesis_dir}")


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
    parser.add_argument("--generate-genesis", action="store_true",
                        help="Generate genesis directory")
    parser.add_argument("--generate-topology", action="store_true",
                        help="Generate GML topology with bandwidth and regions")
    parser.add_argument("--genesis-dir", type=str, default=None,
                        help="Path to genesis directory (default: /tmp/qlean-simulations/<n>-fake)")
    parser.add_argument("--subnet-count", type=int, default=SUBNET_COUNT,
                        help=f"Number of subnets/aggregators (default: {SUBNET_COUNT})")
    args = parser.parse_args()

    n = args.nodes
    stop_time = args.stop_time if args.stop_time.endswith("s") else f"{args.stop_time}s"
    max_bootnodes = MAX_BOOTNODES
    subnet_count = args.subnet_count

    # Determine genesis directory
    sim_dir = Path(f"/tmp/qlean-simulations/{n}-fake")
    if args.genesis_dir:
        genesis_dir = Path(args.genesis_dir).resolve()
    else:
        genesis_dir = sim_dir

    topology_dir = sim_dir
    output_dir = Path(f"/tmp/qlean-sim-{n}/output")
    data_dir = Path(f"/tmp/qlean-sim-{n}/data")

    # Build image if needed
    if args.build or (not args.no_build and not docker_image_exists(IMAGE)):
        build_image()
    else:
        print(f"==> Using existing image: {IMAGE}")

    # Generate genesis if requested
    if args.generate_genesis:
        generate_genesis(n, subnet_count, genesis_dir)

    # Generate topology if requested
    if args.generate_topology:
        print(f"==> Generating topology for {n} nodes...")
        topology_dir.mkdir(parents=True, exist_ok=True)
        subprocess.run([
            sys.executable,
            str(PROJECT / "scripts" / "gen_topology.py"),
            str(n),
            str(topology_dir)
        ], check=True)

    if not genesis_dir.is_dir():
        die(f"Genesis directory not found: {genesis_dir}")
    if not topology_dir.is_dir():
        die(f"Topology directory not found: {topology_dir}")

    print(f"==> Nodes:         {n}")
    print(f"==> Stop time:     {stop_time}")
    print(f"==> Max-bootnodes: {max_bootnodes}")
    print(f"==> Subnet count:  {subnet_count}")
    print(f"==> Genesis:       {genesis_dir}")
    print(f"==> Topology:      {topology_dir}")
    print(f"==> Output:        {output_dir}")

    # Clean previous run data (stale blocks break finalization)
    for d in (output_dir, data_dir):
        if d.exists():
            print(f"==> Cleaning previous {d.name}...")
            shutil.rmtree(d)
        d.mkdir(parents=True, exist_ok=True)

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
