#!/bin/bash
set -euo pipefail

QLEAN_BIN="${QLEAN_BIN:-/opt/qlean/bin/qlean}"
GENESIS_DIR="${1:-/genesis}"
OUTPUT_DIR="${OUTPUT_DIR:-/output}"
STOP_TIME="${STOP_TIME:-120s}"
TOPOLOGY_DIR="${TOPOLOGY_DIR:-/topology}"
UDP_PORT_BASE="${UDP_PORT_BASE:-10000}"
METRICS_PORT_BASE="${METRICS_PORT_BASE:-9100}"
MAX_BOOTNODES="${MAX_BOOTNODES:-25}"

echo "==> Qlean Shadow Simulation"
echo "==> Genesis dir:  ${GENESIS_DIR}"
echo "==> Output dir:   ${OUTPUT_DIR}"
echo "==> Topology dir: ${TOPOLOGY_DIR}"
echo "==> Stop time:    ${STOP_TIME}"
echo "==> Max bootnodes: ${MAX_BOOTNODES}"
echo "==> Qlean bin:    ${QLEAN_BIN}"

mkdir -p "${OUTPUT_DIR}"

# Verify genesis directory
if [ ! -d "${GENESIS_DIR}" ]; then
    echo "ERROR: Genesis directory not found: ${GENESIS_DIR}"
    exit 1
fi

CONFIG_YAML="${GENESIS_DIR}/config.yaml"
NODES_YAML="${GENESIS_DIR}/nodes.yaml"
VALIDATORS_YAML="${GENESIS_DIR}/validators.yaml"

for f in "${CONFIG_YAML}" "${NODES_YAML}" "${VALIDATORS_YAML}"; do
    if [ ! -f "$f" ]; then
        echo "ERROR: Required file missing: $f"
        exit 1
    fi
done

# Count nodes from node key files
NODE_COUNT=$(ls -1 "${GENESIS_DIR}"/node_*.key 2>/dev/null | wc -l)
if [ "$NODE_COUNT" -eq 0 ]; then
    echo "ERROR: No node_*.key files found in ${GENESIS_DIR}"
    exit 1
fi
echo "==> Node count: ${NODE_COUNT}"

# Genesis time (Shadow uses simulated time)
GENESIS_TIME=$(grep 'GENESIS_TIME:' "${CONFIG_YAML}" | awk -F': ' '{print $2}')
echo "==> GENESIS_TIME: ${GENESIS_TIME}"

# Load bandwidth tiers if available
declare -A BANDWIDTHS
if [ -f "${TOPOLOGY_DIR}/bandwidths.json" ]; then
    echo "==> Loading bandwidth tiers from ${TOPOLOGY_DIR}/bandwidths.json"
    while IFS="=" read -r key value; do
        BANDWIDTHS["$key"]="$value"
    done < <(python3 -c "
import json
with open('${TOPOLOGY_DIR}/bandwidths.json') as f:
    bw = json.load(f)
for k, v in bw.items():
    print(f'{k}={v}')
")
fi

# Check for GML topology
GML_FILE="${TOPOLOGY_DIR}/topology.gml"
USE_GML=false
if [ -f "${GML_FILE}" ]; then
    USE_GML=true
    echo "==> Using GML topology: ${GML_FILE}"
else
    echo "==> No GML file found, using 1_gbit_switch"
fi

# Generate shadow.yaml
SHADOW_YAML="${OUTPUT_DIR}/shadow.yaml"
echo "==> Generating ${SHADOW_YAML}..."

if $USE_GML; then
    cat > "${SHADOW_YAML}" << YAMLHEAD
general:
  stop_time: ${STOP_TIME}
  model_unblocked_syscall_latency: true
experimental:
  native_preemption_enabled: true
network:
  graph:
    type: gml
    file:
      path: ${GML_FILE}
  use_shortest_path: true
hosts:
YAMLHEAD
else
    cat > "${SHADOW_YAML}" << YAMLHEAD
general:
  stop_time: ${STOP_TIME}
  model_unblocked_syscall_latency: true
experimental:
  native_preemption_enabled: true
network:
  graph:
    type: 1_gbit_switch
hosts:
YAMLHEAD
fi

for ((i=0; i<NODE_COUNT; i++)); do
    NODE_NAME="node_${i}"
    SHADOW_HOST="node${i}"
    IP_LAST=$((i))
    UDP_PORT=$((UDP_PORT_BASE + i))
    METRICS_PORT=$((METRICS_PORT_BASE + i))
    NODE_KEY_FILE="${GENESIS_DIR}/node_${i}.key"
    DATA_DIR="/data/${NODE_NAME}"

    # Bandwidth: use tier from bandwidths.json or default 50 Mbit
    BW="${BANDWIDTHS[${NODE_NAME}]:-50 Mbit}"

    ARGS="--genesis-dir ${GENESIS_DIR}"
    ARGS="${ARGS} --bootnodes ${NODES_YAML}"
    ARGS="${ARGS} --node-id ${NODE_NAME}"
    ARGS="${ARGS} --node-key ${NODE_KEY_FILE}"
    ARGS="${ARGS} --listen-addr /ip4/0.0.0.0/udp/${UDP_PORT}/quic-v1"
    ARGS="${ARGS} --metrics-port ${METRICS_PORT}"
    ARGS="${ARGS} --base-path ${DATA_DIR}"
    ARGS="${ARGS} --max-bootnodes ${MAX_BOOTNODES}"

    # Check if this node is an aggregator (from validator-config.yaml)
    if [ -f "${GENESIS_DIR}/validator-config.yaml" ]; then
        IS_AGG=$(awk -v name="${NODE_NAME}" '
            /^[[:space:]]*-[[:space:]]*name:/ {
                if (in_block && agg) { found = 1; exit }
                in_block = 0; agg = 0
                if ($0 ~ name "$") { in_block = 1 }
                next
            }
            in_block && /is_aggregator:[[:space:]]*[Tt]rue/ { agg = 1 }
            END { if (agg || found) print "true" }
        ' "${GENESIS_DIR}/validator-config.yaml")
        if [ "${IS_AGG}" = "true" ]; then
            ARGS="${ARGS} --is-aggregator"
        fi
    fi

    # Escape for YAML double-quoted string
    ARGS_ESCAPED="${ARGS//\\/\\\\}"
    ARGS_ESCAPED="${ARGS_ESCAPED//\"/\\\"}"

    # network_node_id: use node index for GML, 0 for 1_gbit_switch
    if $USE_GML; then
        NET_NODE_ID=${i}
    else
        NET_NODE_ID=0
    fi

    cat >> "${SHADOW_YAML}" << YAMLHOST
  ${SHADOW_HOST}:
    network_node_id: ${NET_NODE_ID}
    ip_addr: 10.0.0.${IP_LAST}
    bandwidth_up: "${BW}"
    bandwidth_down: "${BW}"
    processes:
      - path: ${QLEAN_BIN}
        args: "${ARGS_ESCAPED}"
        expected_final_state: running

YAMLHOST
done

echo "==> Shadow config written (${NODE_COUNT} hosts)"

# Clean old shadow data
rm -rf "${OUTPUT_DIR}/shadow.data"

echo "==> Starting Shadow simulation..."
cd "${OUTPUT_DIR}"
shadow --progress true ${SHADOW_FLAGS:-} "${SHADOW_YAML}"

echo "==> Simulation complete. Data in ${OUTPUT_DIR}/shadow.data/"
