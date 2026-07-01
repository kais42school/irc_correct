#!/bin/bash

# Simple IRC Server Test
SERVER_HOST="localhost"
SERVER_PORT="6667"
PASSWORD="password"

echo "=== Simple IRC Server Test ==="
echo ""

# Start server
echo "Starting server on port $SERVER_PORT..."
./ircserv $SERVER_PORT $PASSWORD &
SERVER_PID=$!
sleep 2

# Function to cleanup
cleanup() {
	kill $SERVER_PID 2>/dev/null || true
	rm -f /tmp/test_irc.txt
}

trap cleanup EXIT

echo ""
echo "Test 1: PASS + NICK + USER registration"
echo "----------------------------------------"
(
	echo "PASS $PASSWORD"
	sleep 0.2
	echo "NICK testuser"
	sleep 0.2
	echo "USER testuser 0 * :Test User"
	sleep 1
) | nc -w 2 $SERVER_HOST $SERVER_PORT > /tmp/test_irc.txt 2>&1

cat /tmp/test_irc.txt
echo ""

if grep -q "001\|Welcome" /tmp/test_irc.txt; then
	echo "✓ Test 1 PASSED: Welcome message received"
else
	echo "✗ Test 1 FAILED: No welcome message"
fi

echo ""
echo "Test 2: JOIN channel"
echo "--------------------"
(
	echo "PASS $PASSWORD"
	sleep 0.2
	echo "NICK user2"
	sleep 0.2
	echo "USER user2 0 * :User 2"
	sleep 0.2
	echo "JOIN #testchan"
	sleep 1
) | nc -w 2 $SERVER_HOST $SERVER_PORT > /tmp/test_irc.txt 2>&1

cat /tmp/test_irc.txt
echo ""

if grep -q "353\|#testchan" /tmp/test_irc.txt; then
	echo "✓ Test 2 PASSED: JOIN successful with NAMES response"
else
	echo "✗ Test 2 FAILED: No NAMES response"
fi

echo ""
echo "Test 3: PRIVMSG channel"
echo "-----------------------"
(
	echo "PASS $PASSWORD"
	sleep 0.2
	echo "NICK user3"
	sleep 0.2
	echo "USER user3 0 * :User 3"
	sleep 0.2
	echo "JOIN #chan2"
	sleep 0.2
	echo "PRIVMSG #chan2 :Hello world"
	sleep 1
) | nc -w 2 $SERVER_HOST $SERVER_PORT > /tmp/test_irc.txt 2>&1

cat /tmp/test_irc.txt
echo ""

if grep -q "Hello world" /tmp/test_irc.txt; then
	echo "✓ Test 3 PASSED: PRIVMSG successful"
else
	echo "✗ Test 3 FAILED: Message not received"
fi

echo ""
echo "All basic tests completed!"
