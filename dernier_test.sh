#!/usr/bin/env bash
#
# check_ft_irc.sh — pre-defense checklist for ft_irc
#
# Combines automated static checks (grep-based, same heuristics as the
# python test suite) with a printed manual checklist covering everything
# in the 42 eval grid that can't be checked by a script (needs a human
# with a real IRC client, valgrind, or eyes on the README).
#
# Usage:
#   ./check_ft_irc.sh
#
# Run this from the root of your ft_irc project (where the Makefile is).

set -uo pipefail

GREEN='\033[32m'
RED='\033[31m'
YELLOW='\033[33m'
BOLD='\033[1m'
RESET='\033[0m'

PASS_COUNT=0
FAIL_COUNT=0
WARN_COUNT=0

pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    printf "  ${GREEN}✓ PASS${RESET}  %s\n" "$1"
}

fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    printf "  ${RED}✗ FAIL${RESET}  %s\n" "$1"
    [ -n "${2:-}" ] && printf "          %s\n" "$2"
}

warn() {
    WARN_COUNT=$((WARN_COUNT + 1))
    printf "  ${YELLOW}! WARN${RESET}  %s\n" "$1"
    [ -n "${2:-}" ] && printf "          %s\n" "$2"
}

section() {
    echo
    echo "============================================================"
    echo " $1"
    echo "============================================================"
}

# ---------------------------------------------------------------------------
# Locate source files (skip .git)
# ---------------------------------------------------------------------------

