#!/usr/bin/env python3
"""Generate Shadow topology with geo latencies and bandwidth tiers.

Usage:
  python3 gen_topology.py <node_count> <output_dir> [--seed SEED]

Produces:
  - topology.gml: graph with per-edge latencies
  - bandwidths.json: {"node_0": "50 Mbit", ...} bandwidth assignments
"""

import argparse
import json
import math
import random
import sys
from pathlib import Path

# Simplified latency model: 6 regions with approximate RTTs (ms)
REGIONS = {
    "us-east":    {"us-east": 20,  "us-west": 60,  "europe": 80,  "asia": 150, "sa": 120, "africa": 180},
    "us-west":    {"us-east": 60,  "us-west": 20,  "europe": 130, "asia": 110, "sa": 160, "africa": 200},
    "europe":     {"us-east": 80,  "us-west": 130, "europe": 15,  "asia": 100, "sa": 170, "africa": 80},
    "asia":       {"us-east": 150, "us-west": 110, "europe": 100, "asia": 20,  "sa": 250, "africa": 160},
    "sa":         {"us-east": 120, "us-west": 160, "europe": 170, "asia": 250, "sa": 25,  "africa": 220},
    "africa":     {"us-east": 180, "us-west": 200, "europe": 80,  "asia": 160, "sa": 220, "africa": 30},
}

# Node distribution weights (roughly proportional to validator distribution)
REGION_WEIGHTS = {
    "us-east": 0.30,
    "us-west": 0.15,
    "europe":  0.25,
    "asia":    0.20,
    "sa":      0.05,
    "africa":  0.05,
}

SUPERNODE_FRACTION = 0.05  # 5% of nodes get 1Gbps
SUPERNODE_BANDWIDTH = "1 Gbit"
NORMAL_BANDWIDTH = "50 Mbit"
JITTER_RATIO = 0.3          # ±30% jitter on latency
LINK_PACKET_LOSS = 0.0


def assign_regions(node_count: int, rng: random.Random) -> dict[str, str]:
    """Assign each node to a weighted random region."""
    regions = list(REGION_WEIGHTS.keys())
    weights = [REGION_WEIGHTS[r] for r in regions]
    # Normalize weights
    total = sum(weights)
    probs = [w / total for w in weights]

    assignments = {}
    for i in range(node_count):
        assignments[f"node_{i}"] = rng.choices(regions, weights=probs, k=1)[0]
    return assignments


def bandwidth_tiers(node_count: int, rng: random.Random) -> dict[str, str]:
    """Assign bandwidth tiers: 5% supernodes, 95% normal."""
    n_super = max(1, int(node_count * SUPERNODE_FRACTION))
    indices = list(range(node_count))
    rng.shuffle(indices)
    super_indices = set(indices[:n_super])

    tiers = {}
    for i in range(node_count):
        tiers[f"node_{i}"] = SUPERNODE_BANDWIDTH if i in super_indices else NORMAL_BANDWIDTH
    return tiers


