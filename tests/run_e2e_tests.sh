#!/usr/bin/env bash
# End-to-end stress & integration tests for the unified htsim_uec binary
# Covers: larger topologies, heavy workloads, all routing combos, all-to-all,
#         FT 1024 nodes, SF p7q9 (1134 hosts), bisection traffic, mixed sizes
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
HTSIM_DIR="$REPO_DIR"
UEC_BIN="$HTSIM_DIR/htsim/sim/datacenter/htsim_uec"
TOPO_DIR="$HTSIM_DIR/htsim/sim/datacenter/topologies"
TEST_DIR="$SCRIPT_DIR"
TM_DIR="$TEST_DIR/e2e_tms"
OUT_DIR="$TEST_DIR/e2e_out"

mkdir -p "$TM_DIR" "$OUT_DIR"

PASS=0
FAIL=0
TOTAL=0

# ============================================================
# Helper
# ============================================================
run_e2e_test() {
    local label="$1"
    local cmd="$2"
    local expected_flows="$3"  # 0 = just check basic completion
    local timeout_sec="${4:-120}"

    TOTAL=$((TOTAL + 1))
    local outfile="$OUT_DIR/out_${label}.txt"
    echo -n "  [$TOTAL] $label ... "

    if timeout "$timeout_sec" bash -c "$cmd" > "$outfile" 2>&1; then
        local exit_code=0
    else
        local exit_code=$?
    fi

    if [ $exit_code -ne 0 ]; then
        if [ $exit_code -eq 124 ]; then
            echo "FAIL (timeout after ${timeout_sec}s)"
        else
            echo "FAIL (exit $exit_code)"
            tail -5 "$outfile"
        fi
        FAIL=$((FAIL + 1)); return
    fi

    if ! grep -q "Done" "$outfile"; then
        echo "FAIL (no 'Done')"
        tail -5 "$outfile"
        FAIL=$((FAIL + 1)); return
    fi

    local stats_line new_pkts ack_pkts rtx_pkts nack_pkts bounced_pkts
    stats_line=$(grep "^New:" "$outfile" || grep "New:" "$outfile" | tail -1 || true)
    if [ -z "$stats_line" ]; then
        echo "FAIL (no stats)"
        FAIL=$((FAIL + 1)); return
    fi
    new_pkts=$(echo "$stats_line" | grep -oP 'New: \K[0-9]+')
    ack_pkts=$(echo "$stats_line" | grep -oP 'ACKs: \K[0-9]+')
    rtx_pkts=$(echo "$stats_line" | grep -oP 'Rtx: \K[0-9]+' || echo 0)
    nack_pkts=$(echo "$stats_line" | grep -oP 'NACKs: \K[0-9]+' || echo 0)
    bounced_pkts=$(echo "$stats_line" | grep -oP 'Bounced: \K[0-9]+' || echo 0)

    if [ "$new_pkts" -eq 0 ] || [ "$ack_pkts" -eq 0 ]; then
        echo "FAIL (New=$new_pkts ACK=$ack_pkts)"
        FAIL=$((FAIL + 1)); return
    fi

    if [ "$expected_flows" -gt 0 ]; then
        local finished
        finished=$(grep -c "finished at" "$outfile" || echo 0)
        if [ "$finished" -ne "$expected_flows" ]; then
            echo "FAIL (finished $finished of $expected_flows flows)"
            FAIL=$((FAIL + 1)); return
        fi
    fi

    echo "PASS | New:$new_pkts Rtx:$rtx_pkts ACK:$ack_pkts NACK:$nack_pkts Bounce:$bounced_pkts"
    PASS=$((PASS + 1))
}

# ============================================================
# TM generators
# ============================================================
make_bisection_tm() {
    local num_hosts=$1 outfile=$2
    local half=$((num_hosts / 2))
    {
        echo "Nodes $num_hosts"
        echo "Connections $half"
        for i in $(seq 0 $((half - 1))); do
            echo "$i->$((i + half)) start 0 size 1048576"
        done
    } > "$outfile"
}

