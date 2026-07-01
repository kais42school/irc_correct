#!/bin/bash

# Simple Bot Test - Manual Testing
# Just start the server and bot, then manually test or let them run

cd /home/bhamoum/42/irc

echo "========================================"
echo "         IRC Bot Manual Test"
echo "========================================"
echo

# Clean up
killall -9 ircserv ircbot 2>/dev/null
sleep 1

# Build
echo "Building..."
make clean > /dev/null 2>&1
if ! make > /dev/null 2>&1; then
    echo "✗ Build failed"
    exit 1
fi
echo "✓ Built successfully"
echo

# Start server
echo "Starting server..."
./ircserv 6667 password > /tmp/server.log 2>&1 &
SERVER_PID=$!
sleep 1
echo "✓ Server started (PID: $SERVER_PID)"

# Start bot
echo "Starting bot..."
./ircbot 127.0.0.1 6667 password "TestBot" "#general" > /tmp/bot.log 2>&1 &
BOT_PID=$!
sleep 2
echo "✓ Bot started (PID: $BOT_PID)"
echo

echo "========================================"
echo "Testing bot with manual nc connection"
echo "========================================"
echo

# Open a connection and send commands
exec 3<>/dev/tcp/127.0.0.1/6667

# Register
echo -en "PASS password\r\n" >&3
echo -en "NICK testuser\r\n" >&3
echo -en "USER testuser 0 * :Test User\r\n" >&3

# Read welcome
echo "Reading welcome..."
for i in {1..10}; do
    read -r -t 0.5 line <&3 2>/dev/null
    if [ -n "$line" ]; then
        echo "  < $line"
    fi
done

# Join channel
echo "Joining #general..."
echo -en "JOIN #general\r\n" >&3

# Read JOIN response
for i in {1..10}; do
    read -r -t 0.5 line <&3 2>/dev/null
    if [ -n "$line" ]; then
        echo "  < $line"
    fi
done

# Send bot commands
echo "Sending bot commands..."

echo "  > PRIVMSG #general :!help"
echo -en "PRIVMSG #general :!help\r\n" >&3

echo "  Reading responses..."
for i in {1..10}; do
    read -r -t 1 line <&3 2>/dev/null
    if [ -n "$line" ]; then
        echo "  < $line"
    fi
done

echo "  > PRIVMSG #general :!time"
echo -en "PRIVMSG #general :!time\r\n" >&3

echo "  Reading responses..."
for i in {1..10}; do
    read -r -t 1 line <&3 2>/dev/null
    if [ -n "$line" ]; then
        echo "  < $line"
    fi
done

echo "  > PRIVMSG #general :!echo Test message"
echo -en "PRIVMSG #general :!echo Test message\r\n" >&3

echo "  Reading responses..."
for i in {1..10}; do
    read -r -t 1 line <&3 2>/dev/null
    if [ -n "$line" ]; then
        echo "  < $line"
    fi
done

# Close connection
exec 3>&-

echo
echo "========================================"
echo "Test complete!"
echo "========================================"
echo

# Show logs
echo "Bot log:"
echo "----------------------------------------"
tail -30 /tmp/bot.log

echo
echo "Server log:"
echo "----------------------------------------"
tail -20 /tmp/server.log

# Cleanup
echo
echo "Cleaning up..."
kill $BOT_PID $SERVER_PID 2>/dev/null
sleep 1
echo "Done!"
