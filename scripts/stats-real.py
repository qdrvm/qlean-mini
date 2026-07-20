#!/usr/bin/env python3
"""Print attestation propagation stats from a Real Shadow simulation run.

Usage:
  uv run scripts/stats-real.py           # default: 64 nodes, all slots
  uv run scripts/stats-real.py 4         # 4-node run
  uv run scripts/stats-real.py 64 5      # 64 nodes, focus on slot 5
  uv run scripts/stats-real.py 64 --all  # show all per-slot P99 values
"""

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path


def parse_events(data_dir: str):
    """Parse RECEIVE-ATTESTATION events from shadow stdout files."""
    hosts_dir = Path(data_dir) / "hosts"
    if not hosts_dir.is_dir():
        hosts_dir = Path(data_dir) / "shadow.data" / "hosts"
    if not hosts_dir.is_dir():
        print(f"ERROR: hosts directory not found under {data_dir}", file=sys.stderr)
        sys.exit(1)

    node_events: dict[str, list[dict]] = defaultdict(list)
    for host_dir in sorted(hosts_dir.iterdir()):
        if not host_dir.is_dir():
            continue
        for stdout_file in sorted(host_dir.glob("*.stdout")):
            with open(stdout_file) as f:
                for line in f:
                    if "RECEIVE-ATTESTATION" in line:
                        evt = _parse_line(line)
                        if evt:
                            evt["node"] = host_dir.name
                            node_events[host_dir.name].append(evt)

    total = sum(len(v) for v in node_events.values())
    if total == 0:
        print("ERROR: No RECEIVE-ATTESTATION events found", file=sys.stderr)
        sys.exit(1)
    return node_events


def _parse_line(line: str) -> dict | None:
    m = re.search(
        r'\["LEAN-INTEROP-TEST",\s*(\d+),\s*"RECEIVE-ATTESTATION",\s*\[(\d+),\s*\[([^\]]+)\]',
        line,
    )
    if not m:
        return None
    parts = [p.strip().strip('"') for p in m.group(3).split(",")]
    if len(parts) < 5:
        return None
    return {
        "ts_ms": int(m.group(1)),
        "validator_id": int(m.group(2)),
        "slot": int(parts[3]),
        "block_hash": parts[4],
    }


def compute_p99(node_events, genesis_ms, target_slot=None):
    """Compute per-slot P99 time-to-95%-coverage across all nodes."""
    slot_publishers: dict[int, set[int]] = defaultdict(set)
    for events in node_events.values():
        for evt in events:
            slot_publishers[evt["slot"]].add(evt["validator_id"])

    rows = []
    for slot in sorted(slot_publishers.keys()):
        if target_slot is not None and slot != target_slot:
            continue

        pubs = slot_publishers[slot]
        n_publishers = len(pubs)
        if n_publishers < 3:
            continue

        node_times = []
        for node, events in node_events.items():
            received = {}
            for evt in events:
                if evt["slot"] != slot:
                    continue
                offset = evt["ts_ms"] - genesis_ms
                if evt["validator_id"] not in received or offset < received[evt["validator_id"]]:
                    received[evt["validator_id"]] = offset

            if len(received) < 3:
                continue

            times = sorted(received.values())
            idx = min(len(times) - 1, int(0.95 * n_publishers) - 1)
            node_times.append(times[idx] - times[0])

        if node_times:
            s = sorted(node_times)
            n = len(s)
            rows.append((slot, n, n_publishers, s[n // 2], s[int(0.95 * n)], s[int(0.99 * n)]))
    return rows


def main():
    parser = argparse.ArgumentParser(description="Print attestation propagation stats for real simulation")
    parser.add_argument("nodes", nargs="?", type=int, default=64,
                        help="Number of nodes (default: 64)")
    parser.add_argument("slot", nargs="?", type=int, default=None,
                        help="Focus on a single slot (default: show summary)")
    parser.add_argument("--all", action="store_true",
                        help="Show all per-slot P99 values")
    args = parser.parse_args()

    data_dir = f"/tmp/qlean-real-sim-{args.nodes}/output/shadow.data"
    node_events = parse_events(data_dir)

    # Estimate genesis time (round to nearest second)
    genesis_ms = None
    for events in node_events.values():
        for evt in events:
            if genesis_ms is None or evt["ts_ms"] < genesis_ms:
                genesis_ms = evt["ts_ms"]
    genesis_ms = (genesis_ms // 1_000_000) * 1_000_000

    nodes = len(node_events)
    total = sum(len(v) for v in node_events.values())
    slots = sorted({e["slot"] for events in node_events.values() for e in events})
    print(f"Nodes: {nodes}  Events: {total}  Slots: {slots[0]}-{slots[-1]} ({len(slots)} slots)")
    print()

    rows = compute_p99(node_events, genesis_ms, target_slot=args.slot)

    if args.slot is not None:
        for slot, n, pubs, p50, p95, p99 in rows:
            print(f"Slot {slot}: {n} nodes, {pubs} publishers")
            print(f"  P50:  {p50:7.0f} ms")
            print(f"  P95:  {p95:7.0f} ms")
            print(f"  P99:  {p99:7.0f} ms")
            return

    if args.all:
        print(f"{'Slot':>5} {'Nodes':>5} {'Pubs':>5} {'P50(ms)':>8} {'P95(ms)':>8} {'P99(ms)':>8}")
        print("-" * 47)
        for slot, n, pubs, p50, p95, p99 in rows:
            print(f"{slot:5d} {n:5d} {pubs:5d} {p50:8.0f} {p95:8.0f} {p99:8.0f}")
        return

    # Summary: aggregate P99 across all (node, slot) pairs
    all_times = []
    for events in node_events.values():
        for evt in events:
            all_times.append(evt["ts_ms"] - genesis_ms)
    all_times.sort()
    n_all = len(all_times)

    print(f"Per-slot P99 time-to-95%-coverage ({len(rows)} slots):")
    for slot, n, pubs, p50, p95, p99 in rows:
        print(f"  Slot {slot:3d}: P99={p99:6.0f} ms  ({n} nodes, {pubs} pubs)")

    # Collect all per-node-per-slot P99 times for aggregate
    all_p99 = []
    for _, _, _, _, _, p99 in rows:
        all_p99.append(p99)
    all_p99.sort()
    n = len(all_p99)
    print(f"\nAggregate P99 across all slots: {all_p99[min(int(0.99 * n), n - 1)]:.0f} ms")


if __name__ == "__main__":
    main()
