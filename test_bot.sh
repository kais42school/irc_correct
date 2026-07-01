#!/bin/bash

# Bot Test Script
# Tests bot functionality including command responses

SERVER="127.0.0.1"
PORT="6667"
PASS="password"

echo "=========================================="
echo "         IRC Bot Test Suite"
echo "=========================================="
echo

# Clean up
pkill -9 ircserv 2>/dev/null
pkill -9 ircbot 2>/dev/null
sleep 1

# Start server
echo "Starting server..."
rm -f /tmp/server.log
cd /home/bhamoum/42/irc
./ircserv 6667 password > /tmp/server.log 2>&1 &
SERVER_PID=$!
sleep 1

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "✗ Server failed to start"
    exit 1
fi
echo "✓ Server started (PID: $SERVER_PID)"

# Start bot
echo "Starting bot..."
rm -f /tmp/bot.log
./ircbot 127.0.0.1 6667 password "TestBot" "#general" > /tmp/bot.log 2>&1 &
BOT_PID=$!
sleep 2

if ! kill -0 $BOT_PID 2>/dev/null; then
    echo "✗ Bot failed to start"
    kill $SERVER_PID
    exit 1
fi
echo "✓ Bot started (PID: $BOT_PID)"
echo

echo "=== Running Bot Command Tests ==="
echo

# Test 1: !help command
echo "Test 1: !help command"
response=$(timeout 3 bash -c "
    exec 3<>/dev/tcp/$SERVER/$PORT
    echo -en 'PASS password\r\n' >&3
    echo -en 'NICK user1\r\n' >&3
    echo -en 'USER user1 0 * :User One\r\n' >&3
    
    # Read welcome
    for i in {1..10}; do
        read -r -t 0.5 line <&3 2>/dev/null
    done
    
    # Join channel
    echo -en 'JOIN #general\r\n' >&3
    for i in {1..10}; do
        read -r -t 0.5 line <&3 2>/dev/null
    done
    
    # Send help command
    echo -en 'PRIVMSG #general :!help\r\n' >&3
    
    # Read responses (bot should respond with help message)
    for i in {1..15}; do
        read -r -t 1 line <&3 2>/dev/null
        echo \"\$line\"
    done
    exec 3>&-
" 2>&1)

if echo "$response" | grep -q "Available commands"; then
    echo "✓ PASS - Bot responded to !help"
else
    echo "✗ FAIL - Bot did not respond to !help"
fi
echo

# Test 2: !time command
echo "Test 2: !time command"
response=$(timeout 3 bash -c "
    exec 3<>/dev/tcp/$SERVER/$PORT
    echo -en 'PASS password\r\n' >&3
    echo -en 'NICK user2\r\n' >&3
    echo -en 'USER user2 0 * :User Two\r\n' >&3
    
    for i in {1..10}; do
        read -r -t 0.5 line <&3 2>/dev/null
    done
    
    echo -en 'JOIN #general\r\n' >&3
    for i in {1..10}; do
        read -r -t 0.5 line <&3 2>/dev/null
    done
    
    echo -en 'PRIVMSG #general :!time\r\n' >&3
    
    for i in {1..15}; do
        read -r -t 1 line <&3 2>/dev/null
        echo \"\$line\"
    done
    exec 3>&-
" 2>&1)

if echo "$response" | grep -q "Current server time"; then
    echo "✓ PASS - Bot responded to !time"
else
    echo "✗ FAIL - Bot did not respond to !time"
fi
echo

# Test 3: !uptime command
echo "Test 3: !uptime command"
response=$(timeout 3 bash -c "
    exec 3<>/dev/tcp/$SERVER/$PORT
    echo -en 'PASS password\r\n' >&3
    echo -en 'NICK user3\r\n' >&3
    echo -en 'USER user3 0 * :User Three\r\n' >&3
    
    for i in {1..10}; do
        read -r -t 0.5 line <&3 2>/dev/null
    done
    
    echo -en 'JOIN #general\r\n' >&3
    for i in {1..10}; do
        read -r -t 0.5 line <&3 2>/dev/null
    done
    
    echo -en 'PRIVMSG #general :!uptime\r\n' >&3
    
    for i in {1..15}; do
        read -r -t 1 line <&3 2>/dev/null
        echo \"\$line\"
    done
    exec 3>&-
" 2>&1)

if echo "$response" | grep -q "Bot uptime"; then
    echo "✓ PASS - Bot responded to !uptime"
else
    echo "✗ FAIL - Bot did not respond to !uptime"
fi
echo

# Test 4: !echo command
echo "Test 4: !echo command"
response=$(timeout 3 bash -c "
    exec 3<>/dev/tcp/$SERVER/$PORT
    echo -en 'PASS password\r\n' >&3
    echo -en 'NICK user4\r\n' >&3
    echo -en 'USER user4 0 * :User Four\r\n' >&3
    
    for i in {1..10}; do
        read -r -t 0.5 line <&3 2>/dev/null
    done
    
    echo -en 'JOIN #general\r\n' >&3
    for i in {1..10}; do
        read -r -t 0.5 line <&3 2>/dev/null
    done
    
    echo -en 'PRIVMSG #general :!echo Hello Bot\r\n' >&3
    
    for i in {1..15}; do
        read -r -t 1 line <&3 2>/dev/null
        echo \"\$line\"
    done
    exec 3>&-
" 2>&1)

if echo "$response" | grep -q "Echo: Hello Bot"; then
    echo "✓ PASS - Bot echoed message"
else
    echo "✗ FAIL - Bot did not echo"
fi
echo

# Test 5: Welcome message on join
echo "Test 5: Welcome message on user join"
response=$(timeout 3 bash -c "
    exec 3<>/dev/tcp/$SERVER/$PORT
    echo -en 'PASS password\r\n' >&3
    echo -en 'NICK user5\r\n' >&3
    echo -en 'USER user5 0 * :User Five\r\n' >&3
    
    for i in {1..10}; do
        read -r -t 0.5 line <&3 2>/dev/null
    done
    
    echo -en 'JOIN #general\r\n' >&3
    
    for i in {1..20}; do
        read -r -t 1 line <&3 2>/dev/null
        echo \"\$line\"
    done
    exec 3>&-
" 2>&1)

if echo "$response" | grep -q "Welcome"; then
    echo "✓ PASS - Bot sent welcome message"
else
    echo "✗ FAIL - Bot did not send welcome"
fi
echo

# Cleanup
echo "=========================================="
echo "Cleaning up..."
kill $BOT_PID 2>/dev/null
kill $SERVER_PID 2>/dev/null
sleep 1

echo "✓ Done!"
echo

# Show bot log
echo "Bot Log (first 30 lines):"
echo "----------------------------------------"
head -30 /tmp/bot.log 2>/dev/null || echo "(No log file)"
