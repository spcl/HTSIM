#!/usr/bin/env bash
# Extended reachability & connectivity tests for HTSIM dragonfly and slimfly
# Tests: one-to-all, all-to-one, same-switch, cross-group, edge cases
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
HTSIM_DIR="$REPO_DIR"
UEC_BIN="$HTSIM_DIR/htsim/sim/datacenter/htsim_uec"
TOPO_DIR="$HTSIM_DIR/htsim/sim/datacenter/topologies"
TEST_DIR="$SCRIPT_DIR"

PASS=0
FAIL=0
TOTAL=0

FLOW_SIZE=4096  # 1 packet — minimal for reachability check

# ============================================================
# TM generators
# ============================================================

# One-to-all: node 0 → every other node
make_one_to_all_tm() {
    local num_hosts=$1 outfile=$2
    local n=$((num_hosts - 1))
    {
        echo "Nodes $num_hosts"
        echo "Connections $n"
        for dst in $(seq 1 $((num_hosts - 1))); do
            echo "0->$dst start 0 size $FLOW_SIZE"
        done
    } > "$outfile"
}

# All-to-one: every node → node 0
make_all_to_one_tm() {
    local num_hosts=$1 outfile=$2
    local n=$((num_hosts - 1))
    {
        echo "Nodes $num_hosts"
        echo "Connections $n"
        for src in $(seq 1 $((num_hosts - 1))); do
            echo "$src->0 start 0 size $FLOW_SIZE"
        done
    } > "$outfile"
}

# Same-switch: flows between hosts on the same switch (intra-switch)
make_same_switch_tm() {
    local num_hosts=$1 p=$2 outfile=$3
    local num_switches=$((num_hosts / p))
    local count=0
    local lines=""
    # For each switch, first host → last host on that switch
    for sw in $(seq 0 $((num_switches - 1))); do
        local src=$((sw * p))
        local dst=$((sw * p + p - 1))
        if [ "$src" -ne "$dst" ]; then
            lines+="$src->$dst start 0 size $FLOW_SIZE"$'\n'
            count=$((count + 1))
        fi
    done
    {
        echo "Nodes $num_hosts"
        echo "Connections $count"
        printf "%s" "$lines"
    } > "$outfile"
}

# Cross-group: for dragonfly, pick one host from each group → host in a different group
# groups = a*h+1, each group has a switches, each switch has p hosts
make_cross_group_tm() {
    local num_hosts=$1 p=$2 a=$3 h=$4 outfile=$5
    local num_groups=$(( a * h + 1 ))
    local count=0
    local lines=""
    for g in $(seq 0 $((num_groups - 1))); do
        local src=$((g * a * p))           # first host in group g
        local dst_group=$(( (g + num_groups / 2) % num_groups ))
        local dst=$((dst_group * a * p))   # first host in opposite group
        if [ "$src" -ne "$dst" ] && [ "$src" -lt "$num_hosts" ] && [ "$dst" -lt "$num_hosts" ]; then
            lines+="$src->$dst start 0 size $FLOW_SIZE"$'\n'
            count=$((count + 1))
        fi
    done
    {
        echo "Nodes $num_hosts"
        echo "Connections $count"
        printf "%s" "$lines"
    } > "$outfile"
}

# Cross-partition: for slimfly, pick hosts across the two partitions (switch 0..q²-1 and q²..2q²-1)
make_cross_partition_tm() {
    local num_hosts=$1 p=$2 q=$3 outfile=$4
    local q2=$((q * q))
    local count=0
    local lines=""
    # For each switch in partition 0, connect to corresponding switch in partition 1
    for sw in $(seq 0 $((q2 - 1))); do
        local src=$((sw * p))                     # first host on switch sw (partition 0)
        local dst=$(( (q2 + sw) * p ))            # first host on switch q²+sw (partition 1)
        if [ "$src" -lt "$num_hosts" ] && [ "$dst" -lt "$num_hosts" ]; then
            lines+="$src->$dst start 0 size $FLOW_SIZE"$'\n'
            count=$((count + 1))
        fi
    done
    {
        echo "Nodes $num_hosts"
        echo "Connections $count"
        printf "%s" "$lines"
    } > "$outfile"
}

# Maximum-distance pairs: for dragonfly, hosts at group 0 ↔ last group
make_max_distance_df_tm() {
    local num_hosts=$1 p=$2 a=$3 h=$4 outfile=$5
    local num_groups=$(( a * h + 1 ))
    local last_group=$((num_groups - 1))
    local count=0
    local lines=""
    # p flows from group 0 → last group
    for i in $(seq 0 $((p - 1))); do
        local src=$i
        local dst=$((last_group * a * p + i))
        if [ "$dst" -lt "$num_hosts" ]; then
            lines+="$src->$dst start 0 size $FLOW_SIZE"$'\n'
            count=$((count + 1))
        fi
    done
    {
        echo "Nodes $num_hosts"
        echo "Connections $count"
        printf "%s" "$lines"
    } > "$outfile"
}

