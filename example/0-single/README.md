# 0-single — run a single-node devnet

This example shows how to start a one-node network using the `qlean` binary built from this repository.

Files in this folder:
- `genesis/config.yaml` — genesis settings (e.g., `GENESIS_TIME`, `VALIDATOR_COUNT`)
- `genesis/validators.yaml` — validator registry for this example
- `genesis/nodes.yaml` — static bootnodes list used at startup

Prerequisites:
- Build the project first. See the root README “Quick start” for setup and build steps.
- Run commands from the repository root so relative paths resolve correctly.

## Start the node

Example CLI command:

```zsh
./build/src/executable/qlean \
  --modules-dir ./build/src/modules \
  --bootnodes example/0-single/genesis/nodes.yaml \
  --validator-registry-path example/0-single/genesis/validators.yaml \
  --node-id node_0 \
  --genesis example/0-single/genesis/config.yaml \
  --node-key cb920fbda3b96e18f03e22825f4a5a61343ec43c7be1c8c4a717fffee2f4c4ce \
  --listen-addr /ip4/0.0.0.0/udp/9000/quic-v1
```

## What each flag means

- `--modules-dir ./build/src/modules`
  - Where the node looks for loadable modules built by this repository. The default build places them under `./build/src/modules`.
- `--bootnodes example/0-single/genesis/nodes.yaml`
  - A YAML file with a list of peers (ENRs or multiaddrs) used for initial connectivity.
- `--validator-registry-path example/0-single/genesis/validators.yaml`
  - The validator registry for this devnet. For a single-node network, it typically contains a single validator.
- `--node-id node_0`
  - A human-friendly identifier used in logs and for distinguishing nodes when running multiple instances.
- `--genesis <path>`
  - Genesis configuration describing initial chain parameters, such as `GENESIS_TIME` and the number of validators.
- `--node-key <hex>`
  - Hex-encoded libp2p private key. Using a fixed key gives a stable PeerId across restarts. You can generate one with:
    
    ```zsh
    ./build/src/executable/qlean key generate-node-key
    ```
- `--listen-addr /ip4/0.0.0.0/udp/9000/quic-v1`
  - libp2p multiaddress to bind the QUIC transport. Adjust the port if `9000` is taken, or bind to `127.0.0.1` if you want local-only access.

## Tips

- If the binary cannot find modules, double-check you built the project and the `--modules-dir` path is correct.
- You can run multiple nodes by copying this command and changing `--node-id`, `--node-key`, ports in `--listen-addr`, and using appropriate bootnodes.

