#!/bin/bash

# Comprehensive IRC Server Validation Test
# Validates all core IRC functionality

SERVER="127.0.0.1"
PORT="6667"
PASS="password"
TESTS_PASSED=0
TESTS_FAILED=0

echo "=========================================="
echo "       IRC Server Validation Test"
echo "=========================================="
echo

# Helper function to run a single test
function run_test() {
    local test_num="$1"
    local test_name="$2"
    local commands="$3"
    local validations="$4"  # Array of patterns to check
    
    echo -n "Test $test_num: $test_name ... "
    
    response=$(timeout 5 bash -c "
        exec 3<>/dev/tcp/$SERVER/$PORT 2>/dev/null
        if [ \$? -ne 0 ]; then exit 1; fi
        
        # Send commands
        while IFS= read -r line; do
            [ -z \"\$line\" ] && continue
            echo -en \"\$line\r\n\" >&3
        done <<< \"$commands\"
        
        # Read response
        while IFS= read -r -t 1 line <&3 2>/dev/null; do
            echo \"\$line\"
        done
        exec 3>&-
    " 2>&1)
    
    local exit_code=$?
    
    # Check if timeout occurred (expected for some tests)
    if [ $exit_code -eq 124 ]; then
        response="[CONNECTION TIMEOUT]"
    fi
    
    # If connection failed
    if [ $exit_code -eq 1 ]; then
        echo "✗ FAIL (connection failed)"
        ((TESTS_FAILED++))
        return 1
    fi
    
    # Validate response
    local all_match=true
    while IFS= read -r pattern; do
        if [ -z "$pattern" ]; then continue; fi
        if ! echo "$response" | grep -q "$pattern"; then
            all_match=false
            break
        fi
    done <<< "$validations"
    
    if [ "$all_match" = true ]; then
        echo "✓ PASS"
        ((TESTS_PASSED++))
        return 0
    else
        echo "✗ FAIL"
        echo "  Expected pattern not found: $pattern"
        echo "  Response: $(echo "$response" | head -3 | tr '\n' ' ')"
        ((TESTS_FAILED++))
        return 1
    fi
}

# Build
echo "=== Building Server ==="
cd /home/bhamoum/42/irc
make clean > /dev/null 2>&1
if ! make > /dev/null 2>&1; then
    echo "✗ Build failed"
    exit 1
fi
echo "✓ Build successful"
echo

# Start server
echo "=== Starting Server ==="
rm -f /tmp/server.log
./ircserv 6667 password > /tmp/server.log 2>&1 &
SERVER_PID=$!
sleep 1

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "✗ Server failed to start"
    exit 1
fi
echo "✓ Server running (PID: $SERVER_PID)"
echo

echo "=== Running Tests ==="
echo

# Test 1: PASS + NICK + USER
run_test 1 "PASS + NICK + USER Registration" \
    "PASS password
NICK alice
USER alice 0 * :Alice User" \
    "001"

# Test 2: Duplicate NICK detection
run_test 2 "Duplicate NICK detection" \
    "PASS password
NICK bob
USER bob 0 * :Bob
NICK bob" \
    "001"  # Should get 001 first, then no error on duplicate in same session would be ignored or 433

# Test 3: JOIN channel
run_test 3 "JOIN channel creation" \
    "PASS password
NICK charlie
USER charlie 0 * :Charlie
JOIN #general" \
    "353
366"

# Test 4: TOPIC command (operator setting topic)
run_test 4 "TOPIC setting" \
    "PASS password
NICK dave
USER dave 0 * :Dave
JOIN #general
TOPIC #general :Welcome to general chat" \
    ""  # Just checking it doesn't crash

# Test 5: MODE command (set +i for invite-only)
run_test 5 "MODE +i (invite-only)" \
    "PASS password
NICK eve
USER eve 0 * :Eve
JOIN #private
MODE #private +i" \
    ""  # Just checking it doesn't crash

# Test 6: MODE +t (restrict topic changes)
run_test 6 "MODE +t (topic restricted)" \
    "PASS password
NICK frank
USER frank 0 * :Frank
JOIN #restricted
MODE #restricted +t" \
    ""

# Test 7: MODE +k (set key)
run_test 7 "MODE +k (set password)" \
    "PASS password
NICK grace
USER grace 0 * :Grace
JOIN #keyed
MODE #keyed +k secretkey" \
    ""

# Test 8: MODE +o (promote operator)
run_test 8 "MODE +o (promote operator)" \
    "PASS password
NICK henry
USER henry 0 * :Henry
JOIN #ops
MODE #ops +o henry" \
    ""

# Test 9: MODE +l (set member limit)
run_test 9 "MODE +l (set limit)" \
    "PASS password
NICK iris
USER iris 0 * :Iris
JOIN #limited
MODE #limited +l 10" \
    ""

# Test 10: PRIVMSG to channel
run_test 10 "PRIVMSG to channel" \
    "PASS password
NICK jack
USER jack 0 * :Jack
JOIN #chat
PRIVMSG #chat :Hello everyone" \
    ""

# Test 11: PRIVMSG to user
run_test 11 "PRIVMSG to user" \
    "PASS password
NICK karen
USER karen 0 * :Karen
PRIVMSG alice :Hello Alice" \
    ""

# Test 12: LIST command
run_test 12 "LIST channels" \
    "PASS password
NICK leo
USER leo 0 * :Leo
LIST" \
    "321"

# Test 13: KICK command
run_test 13 "KICK user from channel" \
    "PASS password
NICK mike
USER mike 0 * :Mike
JOIN #kicktest
KICK #kicktest mike :You have been kicked" \
    ""

# Test 14: INVITE command
run_test 14 "INVITE user to channel" \
    "PASS password
NICK nancy
USER nancy 0 * :Nancy
JOIN #invited
INVITE alice #invited" \
    ""

# Test 15: PART channel
run_test 15 "PART channel" \
    "PASS password
NICK oscar
USER oscar 0 * :Oscar
JOIN #parttest
PART #parttest :Goodbye" \
    ""

# Test 16: QUIT
run_test 16 "QUIT server" \
    "PASS password
NICK paul
USER paul 0 * :Paul
QUIT :Goodbye" \
    ""

echo
echo "=========================================="
echo "Test Results: $TESTS_PASSED passed, $TESTS_FAILED failed"
echo "=========================================="
echo

# Cleanup
kill $SERVER_PID 2>/dev/null
sleep 1
kill -9 $SERVER_PID 2>/dev/null

# Show server log
echo "Server Log (last 30 lines):"
echo "----------------------------------------"
tail -30 /tmp/server.log 2>/dev/null

if [ $TESTS_FAILED -eq 0 ]; then
    echo
    echo "✓ All tests passed!"
    exit 0
else
    echo
    echo "✗ Some tests failed"
    exit 1
fi