make_all_to_all_tm() {
    local num_hosts=$1 outfile=$2 max_flows=${3:-0}
    local count=0
    local lines=""
    for src in $(seq 0 $((num_hosts - 1))); do
        for dst in $(seq 0 $((num_hosts - 1))); do
            if [ "$src" -ne "$dst" ]; then
                lines+="$src->$dst start 0 size 4096"$'\n'
                count=$((count + 1))
                if [ "$max_flows" -gt 0 ] && [ "$count" -ge "$max_flows" ]; then
                    break 2
                fi
            fi
        done
    done
    {
        echo "Nodes $num_hosts"
        echo "Connections $count"
        printf "%s" "$lines"
    } > "$outfile"
}

make_random_permutation_tm() {
    local num_hosts=$1 outfile=$2 flow_size=${3:-1048576}
    # Each host sends to (host + num_hosts/2) % num_hosts — a simple permutation
    local half=$((num_hosts / 2))
    {
        echo "Nodes $num_hosts"
        echo "Connections $num_hosts"
        for src in $(seq 0 $((num_hosts - 1))); do
            local dst=$(( (src + half) % num_hosts ))
            echo "$src->$dst start 0 size $flow_size"
        done
    } > "$outfile"
}

make_heavy_incast_tm() {
    local num_hosts=$1 outfile=$2 num_senders=$3 target=$4
    {
        echo "Nodes $num_hosts"
        echo "Connections $num_senders"
        for src in $(seq 0 $((num_senders - 1))); do
            local s=$src
            [ "$s" -eq "$target" ] && s=$((num_hosts - 1))
            echo "$s->$target start 0 size 1048576"
        done
    } > "$outfile"
}

make_mixed_size_tm() {
    local num_hosts=$1 outfile=$2
    # Flows with varying sizes: 4KB, 64KB, 256KB, 1MB, 4MB
    {
        echo "Nodes $num_hosts"
        echo "Connections 5"
        echo "0->$((num_hosts/5)) start 0 size 4096"
        echo "$((num_hosts/5))->$((2*num_hosts/5)) start 0 size 65536"
        echo "$((2*num_hosts/5))->$((3*num_hosts/5)) start 0 size 262144"
        echo "$((3*num_hosts/5))->$((4*num_hosts/5)) start 0 size 1048576"
        echo "1->$((num_hosts-1)) start 0 size 4194304"
    } > "$outfile"
}

# ============================================================
# Generate TMs
# ============================================================
echo "=== Generating E2E traffic matrices ==="

# --- Fat Tree 128 ---
make_bisection_tm 128 "$TM_DIR/ft128_bisection.tm"
make_random_permutation_tm 128 "$TM_DIR/ft128_permutation.tm"
make_heavy_incast_tm 128 "$TM_DIR/ft128_incast32.tm" 32 64
make_mixed_size_tm 128 "$TM_DIR/ft128_mixed.tm"

# --- Fat Tree 1024 ---
cat > "$TM_DIR/ft1024_1flow.tm" <<'EOF'
Nodes 1024
Connections 1
0->1023 start 0 size 1048576
EOF

cat > "$TM_DIR/ft1024_20flow.tm" <<EOF
Nodes 1024
Connections 20
$(for i in $(seq 0 19); do echo "$((i*50))->$((1023 - i*50)) start 0 size 1048576"; done)
EOF

make_heavy_incast_tm 1024 "$TM_DIR/ft1024_incast32.tm" 32 512
make_mixed_size_tm 1024 "$TM_DIR/ft1024_mixed.tm"

# --- Dragonfly p3a6h3 (342 hosts) ---
make_bisection_tm 342 "$TM_DIR/df342_bisection.tm"
make_random_permutation_tm 342 "$TM_DIR/df342_permutation.tm"
make_heavy_incast_tm 342 "$TM_DIR/df342_incast16.tm" 16 170
make_mixed_size_tm 342 "$TM_DIR/df342_mixed.tm"

# --- Dragonfly p4a8h4 (1056 hosts) ---
cat > "$TM_DIR/df1056_20flow.tm" <<EOF
Nodes 1056
Connections 20
$(for i in $(seq 0 19); do echo "$((i*50))->$((1055 - i*50)) start 0 size 1048576"; done)
EOF
make_heavy_incast_tm 1056 "$TM_DIR/df1056_incast16.tm" 16 528
make_mixed_size_tm 1056 "$TM_DIR/df1056_mixed.tm"

