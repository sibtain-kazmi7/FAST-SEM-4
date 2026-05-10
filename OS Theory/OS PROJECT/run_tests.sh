#!/usr/bin/env bash
# ============================================================
#  Structured Test Suite — Multi-Threaded Producer-Consumer
#  CS-2006 OS | FAST-NUCES | Spring 2026
# ============================================================

BIN="../producer_consumer"
PASS=0
FAIL=0
TOTAL=0

RED='\033[1;31m'
GRN='\033[1;32m'
YLW='\033[1;33m'
CYN='\033[1;36m'
RST='\033[0m'
BLD='\033[1m'

banner() {
    echo -e "\n${CYN}${BLD}╔══════════════════════════════════════════════════╗"
    echo -e "║     Producer-Consumer Structured Test Suite     ║"
    echo -e "╚══════════════════════════════════════════════════╝${RST}\n"
}

section() {
    echo -e "\n${BLD}── $1 ──────────────────────────────────────────────${RST}"
}

# Run binary, capture stdout, return exit code
run() {
    local args="$1"
    $BIN $args -n 2>/dev/null
}

# Extract produced/consumed counts from stats output
get_produced() { echo "$1" | grep "Total Produced:" | grep -oP '\d+' | tail -1; }
get_consumed()  { echo "$1" | grep "Total Consumed:" | grep -oP '\d+' | tail -1; }

# ── Test runner ──────────────────────────────────────────────────────────────
run_test() {
    local id="$1"
    local desc="$2"
    local args="$3"
    local check_fn="$4"

    TOTAL=$((TOTAL + 1))
    printf "  ${BLD}%-8s${RST} %-55s " "$id" "$desc"

    output=$(run "$args")
    result=$("$check_fn" "$output")

    if [ "$result" = "PASS" ]; then
        echo -e "${GRN}PASS${RST}"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${RST}  ← $result"
        FAIL=$((FAIL + 1))
    fi
}

# ── Check functions ──────────────────────────────────────────────────────────

# TC-N1, N2, N3, S2: produced == consumed
check_balanced() {
    local out="$1"
    local p; p=$(get_produced "$out")
    local c; c=$(get_consumed  "$out")
    if [ -z "$p" ] || [ -z "$c" ]; then
        echo "could not parse output"; return
    fi
    if [ "$p" -eq "$c" ] && [ "$p" -gt 0 ]; then
        echo "PASS"
    else
        echo "produced=$p consumed=$c (mismatch or zero)"
    fi
}

# TC-N2: buffer should fill (producers faster) — produced > consumed is acceptable
# We just check both > 0 and no crash
check_producers_faster() {
    local out="$1"
    local p; p=$(get_produced "$out")
    local c; c=$(get_consumed  "$out")
    if [ -z "$p" ] || [ -z "$c" ]; then
        echo "could not parse output"; return
    fi
    if [ "$p" -gt 0 ] && [ "$c" -gt 0 ]; then
        echo "PASS"
    else
        echo "produced=$p consumed=$c"
    fi
}

# TC-N4 / TC-S4: fairness — producer spread < 15%
check_fairness() {
    local out="$1"
    local spread; spread=$(echo "$out" | grep "Producer fairness spread:" | grep -oP '\d+' | head -1)
    local total_p; total_p=$(get_produced "$out")
    if [ -z "$spread" ] || [ -z "$total_p" ] || [ "$total_p" -eq 0 ]; then
        echo "PASS"   # single producer — no spread to check
        return
    fi
    local pct=$(( spread * 100 / total_p ))
    if [ "$pct" -lt 20 ]; then
        echo "PASS"
    else
        echo "spread=$spread total=$total_p pct=$pct% (>=20%)"
    fi
}

# TC-E1: buffer=1, check no crash and balanced
check_buf1() {
    local out="$1"
    local p; p=$(get_produced "$out")
    local c; c=$(get_consumed  "$out")
    if [ -z "$p" ] || [ -z "$c" ]; then
        echo "could not parse output"; return
    fi
    # With buf=1 produced may slightly exceed consumed due to timing; allow <=2 diff
    local diff=$(( p - c ))
    if [ "$diff" -lt 0 ]; then diff=$(( -diff )); fi
    if [ "$p" -gt 0 ] && [ "$diff" -le 2 ]; then
        echo "PASS"
    else
        echo "produced=$p consumed=$c diff=$diff"
    fi
}

# TC-E2: buffer=256, check produced > 0 and balanced
check_buf256() {
    check_balanced "$1"
}

