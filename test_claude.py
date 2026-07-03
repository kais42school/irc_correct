#!/usr/bin/env python3
"""
test_ft_irc.py — functional + static test suite for ft_irc

Design principle: functional tests check BEHAVIOR (did the message arrive?
was the operator check enforced? does the server survive a bad client?),
never exact string formatting. That's what a real IRC client cares about,
and it's what the peer-evaluation actually checks (see the eval PDF: it's
all done by hand with nc/irssi, nobody greps raw bytes).

Static checks (grep-based) approximate the "read the code" part of the
42 evaluation grid: number of poll()/select()/epoll/kqueue call sites,
fcntl() usage pattern, absence of fork(), Makefile rules, compiler flags.
These are heuristics, not a substitute for an actual code review — they
just catch the most common instant-zero mistakes before you walk into
your defense.

Usage:
    python3 test_ft_irc.py [port] [password]

Defaults: port=6667, password=password
Assumes ./ircserv is already built in the current directory (run `make` first).
"""

import socket
import subprocess
import sys
import time
import os
import re
import signal
import glob

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 6667
PASSWORD = sys.argv[2] if len(sys.argv) > 2 else "password"
HOST = "127.0.0.1"
BINARY = "./ircserv"

passed = 0
failed = 0
warned = 0


def ok(name):
    global passed
    passed += 1
    print(f"  \033[32m✓ PASS\033[0m  {name}")


def fail(name, detail=""):
    global failed
    failed += 1
    print(f"  \033[31m✗ FAIL\033[0m  {name}")
    if detail:
        print(f"          {detail}")


def warn(name, detail=""):
    global warned
    warned += 1
    print(f"  \033[33m! WARN\033[0m  {name}")
    if detail:
        print(f"          {detail}")


