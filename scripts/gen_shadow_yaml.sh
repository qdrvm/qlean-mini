#!/usr/bin/env bash
# Generate a Shadow network YAML from a genesis folder (e.g., genesis_test)
# Default output name: shadow_network.yaml
# Usage:
#   gen_shadow_yaml.sh -g <genesis_dir> [-o <output_yaml>] [options]
# Options:
#   -g GENESIS_DIR           Path to the genesis directory containing config.yaml, validators.yaml, nodes.yaml, node_*.key (required)
#   -o OUTPUT_YAML           Output YAML path (default: ./shadow_network.yaml)
#   -t STOP_TIME             Shadow stop_time value (default: 60s)
#   -u UDP_BASE              Base UDP port for --listen-addr (default: 9000)
#   -p PROM_BASE             Base Prometheus port (default: 9100)
#   -i IP_BASE_LAST_OCTET    Base last octet for IPs starting at 10.0.0.X (default: 10)
#   -x QLEAN_PATH            Path to qlean executable (default: <repo_root>/build/src/executable/qlean)
#   -m MODULES_DIR           Path to modules dir (default: <repo_root>/build/src/modules)
#   -r PROJECT_ROOT          Project root to use for defaults (default: parent dir of this script)
#
# Notes:
# - Node count is inferred from node_*.key files in GENESIS_DIR.
# - Ports increment by +index per node.
# - IPs are assigned as 10.0.0.(IP_BASE_LAST_OCTET + index)
# - Paths are emitted literally in YAML and quoted; override with -x/-m if needed.

set -euo pipefail

print_usage() {
  sed -n '1,45p' "$0" | sed 's/^# \{0,1\}//'
}

# Defaults
OUTPUT_YAML="shadow_network.yaml"
STOP_TIME="60s"
UDP_BASE=9000
PROM_BASE=9100
IP_BASE_LAST_OCTET=10
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT_DEFAULT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ROOT="$PROJECT_ROOT_DEFAULT"
QLEAN_PATH_DEFAULT="$PROJECT_ROOT/build/src/executable/qlean"
MODULES_DIR_DEFAULT="$PROJECT_ROOT/build/src/modules"
QLEAN_PATH="$QLEAN_PATH_DEFAULT"
MODULES_DIR="$MODULES_DIR_DEFAULT"
GENESIS_DIR=""

while getopts ":g:o:t:u:p:i:x:m:r:h" opt; do
  case $opt in
    g) GENESIS_DIR="$OPTARG" ;;
    o) OUTPUT_YAML="$OPTARG" ;;
    t) STOP_TIME="$OPTARG" ;;
    u) UDP_BASE="$OPTARG" ;;
    p) PROM_BASE="$OPTARG" ;;
    i) IP_BASE_LAST_OCTET="$OPTARG" ;;
    x) QLEAN_PATH="$OPTARG" ;;
    m) MODULES_DIR="$OPTARG" ;;
    r) PROJECT_ROOT="$OPTARG" ; QLEAN_PATH_DEFAULT="$PROJECT_ROOT/build/src/executable/qlean"; MODULES_DIR_DEFAULT="$PROJECT_ROOT/build/src/modules" ;;
    h) print_usage; exit 0 ;;
    :) echo "Error: Option -$OPTARG requires an argument" >&2; print_usage; exit 2 ;;
    \?) echo "Error: Invalid option -$OPTARG" >&2; print_usage; exit 2 ;;
  esac
done

# Apply defaults that depend on PROJECT_ROOT if user didn't override
if [[ "$QLEAN_PATH" == "$QLEAN_PATH_DEFAULT" && ! -x "$QLEAN_PATH_DEFAULT" ]]; then
  # Keep default even if not built yet; just warn
  echo "Warning: qlean not found at $QLEAN_PATH_DEFAULT; ensure you build it or pass -x" >&2
fi
if [[ "$MODULES_DIR" == "$MODULES_DIR_DEFAULT" && ! -d "$MODULES_DIR_DEFAULT" ]]; then
  echo "Warning: modules dir not found at $MODULES_DIR_DEFAULT; ensure you build modules or pass -m" >&2
