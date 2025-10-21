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
sed -i "s/GENESIS_TIME: .*/GENESIS_TIME: $future_time/" example/1-network/genesis/config.yaml
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
  --node-key cb920fbda3b96e18f03e22825f4a5a61343ec43c7be1c8c4a717fffee2f4c4ce \
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
  --node-key a87e7d23bb1de4613b67002b700bce41e031f4ab1529a3436bd73c893ea039b3 \
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
  --node-key f2f53f6acf312c5e92c2a611bbca7a1932b4db0b9e0c43bec413badca9b76760 \
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
  --node-key fa5ddbec80f964d17d28221c2c5bac0f4a3f9cfcf4b86674e605f459e195a1c4 \
  --listen-addr /ip4/0.0.0.0/udp/9003/quic-v1 \
  --prometheus-port 9103
```

## Notes and tips

- Start Node 0 first so others can discover it via the bootnodes list; the rest can follow in any order.
- Ports (9000–9003) must be free; adjust if they’re taken. Prometheus ports (9100–9103) are optional and can be changed or omitted.
- For an explanation of the common flags, see `example/0-single/README.md` (the meanings are the same).