class IRCClient:
    """Minimal raw IRC socket client for testing. No protocol assumptions
    beyond RFC framing (\\r\\n terminated lines)."""

    def __init__(self, timeout=2.0, host=HOST):
        self.sock = socket.create_connection((host, PORT), timeout=timeout)
        self.sock.settimeout(timeout)
        self.buf = b""

    def send(self, line):
        if not line.endswith("\r\n"):
            line += "\r\n"
        self.sock.sendall(line.encode())

    def send_raw(self, data):
        """Send raw bytes without appending \\r\\n — for partial-command tests."""
        self.sock.sendall(data.encode() if isinstance(data, str) else data)

    def read_for(self, duration=0.5):
        """Collect whatever arrives within `duration` seconds."""
        end = time.time() + duration
        self.sock.settimeout(0.2)
        data = b""
        while time.time() < end:
            try:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                data += chunk
            except socket.timeout:
                continue
            except OSError:
                break
        self.buf += data
        return data.decode(errors="replace")

    def register(self, nick, user="user", password=PASSWORD):
        self.send(f"PASS {password}")
        self.send(f"NICK {nick}")
        self.send(f"USER {user} 0 * :{user}")
        return self.read_for(0.4)

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def start_server():
    if not os.path.exists(BINARY):
        print(f"Binary {BINARY} not found. Run `make` first.")
        sys.exit(1)
    proc = subprocess.Popen(
        [BINARY, str(PORT), PASSWORD],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    time.sleep(0.5)
    if proc.poll() is not None:
        print("Server exited immediately. Output:")
        print(proc.stdout.read().decode(errors="replace"))
        sys.exit(1)
    return proc


def stop_server(proc):
    proc.send_signal(signal.SIGTERM)
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()


# ---------------------------------------------------------------------------
# Static checks (source review heuristics)
# ---------------------------------------------------------------------------

def source_files():
    exts = ("*.cpp", "*.hpp", "*.h", "*.tpp", "*.ipp")
    files = []
    for ext in exts:
        files.extend(glob.glob(f"**/{ext}", recursive=True))
    # skip anything under common vendor/test dirs
    return [f for f in files if "/.git/" not in f and not f.startswith(".git/")]


def test_static_poll_count():
    print("\n--- Static check: single poll()/select()/epoll/kqueue usage ---")
    files = source_files()
    if not files:
        warn("No source files found for static analysis", "check you're running from project root")
        return

    pattern = re.compile(r"\b(poll|select|epoll_wait|epoll_create\d*|kqueue|kevent)\s*\(")
    hits = []
    for f in files:
        try:
            with open(f, errors="replace") as fh:
                for lineno, line in enumerate(fh, 1):
                    if pattern.search(line):
                        hits.append((f, lineno, line.strip()))
        except OSError:
            continue

    if not hits:
        fail("At least one poll()/select()/epoll/kqueue call site found", "none found — required for I/O multiplexing")
        return

    distinct_files = sorted(set(f for f, _, _ in hits))
    print(f"          Found {len(hits)} call site(s) across {len(distinct_files)} file(s):")
    for f, lineno, line in hits:
        print(f"            {f}:{lineno}: {line}")

    if len(hits) == 1:
        ok("Exactly one poll()/equivalent call site found (manual review still needed to confirm it's called before every accept/read/write)")
    else:
        warn(f"{len(hits)} call sites found for poll()/select()/epoll/kqueue",
             "subject requires exactly ONE. Multiple hits can be legitimate (e.g. one real call + comments/strings), but verify manually — this is graded as an instant 0 if wrong.")


def test_static_fcntl():
    print("\n--- Static check: fcntl() usage pattern ---")
    files = source_files()
    pattern = re.compile(r"fcntl\s*\([^)]*\)")
    allowed = re.compile(r"^fcntl\s*\(\s*[^,]+\s*,\s*F_SETFL\s*,\s*O_NONBLOCK\s*\)$")
    hits = []
    for f in files:
        try:
            with open(f, errors="replace") as fh:
                for lineno, line in enumerate(fh, 1):
                    for m in pattern.finditer(line):
                        hits.append((f, lineno, m.group(0).strip()))
        except OSError:
            continue

    if not hits:
        print("          No fcntl() calls found (fine if you're not on macOS / don't need it).")
        return

    bad = [(f, l, c) for f, l, c in hits if not allowed.match(re.sub(r"\s+", " ", c))]
    for f, lineno, call in hits:
        print(f"            {f}:{lineno}: {call}")

    if bad:
        fail(f"{len(bad)} fcntl() call(s) don't match the only allowed form",
             'Only "fcntl(fd, F_SETFL, O_NONBLOCK);" is permitted by the subject')
    else:
        ok("All fcntl() calls match the allowed form fcntl(fd, F_SETFL, O_NONBLOCK)")


def test_static_fork():
    print("\n--- Static check: forking is prohibited ---")
    files = source_files()
    pattern = re.compile(r"\bfork\s*\(")
    hits = []
    for f in files:
        try:
            with open(f, errors="replace") as fh:
                for lineno, line in enumerate(fh, 1):
                    if pattern.search(line):
                        hits.append((f, lineno, line.strip()))
        except OSError:
            continue

    if hits:
        for f, lineno, line in hits:
            print(f"            {f}:{lineno}: {line}")
        fail("No fork() calls found", "forking is explicitly forbidden by the subject")
    else:
        ok("No fork() calls found")


def test_static_makefile():
    print("\n--- Static check: Makefile rules ---")
    if not os.path.exists("Makefile"):
        fail("Makefile exists", "no Makefile found in project root")
        return
    with open("Makefile", errors="replace") as fh:
        content = fh.read()

    required = ["all", "clean", "fclean", "re"]
    missing = [r for r in required if not re.search(rf"^{r}\s*:", content, re.MULTILINE)]
    if missing:
        fail("Makefile has required rules (all, clean, fclean, re)", f"missing: {', '.join(missing)}")
    else:
        ok("Makefile has all, clean, fclean, re rules")

    if re.search(r"^NAME\s*=", content, re.MULTILINE) or re.search(r"^NAME\s*:=", content, re.MULTILINE):
        ok("Makefile defines a NAME variable")
    else:
        warn("Makefile defines a NAME variable", "couldn't find NAME = ... — check manually if named differently")

    flags_ok = all(f in content for f in ["-Wall", "-Wextra", "-Werror"])
    if flags_ok:
        ok("Makefile uses -Wall -Wextra -Werror")
    else:
        fail("Makefile uses -Wall -Wextra -Werror", "one or more flags missing")

    if "c++98" in content.lower() or "std=c++98" in content:
        ok("Makefile targets C++98 standard")
    else:
        warn("Makefile targets C++98 standard", "couldn't find -std=c++98 — verify manually, subject requires the code to still compile with it even if not the default flag")


def run_static_checks():
    print("\n============================================================")
    print(" STATIC CODE CHECKS (heuristic — manual review still needed)")
    print("============================================================")
    test_static_poll_count()
    test_static_fcntl()
    test_static_fork()
    test_static_makefile()


def run_valgrind_check():
    """Optional: only runs if valgrind is installed. Starts the server
    under valgrind, does a small session, then checks the leak summary."""
    print("\n--- Optional: valgrind leak check ---")
    if subprocess.call(["which", "valgrind"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) != 0:
        warn("valgrind not installed — skipping leak check", "run manually: valgrind --leak-check=full --show-leak-kinds=all ./ircserv <port> <password>")
        return

    log_path = "/tmp/ft_irc_valgrind.log"
    proc = subprocess.Popen(
        ["valgrind", "--leak-check=full", "--show-leak-kinds=all",
         f"--log-file={log_path}", BINARY, str(PORT + 1), PASSWORD],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    time.sleep(1.5)
    if proc.poll() is not None:
        warn("Server under valgrind exited immediately", "skipping leak check — try running it manually")
        return

    old_port = PORT
    try:
        try:
            c = socket.create_connection((HOST, old_port + 1), timeout=2)
            c.settimeout(2)
            c.sendall(f"PASS {PASSWORD}\r\nNICK vg\r\nUSER vg 0 * :vg\r\n".encode())
            time.sleep(0.3)
            try:
                c.recv(4096)
            except OSError:
                pass
            c.close()
        except OSError as e:
            warn("Could not connect to valgrind-wrapped server", str(e))
    finally:
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    time.sleep(0.5)
    if not os.path.exists(log_path):
        warn("No valgrind log produced")
        return
    with open(log_path, errors="replace") as fh:
        log = fh.read()

    m = re.search(r"definitely lost:\s*([\d,]+)\s*bytes", log)
    if m and m.group(1).replace(",", "") != "0":
        fail(f"Valgrind reports definitely-lost bytes: {m.group(1)}", f"see {log_path}")
    elif "no leaks are possible" in log.lower() or (m and m.group(1).replace(",", "") == "0"):
        ok("Valgrind reports no definitely-lost bytes (short session only — test longer manually)")
    else:
        warn("Could not parse valgrind leak summary", f"check manually: {log_path}")


# ---------------------------------------------------------------------------
# Functional tests
# ---------------------------------------------------------------------------

def test_registration():
    print("\n--- Registration (PASS/NICK/USER) ---")
    c = IRCClient()
    resp = c.register("alice")
    if "001" in resp:
        ok("Client registers and receives a welcome (001)")
    else:
        fail("Client registers and receives a welcome (001)", f"got: {resp!r}")
    c.close()


def test_wrong_password():
    print("\n--- Wrong password rejected ---")
    c = IRCClient()
    c.send("PASS wrongpassword")
    resp = c.read_for(0.4)
    try:
        c.send("NICK bob")
        c.send("USER bob 0 * :bob")
        resp += c.read_for(0.4)
    except (BrokenPipeError, OSError):
        pass  # server closed the connection, which is the expected behavior
    if "001" in resp:
        fail("Wrong password is rejected", f"client got registered anyway: {resp!r}")
    else:
        ok("Wrong password is rejected")
    c.close()


def test_join_and_broadcast():
    print("\n--- JOIN + PRIVMSG broadcast to channel ---")
    a = IRCClient()
    b = IRCClient()
    a.register("alice")
    b.register("bob")

    a.send("JOIN #test")
    a.read_for(0.3)
    b.send("JOIN #test")
    b.read_for(0.3)

    a.send("PRIVMSG #test :hello channel")
    time.sleep(0.3)
    b_resp = b.read_for(0.4)
    a_resp = a.read_for(0.3)

    if "hello channel" in b_resp:
        ok("Other channel member receives PRIVMSG")
    else:
        fail("Other channel member receives PRIVMSG", f"bob got: {b_resp!r}")

    if "hello channel" in a_resp:
        fail("Sender should NOT receive an echo of their own PRIVMSG", f"alice got: {a_resp!r}")
    else:
        ok("Sender does not receive an echo of their own PRIVMSG")

    a.close()
    b.close()


def test_privmsg_without_join():
    print("\n--- PRIVMSG without being a channel member is rejected ---")
    a = IRCClient()
    b = IRCClient()
    a.register("carol")
    b.register("dave")
    a.send("JOIN #private1")
    a.read_for(0.3)
    b.send("PRIVMSG #private1 :sneaky message")
    resp = b.read_for(0.4)
    if "442" in resp:
        ok("Server rejects PRIVMSG from a non-member (442)")
    else:
        fail("Server rejects PRIVMSG from a non-member (442)", f"got: {resp!r}")
    a.close()
    b.close()


def test_operator_privileges():
    print("\n--- Operator privileges (MODE, KICK) ---")
    op = IRCClient()
    reg = IRCClient()
    op.register("opuser")
    reg.register("reguser")

    op.send("JOIN #opchan")
    op.read_for(0.3)
    reg.send("JOIN #opchan")
    reg.read_for(0.3)

    reg.send("MODE #opchan +i")
    resp = reg.read_for(0.4)
    if "482" in resp:
        ok("Non-operator is refused MODE (482)")
    else:
        fail("Non-operator is refused MODE (482)", f"got: {resp!r}")

    op.send("MODE #opchan +i")
    resp = op.read_for(0.4)
    if "482" in resp:
        fail("Operator can set MODE +i", f"got: {resp!r}")
    else:
        ok("Operator can set MODE +i")

    reg.send("KICK #opchan opuser")
    resp = reg.read_for(0.4)
    if "482" in resp:
        ok("Non-operator is refused KICK (482)")
    else:
        fail("Non-operator is refused KICK (482)", f"got: {resp!r}")

    op.send("KICK #opchan reguser :bye")
    resp_op = op.read_for(0.3)
    resp_reg = reg.read_for(0.3)
    if "KICK" in resp_op or "KICK" in resp_reg:
        ok("Operator can KICK a member")
    else:
        fail("Operator can KICK a member", f"op got: {resp_op!r} / reg got: {resp_reg!r}")

    op.close()
    reg.close()


def test_topic_command():
    print("\n--- TOPIC command (view, set, +t restriction) ---")
    op = IRCClient()
    reg = IRCClient()
    op.register("topicop")
    reg.register("topicreg")

    op.send("JOIN #topictest")
    op.read_for(0.3)
    reg.send("JOIN #topictest")
    reg.read_for(0.3)

    # Set a topic as operator
    op.send("TOPIC #topictest :welcome to the channel")
    resp = op.read_for(0.4)
    time.sleep(0.2)
    reg_resp = reg.read_for(0.4)
    if "welcome to the channel" in resp or "welcome to the channel" in reg_resp:
        ok("Operator can set the channel TOPIC")
    else:
        fail("Operator can set the channel TOPIC", f"op got: {resp!r} / reg got: {reg_resp!r}")

    # View the topic (no argument)
    reg.send("TOPIC #topictest")
    resp = reg.read_for(0.4)
    if "welcome to the channel" in resp or re.search(r"\b332\b", resp):
        ok("TOPIC with no argument returns the current topic (332)")
    else:
        fail("TOPIC with no argument returns the current topic (332)", f"got: {resp!r}")

    # Restrict TOPIC to operators with +t, then a regular user should be refused
    op.send("MODE #topictest +t")
    op.read_for(0.3)
    reg.send("TOPIC #topictest :hijacked topic")
    resp = reg.read_for(0.4)
    if "482" in resp:
        ok("With +t set, non-operator is refused TOPIC change (482)")
    else:
        fail("With +t set, non-operator is refused TOPIC change (482)", f"got: {resp!r}")

    # Operator should still be able to change it
    op.send("TOPIC #topictest :operator override")
    resp = op.read_for(0.4)
    if "482" in resp:
        fail("Operator can still set TOPIC when +t is set", f"got: {resp!r}")
    else:
        ok("Operator can still set TOPIC when +t is set")

    op.close()
    reg.close()


def test_mode_user_limit_and_op():
    print("\n--- MODE +l (user limit) and +o (give/take operator) ---")
    op = IRCClient()
    reg1 = IRCClient()
    reg2 = IRCClient()
    op.register("limitop")
    reg1.register("limituser1")
    reg2.register("limituser2")

    op.send("JOIN #limitchan")
    op.read_for(0.3)

    # Limit the channel to 1 user (the operator already in it)
    op.send("MODE #limitchan +l 1")
    op.read_for(0.3)

    reg1.send("JOIN #limitchan")
    resp = reg1.read_for(0.4)
    if "471" in resp:
        ok("MODE +l blocks JOIN once the user limit is reached (471)")
    else:
        fail("MODE +l blocks JOIN once the user limit is reached (471)", f"got: {resp!r}")

    # Raise the limit and confirm the join now succeeds
    op.send("MODE #limitchan +l 5")
    op.read_for(0.3)
    reg1.send("JOIN #limitchan")
    resp = reg1.read_for(0.4)
    if "471" in resp:
        fail("Raising the limit allows JOIN to succeed", f"got: {resp!r}")
    else:
        ok("Raising the limit allows JOIN to succeed")

    # Give operator status to reg1 with MODE +o, then reg1 should be able to KICK
    op.send("MODE #limitchan +o limituser1")
    op.read_for(0.3)

    reg2.send("JOIN #limitchan")
    reg2.read_for(0.3)

    reg1.send("KICK #limitchan limituser2 :testing new op powers")
    resp1 = reg1.read_for(0.4)
    resp2 = reg2.read_for(0.4)
    if "482" in resp1:
        fail("MODE +o grants operator privileges (KICK should now work)", f"got: {resp1!r}")
    else:
        ok("MODE +o grants operator privileges (promoted user can KICK)")

    op.close()
    reg1.close()
    reg2.close()


def test_invite_only_channel():
    print("\n--- Invite-only channel (+i) blocks JOIN, INVITE unblocks it ---")
    op = IRCClient()
    outsider = IRCClient()
    op.register("hostuser")
    outsider.register("guestuser")

    op.send("JOIN #vip")
    op.read_for(0.3)
    op.send("MODE #vip +i")
    op.read_for(0.3)

    outsider.send("JOIN #vip")
    resp = outsider.read_for(0.4)
    if "473" in resp:
        ok("Uninvited user is refused JOIN on +i channel (473)")
    else:
        fail("Uninvited user is refused JOIN on +i channel (473)", f"got: {resp!r}")

    op.send("INVITE guestuser #vip")
    op.read_for(0.3)
    outsider.send("JOIN #vip")
    resp = outsider.read_for(0.4)
    if "473" in resp:
        fail("Invited user can JOIN +i channel", f"got: {resp!r}")
    else:
        ok("Invited user can JOIN +i channel after INVITE")

    op.close()
    outsider.close()


def test_channel_key():
    print("\n--- Channel key (+k) ---")
    op = IRCClient()
    outsider = IRCClient()
    op.register("keyhost")
    outsider.register("keyguest")

    op.send("JOIN #locked")
    op.read_for(0.3)
    op.send("MODE #locked +k secret123")
    op.read_for(0.3)

    outsider.send("JOIN #locked wrongkey")
    resp = outsider.read_for(0.4)
    if "475" in resp:
        ok("Wrong channel key is refused (475)")
    else:
        fail("Wrong channel key is refused (475)", f"got: {resp!r}")

    outsider.send("JOIN #locked secret123")
    resp = outsider.read_for(0.4)
    if "475" in resp:
        fail("Correct channel key is accepted", f"got: {resp!r}")
    else:
        ok("Correct channel key is accepted")

    op.close()
    outsider.close()


def test_partial_command():
    print("\n--- Partial / fragmented command ---")
    a = IRCClient()
    a.register("frag")
    a.send("JOIN #fragtest")
    a.read_for(0.3)

    a.send_raw("PRIV")
    time.sleep(0.2)
    a.send_raw("MSG #fragtest :fragmented message\r\n")
    a.read_for(0.5)

    a.send("PING keepalive")
    resp2 = a.read_for(0.4)
    if "PONG" in resp2 or "keepalive" in resp2:
        ok("Server stays responsive after a fragmented command")
    else:
        fail("Server stays responsive after a fragmented command", f"got: {resp2!r}")
    a.close()


def test_abrupt_disconnect():
    print("\n--- Abrupt disconnect doesn't affect other clients ---")
    victim = IRCClient()
    victim.register("victim")
    victim.send("JOIN #survivors")
    victim.read_for(0.3)

    victim.send_raw("JOIN #another")  # no trailing \r\n on purpose
    victim.sock.close()

    time.sleep(0.3)

    fresh = IRCClient()
    resp = fresh.register("survivor")
    if "001" in resp:
        ok("New client can still register after another client was killed abruptly")
    else:
        fail("New client can still register after another client was killed abruptly", f"got: {resp!r}")
    fresh.close()


def test_slow_client_flood():
    print("\n--- Slow/stopped client (^-Z equivalent) doesn't block the server ---")
    # Simulate a client that stops reading its socket (as if suspended with
    # Ctrl-Z) while another client floods the channel with messages. The
    # slow client's TCP receive buffer will eventually fill up; the server
    # must not block on write() to that one client and must keep serving
    # everybody else. Once the slow client "resumes" (starts reading again)
    # it should receive the backlog.
    slow = IRCClient()
    flooder = IRCClient()
    slow.register("slowpoke")
    flooder.register("flooder")

    slow.send("JOIN #floodtest")
    slow.read_for(0.3)
    flooder.send("JOIN #floodtest")
    flooder.read_for(0.3)

    # slow stops reading now — just send a burst of messages from flooder
    n_messages = 200
    for i in range(n_messages):
        flooder.send(f"PRIVMSG #floodtest :flood message number {i}")

    # While the slow client isn't draining its socket, confirm the server
    # is still responsive to a brand-new, unrelated client.
    time.sleep(0.5)
    probe = IRCClient()
    resp = probe.register("floodprobe")
    if "001" in resp:
        ok("Server stays responsive to new clients while another client isn't reading")
    else:
        fail("Server stays responsive to new clients while another client isn't reading", f"got: {resp!r}")
    probe.close()

    # Now let the slow client "resume": drain its buffer and check it
    # eventually receives at least some of the backlog, and the server is
    # still alive afterwards.
    backlog = slow.read_for(1.5)
    if "flood message number" in backlog:
        ok("Slow client receives backlog once it resumes reading")
    else:
        warn("Slow client receives backlog once it resumes reading",
             f"received nothing recognizable — may be fine if server drops slow readers, but verify manually: {backlog[:200]!r}")

    flooder.send("PING stillalive")
    resp = flooder.read_for(0.4)
    if "PONG" in resp or "stillalive" in resp:
        ok("Server still operational after flood + slow reader scenario")
    else:
        fail("Server still operational after flood + slow reader scenario", f"got: {resp!r}")

    slow.close()
    flooder.close()
    probe.close()


def test_notice():
    print("\n--- NOTICE command ---")
    a = IRCClient()
    b = IRCClient()
    a.register("noticer")
    b.register("noticee")
    a.send("JOIN #noticechan")
    a.read_for(0.3)
    b.send("JOIN #noticechan")
    b.read_for(0.3)

    a.send("NOTICE #noticechan :heads up")
    time.sleep(0.3)
    resp = b.read_for(0.4)
    if "heads up" in resp:
        ok("NOTICE is delivered to channel members")
    else:
        fail("NOTICE is delivered to channel members", f"bob got: {resp!r}")
    a.close()
    b.close()


def test_concurrent_clients():
    print("\n--- Multiple simultaneous clients (server doesn't block) ---")
    clients = []
    try:
        for i in range(8):
            c = IRCClient()
            resp = c.register(f"conc{i}")
            clients.append((c, resp))

        all_ok = all("001" in resp for _, resp in clients)
        if all_ok:
            ok("8 concurrent clients all registered successfully")
        else:
            fail("8 concurrent clients all registered successfully",
                 f"{sum(1 for _, r in clients if '001' in r)}/8 succeeded")

        for c, _ in clients:
            c.send("JOIN #crowd")
        time.sleep(0.5)
        for c, _ in clients:
            c.read_for(0.1)

        probe = IRCClient()
        resp = probe.register("probe")
        if "001" in resp:
            ok("Server still responsive after concurrent load")
        else:
            fail("Server still responsive after concurrent load", f"got: {resp!r}")
        probe.close()
    finally:
        for c, _ in clients:
            c.close()


def test_listen_all_interfaces():
    print("\n--- Server listens on all interfaces, not just 127.0.0.1 ---")
    # Try to discover a non-loopback local IP and connect through it.
    # This is a best-effort check: on some sandboxed/CI environments there
    # may be no other interface available, in which case we warn instead
    # of failing outright.
    candidate_ip = None
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        candidate_ip = s.getsockname()[0]
        s.close()
    except OSError:
        pass

    if not candidate_ip or candidate_ip == "127.0.0.1":
        warn("Could not determine a non-loopback local IP to test with",
             "run this test on a machine with a real network interface, or test manually with your reference IRC client from another host")
        return

    try:
        c = IRCClient(timeout=2.0, host=candidate_ip)
        resp = c.register("ifacecheck")
        if "001" in resp:
            ok(f"Server accepts connections on non-loopback interface ({candidate_ip})")
        else:
            fail(f"Server accepts connections on non-loopback interface ({candidate_ip})", f"got: {resp!r}")
        c.close()
    except OSError as e:
        fail(f"Server accepts connections on non-loopback interface ({candidate_ip})", str(e))


def main():
    print("=== ft_irc test suite ===")

    run_static_checks()

    proc = start_server()
    print(f"\nServer started on port {PORT} (pid {proc.pid})")

    try:
        print("\n============================================================")
        print(" FUNCTIONAL TESTS")
        print("============================================================")
        test_registration()
        test_wrong_password()
        test_join_and_broadcast()
        test_privmsg_without_join()
        test_operator_privileges()
        test_topic_command()
        test_mode_user_limit_and_op()
        test_invite_only_channel()
        test_channel_key()
        test_partial_command()
        test_abrupt_disconnect()
        test_slow_client_flood()
        test_notice()
        test_concurrent_clients()
        test_listen_all_interfaces()
    finally:
        stop_server(proc)

    run_valgrind_check()

    print("\n=== Summary ===")
    print(f"  {passed} passed, {failed} failed, {warned} warnings")
    if warned:
        print("  (warnings are things to verify manually — they don't fail the run)")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
