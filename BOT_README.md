# IRC Server with Bot Bonus Feature

## Project Overview

This project implements a full-featured IRC (Internet Relay Chat) server in C++98 with a bonus IRC bot feature. The implementation includes:

### Core Server Features (ft_irc)
- ✅ Non-blocking I/O with single `poll()` system call
- ✅ Password authentication (PASS command)
- ✅ Client registration (NICK/USER commands)
- ✅ Channel management (JOIN/PART/QUIT)
- ✅ Private messaging (PRIVMSG)
- ✅ Channel operators with privileges
- ✅ Channel modes: +i (invite-only), +t (topic restricted), +k (password), +o (operator), +l (member limit)
- ✅ Channel commands: TOPIC, MODE, KICK, INVITE, LIST
- ✅ Proper IRC protocol compliance with numeric replies

### Bonus Feature: IRC Bot
- ✅ Connects as a regular IRC client
- ✅ Joins a default channel and monitors messages
- ✅ Responds to `!` prefixed commands from users
- ✅ Available commands:
  - `!help` - Lists all available commands
  - `!time` - Shows current server time
  - `!uptime` - Shows bot uptime
  - `!echo <message>` - Echoes back a message
  - `!join <channel>` - Bot joins a new channel
  - `!users <channel>` - Lists users in a channel
- ✅ Auto-greets new users when they join a channel
- ✅ Runs as separate executable (no impact on server)

## Building

```bash
cd /home/bhamoum/42/irc
make              # Builds both ircserv and ircbot
make clean        # Removes object files
make fclean       # Removes all binaries and object files
make re           # Rebuilds everything from scratch
```

## Running the Server

```bash
./ircserv <port> <password>
# Example: ./ircserv 6667 password
```

The server will:
- Listen on the specified port
- Require password authentication before allowing commands
- Log all client connections/disconnections and commands to stdout
- Handle multiple concurrent clients via poll()
- Support all IRC features listed above

## Running the Bot

```bash
./ircbot <server> <port> <password> [nickname] [channel]
# Example: ./ircbot 127.0.0.1 6667 password IRCBot #general
```

The bot will:
- Connect to the specified server with the given password
- Register with the provided nickname (default: IRCBot)
- Join the specified channel (default: #general)
- Listen for incoming messages and respond to commands
- Log all activities to stdout

## Testing

### Quick Validation
```bash
# Run server validation tests (16 comprehensive tests)
./validate_server.sh

# Run bot functionality test
./test_bot_manual.sh
```

### Manual Testing
Connect with nc or an IRC client:
```bash
# Terminal 1 - Start server
./ircserv 6667 password

# Terminal 2 - Start bot
./ircbot 127.0.0.1 6667 password TestBot "#general"

# Terminal 3 - Connect as user and test
nc localhost 6667
> PASS password
> NICK myuser
> USER myuser 0 * :My User
> JOIN #general
> PRIVMSG #general :!help
> PRIVMSG #general :!time
```

## Architecture

### Server (`Server.cpp`, `Server.hpp`)
- Event loop using `poll()` for multiplexing I/O
- Per-client state management in `ClientInfo` struct
- Per-command handler functions for clean separation
- Non-blocking socket operations with proper error handling
- Channel management with operator privileges and modes

### Channel (`Channel.cpp`, `Channel.hpp`)
- Member and operator tracking
- Mode support (i, t, k, o, l)
- Topic management with operator restrictions
- Password protection and member limits
- Ban list management

### Bot (`Bot.cpp`, `Bot.hpp`, `bot_main.cpp`)
- Runs as separate IRC client process
- Connects like any other IRC user
- Message parsing and command dispatch
- Poll-based event loop for responsiveness
- Extensible command handler framework

## Implementation Details

### Registration Flow
1. Client connects and sends PASS command
2. Server validates password
3. Client sends NICK and USER commands
4. Server recognizes both received and sends 001 welcome
5. Client becomes registered

### Channel Operation
- First joiner automatically becomes operator
- Operators can set TOPIC and MODE
- MODE supports: i(invite-only), t(topic-restricted), k(key), o(promote), l(limit)
- All changes broadcast to channel members

### Bot Interaction
- Bot joins channel and greets new joiners
- Listens for PRIVMSG with `!` prefix
- Parses command name and arguments
- Executes appropriate handler and sends response
- All responses broadcast to the channel or user

## Code Quality

- ✅ Compiled with `-Wall -Wextra -Werror` (zero warnings)
- ✅ C++98 standard compliance
- ✅ Address sanitizer enabled during compilation
- ✅ No memory leaks or undefined behavior
- ✅ Proper error handling and cleanup
- ✅ Non-blocking I/O throughout
- ✅ No forking or threading (single process)

## File Structure

```
/home/bhamoum/42/irc/
├── main.cpp              # Server entry point
├── Server.cpp/.hpp       # Server implementation
├── Channel.cpp/.hpp      # Channel management
├── bot_main.cpp          # Bot entry point
├── Bot.cpp/.hpp          # Bot implementation
├── Makefile              # Build configuration
├── validate_server.sh    # Server validation tests
├── test_bot_manual.sh    # Bot functionality test
└── README (this file)
```

## Features Demonstrated

### Server Features
- ✅ IRC Protocol Implementation (RFC 2812 compliant)
- ✅ Non-blocking I/O (select/poll pattern)
- ✅ Multi-client support without threading
- ✅ Proper state management and cleanup
- ✅ Robust error handling
- ✅ Real-time message broadcasting
- ✅ Channel operator privileges

### Bot Features  
- ✅ Client implementation (connects like a user)
- ✅ Event-driven architecture
- ✅ Command parsing and dispatch
- ✅ Extensible command framework
- ✅ Multi-user interaction
- ✅ Channel monitoring and participation

## Known Limitations

- Single process server (no forking)
- Limited to 512 byte buffer per command
- Memory grows with number of clients (no cleanup of old channel history)
- No persistence (data lost on shutdown)
- No DCC file transfer support (could be added as another bonus)

## Testing Results

All 16 server validation tests pass:
- ✅ PASS authentication
- ✅ NICK/USER registration  
- ✅ JOIN channel creation
- ✅ TOPIC command
- ✅ MODE +i/+t/+k/+o/+l support
- ✅ PRIVMSG to channels and users
- ✅ LIST command
- ✅ KICK/INVITE commands
- ✅ PART/QUIT commands

Bot manual test passes:
- ✅ Server and bot connection
- ✅ User registration and JOIN
- ✅ Welcome message from bot
- ✅ !help command response
- ✅ !time command response
- ✅ !echo command response
- ✅ Multi-user interaction
- ✅ Proper message routing and broadcasting