fi

# Validate genesis dir
if [[ -z "$GENESIS_DIR" ]]; then
  echo "Error: -g GENESIS_DIR is required" >&2
  print_usage
  exit 2
fi
if [[ ! -d "$GENESIS_DIR" ]]; then
  echo "Error: GENESIS_DIR '$GENESIS_DIR' does not exist or is not a directory" >&2
  exit 2
fi

# Resolve absolute paths using Python for macOS portability (realpath -f is not standard)
py_abspath() { python3 - "$1" <<'PY'
import os,sys
p=sys.argv[1]
print(os.path.abspath(p))
PY
}

GENESIS_DIR_ABS="$(py_abspath "$GENESIS_DIR")"
QLEAN_PATH_ABS="$(py_abspath "$QLEAN_PATH")"
MODULES_DIR_ABS="$(py_abspath "$MODULES_DIR")"
OUTPUT_YAML_ABS="$(py_abspath "$(dirname "$OUTPUT_YAML")")/$(basename "$OUTPUT_YAML")"

CONFIG_YAML="$GENESIS_DIR_ABS/config.yaml"
VALIDATORS_YAML="$GENESIS_DIR_ABS/validators.yaml"
VALIDATOR_CONFIG_YAML="$GENESIS_DIR_ABS/validator-config.yaml"
NODES_YAML="$GENESIS_DIR_ABS/nodes.yaml"

# Prefer validator-config.yaml for parsing enrFields (it's used in some genesis folders).
if [[ -f "$VALIDATOR_CONFIG_YAML" ]]; then
  PARSE_VALIDATOR_FILE="$VALIDATOR_CONFIG_YAML"
else
  PARSE_VALIDATOR_FILE="$VALIDATORS_YAML"
fi

for f in "$CONFIG_YAML" "$VALIDATORS_YAML" "$NODES_YAML"; do
  if [[ ! -f "$f" ]]; then
    echo "Error: required file missing in genesis dir: $f" >&2
    exit 2
  fi
done

# Collect node keys (portable, numerically sorted by index)
export GENESIS_DIR_ABS
NODE_KEY_FILES=()
while IFS= read -r line; do
  NODE_KEY_FILES+=("$line")