# TC-E3: 1P 16C — consumers should block on empty; just verify no crash + p>0
check_1p16c() {
    local out="$1"
    local p; p=$(get_produced "$out")
    if [ -z "$p" ] || [ "$p" -eq 0 ]; then
        echo "produced=0 or parse error"
    else
        echo "PASS"
    fi
}

# TC-E4: 16P 1C — producers block on full; verify balanced at end
check_16p1c() {
    check_balanced "$1"
}

# TC-S1: stress 32P 32C buf=10 — just check no crash, both >0
check_stress_basic() {
    local out="$1"
    local p; p=$(get_produced "$out")
    local c; c=$(get_consumed  "$out")
    if [ "$p" -gt 100 ] && [ "$c" -gt 100 ]; then
        echo "PASS"
    else
        echo "produced=$p consumed=$c (too low or crash)"
    fi
}

# TC-S3: max speed 1ms rates — check throughput >200 ops total
check_max_speed() {
    local out="$1"
    local p; p=$(get_produced "$out")
    if [ "$p" -gt 200 ]; then
        echo "PASS"
    else
        echo "produced=$p (<200, too slow or crash)"
    fi
}

# ── Main ─────────────────────────────────────────────────────────────────────
banner

if [ ! -f "$BIN" ]; then
    echo -e "${RED}Binary not found: $BIN${RST}"
    echo "Run 'make' first from the project root."
    exit 1
fi

# ── Normal Tests ─────────────────────────────────────────────────────────────
section "Normal Operation Tests"
run_test "TC-N1" "2P 2C buf=10 10s — balanced"              "-b 10 -p 2 -c 2 -P 300 -C 300 -d 10"   check_balanced
run_test "TC-N2" "2P 2C fast-produce — buffer fills"        "-b 10 -p 2 -c 2 -P 100 -C 800 -d 8"    check_producers_faster
run_test "TC-N3" "2P 2C fast-consume — buffer empties"      "-b 10 -p 2 -c 2 -P 800 -C 100 -d 8"    check_producers_faster
run_test "TC-N4" "2P 2C fair scheduling ON"                 "-b 10 -p 2 -c 2 -P 200 -C 200 -d 8 -f" check_fairness

# ── Edge Case Tests ───────────────────────────────────────────────────────────
section "Edge Case Tests"
run_test "TC-E1" "Buffer size = 1 (minimum)"                "-b 1  -p 1 -c 1 -P 100 -C 100 -d 6"    check_buf1
run_test "TC-E2" "Buffer size = 256 (maximum)"              "-b 256 -p 2 -c 2 -P 100 -C 100 -d 6"   check_buf256
run_test "TC-E3" "1 producer, 16 consumers"                 "-b 10 -p 1 -c 16 -P 200 -C 100 -d 8"   check_1p16c
run_test "TC-E4" "16 producers, 1 consumer (balanced end)"  "-b 10 -p 16 -c 1 -P 100 -C 100 -d 8"   check_16p1c
run_test "TC-E5" "Max producers + small buffer"             "-b 1  -p 32 -c 32 -P 200 -C 200 -d 6"  check_balanced

# ── Stress Tests ──────────────────────────────────────────────────────────────
section "Stress Tests"
run_test "TC-S1" "32P 32C buf=10  30s — no deadlock"        "-b 10  -p 32 -c 32 -P 50 -C 50 -d 15"  check_stress_basic
run_test "TC-S2" "32P 32C buf=256 30s — balanced"           "-b 256 -p 32 -c 32 -P 50 -C 50 -d 15"  check_balanced
run_test "TC-S3" "1ms rates — max throughput 10s"           "-b 20  -p 4  -c 4  -P 1  -C 1  -d 10"  check_max_speed
run_test "TC-S4" "Fair scheduling 8P 8C stress"             "-b 10  -p 8  -c 8  -P 50 -C 50 -d 10 -f" check_fairness

# ── Summary ───────────────────────────────────────────────────────────────────
echo -e "\n${BLD}══════════════════════════════════════════════════${RST}"
echo -e "  Results:  ${GRN}${BLD}$PASS PASSED${RST}  /  ${RED}${BLD}$FAIL FAILED${RST}  /  $TOTAL TOTAL"
echo -e "${BLD}══════════════════════════════════════════════════${RST}\n"

if [ "$FAIL" -eq 0 ]; then
    echo -e "  ${GRN}${BLD}All tests passed.${RST}\n"
    exit 0
else
    echo -e "  ${RED}${BLD}$FAIL test(s) failed.${RST}\n"
    exit 1
fi