# ============================================================
# Test runner
# ============================================================
run_reachability_test() {
    local label="$1"
    local binary="$2"
    local topo_path="$3"
    local routing="$4"
    local tm_file="$5"
    local expected_flows="$6"
    local topology="${7:-}"    # optional: dragonfly, slimfly
    
    TOTAL=$((TOTAL + 1))
    local outfile="$TEST_DIR/out_reach_${label}.txt"
    
    echo -n "  [$TOTAL] $label ($expected_flows flows) ... "
    
    # Build command
    local cmd
    if [ -n "$topology" ]; then
        cmd="$binary -topology $topology -topo $topo_path -routing $routing -tm $tm_file"
    else
        cmd="$binary -topo $topo_path -routing $routing -tm $tm_file"
    fi
    
    # Run with timeout
    if timeout 120 $cmd > "$outfile" 2>&1; then
        local exit_code=0
    else
        local exit_code=$?
    fi
    
    # Check 1: exited cleanly
    if [ $exit_code -ne 0 ]; then
        echo "FAIL (exit code $exit_code)"
        tail -3 "$outfile"
        FAIL=$((FAIL + 1))
        return
    fi
    
    # Check 2: "Done" in output
    if ! grep -q "Done" "$outfile"; then
        echo "FAIL (no 'Done')"
        tail -3 "$outfile"
        FAIL=$((FAIL + 1))
        return
    fi
    
    # Check 3: count finished flows
    local finished_count
    finished_count=$(grep -c "finished at" "$outfile" || echo 0)
    
    if [ "$finished_count" -ne "$expected_flows" ]; then
        echo "FAIL (only $finished_count of $expected_flows flows finished)"
        FAIL=$((FAIL + 1))
        return
    fi
    
    # Check 4: all flows got correct bytes
    local wrong_bytes
    wrong_bytes=$(grep "finished at" "$outfile" | grep -cv "total bytes $FLOW_SIZE" || true)
    if [ "$wrong_bytes" -gt 0 ]; then
        echo "FAIL ($wrong_bytes flows got wrong byte count)"
        FAIL=$((FAIL + 1))
        return
    fi
    
    # Check 5: New packets count (1 flow = 1 packet for 4096B)
    local stats_line new_pkts
    stats_line=$(grep "New:" "$outfile" | tail -1)
    new_pkts=$(echo "$stats_line" | grep -oP 'New: \K[0-9]+')
    
    if [ "$new_pkts" -ne "$expected_flows" ]; then
        echo "FAIL (New=$new_pkts, expected $expected_flows)"
        FAIL=$((FAIL + 1))
        return
    fi
    
    local rtx_pkts ack_pkts
    rtx_pkts=$(echo "$stats_line" | grep -oP 'Rtx: \K[0-9]+')
    ack_pkts=$(echo "$stats_line" | grep -oP 'ACKs: \K[0-9]+')
    
    echo "PASS (all $finished_count flows finished, New=$new_pkts Rtx=$rtx_pkts ACK=$ack_pkts)"
    PASS=$((PASS + 1))
}

# ============================================================
# Generate TMs
# ============================================================
echo "=== Generating reachability traffic matrices ==="

# --- Dragonfly p3a6h3: 342 hosts, p=3, a=6, h=3 ---
make_one_to_all_tm   342 "$TEST_DIR/df_p3_one2all.tm"
make_all_to_one_tm   342 "$TEST_DIR/df_p3_all2one.tm"
make_same_switch_tm  342 3 "$TEST_DIR/df_p3_sameswitch.tm"
make_cross_group_tm  342 3 6 3 "$TEST_DIR/df_p3_crossgroup.tm"
make_max_distance_df_tm 342 3 6 3 "$TEST_DIR/df_p3_maxdist.tm"

# --- Slimfly p4q5: 200 hosts, p=4, q=5 ---
make_one_to_all_tm    200 "$TEST_DIR/sf_p4_one2all.tm"
make_all_to_one_tm    200 "$TEST_DIR/sf_p4_all2one.tm"
make_same_switch_tm   200 4 "$TEST_DIR/sf_p4_sameswitch.tm"
make_cross_partition_tm 200 4 5 "$TEST_DIR/sf_p4_crosspart.tm"

echo "Done"
echo ""

# ============================================================
# Dragonfly p3a6h3 — MINIMAL & SOURCE routing
# ============================================================
DF_P3="$TOPO_DIR/dragonfly/p3a6h3"

echo "=== Dragonfly p3a6h3: One-to-All Reachability (node 0 → all 341 others) ==="
run_reachability_test "df_p3_one2all_MINIMAL" "$UEC_BIN" "$DF_P3" "MINIMAL" "$TEST_DIR/df_p3_one2all.tm" 341 "dragonfly"
run_reachability_test "df_p3_one2all_SOURCE"  "$UEC_BIN" "$DF_P3" "SOURCE"  "$TEST_DIR/df_p3_one2all.tm" 341 "dragonfly"
echo ""

