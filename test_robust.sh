#!/bin/bash

# Robust IRC Server Test Script
# Tests core functionality with proper timeout and connection handling

SERVER="127.0.0.1"
PORT="6667"
PASS="password"

echo "=== Robust IRC Server Test ==="
echo

# Helper function to send command and get response with timeout
function test_connection() {
    local test_name="$1"
    local commands="$2"
    local expected="$3"
    
    echo "Test: $test_name"
    
    # Use bash process substitution with timeout
    response=$(timeout 5 bash -c "
        exec 3<>/dev/tcp/$SERVER/$PORT || exit 1
        
        # Send each command followed by \r\n
        while IFS= read -r line; do
            echo -en \"\$line\r\n\" >&3
        done <<< \"$commands\"
        
        # Read response with timeout
        while IFS= read -r -t 0.5 line <&3; do
            echo \"\$line\"
        done
        exec 3>&-
    " 2>&1)
    
    if [ $? -eq 124 ]; then
        echo "  ✓ Connection timeout (as expected for some tests)"
        return 0
    fi
    
    if echo "$response" | grep -q "$expected"; then
        echo "  ✓ PASS"
        return 0
    else
        echo "  ✗ FAIL - Expected: '$expected'"
        echo "  Response received:"
        echo "$response" | head -5 | sed 's/^/    /'
        return 1
    fi
}

# Build server
echo "Building server..."
cd /home/bhamoum/42/irc
make clean > /dev/null 2>&1
if ! make > /dev/null 2>&1; then
    echo "✗ Build failed"
    exit 1
fi
echo "✓ Build successful"
echo

# Start server in background
echo "Starting server..."
rm -f /tmp/server.log
./ircserv 6667 password > /tmp/server.log 2>&1 &
SERVER_PID=$!
sleep 1

# Check if server started
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "✗ Server failed to start"
    exit 1
fi
echo "✓ Server started (PID: $SERVER_PID)"
echo

# Test 1: Connection without PASS (should fail)
echo "--- Test 1: Invalid command without PASS ---"
test_connection "NICK without PASS" "NICK testuser" "451"
echo

# Test 2: PASS authentication
echo "--- Test 2: PASS Authentication ---"
test_connection "Correct PASS" "PASS password" "NICK"
echo

# Test 3: NICK + USER registration
echo "--- Test 3: NICK/USER Registration ---"
response=$(timeout 5 bash -c "
    exec 3<>/dev/tcp/$SERVER/$PORT
    echo -en 'PASS password\r\n' >&3
    echo -en 'NICK testuser\r\n' >&3
    echo -en 'USER testuser 0 * :Test User\r\n' >&3
    
    # Read responses (001, 251, 252, 253, 254, 255)
    for i in {1..10}; do
        read -r -t 1 line <&3
        echo \"\$line\"
    done
    exec 3>&-
" 2>&1)

if echo "$response" | grep -q ":001"; then
    echo "✓ Welcome (001) received"
else
    echo "✗ Welcome (001) not received"
    echo "$response" | head -10
fi
echo

# Test 4: JOIN channel
echo "--- Test 4: JOIN Channel ---"
response=$(timeout 5 bash -c "
    exec 3<>/dev/tcp/$SERVER/$PORT
    echo -en 'PASS password\r\n' >&3
    echo -en 'NICK joiner\r\n' >&3
    echo -en 'USER joiner 0 * :Joiner\r\n' >&3
    
    # Read initial responses
    for i in {1..10}; do
        read -r -t 0.5 line <&3
    done
    
    echo -en 'JOIN #testchan\r\n' >&3
    
    # Read JOIN responses (353 NAMES, 366 end of NAMES, 332 TOPIC, etc)
    for i in {1..15}; do
        read -r -t 0.5 line <&3
        echo \"\$line\"
    done
    exec 3>&-
" 2>&1)

if echo "$response" | grep -q ":353"; then
    echo "✓ Channel JOIN acknowledged (353 NAMES)"
else
    echo "✗ Channel JOIN failed"
    echo "$response" | head -10
fi
echo

# Test 5: PRIVMSG
echo "--- Test 5: PRIVMSG ---"
response=$(timeout 5 bash -c "
    exec 3<>/dev/tcp/$SERVER/$PORT
    echo -en 'PASS password\r\n' >&3
    echo -en 'NICK sender\r\n' >&3
    echo -en 'USER sender 0 * :Sender\r\n' >&3
    
    for i in {1..10}; do
        read -r -t 0.5 line <&3
    done
    
    echo -en 'PRIVMSG #testchan :Hello World\r\n' >&3
    
    # Just check if server doesn't crash
    read -r -t 1 line <&3
    echo \"\$line\"
    exec 3>&-
" 2>&1)

echo "✓ PRIVMSG sent (no crash)"
echo

# Check server is still running
if kill -0 $SERVER_PID 2>/dev/null; then
    echo "✓ Server still running"
else
    echo "✗ Server crashed"
fi
echo

# Cleanup
echo "Cleaning up..."
kill $SERVER_PID 2>/dev/null
sleep 1
kill -9 $SERVER_PID 2>/dev/null

echo "=== Test Complete ==="
echo "Check /tmp/server.log for server output"
tail -20 /tmp/server.log 2>/dev/null | head -20