SRC_FILES=$(find . -path './.git' -prune -o \
    \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.tpp' -o -name '*.ipp' \) \
    -print 2>/dev/null)

if [ -z "$SRC_FILES" ]; then
    warn "No source files found" "run this script from the project root"
fi

# ---------------------------------------------------------------------------
# Static check: poll()/select()/epoll/kqueue call sites
# ---------------------------------------------------------------------------

check_poll() {
    echo
    echo "--- Static check: single poll()/select()/epoll/kqueue usage ---"
    local hits
    hits=$(grep -nE '\b(poll|select|epoll_wait|epoll_create[0-9]*|kqueue|kevent)[[:space:]]*\(' $SRC_FILES 2>/dev/null)

    if [ -z "$hits" ]; then
        fail "At least one poll()/select()/epoll/kqueue call site found" "none found — required for I/O multiplexing"
        return
    fi

    local count
    count=$(echo "$hits" | wc -l | tr -d ' ')
    echo "$hits" | sed 's/^/          /'

    if [ "$count" -eq 1 ]; then
        pass "Exactly one poll()/equivalent call site found (manual review still needed to confirm it's called before every accept/read/write)"
    else
        warn "$count call sites found for poll()/select()/epoll/kqueue" \
             "subject requires exactly ONE real call. Some hits above may be comments/doxygen — check manually which ones are actual calls. If more than one is a REAL call, or if the bot/client also uses one inside the same ircserv binary, this is an instant 0."
    fi
}

# ---------------------------------------------------------------------------
# Static check: fcntl() usage pattern
# ---------------------------------------------------------------------------

check_fcntl() {
    echo
    echo "--- Static check: fcntl() usage pattern ---"
    local hits
    hits=$(grep -noE 'fcntl[[:space:]]*\([^)]*\)' $SRC_FILES 2>/dev/null)

    if [ -z "$hits" ]; then
        echo "          No fcntl() calls found (fine if you're not on macOS / don't need it)."
        return
    fi

    echo "$hits" | sed 's/^/          /'

    local bad
    bad=$(echo "$hits" | grep -vE 'fcntl[[:space:]]*\([^,]+,[[:space:]]*F_SETFL[[:space:]]*,[[:space:]]*O_NONBLOCK[[:space:]]*\)$')

    if [ -n "$bad" ]; then
        local bad_count
        bad_count=$(echo "$bad" | wc -l | tr -d ' ')
        fail "$bad_count fcntl() call(s) don't match the only allowed form" \
             'Only "fcntl(fd, F_SETFL, O_NONBLOCK);" is permitted by the subject — no F_GETFL, no OR-ing existing flags'
    else
        pass "All fcntl() calls match the allowed form fcntl(fd, F_SETFL, O_NONBLOCK)"
    fi
}

# ---------------------------------------------------------------------------
# Static check: fork() must be absent
# ---------------------------------------------------------------------------

check_fork() {
    echo
    echo "--- Static check: forking is prohibited ---"
    local hits
    hits=$(grep -nE '\bfork[[:space:]]*\(' $SRC_FILES 2>/dev/null)

    if [ -n "$hits" ]; then
        echo "$hits" | sed 's/^/          /'
        fail "No fork() calls found" "forking is explicitly forbidden by the subject"
    else
        pass "No fork() calls found"
    fi
}

# ---------------------------------------------------------------------------
# Static check: Makefile
# ---------------------------------------------------------------------------

check_makefile() {
    echo
    echo "--- Static check: Makefile rules ---"
    if [ ! -f Makefile ]; then
        fail "Makefile exists" "no Makefile found in project root"
        return
    fi

    local missing=""
    for rule in all clean fclean re; do
        grep -qE "^${rule}[[:space:]]*:" Makefile || missing="$missing $rule"
    done
    if [ -n "$missing" ]; then
        fail "Makefile has required rules (all, clean, fclean, re)" "missing:$missing"
    else
        pass "Makefile has all, clean, fclean, re rules"
    fi

    if grep -qE '^NAME[[:space:]]*(=|:=)' Makefile; then
        pass "Makefile defines a NAME variable"
    else
        warn "Makefile defines a NAME variable" "couldn't find NAME = ... — check manually if named differently"
    fi

    if grep -q -- '-Wall' Makefile && grep -q -- '-Wextra' Makefile && grep -q -- '-Werror' Makefile; then
        pass "Makefile uses -Wall -Wextra -Werror"
    else
        fail "Makefile uses -Wall -Wextra -Werror" "one or more flags missing"
    fi

    if grep -qi 'c++98' Makefile; then
        pass "Makefile targets C++98 standard"
    else
        warn "Makefile targets C++98 standard" "couldn't find -std=c++98 — verify the code still compiles with it even if not the default flag"
    fi
}

# ---------------------------------------------------------------------------
# Static check: no relinking (basic heuristic — object files tracked)
# ---------------------------------------------------------------------------

check_relink() {
    echo
    echo "--- Static check: incremental build (no unnecessary relink) ---"
    if [ ! -f Makefile ]; then
        warn "Skipped (no Makefile)"
        return
    fi
    if grep -qE '\.o[[:space:]]*:' Makefile || grep -qE '%\.o[[:space:]]*:[[:space:]]*%\.cpp' Makefile; then
        pass "Makefile appears to compile .cpp to .o with per-file rules (supports incremental rebuilds)"
    else
        warn "Could not confirm incremental object-file rules" "verify manually: touch one .cpp, run 'make', and check only that file recompiles"
    fi
}

# ---------------------------------------------------------------------------
# Manual checklist (things a script can't verify — needs a human)
# ---------------------------------------------------------------------------

print_manual_checklist() {
    section "MANUAL CHECKLIST (run these yourself before/at your defense)"
    cat <<'EOF'

[ Compilation ]
  [ ] make && make && make          -> second/third make does NOT relink anything
  [ ] make re                       -> full rebuild works from clean
  [ ] compiles with -std=c++98 added explicitly (subject requires it still compiles)

[ poll() usage (grading is by code reading — grep can't fully verify this) ]
  [ ] poll() (or equivalent) is called before EVERY accept/read/write, no exceptions
  [ ] no code path calls read/recv or write/send on a fd without going through poll() first
  [ ] after poll() returns, no logic branches on errno/EAGAIN to "read again" outside poll()
  [ ] if you have a bonus bot, confirm whether it's a SEPARATE process/binary from ircserv;
      if it's linked into ircserv itself, its poll() must be the SAME single poll(), not a second one

[ fcntl() ]
  [ ] every fcntl() call is EXACTLY: fcntl(fd, F_SETFL, O_NONBLOCK);
  [ ] no F_GETFL, no flag OR-ing, no other fcntl() usage anywhere

[ Networking basics ]
  [ ] server binds and listens on all interfaces (not hardcoded to 127.0.0.1)
  [ ] nc -C 127.0.0.1 <port> can connect, send commands, and gets replies
  [ ] fragmented command test from the subject works:
        $ nc -C 127.0.0.1 <port>
        com^Dman^Dd     (send in 3 pieces with Ctrl-D, then \n)
  [ ] server handles multiple simultaneous connections without blocking
      (test nc + your reference IRC client at the same time)

[ Networking edge cases ]
  [ ] kill a client mid-command (Ctrl-C on nc) -> other clients unaffected
  [ ] kill a client with HALF a command sent -> server not left in a bad state
  [ ] suspend a client (Ctrl-Z) while connected to a channel, flood the channel
      from another client -> server does not hang; check for leaks during this
  [ ] when the suspended client resumes (fg), it receives the backlog normally

[ Registration & identity ]
  [ ] PASS / NICK / USER works with both nc and your reference IRC client
  [ ] wrong password is rejected cleanly (no crash, no partial registration)
  [ ] duplicate nickname is rejected
  [ ] client can't send commands before completing registration (except allowed ones)

[ Channel operations ]
  [ ] JOIN works, PRIVMSG to a channel reaches all members except the sender
  [ ] PRIVMSG to a channel you haven't joined is rejected (442)
  [ ] NOTICE works the same way as PRIVMSG for delivery
  [ ] private message directly to a nickname (not a channel) works

[ Operator commands — test as BOTH a regular user (must fail) and an operator (must work) ]
  [ ] KICK   — eject a client from a channel
  [ ] INVITE — invite a client to a channel
  [ ] TOPIC  — view and change the channel topic
  [ ] MODE +i — invite-only toggle
  [ ] MODE +t — restrict TOPIC to operators toggle
  [ ] MODE +k — set/remove channel key (password)
  [ ] MODE +o — give/take operator privilege
  [ ] MODE +l — set/remove user limit

[ Memory ]
  [ ] valgrind --leak-check=full --show-leak-kinds=all ./ircserv <port> <password>
      -> run a full session (connect, join, chat, part, quit) then check the summary
  [ ] repeat the valgrind run while doing the flood/^-Z scenario above
  [ ] no "definitely lost" bytes at the end of a normal session

[ Stability ]
  [ ] server never crashes/segfaults under any of the above, including malformed input
  [ ] server survives running out of some resource gracefully (e.g. many fds) — no crash

[ README.md ]
  [ ] first line, italicized: "This project has been created as part of the 42
      curriculum by <login1>[, <login2>[, ...]]."
  [ ] "Description" section
  [ ] "Instructions" section (compile/install/run)
  [ ] "Resources" section (references + how AI was used and for which parts)
  [ ] written in English

[ Bonus (only evaluated if mandatory part is 100% perfect) ]
  [ ] file transfer works with your reference client
  [ ] bot is present and functional

EOF
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

section "STATIC CODE CHECKS (heuristic — manual review still needed)"
check_poll
check_fcntl
check_fork
check_makefile
check_relink

print_manual_checklist

echo
echo "=== Summary (automated checks only) ==="
echo "  ${PASS_COUNT} passed, ${FAIL_COUNT} failed, ${WARN_COUNT} warnings"
echo "  The manual checklist above is NOT counted here — go through it by hand."

[ "$FAIL_COUNT" -gt 0 ] && exit 1
exit 0
