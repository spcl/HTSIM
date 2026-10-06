#!/usr/bin/env bash
# Edge case & parameter variation tests for the unified htsim_uec binary
# Covers: default topology, explicit FT flag, cwnd variations, queue sizes,
#         tiny/large flows, staggered starts, single-packet flows, incast
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
HTSIM_DIR="$REPO_DIR"
UEC_BIN="$HTSIM_DIR/htsim/sim/datacenter/htsim_uec"
TOPO_DIR="$HTSIM_DIR/htsim/sim/datacenter/topologies"
TEST_DIR="$SCRIPT_DIR"
TM_DIR="$TEST_DIR/edge_tms"
OUT_DIR="$TEST_DIR/edge_out"

mkdir -p "$TM_DIR" "$OUT_DIR"

PASS=0
FAIL=0
TOTAL=0

# ============================================================
# Helpers
# ============================================================
run_edge_test() {
    local label="$1"
    local cmd="$2"
    local check_type="$3"  # "basic" | "flow_count:N" | "no_crash"

    TOTAL=$((TOTAL + 1))
    local outfile="$OUT_DIR/out_${label}.txt"
    echo -n "  [$TOTAL] $label ... "

    if timeout 90 bash -c "$cmd" > "$outfile" 2>&1; then
        local exit_code=0
    else
        local exit_code=$?
    fi

    # --- no_crash: only checks it doesn't segfault ---
    if [ "$check_type" = "no_crash" ]; then
        if [ $exit_code -eq 139 ] || [ $exit_code -eq 136 ]; then
            echo "FAIL (segfault/FPE, exit $exit_code)"
            FAIL=$((FAIL + 1)); return
        fi
        echo "PASS (exit $exit_code)"
        PASS=$((PASS + 1)); return
    fi

    # --- expected_abort: expect assertion failure (exit 134) or nonzero exit ---
    if [ "$check_type" = "expected_abort" ]; then
        if [ $exit_code -ne 0 ]; then
            echo "PASS (correctly aborted, exit $exit_code)"
            PASS=$((PASS + 1)); return
        fi
        echo "FAIL (expected abort but got exit 0)"
        FAIL=$((FAIL + 1)); return
    fi

    # --- basic / flow_count: must complete ---
    if [ $exit_code -ne 0 ]; then
        echo "FAIL (exit code $exit_code)"
        tail -5 "$outfile"
        FAIL=$((FAIL + 1)); return
    fi

    if ! grep -q "Done" "$outfile"; then
        echo "FAIL (no 'Done')"
        tail -5 "$outfile"
        FAIL=$((FAIL + 1)); return
    fi

    local stats_line new_pkts ack_pkts
    stats_line=$(grep "^New:" "$outfile" || grep "New:" "$outfile" | tail -1 || true)
    if [ -z "$stats_line" ]; then
        echo "FAIL (no stats)"
        FAIL=$((FAIL + 1)); return
    fi
    new_pkts=$(echo "$stats_line" | grep -oP 'New: \K[0-9]+')
    ack_pkts=$(echo "$stats_line" | grep -oP 'ACKs: \K[0-9]+')

    if [ "$new_pkts" -eq 0 ] || [ "$ack_pkts" -eq 0 ]; then
        echo "FAIL (New=$new_pkts, ACK=$ack_pkts)"
        FAIL=$((FAIL + 1)); return
    fi

    # --- flow_count check ---
    if [[ "$check_type" == flow_count:* ]]; then
        local expected=${check_type#flow_count:}
        local finished
        finished=$(grep -c "finished at" "$outfile" || echo 0)
        if [ "$finished" -ne "$expected" ]; then
            echo "FAIL (finished $finished of $expected flows)"
            FAIL=$((FAIL + 1)); return
        fi
    fi

    local rtx_pkts nack_pkts bounced_pkts
    rtx_pkts=$(echo "$stats_line" | grep -oP 'Rtx: \K[0-9]+' || echo 0)
    nack_pkts=$(echo "$stats_line" | grep -oP 'NACKs: \K[0-9]+' || echo 0)
    bounced_pkts=$(echo "$stats_line" | grep -oP 'Bounced: \K[0-9]+' || echo 0)
    echo "PASS | New:$new_pkts Rtx:$rtx_pkts ACK:$ack_pkts NACK:$nack_pkts Bounce:$bounced_pkts"
    PASS=$((PASS + 1))
}

# ============================================================
# Generate traffic matrices
# ============================================================
echo "=== Generating edge-case traffic matrices ==="

# --- Fat Tree 128 nodes ---
cat > "$TM_DIR/ft128_1flow.tm" <<'EOF'
Nodes 128
Connections 1
0->127 start 0 size 1048576
EOF

cat > "$TM_DIR/ft128_5flow.tm" <<'EOF'
Nodes 128
Connections 5
0->127 start 0 size 1048576
1->126 start 0 size 1048576
10->100 start 0 size 1048576
30->90 start 0 size 1048576
64->63 start 0 size 1048576
EOF

# Tiny flow: 1 packet (4096 bytes)
cat > "$TM_DIR/ft128_tiny.tm" <<'EOF'
Nodes 128
Connections 1
0->127 start 0 size 4096
EOF

# Large flow: 10 MiB
cat > "$TM_DIR/ft128_large.tm" <<'EOF'
Nodes 128
Connections 1
0->127 start 0 size 10485760
EOF

# Staggered starts (different start times in ns)
cat > "$TM_DIR/ft128_staggered.tm" <<'EOF'
Nodes 128
Connections 4
0->64 start 0 size 1048576
1->65 start 1000 size 1048576
2->66 start 5000 size 1048576
3->67 start 10000 size 1048576
EOF

# Incast: many sources → one destination
cat > "$TM_DIR/ft128_incast_16to1.tm" <<'EOF'
Nodes 128
Connections 16
0->127 start 0 size 1048576
1->127 start 0 size 1048576
2->127 start 0 size 1048576
3->127 start 0 size 1048576
4->127 start 0 size 1048576
5->127 start 0 size 1048576
6->127 start 0 size 1048576
7->127 start 0 size 1048576
8->127 start 0 size 1048576
9->127 start 0 size 1048576
10->127 start 0 size 1048576
11->127 start 0 size 1048576
12->127 start 0 size 1048576
13->127 start 0 size 1048576
14->127 start 0 size 1048576
15->127 start 0 size 1048576
EOF

# Outcast: one source → many destinations
cat > "$TM_DIR/ft128_outcast_1to16.tm" <<'EOF'
Nodes 128
Connections 16
0->8 start 0 size 1048576
0->16 start 0 size 1048576
0->24 start 0 size 1048576
0->32 start 0 size 1048576
0->40 start 0 size 1048576
0->48 start 0 size 1048576
0->56 start 0 size 1048576
0->64 start 0 size 1048576
0->72 start 0 size 1048576
0->80 start 0 size 1048576
0->88 start 0 size 1048576
0->96 start 0 size 1048576
0->104 start 0 size 1048576
0->112 start 0 size 1048576
0->120 start 0 size 1048576
0->127 start 0 size 1048576
EOF

# Same-pod traffic (nodes 0-15 are in the same pod for 128-node FT with podsize 16)
cat > "$TM_DIR/ft128_samepod.tm" <<'EOF'
Nodes 128
Connections 4
0->15 start 0 size 1048576
1->14 start 0 size 1048576
2->13 start 0 size 1048576
3->12 start 0 size 1048576
EOF

# Cross-pod traffic (nodes in different pods)
cat > "$TM_DIR/ft128_crosspod.tm" <<'EOF'
Nodes 128
Connections 4
0->16 start 0 size 1048576
1->32 start 0 size 1048576
2->48 start 0 size 1048576
3->64 start 0 size 1048576
EOF

# --- Dragonfly p3a6h3 (342 hosts) edge TMs ---
cat > "$TM_DIR/df342_tiny.tm" <<'EOF'
Nodes 342
Connections 1
0->341 start 0 size 4096
EOF

cat > "$TM_DIR/df342_large.tm" <<'EOF'
Nodes 342
Connections 1
0->200 start 0 size 10485760
EOF

cat > "$TM_DIR/df342_staggered.tm" <<'EOF'
Nodes 342
Connections 4
0->100 start 0 size 1048576
50->200 start 2000 size 1048576
100->300 start 5000 size 1048576
150->341 start 10000 size 1048576
EOF

cat > "$TM_DIR/df342_incast_8to1.tm" <<EOF
Nodes 342
Connections 8
$(for i in $(seq 0 7); do echo "$i->341 start 0 size 1048576"; done)
EOF

# --- Slimfly p4q5 (200 hosts) edge TMs ---
cat > "$TM_DIR/sf200_tiny.tm" <<'EOF'
Nodes 200
Connections 1
0->199 start 0 size 4096
EOF

cat > "$TM_DIR/sf200_large.tm" <<'EOF'
Nodes 200
Connections 1
0->150 start 0 size 10485760
EOF

cat > "$TM_DIR/sf200_staggered.tm" <<'EOF'
Nodes 200
Connections 4
0->50 start 0 size 1048576
25->100 start 2000 size 1048576
75->150 start 5000 size 1048576
100->199 start 10000 size 1048576
EOF

cat > "$TM_DIR/sf200_incast_8to1.tm" <<EOF
Nodes 200
Connections 8
$(for i in $(seq 0 7); do echo "$i->199 start 0 size 1048576"; done)
EOF

echo "Done generating edge-case TMs"
echo ""

# ============================================================
# Section 1: Fat Tree — default topology (no -topology flag)
# ============================================================
FT128="$TOPO_DIR/fat_tree_128_1os.topo"

echo "=== Section 1: Fat Tree basic (default topology type) ==="

run_edge_test "ft_default_no_flag" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_1flow.tm -cwnd 30 -q 88" \
    "basic"

run_edge_test "ft_explicit_fattree_flag" \
    "$UEC_BIN -topology fattree -topo $FT128 -tm $TM_DIR/ft128_1flow.tm -cwnd 30 -q 88" \
    "basic"

echo ""

# ============================================================
# Section 2: Fat Tree — parameter variations
# ============================================================
echo "=== Section 2: Fat Tree parameter variations ==="

# Different cwnd values
for cwnd in 5 15 30 50 100; do
    run_edge_test "ft_cwnd_${cwnd}" \
        "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_1flow.tm -cwnd $cwnd -q 88" \
        "basic"
done
echo ""

# Different queue sizes
echo "  -- Queue size variations --"
# qsize=20 is too small for ECN thresholds — expect assertion abort
run_edge_test "ft_qsize_20_expected_abort" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_1flow.tm -cwnd 30 -q 20" \
    "expected_abort"
for qsize in 50 88 150 300; do
    run_edge_test "ft_qsize_${qsize}" \
        "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_1flow.tm -cwnd 30 -q $qsize" \
        "basic"
done
echo ""

# ============================================================
# Section 3: Fat Tree — flow patterns
# ============================================================
echo "=== Section 3: Fat Tree flow patterns ==="

run_edge_test "ft_tiny_flow" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_tiny.tm -cwnd 30 -q 88" \
    "flow_count:1"

run_edge_test "ft_large_flow_10MiB" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_large.tm -cwnd 30 -q 88" \
    "basic"

run_edge_test "ft_5flows" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_5flow.tm -cwnd 30 -q 88" \
    "flow_count:5"

run_edge_test "ft_staggered_starts" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_staggered.tm -cwnd 30 -q 88" \
    "flow_count:4"

run_edge_test "ft_incast_16to1" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_incast_16to1.tm -cwnd 30 -q 88" \
    "flow_count:16"

run_edge_test "ft_outcast_1to16" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_outcast_1to16.tm -cwnd 30 -q 88" \
    "flow_count:16"

run_edge_test "ft_samepod" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_samepod.tm -cwnd 30 -q 88" \
    "flow_count:4"

run_edge_test "ft_crosspod" \
    "$UEC_BIN -topo $FT128 -tm $TM_DIR/ft128_crosspod.tm -cwnd 30 -q 88" \
    "flow_count:4"

echo ""

# ============================================================
# Section 4: Dragonfly — edge cases
# ============================================================
echo "=== Section 4: Dragonfly edge cases ==="
DF_P3="$TOPO_DIR/dragonfly/p3a6h3"

run_edge_test "df_tiny_flow" \
    "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_tiny.tm -routing MINIMAL -q 88" \
    "flow_count:1"

run_edge_test "df_large_flow_10MiB" \
    "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_large.tm -routing MINIMAL -q 88" \
    "basic"

run_edge_test "df_staggered_starts" \
    "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_staggered.tm -routing MINIMAL -q 88" \
    "flow_count:4"

run_edge_test "df_incast_8to1" \
    "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_incast_8to1.tm -routing MINIMAL -q 88" \
    "flow_count:8"

# cwnd variations on DF (DF uses cwnd as raw bytes, not packet count)
# Values must be large enough to fit at least 1 packet (~4160 bytes)
for cwnd in 20000 100000 500000; do
    run_edge_test "df_cwnd_${cwnd}" \
        "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_tiny.tm -routing MINIMAL -q 88 -cwnd $cwnd" \
        "basic"
done

# queue size variations on DF
for qsize in 30 88 200; do
    run_edge_test "df_qsize_${qsize}" \
        "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_tiny.tm -routing MINIMAL -q $qsize" \
        "basic"
done

echo ""

# ============================================================
# Section 5: SlimFly — edge cases
# ============================================================
echo "=== Section 5: SlimFly edge cases ==="
SF_P4="$TOPO_DIR/slimfly/p4q5"

run_edge_test "sf_tiny_flow" \
    "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_tiny.tm -routing MINIMAL -q 88" \
    "flow_count:1"

run_edge_test "sf_large_flow_10MiB" \
    "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_large.tm -routing MINIMAL -q 88" \
    "basic"

run_edge_test "sf_staggered_starts" \
    "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_staggered.tm -routing MINIMAL -q 88" \
    "flow_count:4"

run_edge_test "sf_incast_8to1" \
    "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_incast_8to1.tm -routing MINIMAL -q 88" \
    "flow_count:8"

# cwnd variations on SF (SF uses cwnd as raw bytes, not packet count)
# Values must be large enough to fit at least 1 packet (~4160 bytes)
for cwnd in 20000 100000 500000; do
    run_edge_test "sf_cwnd_${cwnd}" \
        "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_tiny.tm -routing MINIMAL -q 88 -cwnd $cwnd" \
        "basic"
done

# queue size variations on SF
for qsize in 30 88 200; do
    run_edge_test "sf_qsize_${qsize}" \
        "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_tiny.tm -routing MINIMAL -q $qsize" \
        "basic"
done

echo ""

# ============================================================
# Section 6: Cross-topology consistency
# Same workload pattern on all 3 topologies should all complete
# ============================================================
echo "=== Section 6: Cross-topology consistency ==="

# 1-flow, 1 MiB on each topology
run_edge_test "consistency_ft_1flow" \
    "$UEC_BIN -topology fattree -topo $FT128 -tm $TM_DIR/ft128_1flow.tm -cwnd 30 -q 88" \
    "flow_count:1"

# DF 1-flow with each routing algo
for routing in MINIMAL VALIANT UGAL_L SOURCE; do
    run_edge_test "consistency_df_${routing}" \
        "$UEC_BIN -topology dragonfly -topo $DF_P3 -tm $TM_DIR/df342_tiny.tm -routing $routing -q 88" \
        "flow_count:1"
done

# SF 1-flow with each routing algo
for routing in MINIMAL VALIANT UGAL_L SOURCE; do
    run_edge_test "consistency_sf_${routing}" \
        "$UEC_BIN -topology slimfly -topo $SF_P4 -tm $TM_DIR/sf200_tiny.tm -routing $routing -q 88" \
        "flow_count:1"
done

echo ""

# ============================================================
# Summary
# ============================================================
echo "========================================="
echo "  EDGE CASE RESULTS: $PASS passed / $FAIL failed / $TOTAL total"
echo "========================================="
if [ $FAIL -gt 0 ]; then
    echo "SOME TESTS FAILED"
    exit 1
else
    echo "ALL EDGE CASE TESTS PASSED"
    exit 0
fi