# --- Slimfly p4q5 (200 hosts) ---
make_bisection_tm 200 "$TM_DIR/sf200_bisection.tm"
make_random_permutation_tm 200 "$TM_DIR/sf200_permutation.tm"
make_heavy_incast_tm 200 "$TM_DIR/sf200_incast16.tm" 16 100
make_mixed_size_tm 200 "$TM_DIR/sf200_mixed.tm"

# --- Slimfly p7q9 (1134 hosts) ---
cat > "$TM_DIR/sf1134_1flow.tm" <<'EOF'
Nodes 1134
Connections 1
0->1133 start 0 size 1048576
EOF

cat > "$TM_DIR/sf1134_20flow.tm" <<EOF
Nodes 1134
Connections 20
$(for i in $(seq 0 19); do echo "$((i*50))->$((1133 - i*50)) start 0 size 1048576"; done)
EOF
make_heavy_incast_tm 1134 "$TM_DIR/sf1134_incast16.tm" 16 567
make_mixed_size_tm 1134 "$TM_DIR/sf1134_mixed.tm"

echo "Done generating E2E TMs"
echo ""

FT128="$TOPO_DIR/fat_tree_128_1os.topo"
FT1024="$TOPO_DIR/fat_tree_1024_1os.topo"
DF_P3="$TOPO_DIR/dragonfly/p3a6h3"
DF_P4="$TOPO_DIR/dragonfly/p4a8h4"
SF_P4="$TOPO_DIR/slimfly/p4q5"
SF_P7="$TOPO_DIR/slimfly/p7q9"

# ============================================================
# Section 1: Fat Tree E2E
# ============================================================
echo "=== Section 1: Fat Tree 128 — E2E patterns ==="

run_e2e_test "ft128_bisection_64flows" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_bisection.tm -cwnd 30 -q 88" \
    64

run_e2e_test "ft128_permutation_128flows" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_permutation.tm -cwnd 30 -q 88" \
    128

run_e2e_test "ft128_incast_32to1" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_incast32.tm -cwnd 30 -q 88" \
    32

run_e2e_test "ft128_mixed_sizes" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_mixed.tm -cwnd 30 -q 88" \
    5

echo ""
echo "=== Section 2: Fat Tree 1024 — larger scale ==="

run_e2e_test "ft1024_1flow" \
    "$UEC_BIN -topo $FT1024 -tm $TM_DIR/ft1024_1flow.tm -cwnd 30 -q 88" \
    1 180

run_e2e_test "ft1024_20flows" \
    "$UEC_BIN -topo $FT1024 -tm $TM_DIR/ft1024_20flow.tm -cwnd 30 -q 88" \
    20 180

run_e2e_test "ft1024_incast_32to1" \
    "$UEC_BIN -topo $FT1024 -tm $TM_DIR/ft1024_incast32.tm -cwnd 30 -q 88" \
    32 180

run_e2e_test "ft1024_mixed_sizes" \
    "$UEC_BIN -topo $FT1024 -tm $TM_DIR/ft1024_mixed.tm -cwnd 30 -q 88" \
    5 180

echo ""

# ============================================================
# Section 3: Dragonfly E2E
# ============================================================
echo "=== Section 3: Dragonfly p3a6h3 (342 hosts) — E2E patterns ==="

run_e2e_test "df342_bisection_MINIMAL" \
    "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_bisection.tm -routing MINIMAL -q 88" \
    171

run_e2e_test "df342_bisection_VALIANT" \
    "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_bisection.tm -routing VALIANT -q 88" \
    171

run_e2e_test "df342_permutation_MINIMAL" \
    "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_permutation.tm -routing MINIMAL -q 88" \
    342

run_e2e_test "df342_permutation_UGAL_L" \
    "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_permutation.tm -routing UGAL_L -q 88" \
    342

run_e2e_test "df342_incast_16_MINIMAL" \
    "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_incast16.tm -routing MINIMAL -q 88" \
    16

run_e2e_test "df342_mixed_SOURCE" \
    "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_mixed.tm -routing SOURCE -q 88" \
    5

echo ""
echo "=== Section 4: Dragonfly p4a8h4 (1056 hosts) — larger scale ==="

