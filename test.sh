#!/bin/bash

# Comprehensive IRC Server Test Suite
# Tests all major IRC functionality

SERVER_HOST="localhost"
SERVER_PORT="6667"
PASSWORD="password"
SERVER_PID=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Cleanup function
cleanup() {
	echo -e "\n${YELLOW}=== Cleaning up ===${NC}"
	# Kill all background processes
	pkill -P $$ 2>/dev/null || true
	if [ ! -z "$SERVER_PID" ]; then
		kill $SERVER_PID 2>/dev/null || true
		sleep 0.5
		kill -9 $SERVER_PID 2>/dev/null || true
	fi
	# Remove all test files
	rm -f /tmp/irc_test_* 2>/dev/null || true
}

# Set trap to cleanup on exit
trap cleanup EXIT INT TERM

# Test function
run_test() {
	local test_name="$1"
	TESTS_RUN=$((TESTS_RUN + 1))
	echo -e "\n${YELLOW}Test $TESTS_RUN: $test_name${NC}"
}

pass_test() {
	TESTS_PASSED=$((TESTS_PASSED + 1))
	echo -e "${GREEN}✓ PASSED${NC}"
}

fail_test() {
	TESTS_FAILED=$((TESTS_FAILED + 1))
	echo -e "${RED}✗ FAILED: $1${NC}"
}

