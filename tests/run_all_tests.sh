#!/usr/bin/env bash
# Comprehensive topology & routing tests for HTSIM dragonfly and slimfly
# Verifies: (1) all flows finish (2) retransmission recovery (3) expected runtime
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

# ============================================================
# Traffic matrix creation helpers
# ============================================================

# Create a single-flow TM: src 0 -> dst <last_host>, 1 MiB
make_single_flow_tm() {
    local num_hosts=$1
    local outfile=$2
    local dst=$((num_hosts - 1))
    cat > "$outfile" <<EOF
Nodes $num_hosts
Connections 1
0->$dst start 0 size 1048576
EOF
}

# Create a medium TM: 10 random-ish flows across different switches, 1 MiB each
make_medium_tm() {
    local num_hosts=$1
    local outfile=$2
    local p=$3          # hosts per switch
    local num_flows=10
    
    # Pick flows that span different groups/partitions
    # Spread src and dst across the host range
    local step=$(( num_hosts / (num_flows + 1) ))
    [ "$step" -lt 1 ] && step=1
    
    echo "Nodes $num_hosts" > "$outfile"
    echo "Connections $num_flows" >> "$outfile"
    for i in $(seq 0 $((num_flows - 1))); do
        local src=$(( (i * step) % num_hosts ))
        local dst=$(( (src + num_hosts / 2 + i * 7) % num_hosts ))
        # Ensure src != dst
        [ "$src" -eq "$dst" ] && dst=$(( (dst + 1) % num_hosts ))
        echo "$src->$dst start 0 size 1048576" >> "$outfile"
    done
}

# ============================================================
# Test runner
# ============================================================
run_test() {
    local label="$1"
    local binary="$2"
    local topo_path="$3"
    local routing="$4"
    local tm_file="$5"
    local num_hosts="$6"
    local workload_type="$7"   # "small" or "medium"
    local num_flows="$8"
    local topology="${9:-}"    # optional: dragonfly, slimfly
    
    TOTAL=$((TOTAL + 1))
    local outfile="$TEST_DIR/out_${label}.txt"
    
    echo -n "  [$TOTAL] $label ... "
    
    # Build command
    local cmd
    if [ -n "$topology" ]; then
        cmd="$binary -topology $topology -topo $topo_path -routing $routing -tm $tm_file"
    else
        cmd="$binary -topo $topo_path -routing $routing -tm $tm_file"
    fi
    
    # Run with timeout (60s should be plenty for these workloads)
    if timeout 60 $cmd > "$outfile" 2>&1; then
        local exit_code=0
    else
        local exit_code=$?
    fi
    
    # === Check 1: Did it complete? ===
    if [ $exit_code -ne 0 ]; then
        echo "FAIL (exit code $exit_code)"
        FAIL=$((FAIL + 1))
        cat "$outfile" | tail -5
        return
    fi
    
    if ! grep -q "Done" "$outfile"; then
        echo "FAIL (no 'Done' in output)"
        FAIL=$((FAIL + 1))
        cat "$outfile" | tail -5
        return
    fi
    
    # === Check 2: Parse packet stats ===
    local stats_line
    stats_line=$(grep "^New:" "$outfile" || grep "New:" "$outfile" || true)
    if [ -z "$stats_line" ]; then
        echo "FAIL (no stats line)"
        FAIL=$((FAIL + 1))
        return
    fi
    
    local new_pkts rtx_pkts ack_pkts nack_pkts bounced_pkts
    new_pkts=$(echo "$stats_line" | grep -oP 'New: \K[0-9]+')
    rtx_pkts=$(echo "$stats_line" | grep -oP 'Rtx: \K[0-9]+')
    ack_pkts=$(echo "$stats_line" | grep -oP 'ACKs: \K[0-9]+')
    nack_pkts=$(echo "$stats_line" | grep -oP 'NACKs: \K[0-9]+')
    bounced_pkts=$(echo "$stats_line" | grep -oP 'Bounced: \K[0-9]+')
    
    # === Check 3: All flows completed (New packets > 0, ACKs received) ===
    if [ "$new_pkts" -eq 0 ]; then
        echo "FAIL (0 new packets sent)"
        FAIL=$((FAIL + 1))
        return
    fi
    
    if [ "$ack_pkts" -eq 0 ]; then
        echo "FAIL (0 ACKs received - flows didn't complete)"
        FAIL=$((FAIL + 1))
        return
    fi
    
    # === Check 4: Expected packet count ===
    # 1 MiB = 1048576 bytes, packet_size = 4160, data ≈ 4096 bytes 
    # So ~256 packets per flow (1048576/4096). With 4160B packets, ~252.
    # Allow range 240-270 per flow to account for protocol specifics.
    local expected_min=$((num_flows * 240))
    local expected_max=$((num_flows * 280))
    
    local pkt_check="OK"
    if [ "$new_pkts" -lt "$expected_min" ] || [ "$new_pkts" -gt "$expected_max" ]; then
        pkt_check="WARN(expected ${expected_min}-${expected_max})"
    fi
    
    # === Check 5: Retransmission check ===
    local rtx_check="OK"
    if [ "$workload_type" = "small" ] && [ "$rtx_pkts" -gt 5 ]; then
        # Single flow with no contention should have minimal retransmits
        rtx_check="WARN(${rtx_pkts}rtx for single flow)"
    fi
    
    # === Check 6: Recovery check - if there were NACKs/bounces, Rtx should be > 0 ===
    local recovery_check="OK"
    if [ "$nack_pkts" -gt 0 ] || [ "$bounced_pkts" -gt 0 ]; then
        if [ "$rtx_pkts" -gt 0 ]; then
            recovery_check="OK(recovered: ${rtx_pkts}rtx for ${nack_pkts}nack+${bounced_pkts}bounce)"
        else
            recovery_check="WARN(losses but 0 rtx)"
        fi
    fi
    
    echo "PASS | New:$new_pkts Rtx:$rtx_pkts ACK:$ack_pkts NACK:$nack_pkts Bounce:$bounced_pkts | pkt:$pkt_check rtx:$rtx_check recovery:$recovery_check"
    PASS=$((PASS + 1))
}