done < <(python3 - <<'PY'
import os, re, sys
root = os.environ.get('GENESIS_DIR_ABS')
pat = re.compile(r'^node_(\d+)\.key$')
items = []
for name in os.listdir(root):
    m = pat.match(name)
    if m:
        items.append((int(m.group(1)), os.path.join(root, name)))
for _, path in sorted(items, key=lambda x: x[0]):
    print(path)
PY
)
NODE_COUNT=${#NODE_KEY_FILES[@]}
if [[ "$NODE_COUNT" -eq 0 ]]; then
  echo "Error: no node_*.key files found in $GENESIS_DIR_ABS" >&2
  exit 2
fi

# Try to read validator count and warn on mismatch
if VAL_COUNT=$(grep -E '^\s*VALIDATOR_COUNT\s*:' "$CONFIG_YAML" | awk -F: '{gsub(/ /,"",$2); print $2}'); then
  if [[ -n "$VAL_COUNT" && "$VAL_COUNT" != "$NODE_COUNT" ]]; then
    echo "Warning: VALIDATOR_COUNT ($VAL_COUNT) != number of node_*.key files ($NODE_COUNT)" >&2
  fi
fi

# Parse validators.yaml to extract enrFields.ip and enrFields.quic per validator (if present).
# We prefer these values over generated defaults so shadow uses the same IPs/ports as the genesis.
VALIDATOR_IPS=()
VALIDATOR_QUICS=()
# This python snippet prints one line per node index: "<ip> <quic>" (empty strings if not found)
while IFS= read -r _line; do
  VALIDATOR_IPS+=("$(echo "$_line" | awk '{print $1}')")
  VALIDATOR_QUICS+=("$(echo "$_line" | awk '{print $2}')")
done < <(python3 - "$PARSE_VALIDATOR_FILE" "$NODE_COUNT" <<'PY'
import sys, re
path = sys.argv[1]
node_count = int(sys.argv[2])
mapping = {}
name = None
in_enr = False
with open(path) as f:
    for raw in f:
        line = raw.rstrip('\n')
        m = re.match(r'^\s*-\s*name:\s*(\S+)', line)
        if m:
            name = m.group(1)
            in_enr = False
            continue
        if re.match(r'^\s*enrFields:\s*', line):
            in_enr = True
            continue
        if in_enr and name is not None:
            m_ip = re.match(r'^\s*ip:\s*(\S+)', line)
            if m_ip:
                mapping.setdefault(name, {})['ip'] = m_ip.group(1)
                continue
            m_quic = re.match(r'^\s*quic:\s*(\S+)', line)
            if m_quic:
                mapping.setdefault(name, {})['quic'] = m_quic.group(1)
                continue
# Emit ip and quic for node_0 .. node_{N-1}
for i in range(node_count):
    nm = f'node_{i}'
    ent = mapping.get(nm, {})
    ip = ent.get('ip', '')
    quic = str(ent.get('quic', ''))
    print(ip + ' ' + quic)
PY
)

# Helper: YAML double-quoted string escape
yaml_escape() {
  local s="$1"
  s="${s//\\/\\\\}"   # escape backslashes
  s="${s//\"/\\\"}"  # escape double quotes
  printf '%s' "$s"
}

# Start writing YAML
mkdir -p "$(dirname "$OUTPUT_YAML_ABS")"
{
  printf "general:\n"
  printf "  stop_time: %s\n" "$STOP_TIME"
  printf "  model_unblocked_syscall_latency: true\n"
  printf "experimental:\n"
  printf "  native_preemption_enabled: true\n"
  printf "network:\n"
  printf "  graph:\n"
  printf "    type: 1_gbit_switch\n"
  printf "hosts:\n"

  for ((i=0; i<NODE_COUNT; i++)); do
    key_file="${NODE_KEY_FILES[$i]}"
    node_name="node$i"
    # Prefer the IP from validators.yaml if present; otherwise fall back to generated 10.0.0.<base+idx>
    if [[ -n "${VALIDATOR_IPS[$i]}" ]]; then
      ip="${VALIDATOR_IPS[$i]}"
    else
      ip_last=$((IP_BASE_LAST_OCTET + i))
      ip="10.0.0.$ip_last"
    fi

    # Prefer the quic port from validators.yaml if present; otherwise use UDP_BASE + index
    if [[ -n "${VALIDATOR_QUICS[$i]}" ]]; then
      udp_port="${VALIDATOR_QUICS[$i]}"
    else
      udp_port=$((UDP_BASE + i))
    fi

    prom_port=$((PROM_BASE + i))

    # Build args string
    args_str=(
      "--modules-dir" "$MODULES_DIR_ABS"
      "--bootnodes" "$NODES_YAML"
      "--genesis" "$CONFIG_YAML"
      "--validator-registry-path" "$VALIDATORS_YAML"
      "--node-id" "node_${i}"
      "--node-key" "$key_file"
      "--listen-addr" "/ip4/0.0.0.0/udp/${udp_port}/quic-v1"
      "--prometheus-port" "$prom_port"
    )
    # Join args preserving spaces
    IFS=' ' read -r -a _dummy <<< "" # reset
    joined="${args_str[*]}"

    printf "  %s:\n" "$node_name"
    printf "    network_node_id: 0\n"
    printf "    ip_addr: %s\n" "$ip"
    printf "    processes:\n"
    printf "      - path: %s\n" "$QLEAN_PATH_ABS"
    printf "        args: \"%s\"\n" "$(yaml_escape "$joined")"
    printf "        expected_final_state: running\n\n"
  done
} > "$OUTPUT_YAML_ABS"

echo "Wrote $OUTPUT_YAML_ABS with $NODE_COUNT node(s)."
