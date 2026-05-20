#!/usr/bin/env python3
"""Analyze attestation propagation time from Shadow simulation data.

Computes p99 time-to-95%-coverage for attestations across the network.

Usage:
  python3 analyze_propagation.py <shadow_data_dir> [--round N]

Outputs:
  - Per-round stats (mean, median, p95, p99 coverage times)
  - p99 across all (node, slot) pairs
"""

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path


def parse_shadow_timestamp(shadow_ts: str) -> float:
    """Parse Shadow timestamp like '00.01.01 00:00:03.800841' to seconds."""
    # Format: YY.MM.DD HH:MM:SS.ffffff (Shadow's format)
    # Return seconds from simulation start
    m = re.match(r'\d+\.\d+\.\d+\s+(\d+):(\d+):(\d+)\.(\d+)', shadow_ts)
    if m:
        h, m_val, s, us = m.groups()
        return int(h) * 3600 + int(m_val) * 60 + int(s) + int(us) / 1_000_000
    return 0.0


def parse_attestation_event(line: str) -> dict | None:
    """Parse a RECEIVE-ATTESTATION line from Shadow stdout.

    Format:
    ["LEAN-INTEROP-TEST", 946684803800, "RECEIVE-ATTESTATION",
     [validator_id, [source_slot, target_slot, slot, committee_index, "block_hash"]]]

    Returns dict with: sim_time_s, validator_id, slot, block_hash, node_name
    """
    # Extract JSON-like array from the line
    m = re.search(r'\["LEAN-INTEROP-TEST",\s*(\d+),\s*"RECEIVE-ATTESTATION",\s*\[(\d+),\s*\[([^\]]+)\]', line)
    if not m:
        return None

    ts_ms = int(m.group(1))
    validator_id = int(m.group(2))
    inner = m.group(3).strip()

    # Parse inner array: source_slot, target_slot, slot, committee_index, "block_hash"
    parts = [p.strip().strip('"') for p in inner.split(',')]
    if len(parts) < 5:
        return None

    slot = int(parts[2])  # The slot being attested to
    block_hash = parts[4]

    return {
        'ts_ms': ts_ms,
        'validator_id': validator_id,
        'slot': slot,
        'block_hash': block_hash,
    }


