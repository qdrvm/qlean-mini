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

Before starting nodes, set `GENESIS_TIME` in `example/1-network/genesis/config.yaml` to a future Unix timestamp so the chain can start. For example, current time + 20 seconds. Also, clear any existing data repositories to avoid conflicts from previous runs.

```bash
rm -rf data/
future_time=$(( $(date +%s) + 20 ))
sed -i '' "s/GENESIS_TIME: .*/GENESIS_TIME: $future_time/" example/1-network/genesis/config.yaml
```

## Start the 4 validators

Open four terminals (or run in background) and launch each node with its own key and ports.

Node 0:

```bash
./build/src/executable/qlean \
  --modules-dir ./build/src/modules \
  --base-path data/node_0 \
  --bootnodes example/1-network/genesis/nodes.yaml \
  --genesis example/1-network/genesis/config.yaml \
  --validator-registry-path example/1-network/genesis/validators.yaml \
  --validator-keys-manifest example/1-network/genesis/validator-keys-manifest.yaml \
  --node-id node_0 \
  --node-key example/1-network/genesis/node_0.key \
  --xmss-pk example/1-network/genesis/validator_0_pk.json \
  --xmss-sk example/1-network/genesis/validator_0_sk.json \
  --listen-addr /ip4/0.0.0.0/udp/9000/quic-v1 \
  --prometheus-port 9100
```

Node 1:

```bash
./build/src/executable/qlean \
  --modules-dir ./build/src/modules \
  --base-path data/node_1 \
  --bootnodes example/1-network/genesis/nodes.yaml \
  --genesis example/1-network/genesis/config.yaml \
  --validator-registry-path example/1-network/genesis/validators.yaml \
  --validator-keys-manifest example/1-network/genesis/validator-keys-manifest.yaml \
  --node-id node_1 \
  --node-key example/1-network/genesis/node_1.key \
  --xmss-pk example/1-network/genesis/validator_1_pk.json \
  --xmss-sk example/1-network/genesis/validator_1_sk.json \
  --listen-addr /ip4/0.0.0.0/udp/9001/quic-v1 \
  --prometheus-port 9101
```

Node 2:

```bash
./build/src/executable/qlean \
  --modules-dir ./build/src/modules \
  --base-path data/node_2 \
  --bootnodes example/1-network/genesis/nodes.yaml \
  --genesis example/1-network/genesis/config.yaml \
  --validator-registry-path example/1-network/genesis/validators.yaml \
  --validator-keys-manifest example/1-network/genesis/validator-keys-manifest.yaml \
  --node-id node_2 \
  --node-key example/1-network/genesis/node_2.key \
  --xmss-pk example/1-network/genesis/validator_2_pk.json \
  --xmss-sk example/1-network/genesis/validator_2_sk.json \
  --listen-addr /ip4/0.0.0.0/udp/9002/quic-v1 \
  --prometheus-port 9102
```

Node 3:

```bash
./build/src/executable/qlean \
  --modules-dir ./build/src/modules \
  --base-path data/node_3 \
  --bootnodes example/1-network/genesis/nodes.yaml \
  --genesis example/1-network/genesis/config.yaml \
  --validator-registry-path example/1-network/genesis/validators.yaml \
  --validator-keys-manifest example/1-network/genesis/validator-keys-manifest.yaml \
  --node-id node_3 \
  --node-key example/1-network/genesis/node_3.key \
  --xmss-pk example/1-network/genesis/validator_3_pk.json \
  --xmss-sk example/1-network/genesis/validator_3_sk.json \
  --listen-addr /ip4/0.0.0.0/udp/9003/quic-v1 \
  --prometheus-port 9103
```

## Notes and tips

- Start Node 0 first so others can discover it via the bootnodes list; the rest can follow in any order.
- Ports (9000–9003) must be free; adjust if they’re taken. Prometheus ports (9100–9103) are optional and can be changed or omitted.
- For an explanation of the common flags, see `example/0-single/README.md` (the meanings are the same).