echo "=== Dragonfly p3a6h3: All-to-One Reachability (all 341 others → node 0) ==="
run_reachability_test "df_p3_all2one_MINIMAL" "$UEC_BIN" "$DF_P3" "MINIMAL" "$TEST_DIR/df_p3_all2one.tm" 341 "dragonfly"
run_reachability_test "df_p3_all2one_SOURCE"  "$UEC_BIN" "$DF_P3" "SOURCE"  "$TEST_DIR/df_p3_all2one.tm" 341 "dragonfly"
echo ""

echo "=== Dragonfly p3a6h3: Same-Switch (intra-switch) ==="
run_reachability_test "df_p3_sameswitch_MINIMAL" "$UEC_BIN" "$DF_P3" "MINIMAL" "$TEST_DIR/df_p3_sameswitch.tm" 114 "dragonfly"
run_reachability_test "df_p3_sameswitch_SOURCE"  "$UEC_BIN" "$DF_P3" "SOURCE"  "$TEST_DIR/df_p3_sameswitch.tm" 114 "dragonfly"
echo ""

echo "=== Dragonfly p3a6h3: Cross-Group (each group → opposite group) ==="
run_reachability_test "df_p3_crossgroup_MINIMAL" "$UEC_BIN" "$DF_P3" "MINIMAL" "$TEST_DIR/df_p3_crossgroup.tm" 19 "dragonfly"
run_reachability_test "df_p3_crossgroup_SOURCE"  "$UEC_BIN" "$DF_P3" "SOURCE"  "$TEST_DIR/df_p3_crossgroup.tm" 19 "dragonfly"
echo ""

echo "=== Dragonfly p3a6h3: Max Distance (group 0 ↔ last group) ==="
run_reachability_test "df_p3_maxdist_MINIMAL" "$UEC_BIN" "$DF_P3" "MINIMAL" "$TEST_DIR/df_p3_maxdist.tm" 3 "dragonfly"
run_reachability_test "df_p3_maxdist_SOURCE"  "$UEC_BIN" "$DF_P3" "SOURCE"  "$TEST_DIR/df_p3_maxdist.tm" 3 "dragonfly"
echo ""

# ============================================================
# Slimfly p4q5 — MINIMAL & SOURCE routing
# ============================================================
SF_P4="$TOPO_DIR/slimfly/p4q5"

echo "=== Slimfly p4q5: One-to-All Reachability (node 0 → all 199 others) ==="
run_reachability_test "sf_p4_one2all_MINIMAL" "$UEC_BIN" "$SF_P4" "MINIMAL" "$TEST_DIR/sf_p4_one2all.tm" 199 "slimfly"
run_reachability_test "sf_p4_one2all_SOURCE"  "$UEC_BIN" "$SF_P4" "SOURCE"  "$TEST_DIR/sf_p4_one2all.tm" 199 "slimfly"
echo ""

echo "=== Slimfly p4q5: All-to-One Reachability (all 199 others → node 0) ==="
run_reachability_test "sf_p4_all2one_MINIMAL" "$UEC_BIN" "$SF_P4" "MINIMAL" "$TEST_DIR/sf_p4_all2one.tm" 199 "slimfly"
run_reachability_test "sf_p4_all2one_SOURCE"  "$UEC_BIN" "$SF_P4" "SOURCE"  "$TEST_DIR/sf_p4_all2one.tm" 199 "slimfly"
echo ""

echo "=== Slimfly p4q5: Same-Switch (intra-switch) ==="
run_reachability_test "sf_p4_sameswitch_MINIMAL" "$UEC_BIN" "$SF_P4" "MINIMAL" "$TEST_DIR/sf_p4_sameswitch.tm" 50 "slimfly"
run_reachability_test "sf_p4_sameswitch_SOURCE"  "$UEC_BIN" "$SF_P4" "SOURCE"  "$TEST_DIR/sf_p4_sameswitch.tm" 50 "slimfly"
echo ""

echo "=== Slimfly p4q5: Cross-Partition (partition 0 → partition 1) ==="
run_reachability_test "sf_p4_crosspart_MINIMAL" "$UEC_BIN" "$SF_P4" "MINIMAL" "$TEST_DIR/sf_p4_crosspart.tm" 25 "slimfly"
run_reachability_test "sf_p4_crosspart_SOURCE"  "$UEC_BIN" "$SF_P4" "SOURCE"  "$TEST_DIR/sf_p4_crosspart.tm" 25 "slimfly"
echo ""

# ============================================================
# Summary
# ============================================================
echo "========================================="
echo "  REACHABILITY: $PASS passed / $FAIL failed / $TOTAL total"
echo "========================================="
if [ $FAIL -gt 0 ]; then
    echo "SOME TESTS FAILED"
    exit 1
else
    echo "ALL REACHABILITY TESTS PASSED"
    exit 0
fi