def analyze(data_dir: str, target_round: int | None = None):
    """Main analysis function."""
    data_path = Path(data_dir)
    if not data_path.is_dir():
        print(f"ERROR: {data_dir} is not a directory")
        sys.exit(1)

    # Collect all events: node_name -> [(ts_ms, validator_id, slot, block_hash)]
    node_events: dict[str, list[dict]] = defaultdict(list)

    # Host directories in shadow.data/hosts/
    hosts_dir = data_path / "hosts"
    if not hosts_dir.is_dir():
        # Try shadow.data directly
        hosts_dir = data_path / "shadow.data" / "hosts"
    if not hosts_dir.is_dir():
        print(f"ERROR: hosts directory not found under {data_dir}")
        sys.exit(1)

    node_count = 0
    for host_dir in sorted(hosts_dir.iterdir()):
        if not host_dir.is_dir():
            continue
        node_name = host_dir.name
        node_count += 1

        # Read all stdout files for this host
        for stdout_file in sorted(host_dir.glob("*.stdout")):
            with open(stdout_file) as f:
                for line in f:
                    if "RECEIVE-ATTESTATION" in line:
                        evt = parse_attestation_event(line)
                        if evt:
                            evt['node'] = node_name
                            node_events[node_name].append(evt)

    print(f"Nodes: {node_count}")
    total_events = sum(len(v) for v in node_events.values())
    print(f"Total RECEIVE-ATTESTATION events: {total_events}")

    if total_events == 0:
        print("WARNING: No RECEIVE-ATTESTATION events found. Falling back to PUBLISH-ATTESTATION.")
        # Fall back to PUBLISH-ATTESTATION as a proxy
        for host_dir in sorted(hosts_dir.iterdir()):
            if not host_dir.is_dir():
                continue
            node_name = host_dir.name
            for stdout_file in sorted(host_dir.glob("*.stdout")):
                with open(stdout_file) as f:
                    for line in f:
                        if "PUBLISH-ATTESTATION" in line:
                            evt = parse_attestation_event(line.replace("PUBLISH-ATTESTATION", "RECEIVE-ATTESTATION"))
                            if evt:
                                evt['node'] = node_name
                                node_events[node_name].append(evt)
        total_events = sum(len(v) for v in node_events.values())
        print(f"Total PUBLISH-ATTESTATION events (fallback): {total_events}")

    if total_events == 0:
        print("ERROR: No attestation events found at all")
        sys.exit(1)

    # Group events by slot
    slot_publishers: dict[int, set[int]] = defaultdict(set)
    for events in node_events.values():
        for evt in events:
            slot_publishers[evt['slot']].add(evt['validator_id'])

    print(f"Slots with attestations: {len(slot_publishers)}")
    if slot_publishers:
        slots = sorted(slot_publishers.keys())
        print(f"Slot range: {slots[0]} - {slots[-1]}")
        for s in slots[:5]:
            print(f"  Slot {s}: {len(slot_publishers[s])} unique publishers")

    # Determine genesis_time for time offset calculation
    genesis_time_ms = None
    for events in node_events.values():
        for evt in events:
            if genesis_time_ms is None:
                genesis_time_ms = evt['ts_ms']
            genesis_time_ms = min(genesis_time_ms, evt['ts_ms'])

    # Round genesis_time_ms down to nearest second boundary
    if genesis_time_ms:
        genesis_time_ms = (genesis_time_ms // 1_000_000) * 1_000_000
        print(f"Estimated genesis_time_ms: {genesis_time_ms}")

    # Per-slot analysis
    coverage_times: list[float] = []  # All (node, slot) coverage times in ms
    slot_stats: dict[int, dict] = {}  # Per-slot aggregate stats

    for slot in sorted(slot_publishers.keys()):
        publishers = slot_publishers[slot]
        n_publishers = len(publishers)
        if n_publishers < 3:
            continue  # Skip slots with too few publishers

        # For each receiving node, compute time to 95% coverage
        node_coverage: dict[str, float] = {}

        for node_name, events in node_events.items():
            # Get receive times for attestations from each publisher in this slot
            received: dict[int, float] = {}  # publisher -> earliest receive time (ms)
            for evt in events:
                if evt['slot'] != slot:
                    continue
                publisher = evt['validator_id']
                ts_offset = evt['ts_ms'] - genesis_time_ms if genesis_time_ms else evt['ts_ms']
                if publisher not in received or ts_offset < received[publisher]:
                    received[publisher] = ts_offset

            if len(received) == 0:
                continue

            # Sort by receive time
            sorted_times = sorted(received.values())
            # Time to 95% coverage: index at ceil(0.95 * n_publishers) - 1
            threshold_idx = min(len(sorted_times) - 1,
                               int(0.95 * n_publishers) - 1)
            if threshold_idx < 0:
                threshold_idx = 0

            # Time relative to first receive for this node+slot
            first_receive = sorted_times[0]
            coverage_time = sorted_times[threshold_idx] - first_receive
            node_coverage[node_name] = coverage_time

        if node_coverage:
            times = list(node_coverage.values())
            coverage_times.extend(times)

            times_sorted = sorted(times)
            n = len(times_sorted)
            slot_stats[slot] = {
                'n_publishers': n_publishers,
                'n_nodes': n,
                'mean_ms': sum(times) / n,
                'median_ms': times_sorted[n // 2],
                'p95_ms': times_sorted[int(0.95 * n)] if n > 1 else times_sorted[0],
                'p99_ms': times_sorted[int(0.99 * n)] if n > 1 else times_sorted[0],
            }

    # Filter rounds/slots if requested
    if target_round is not None:
        filtered_times = []
        for slot in sorted(slot_publishers.keys()):
            if slot == target_round:
                publishers = slot_publishers[slot]
                n_publishers = len(publishers)
                if n_publishers < 3:
                    continue
                for node_name, events in node_events.items():
                    received = {}
                    for evt in events:
                        if evt['slot'] != slot:
                            continue
                        publisher = evt['validator_id']
                        ts_offset = evt['ts_ms'] - genesis_time_ms if genesis_time_ms else evt['ts_ms']
                        if publisher not in received or ts_offset < received[publisher]:
                            received[publisher] = ts_offset
                    if len(received) == 0:
                        continue
                    sorted_times = sorted(received.values())
                    threshold_idx = min(len(sorted_times) - 1,
                                       int(0.95 * n_publishers) - 1)
                    if threshold_idx < 0:
                        threshold_idx = 0
                    first_receive = sorted_times[0]
                    coverage_time = sorted_times[threshold_idx] - first_receive
                    filtered_times.append(coverage_time)
        if filtered_times:
            print(f"\nTarget round {target_round}: {len(filtered_times)} measurements")
            fs = sorted(filtered_times)
            n = len(fs)
            print(f"  Mean:  {sum(fs)/n:.1f} ms")
            print(f"  P50:   {fs[n//2]:.1f} ms")
            print(f"  P95:   {fs[int(0.95*n)]:.1f} ms")
            print(f"  P99:   {fs[int(0.99*n)]:.1f} ms")
        else:
            print(f"No data for target round {target_round}")

    # Print per-slot stats
    print(f"\nPer-slot stats ({len(slot_stats)} slots):")
    for slot in sorted(slot_stats.keys())[:10]:
        s = slot_stats[slot]
        print(f"  Slot {slot:3d}: n={s['n_nodes']:3d} nodes, "
              f"{s['n_publishers']:3d} pubs, "
              f"mean={s['mean_ms']:7.1f}ms, "
              f"median={s['median_ms']:7.1f}ms, "
              f"p95={s['p95_ms']:7.1f}ms, "
              f"p99={s['p99_ms']:7.1f}ms")

    if len(slot_stats) > 10:
        print(f"  ... and {len(slot_stats) - 10} more slots")

    # Aggregate p99 across all (node, slot) pairs
    if coverage_times:
        sorted_all = sorted(coverage_times)
        n_all = len(sorted_all)
        p99_idx = int(0.99 * n_all)
        p99_idx = min(p99_idx, n_all - 1)

        print(f"\n{'='*60}")
        print(f"AGGREGATE P99 PROPAGATION TIME")
        print(f"{'='*60}")
        print(f"Total (node, slot) measurements: {n_all}")
        print(f"P50:  {sorted_all[n_all//2]:.1f} ms")
        print(f"P95:  {sorted_all[int(0.95*n_all)]:.1f} ms")
        print(f"P99:  {sorted_all[p99_idx]:.1f} ms")
        print(f"Max:  {sorted_all[-1]:.1f} ms")
        print(f"Mean: {sum(sorted_all)/n_all:.1f} ms")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Analyze attestation propagation time")
    parser.add_argument("data_dir", help="Shadow data directory (containing hosts/)")
    parser.add_argument("--round", type=int, help="Focus on a specific round/slot")
    args = parser.parse_args()
    analyze(args.data_dir, args.round)
