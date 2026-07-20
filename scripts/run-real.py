#!/usr/bin/env python3
"""Run a Shadow simulation for qlean with real signatures and aggregation.

Uses the qlean binary built with QLEAN_ENABLE_SHADOW=OFF so the real XMSS
provider is used for signing, verifying, aggregating, and verifying aggregated
signatures.  The run still happens inside the Shadow-arm Docker container.

Usage:
  uv run scripts/run-real.py                     # default: 64 nodes, 180s
  uv run scripts/run-real.py 4                   # 4 nodes
  uv run scripts/run-real.py 128 300             # 128 nodes, 300s stop time
  uv run scripts/run-real.py 64 --generate-genesis  # generate real genesis
  uv run scripts/run-real.py 64 --genesis-dir /path/to/genesis
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
IMAGE = "qlean-real:latest"
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
         "--build-arg", "QLEAN_ENABLE_SHADOW=OFF",
         "-t", IMAGE, str(PROJECT)],
        check=True
    )


def _run_qlean(genesis_dir: Path, n: int, subnet_count: int, shadow: bool) -> None:
    """Run qlean generate-genesis inside the Docker image."""
    genesis_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        "docker", "run", "--rm",
        "--platform", "linux/arm64",
        "--entrypoint", "/opt/qlean/bin/qlean",
        "-v", f"{genesis_dir}:/genesis",
        IMAGE,
        "generate-genesis", "/genesis", str(n), str(subnet_count),
    ]
    if shadow:
        cmd.append("shadow")
    print(f"  genesis-cmd: {' '.join(cmd)}")
    subprocess.run(cmd, check=True)


def _read_pubkeys_from_manifest(manifest: Path) -> list[str]:
    """Read pubkey_hex values (with 0x prefix) from validator-keys-manifest.yaml.

    Returns list in manifest order (index 0, 1, 2, ...).
    """
    text = manifest.read_text()
    pubkeys = re.findall(r'pubkey_hex:\s*(0x[0-9a-fA-F]+)', text)
    if not pubkeys:
        die(f"No pubkey_hex entries found in {manifest}")
    return pubkeys


def _has_real_keys(genesis_dir: Path, n: int) -> bool:
    """Check if genesis dir already has real (non-zero) XMSS key files."""
    keys_dir = genesis_dir / "hash-sig-keys"
    manifest = keys_dir / "validator-keys-manifest.yaml"
    if not manifest.exists():
        return False
    pubkeys = _read_pubkeys_from_manifest(manifest)
    # Real pubkeys are non-zero
    if any(k == "0x" + "00" * 52 for k in pubkeys):
        return False
    # All secret key files must exist
    for i in range(n):
        if not (keys_dir / f"validator_{i}_sk.ssz").exists():
            return False
    return True


def _patch_file_pubkeys(path: Path, real_pubkeys: list[str],
                        strip_0x: bool = False) -> None:
    """Replace all pubkey_hex values in a file with real pubkeys, in order."""
    if not path.exists():
        die(f"File not found: {path}")
    text = path.read_text()
    keys = [k[2:] if strip_0x else k for k in real_pubkeys]
    it = iter(keys)

    def _repl(m: re.Match) -> str:
        return f"pubkey_hex: {next(it)}"

    text = re.sub(r'pubkey_hex:\s*(0x[0-9a-fA-F]+)', _repl, text)
    path.write_text(text)


def _patch_genesis_validators(path: Path, real_pubkeys: list[str]) -> None:
    """Replace GENESIS_VALIDATORS hex entries (no 0x prefix) with real pubkeys."""
    if not path.exists():
        die(f"File not found: {path}")
    text = path.read_text()
    lines = text.splitlines()
    keys = iter(k[2:] for k in real_pubkeys)  # strip 0x prefix
    new_lines: list[str] = []
    for line in lines:
        if line.strip().startswith("- "):
            new_lines.append(f"  - {next(keys)}")
        else:
            new_lines.append(line)
    path.write_text("\n".join(new_lines) + "\n")


def generate_genesis(n: int, subnet_count: int, genesis_dir: Path) -> None:
    """Generate a genesis directory with real XMSS keys and shadow-compatible ENRs.

    Uses two passes of the qlean generate-genesis command:
      1. With 'shadow' flag -> correct shadow ENRs/IPs but fake zero pubkeys.
      2. Without 'shadow' flag -> real XMSS key files but 127.0.0.1 IPs.
    Then merges: real key files + real pubkeys into the shadow-config genesis.
    """
    reuse_keys = genesis_dir.exists() and _has_real_keys(genesis_dir, n)
    if reuse_keys:
        print(f"==> Reusing existing keys from {genesis_dir}")
    else:
        print(f"==> Generating real genesis for {n} nodes (subnet_count={subnet_count})...")

    with tempfile.TemporaryDirectory(prefix="qlean-real-shadow-") as tmp_cfg_dir_str:
        tmp_cfg = Path(tmp_cfg_dir_str)

        # Pass 1: generate genesis with shadow flag -> correct ENRs, fake pubkeys
        print("  [1/2] Generating shadow-config genesis (ENRs/IPs)...")
        _run_qlean(tmp_cfg, n, subnet_count, shadow=True)

        if reuse_keys:
            # Copy existing keys into the fresh shadow config
            print("  [2/2] Using existing key files...")
            src_keys = genesis_dir / "hash-sig-keys"
            dst_keys = tmp_cfg / "hash-sig-keys"
            shutil.copytree(src_keys, dst_keys, dirs_exist_ok=True)
        else:
            # Pass 2: generate genesis without shadow flag -> real key files
            print("  [2/2] Generating real key files...")
            with tempfile.TemporaryDirectory(prefix="qlean-real-keys-") as tmp_keys_dir_str:
                tmp_keys = Path(tmp_keys_dir_str)
                _run_qlean(tmp_keys, n, subnet_count, shadow=False)

                # Copy real key files (.ssz) into shadow genesis
                src_keys = tmp_keys / "hash-sig-keys"
                dst_keys = tmp_cfg / "hash-sig-keys"
                dst_keys.mkdir(parents=True, exist_ok=True)

                for f in src_keys.iterdir():
                    if f.suffix in (".ssz", ".json"):
                        shutil.copy2(f, dst_keys / f.name)

                # Copy the real manifest (has real pubkeys)
                real_manifest = src_keys / "validator-keys-manifest.yaml"
                if real_manifest.exists():
                    shutil.copy2(real_manifest, dst_keys / "validator-keys-manifest.yaml")

        # Read real pubkeys from the manifest
        manifest = dst_keys / "validator-keys-manifest.yaml"
        if not manifest.exists():
            die(f"validator-keys-manifest.yaml not found in {dst_keys}")
        real_pubkeys = _read_pubkeys_from_manifest(manifest)

        # Copy merged genesis to target directory
        if genesis_dir.exists():
            shutil.rmtree(genesis_dir)
        shutil.copytree(tmp_cfg, genesis_dir)

        # Patch pubkeys in config files (replace zero pubkeys with real ones)
        _patch_genesis_validators(
            genesis_dir / "config.yaml", real_pubkeys)
        _patch_file_pubkeys(
            genesis_dir / "annotated_validators.yaml", real_pubkeys)

    print(f"==> Genesis ready at {genesis_dir}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run qlean Shadow simulation with real signatures/aggregation")
    parser.add_argument("nodes", nargs="?", type=int, default=64,
                        help="Number of nodes (default: 64)")
    parser.add_argument("stop_time", nargs="?", default=STOP_TIME,
                        help=f"Simulation stop time (default: {STOP_TIME})")
    parser.add_argument("--no-build", action="store_true",
                        help="Skip Docker image existence check and build")
    parser.add_argument("--build", action="store_true",
                        help="Force rebuild Docker image")
    parser.add_argument("--generate-genesis", action="store_true",
                        help="Generate genesis directory with real XMSS keys")
    parser.add_argument("--generate-topology", action="store_true",
                        help="Generate GML topology with bandwidth and regions")
    parser.add_argument("--genesis-dir", type=str, default=None,
                        help="Path to genesis directory (default: /tmp/qlean-simulations/<n>-real)")
    parser.add_argument("--subnet-count", type=int, default=SUBNET_COUNT,
                        help=f"Number of aggregator/subnet nodes (default: {SUBNET_COUNT})")
    args = parser.parse_args()

    n = args.nodes
    stop_time = args.stop_time if args.stop_time.endswith("s") else f"{args.stop_time}s"
    max_bootnodes = MAX_BOOTNODES
    subnet_count = args.subnet_count

    # Determine genesis directory
    sim_dir = Path(f"/tmp/qlean-simulations/{n}-real")
    if args.genesis_dir:
        genesis_dir = Path(args.genesis_dir).resolve()
    else:
        genesis_dir = sim_dir

    topology_dir = sim_dir
    output_dir = Path(f"/tmp/qlean-real-sim-{n}/output")
    data_dir = Path(f"/tmp/qlean-real-sim-{n}/data")

    # Build or check image
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

    # Validate genesis directory
    if not genesis_dir.is_dir():
        die(
            f"Genesis directory not found: {genesis_dir}\n"
            f"  Use --generate-genesis to create it, or --genesis-dir to specify a path."
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