# Helper function to send IRC commands
send_irc() {
	local fifo="$1"
	shift
	while [ $# -gt 0 ]; do
		echo "$1" >> "$fifo"
		shift
	done
}

# Build the server
echo -e "${YELLOW}=== Building Server ===${NC}"
make -C . clean > /dev/null 2>&1 || true
make -C . -j2 > /dev/null 2>&1 || {
	echo -e "${RED}Build failed${NC}"
	exit 1
}
echo -e "${GREEN}✓ Build successful${NC}"

# Start server
echo -e "\n${YELLOW}=== Starting Server ===${NC}"
./ircserv $SERVER_PORT $PASSWORD > /tmp/irc_server.log 2>&1 &
SERVER_PID=$!
sleep 2
if ! kill -0 $SERVER_PID 2>/dev/null; then
	echo -e "${RED}Server failed to start${NC}"
	cat /tmp/irc_server.log
	exit 1
fi
echo -e "${GREEN}✓ Server started (PID: $SERVER_PID)${NC}"

# ============== TESTS ==============

# Test 1: Basic connection and auth
run_test "Connection, PASS, NICK, USER commands"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK alice"
		sleep 0.1
		echo "USER alice 0 * :Alice User"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_1.out 2>&1
if grep -q "Welcome\|001" /tmp/irc_test_1.out 2>/dev/null; then
	pass_test
else
	fail_test "No welcome message received"
fi

# Test 2: Duplicate nickname rejection
run_test "Duplicate nickname rejection (error 433)"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK bob"
		sleep 0.1
		echo "NICK bob"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_2.out 2>&1
if grep -q "433" /tmp/irc_test_2.out 2>/dev/null; then
	pass_test
else
	fail_test "Nickname not rejected as duplicate (no 433 error)"
fi

# Test 3: JOIN channel
run_test "JOIN channel and receive NAMES (353)"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK user1"
		sleep 0.1
		echo "USER user1 0 * :User One"
		sleep 0.2
		echo "JOIN #testchan"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_3.out 2>&1
if grep -q "353\|NAMES" /tmp/irc_test_3.out 2>/dev/null; then
	pass_test
else
	fail_test "No NAMES response on JOIN (no 353)"
fi

# Test 4: Channel message
run_test "PRIVMSG to channel"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK user2"
		sleep 0.1
		echo "USER user2 0 * :User Two"
		sleep 0.2
		echo "JOIN #channel2"
		sleep 0.2
		echo "PRIVMSG #channel2 :Test message"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_4.out 2>&1
if grep -q "Test message" /tmp/irc_test_4.out 2>/dev/null; then
	pass_test
else
	fail_test "Channel message not echoed back"
fi

# Test 5: Direct message
run_test "PRIVMSG direct message"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK user3"
		sleep 0.1
		echo "USER user3 0 * :User Three"
		sleep 0.2
		echo "PRIVMSG user2 :Direct message"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_5.out 2>&1
if grep -q "Direct message" /tmp/irc_test_5.out 2>/dev/null; then
	pass_test
else
	fail_test "Direct message not sent"
fi

# Test 6: TOPIC command
run_test "TOPIC command (set and query)"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK user4"
		sleep 0.1
		echo "USER user4 0 * :User Four"
		sleep 0.2
		echo "JOIN #topictest"
		sleep 0.2
		echo "TOPIC #topictest :This is a test topic"
		sleep 0.2
		echo "TOPIC #topictest"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_6.out 2>&1
if grep -q "test topic" /tmp/irc_test_6.out 2>/dev/null; then
	pass_test
else
	fail_test "TOPIC not set or retrieved"
fi

# Test 7: LIST command
run_test "LIST command"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK user5"
		sleep 0.1
		echo "USER user5 0 * :User Five"
		sleep 0.2
		echo "LIST"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_7.out 2>&1
if grep -q "321\|322\|323" /tmp/irc_test_7.out 2>/dev/null; then
	pass_test
else
	fail_test "LIST response missing (no 321/322/323)"
fi

# Test 8: MODE command
run_test "MODE command (+i, +t, +k)"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK user6"
		sleep 0.1
		echo "USER user6 0 * :User Six"
		sleep 0.2
		echo "JOIN #modetest"
		sleep 0.2
		echo "MODE #modetest +i"
		sleep 0.1
		echo "MODE #modetest +t"
		sleep 0.1
		echo "MODE #modetest +k testkey"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_8.out 2>&1
if grep -q "MODE" /tmp/irc_test_8.out 2>/dev/null; then
	pass_test
else
	fail_test "MODE commands not processed"
fi

# Test 9: MODE limit
run_test "MODE +l (member limit)"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK user7"
		sleep 0.1
		echo "USER user7 0 * :User Seven"
		sleep 0.2
		echo "JOIN #limitest"
		sleep 0.2
		echo "MODE #limitest +l 50"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_9.out 2>&1
if grep -q "MODE" /tmp/irc_test_9.out 2>/dev/null; then
	pass_test
else
	fail_test "MODE +l not processed"
fi

# Test 10: PART command
run_test "PART channel"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK user8"
		sleep 0.1
		echo "USER user8 0 * :User Eight"
		sleep 0.2
		echo "JOIN #parttest"
		sleep 0.2
		echo "PART #parttest"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_10.out 2>&1
if grep -q "PART\|parttest" /tmp/irc_test_10.out 2>/dev/null; then
	pass_test
else
	fail_test "PART not broadcast"
fi

# Test 11: QUIT command
run_test "QUIT command"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK user9"
		sleep 0.1
		echo "USER user9 0 * :User Nine"
		sleep 0.2
		echo "QUIT"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_11.out 2>&1
if grep -q "QUIT" /tmp/irc_test_11.out 2>/dev/null; then
	pass_test
else
	fail_test "QUIT not processed"
fi

# Test 12: Error handling - non-existent channel
run_test "Error handling - non-existent channel (403)"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK user10"
		sleep 0.1
		echo "USER user10 0 * :User Ten"
		sleep 0.2
		echo "PRIVMSG #nonexist :test"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_12.out 2>&1
if grep -q "403" /tmp/irc_test_12.out 2>/dev/null; then
	pass_test
else
	fail_test "No 403 error for non-existent channel"
fi

# Test 13: Password rejection
run_test "Wrong password rejection (464)"
{
	(
		sleep 0.1
		echo "PASS wrongpass"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_13.out 2>&1
if grep -q "464" /tmp/irc_test_13.out 2>/dev/null; then
	pass_test
else
	fail_test "Wrong password not rejected"
fi

# Test 14: NICK change
run_test "NICK change (renaming)"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK oldname"
		sleep 0.1
		echo "USER oldname 0 * :Old Name"
		sleep 0.2
		echo "NICK newname"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_14.out 2>&1
if grep -q "newname" /tmp/irc_test_14.out 2>/dev/null; then
	pass_test
else
	fail_test "NICK change not processed"
fi

# Test 15: Multiple channels
run_test "JOIN multiple channels"
{
	(
		sleep 0.1
		echo "PASS $PASSWORD"
		sleep 0.1
		echo "NICK user11"
		sleep 0.1
		echo "USER user11 0 * :User Eleven"
		sleep 0.2
		echo "JOIN #chan1"
		sleep 0.1
		echo "JOIN #chan2"
		sleep 0.1
		echo "JOIN #chan3"
		sleep 0.5
	) | timeout 2 nc $SERVER_HOST $SERVER_PORT
} > /tmp/irc_test_15.out 2>&1
if grep -q "353" /tmp/irc_test_15.out 2>/dev/null; then
	pass_test
else
	fail_test "Multiple channel joins failed"
fi

# ============== SUMMARY ==============

echo -e "\n${YELLOW}========================================${NC}"
echo -e "${YELLOW}Test Results Summary${NC}"
echo -e "${YELLOW}========================================${NC}"
echo -e "Tests Run:    $TESTS_RUN"
echo -e "${GREEN}Tests Passed: $TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
	echo -e "${RED}Tests Failed: $TESTS_FAILED${NC}"
else
	echo -e "${GREEN}Tests Failed: $TESTS_FAILED${NC}"
fi
echo -e "${YELLOW}========================================${NC}"

if [ $TESTS_FAILED -eq 0 ]; then
	echo -e "\n${GREEN}All tests passed! ✓${NC}"
	exit 0
else
	echo -e "\n${RED}Some tests failed.${NC}"
	echo -e "\n${YELLOW}Debug info - Server log:${NC}"
	tail -20 /tmp/irc_server.log
	exit 1
fi