run_e2e_test "df1056_20flows_MINIMAL" \
    "$UEC_BIN -topology dragonfly -topo $DF_P4 -tm $TM_DIR/df1056_20flow.tm -routing MINIMAL -q 88" \
    20 180

run_e2e_test "df1056_20flows_VALIANT" \
    "$UEC_BIN -topology dragonfly -topo $DF_P4 -tm $TM_DIR/df1056_20flow.tm -routing VALIANT -q 88" \
    20 180

run_e2e_test "df1056_incast_16_UGAL_L" \
    "$UEC_BIN -topology dragonfly -topo $DF_P4 -tm $TM_DIR/df1056_incast16.tm -routing UGAL_L -q 88" \
    16 180

run_e2e_test "df1056_mixed_MINIMAL" \
    "$UEC_BIN -topology dragonfly -topo $DF_P4 -tm $TM_DIR/df1056_mixed.tm -routing MINIMAL -q 88" \
    5 180

echo ""

# ============================================================
# Section 5: SlimFly E2E
# ============================================================
echo "=== Section 5: Slimfly p4q5 (200 hosts) — E2E patterns ==="

run_e2e_test "sf200_bisection_MINIMAL" \
    "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_bisection.tm -routing MINIMAL -q 88" \
    100

run_e2e_test "sf200_bisection_VALIANT" \
    "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_bisection.tm -routing VALIANT -q 88" \
    100

run_e2e_test "sf200_permutation_MINIMAL" \
    "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_permutation.tm -routing MINIMAL -q 88" \
    200

run_e2e_test "sf200_permutation_SOURCE" \
    "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_permutation.tm -routing SOURCE -q 88" \
    200

run_e2e_test "sf200_incast_16_MINIMAL" \
    "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_incast16.tm -routing MINIMAL -q 88" \
    16

run_e2e_test "sf200_mixed_UGAL_L" \
    "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_mixed.tm -routing UGAL_L -q 88" \
    5

echo ""
echo "=== Section 6: Slimfly p7q9 (1134 hosts) — larger scale ==="

run_e2e_test "sf1134_1flow_MINIMAL" \
    "$UEC_BIN -topology slimfly -topo $SF_P7 -tm $TM_DIR/sf1134_1flow.tm -routing MINIMAL -q 88" \
    1 180

run_e2e_test "sf1134_20flows_MINIMAL" \
    "$UEC_BIN -topology slimfly -topo $SF_P7 -tm $TM_DIR/sf1134_20flow.tm -routing MINIMAL -q 88" \
    20 180

run_e2e_test "sf1134_20flows_SOURCE" \
    "$UEC_BIN -topology slimfly -topo $SF_P7 -tm $TM_DIR/sf1134_20flow.tm -routing SOURCE -q 88" \
    20 180

run_e2e_test "sf1134_incast_16_MINIMAL" \
    "$UEC_BIN -topology slimfly -topo $SF_P7 -tm $TM_DIR/sf1134_incast16.tm -routing MINIMAL -q 88" \
    16 180

run_e2e_test "sf1134_mixed_MINIMAL" \
    "$UEC_BIN -topology slimfly -topo $SF_P7 -tm $TM_DIR/sf1134_mixed.tm -routing MINIMAL -q 88" \
    5 180

echo ""

# ============================================================
# Section 7: All routing algorithms × all topologies (single flow)
# ============================================================
echo "=== Section 7: Routing algorithm matrix (1 flow each) ==="

for routing in MINIMAL VALIANT UGAL_L SOURCE; do
    run_e2e_test "matrix_df342_${routing}" \
        "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_mixed.tm -routing $routing -q 88" \
        5
done

for routing in MINIMAL VALIANT UGAL_L SOURCE; do
    run_e2e_test "matrix_sf200_${routing}" \
        "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_mixed.tm -routing $routing -q 88" \
        5
done

echo ""

# ============================================================
# Summary
# ============================================================
echo "========================================="
echo "  E2E RESULTS: $PASS passed / $FAIL failed / $TOTAL total"
echo "========================================="
if [ $FAIL -gt 0 ]; then
    echo "SOME TESTS FAILED"
    exit 1
else
    echo "ALL E2E TESTS PASSED"
    exit 0
fi
