#!/usr/bin/env python3
"""Run a Shadow simulation for qlean.

The run happens inside the Shadow-arm Docker container.

Usage:
  uv run scripts/run-shadow.py                     # default: 64 nodes, 180s
  uv run scripts/run-shadow.py 4                   # 4 nodes
  uv run scripts/run-shadow.py 128 300             # 128 nodes, 300s stop time
  uv run scripts/run-shadow.py 64 --genesis-dir /path/to/genesis
"""

import argparse
import os
import re
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

# On macOS, /tmp is actually a symbolic link to /private/tmp
TMP_DIR = Path("/tmp").resolve()
XMSS_CACHE = TMP_DIR / "qlean-simulations/xmss-cache"

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
         "--build-arg", "QLEAN_ENABLE_SHADOW=OFF",
         "-t", IMAGE, str(PROJECT)],
        check=True
    )


def generate_genesis(genesis_dir: Path, n: int, subnet_count: int, shadow: bool, fake_xmss: bool) -> None:
    """Run qlean generate-genesis inside the Docker image."""
    genesis_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        "docker", "run", "--rm",
        "--platform", "linux/arm64",
        "--entrypoint", "/opt/qlean/bin/qlean",
        "-v", f"{genesis_dir}:/genesis",
        "-v", f"{XMSS_CACHE}:{XMSS_CACHE}",
        "-e", f"QLEAN_XMSS_CACHE={XMSS_CACHE}",
        IMAGE,
        "generate-genesis", "/genesis", str(n), str(subnet_count),
    ]
    if shadow:
        cmd.append("shadow")
    if fake_xmss:
        cmd.append("fake-xmss")
    print(f"  genesis-cmd: {' '.join(cmd)}")
    subprocess.run(cmd, check=True)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run qlean Shadow simulation")
    parser.add_argument("nodes", nargs="?", type=int, default=64,
                        help="Number of nodes (default: 64)")
    parser.add_argument("stop_time", nargs="?", default=STOP_TIME,
                        help=f"Simulation stop time (default: {STOP_TIME})")
    parser.add_argument("--no-build", action="store_true",
                        help="Skip Docker image existence check and build")
    parser.add_argument("--build", action="store_true",
                        help="Force rebuild Docker image")
    parser.add_argument("--fake-xmss", action="store_true",
                        help="Use fake xmss keys")
    parser.add_argument("--generate-topology", action="store_true",
                        help="Generate GML topology with bandwidth and regions")
    parser.add_argument("--genesis-dir", type=str, default=None,
                        help="Path to genesis directory (default: /tmp/qlean-simulations/<n>-real/genesis)")
    parser.add_argument("--subnet-count", type=int, default=SUBNET_COUNT,
                        help=f"Number of aggregator/subnet nodes (default: {SUBNET_COUNT})")
    args = parser.parse_args()

    n = args.nodes
    stop_time = args.stop_time if args.stop_time.endswith("s") else f"{args.stop_time}s"
    max_bootnodes = MAX_BOOTNODES
    subnet_count = args.subnet_count

    # Determine genesis directory
    sim_dir = TMP_DIR / f"qlean-simulations/{n}-{"fake" if args.fake_xmss else "real"}"
    if args.genesis_dir:
        genesis_dir = Path(args.genesis_dir).resolve()
    else:
        genesis_dir = sim_dir / "genesis"

    topology_dir = sim_dir
    output_dir = sim_dir / "output"
    data_dir = sim_dir / "data"

    # Build or check image
    if args.build or (not args.no_build and not docker_image_exists(IMAGE)):
        build_image()
    else:
        print(f"==> Using existing image: {IMAGE}")

    # Generate genesis
    generate_genesis(genesis_dir, n, subnet_count, True, args.fake_xmss)
    print(f"==> Genesis ready at {genesis_dir}")

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

    # Validate genesis directory
    if not genesis_dir.is_dir():
        die(
            f"Genesis directory not found: {genesis_dir}\n"
            f"  Use --genesis-dir to specify a path."
        )
    if not topology_dir.is_dir():
        die(f"Topology directory not found: {topology_dir}")

    print(f"==> Nodes:         {n}")
    print(f"==> Stop time:     {stop_time}")
    print(f"==> Max-bootnodes: {max_bootnodes}")
    print(f"==> Subnet count:  {subnet_count}")
    print(f"==> Genesis:       {genesis_dir}")
    print(f"==> Topology:      {topology_dir}")
    print(f"==> Output:        {output_dir}")

    # Clean previous run data
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
        "-v", f"{XMSS_CACHE}:{XMSS_CACHE}",
        "-e", "TOPOLOGY_DIR=/topology",
        "-e", f"STOP_TIME={stop_time}",
        "-e", f"MAX_BOOTNODES={max_bootnodes}",
        "-e", f"UDP_PORT_BASE={UDP_PORT_BASE}",
        "-e", f"METRICS_PORT_BASE={METRICS_PORT_BASE}",
        "-e", f"FAKE_XMSS={1 if args.fake_xmss else 0}",
        IMAGE,
        "/genesis",
    ]

    print(f"==> {' '.join(cmd)}")
    print("=" * 60)
    subprocess.run(cmd, check=True)
    print(f"\n==> Done. Output in {output_dir}/shadow.data/")


if __name__ == "__main__":
    main()