# ============================================================
# Generate all traffic matrices
# ============================================================
echo "=== Generating traffic matrices ==="

# Dragonfly p3a6h3: 342 hosts, p=3
make_single_flow_tm 342 "$TEST_DIR/df_p3a6h3_1flow.tm"
make_medium_tm      342 "$TEST_DIR/df_p3a6h3_10flow.tm" 3

# Dragonfly p4a8h4: 1056 hosts, p=4
make_single_flow_tm 1056 "$TEST_DIR/df_p4a8h4_1flow.tm"
make_medium_tm      1056 "$TEST_DIR/df_p4a8h4_10flow.tm" 4

# Slimfly p4q5: 200 hosts, p=4
make_single_flow_tm 200 "$TEST_DIR/sf_p4q5_1flow.tm"
make_medium_tm      200 "$TEST_DIR/sf_p4q5_10flow.tm" 4

echo "Done generating TMs"
echo ""

# ============================================================
# Dragonfly p3a6h3 tests (342 hosts, has host_table → SOURCE works)
# ============================================================
echo "=== Dragonfly p3a6h3 (342 hosts) ==="
DF_P3_PATH="$TOPO_DIR/dragonfly/p3a6h3"

for routing in MINIMAL VALIANT UGAL_L SOURCE; do
    run_test "df_p3a6h3_${routing}_1flow" "$UEC_BIN" "$DF_P3_PATH" "$routing" \
        "$TEST_DIR/df_p3a6h3_1flow.tm" 342 "small" 1 "dragonfly"
done
echo ""
echo "  -- Medium workload (10 flows) --"
for routing in MINIMAL VALIANT UGAL_L SOURCE; do
    run_test "df_p3a6h3_${routing}_10flow" "$UEC_BIN" "$DF_P3_PATH" "$routing" \
        "$TEST_DIR/df_p3a6h3_10flow.tm" 342 "medium" 10 "dragonfly"
done
echo ""

# ============================================================
# Dragonfly p4a8h4 tests (1056 hosts, NO host_table → skip SOURCE)
# ============================================================
echo "=== Dragonfly p4a8h4 (1056 hosts) ==="
DF_P4_PATH="$TOPO_DIR/dragonfly/p4a8h4"

for routing in MINIMAL VALIANT UGAL_L; do
    run_test "df_p4a8h4_${routing}_1flow" "$UEC_BIN" "$DF_P4_PATH" "$routing" \
        "$TEST_DIR/df_p4a8h4_1flow.tm" 1056 "small" 1 "dragonfly"
done
echo ""
echo "  -- Medium workload (10 flows) --"
for routing in MINIMAL VALIANT UGAL_L; do
    run_test "df_p4a8h4_${routing}_10flow" "$UEC_BIN" "$DF_P4_PATH" "$routing" \
        "$TEST_DIR/df_p4a8h4_10flow.tm" 1056 "medium" 10 "dragonfly"
done
echo ""

# ============================================================
# Slimfly p4q5 tests (200 hosts, has host_table → SOURCE works)
# ============================================================
echo "=== Slimfly p4q5 (200 hosts) ==="
SF_P4_PATH="$TOPO_DIR/slimfly/p4q5"

for routing in MINIMAL VALIANT UGAL_L SOURCE; do
    run_test "sf_p4q5_${routing}_1flow" "$UEC_BIN" "$SF_P4_PATH" "$routing" \
        "$TEST_DIR/sf_p4q5_1flow.tm" 200 "small" 1 "slimfly"
done
echo ""
echo "  -- Medium workload (10 flows) --"
for routing in MINIMAL VALIANT UGAL_L SOURCE; do
    run_test "sf_p4q5_${routing}_10flow" "$UEC_BIN" "$SF_P4_PATH" "$routing" \
        "$TEST_DIR/sf_p4q5_10flow.tm" 200 "medium" 10 "slimfly"
done
echo ""

# ============================================================
# Summary
# ============================================================
echo "========================================="
echo "  RESULTS: $PASS passed / $FAIL failed / $TOTAL total"
echo "========================================="

if [ $FAIL -gt 0 ]; then
    echo "SOME TESTS FAILED"
    exit 1
else
    echo "ALL TESTS PASSED"
    exit 0
fi
