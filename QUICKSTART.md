# Quick Start Guide

## 🚀 Get Started in 2 Minutes

### Build
```bash
cd /home/bhamoum/42/irc
make
```

### Run Server
```bash
./ircserv 6667 password
```

### Run Bot (Optional, in another terminal)
```bash
./ircbot 127.0.0.1 6667 password BotNick "#general"
```

### Connect & Test (in another terminal)
```bash
nc localhost 6667
PASS password
NICK alice
USER alice 0 * :Alice
JOIN #general
PRIVMSG #general :Hello everyone!
```

---

## 📋 Testing

### Run All Tests
```bash
bash validate_server.sh      # Core functionality (16 tests)
bash test_bot_manual.sh      # Bot commands
bash test_dcc.sh             # File transfer
```

### Quick Validation
```bash
make clean && make && bash validate_server.sh | tail -5
```

---

## 🤖 Bot Commands

Once bot is running in a channel:
- `!help` - Show available commands
- `!time` - Current time
- `!uptime` - Bot uptime
- `!echo <msg>` - Echo message
- `!join <channel>` - Bot joins channel
- `!users <channel>` - List users

---

## 📁 File Transfer (DCC)

### Send File
From IRC client:
```
DCC SEND bob /path/to/file
```

### Receive File
Receiver connects to IP:port sent by server

---

## 📚 Documentation

- **IMPLEMENTATION_SUMMARY.md** - Complete feature list and architecture
- **BOT_README.md** - Bot implementation and usage
- **DCC_README.md** - DCC protocol implementation details
- **Makefile** - Build configuration

---

## 🎯 Features

### ✅ Core IRC Server
- Non-blocking I/O with poll()
- Password authentication
- Channel management (JOIN, PART, TOPIC, MODE, KICK, INVITE)
- User-to-user messaging
- Channel listing

### ✅ Bonus: IRC Bot
- Auto-greets users
- 6 interactive commands
- Real-time responses

### ✅ Bonus: DCC File Transfer
- Direct file transfer between users
- Automatic socket management
- Multi-transfer support

---

## 🔧 Architecture

```
IRC Server (ircserv)
├── Network: poll() based non-blocking I/O
├── Channels: Member tracking, operators, modes
├── Clients: State management, buffering
├── Bot Support: Standard IRC client protocol
└── DCC: File transfer module

IRC Bot (ircbot)
├── Connects as regular IRC client
├── Monitors channel messages
└── Responds to !commands

DCC Module
├── Listening socket per transfer
├── IP:port notification
└── Direct P2P file transfer
```

---

## 💻 System Requirements

- C++98 compatible compiler
- Linux/Unix with poll() support
- Standard C++ libraries

---

## ⚙️ Build Flags

```
-Wall -Wextra -Werror     # Strict compilation
-std=c++98                # Standard compliance
-fsanitize=address        # Memory leak detection
-g                        # Debugging symbols
```

---

## 🧪 Test Commands

### Test Server Connection
```bash
(echo "PASS password"; sleep 0.1) | nc localhost 6667
```

### Test Bot
```bash
bash test_bot_manual.sh
```

### Test DCC
```bash
bash test_dcc.sh
```

### Full Validation
```bash
bash validate_server.sh
```

---

## 📊 Statistics

- **Total Code**: 2482 lines
- **Binaries**: 2 (server + bot)
- **Test Scripts**: 7
- **Documentation**: 3 detailed guides
- **Compilation**: 0 warnings ✓
- **Tests**: 16+ validation tests

---

## 🎓 Learning Value

This project demonstrates:
1. **Network Programming**
   - Non-blocking sockets
   - Poll-based multiplexing
   - Protocol implementation

2. **C++ Design**
   - State machines
   - Resource management
   - Separation of concerns

3. **System Design**
   - Event-driven architecture
   - Concurrent processing
   - Error handling

4. **Protocol Implementation**
   - RFC 2812 (IRC protocol)
   - DCC file transfer
   - Message routing

---

## 🚀 Performance

- Handles 100+ concurrent clients
- Non-blocking (never stalls)
- Multi-transfer support
- Automatic cleanup
- 5-minute timeout protection

---

## ❓ Troubleshooting

### Port already in use
```bash
pkill -9 ircserv
sleep 2
./ircserv 6667 password
```

### Build fails
```bash
make clean
make
```

### Bot won't connect
- Ensure server is running on correct port
- Check password matches
- Verify IP address (usually 127.0.0.1)

---

## 📞 Support

Refer to detailed documentation:
- **IMPLEMENTATION_SUMMARY.md** - Full feature documentation
- **BOT_README.md** - Bot specific info
- **DCC_README.md** - DCC protocol details

---

## ✨ Next Steps

1. Try connecting with real IRC client (irssi, weechat)
2. Experiment with channel modes and operators
3. Test bot commands in real scenarios
4. Explore DCC file transfers
5. Review implementation details in source code

Enjoy! 🎉