def generate_gml(node_count: int,
                 region_assignments: dict[str, str],
                 bandwidths: dict[str, str],
                 rng: random.Random) -> str:
    """Generate a GML graph with full-mesh geo latencies.

    Uses a central switch topology with per-host latencies to the switch.
    The switch itself has 10 Gbit bandwidth, hosts get their tier bandwidth.
    """
    lines = ["graph [", "  directed 0"]

    # Calculate combined bandwidth capacity for switch
    n_super = sum(1 for b in bandwidths.values() if "Gbit" in b)
    n_normal = node_count - n_super

    # Host nodes
    for i in range(node_count):
        name = f"node_{i}"
        region = region_assignments[name]
        bw = bandwidths[name]
        lines.append(f"  node [")
        lines.append(f"    id {i}")
        lines.append(f'    host_bandwidth_up "{bw}"')
        lines.append(f'    host_bandwidth_down "{bw}"')
        lines.append(f'    label "{name}"')
        lines.append(f'    region "{region}"')
        lines.append(f"  ]")

    # Central switch
    switch_id = node_count
    lines.append(f"  node [")
    lines.append(f"    id {switch_id}")
    lines.append(f'    host_bandwidth_up "10 Gbit"')
    lines.append(f'    host_bandwidth_down "10 Gbit"')
    lines.append(f'    label "switch"')
    lines.append(f"  ]")

    # Self-loop for each host (host ↔ host on same machine won't exist,
    # but Shadow needs them for localhost-equivalent routing)
    for i in range(node_count):
        lines.append(f"  edge [")
        lines.append(f"    source {i}")
        lines.append(f"    target {i}")
        lines.append(f'    latency "1 ms"')
        lines.append(f"    packet_loss {LINK_PACKET_LOSS}")
        lines.append(f"  ]")

    # Switch self-loop
    lines.append(f"  edge [")
    lines.append(f"    source {switch_id}")
    lines.append(f"    target {switch_id}")
    lines.append(f'    latency "1 ms"')
    lines.append(f"    packet_loss {LINK_PACKET_LOSS}")
    lines.append(f"  ]")

    # Edges from each host to central switch with geo latency
    for i in range(node_count):
        name = f"node_{i}"
        region = region_assignments[name]
        # Use intra-region latency as base for host→switch link
        # The switch is "in the cloud", so use the host's self-latency as base
        base_ms = REGIONS[region][region] * 0.5
        # Add jitter
        jitter = base_ms * JITTER_RATIO * (rng.random() * 2 - 1)
        latency = max(0.5, base_ms + jitter)

        lines.append(f"  edge [")
        lines.append(f"    source {i}")
        lines.append(f"    target {switch_id}")
        lines.append(f'    latency "{max(1, round(latency))} ms"')
        lines.append(f"    packet_loss {LINK_PACKET_LOSS}")
        lines.append(f"  ]")

    # Full mesh edges between hosts (for peer-to-peer latency)
    # Only generate edges where latency is geo-dependent
    for i in range(node_count):
        name_i = f"node_{i}"
        region_i = region_assignments[name_i]
        for j in range(i + 1, node_count):
            name_j = f"node_{j}"
            region_j = region_assignments[name_j]

            base_ms = REGIONS[region_i].get(region_j, 200)
            jitter = base_ms * JITTER_RATIO * (rng.random() * 2 - 1)
            latency = max(0.5, base_ms + jitter)

            lines.append(f"  edge [")
            lines.append(f"    source {i}")
            lines.append(f"    target {j}")
            lines.append(f'    latency "{max(1, round(latency))} ms"')
            lines.append(f"    packet_loss {LINK_PACKET_LOSS}")
            lines.append(f"  ]")

    lines.append("]")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate Shadow topology with geo latencies")
    parser.add_argument("node_count", type=int, help="Number of nodes")
    parser.add_argument("output_dir", type=str, help="Output directory")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    args = parser.parse_args()

    rng = random.Random(args.seed)
    out = Path(args.output_dir)
    out.mkdir(parents=True, exist_ok=True)

    regions = assign_regions(args.node_count, rng)
    bandwidths = bandwidth_tiers(args.node_count, rng)

    # Write topology.gml
    gml = generate_gml(args.node_count, regions, bandwidths, rng)
    gml_path = out / "topology.gml"
    gml_path.write_text(gml)
    print(f"Wrote {gml_path} ({args.node_count} nodes, {gml_path.stat().st_size} bytes)")

    # Write bandwidths.json
    bw_path = out / "bandwidths.json"
    bw_path.write_text(json.dumps(bandwidths, indent=2))
    print(f"Wrote {bw_path}")

    # Write region assignments for reference
    region_path = out / "regions.json"
    region_path.write_text(json.dumps(regions, indent=2))
    print(f"Wrote {region_path}")

    # Print summary
    n_super = sum(1 for b in bandwidths.values() if "Gbit" in b)
    region_counts = {}
    for r in regions.values():
        region_counts[r] = region_counts.get(r, 0) + 1
    print(f"Summary: {args.node_count} nodes, {n_super} supernodes, regions={region_counts}")


if __name__ == "__main__":
    main()
