# 1-network — run a 4-validator devnet

This example shows how to start a 4-validator network using the `qlean` binary built from this repository.

Files in this folder:
- `genesis/config.yaml` — genesis settings (e.g., `GENESIS_TIME`, `VALIDATOR_COUNT`)
- `genesis/validators.yaml` — validator registry for this network
- `genesis/nodes.yaml` — bootnodes list used at startup

Prerequisites:
- Build the project first. See the root README “Quick start” for setup and build steps.
- Run commands from the repository root so relative paths resolve correctly.

## Prepare genesis time

Before starting nodes, set `GENESIS_TIME` in `example/1-network/genesis/config.yaml` to a future Unix timestamp so the chain can start. For example, current time + 20 seconds:

```bash
future_time=$(( $(date +%s) + 20 ))
sed -i '' "s/GENESIS_TIME: .*/GENESIS_TIME: $future_time/" example/1-network/genesis/config.yaml
```

## Start the 4 validators

Open four terminals (or run in background) and launch each node with its own key and ports.

Node 0:

```bash
./build/src/executable/qlean \
  --modules-dir ./build/src/modules \
  --bootnodes example/1-network/genesis/nodes.yaml \
  --genesis example/1-network/genesis/config.yaml \
  --validator-registry-path example/1-network/genesis/validators.yaml \
  --node-id node_0 \
  --node-key 0000000000000000010000000000000002000000000000000300000000000000 \
  --listen-addr /ip4/0.0.0.0/udp/9000/quic-v1 \
  --prometheus-port 9100
```

Node 1:

```bash
./build/src/executable/qlean \
  --modules-dir ./build/src/modules \
  --bootnodes example/1-network/genesis/nodes.yaml \
  --genesis example/1-network/genesis/config.yaml \
  --validator-registry-path example/1-network/genesis/validators.yaml \
  --node-id node_1 \
  --node-key 0100000000000000020000000000000003000000000000000400000000000000 \
  --listen-addr /ip4/0.0.0.0/udp/9001/quic-v1 \
  --prometheus-port 9101
```

Node 2:

```bash
./build/src/executable/qlean \
  --modules-dir ./build/src/modules \
  --bootnodes example/1-network/genesis/nodes.yaml \
  --genesis example/1-network/genesis/config.yaml \
  --validator-registry-path example/1-network/genesis/validators.yaml \
  --node-id node_2 \
  --node-key 0200000000000000030000000000000004000000000000000500000000000000 \
  --listen-addr /ip4/0.0.0.0/udp/9002/quic-v1 \
  --prometheus-port 9102
```

Node 3:

```bash
./build/src/executable/qlean \
  --modules-dir ./build/src/modules \
  --bootnodes example/1-network/genesis/nodes.yaml \
  --genesis example/1-network/genesis/config.yaml \
  --validator-registry-path example/1-network/genesis/validators.yaml \
  --node-id node_3 \
  --node-key 0300000000000000040000000000000005000000000000000600000000000000 \
  --listen-addr /ip4/0.0.0.0/udp/9003/quic-v1 \
  --prometheus-port 9103
```

## Notes and tips

- Start Node 0 first so others can discover it via the bootnodes list; the rest can follow in any order.
- Ports (9000–9003) must be free; adjust if they’re taken. Prometheus ports (9100–9103) are optional and can be changed or omitted.
- For an explanation of the common flags, see `example/0-single/README.md` (the meanings are the same).
