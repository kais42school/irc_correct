#!/usr/bin/env python3
"""
test_ft_irc.py — functional test suite for ft_irc

Design principle: tests check BEHAVIOR (did the message arrive? was the
operator check enforced? does the server survive a bad client?), never
exact string formatting. That's what a real IRC client cares about, and
it's what the peer-evaluation actually checks (see the eval PDF: it's all
done by hand with nc/irssi, nobody greps raw bytes).

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
import signal

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

    def __init__(self, timeout=2.0):
        self.sock = socket.create_connection((HOST, PORT), timeout=timeout)
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
# Tests
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
    also_registered = False
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
    # dave never joins #private1
    b.send("PRIVMSG #private1 :sneaky message")
    resp = b.read_for(0.4)
    if "442" in resp:
        ok("Server rejects PRIVMSG from a non-member (442)")
    else:
        fail("Server rejects PRIVMSG from a non-member (442)", f"got: {resp!r}")
    a.close()
    b.close()


def test_operator_privileges():
    print("\n--- Operator privileges (MODE, KICK, TOPIC) ---")
    op = IRCClient()
    reg = IRCClient()
    op.register("opuser")
    reg.register("reguser")

    op.send("JOIN #opchan")
    op.read_for(0.3)
    reg.send("JOIN #opchan")
    reg.read_for(0.3)

    # Regular user tries to set a restricted mode -> should be refused
    reg.send("MODE #opchan +i")
    resp = reg.read_for(0.4)
    if "482" in resp:
        ok("Non-operator is refused MODE (482)")
    else:
        fail("Non-operator is refused MODE (482)", f"got: {resp!r}")

    # Operator sets it -> should succeed (no 482, channel becomes invite-only)
    op.send("MODE #opchan +i")
    resp = op.read_for(0.4)
    if "482" in resp:
        fail("Operator can set MODE +i", f"got: {resp!r}")
    else:
        ok("Operator can set MODE +i")

    # Regular user tries KICK -> refused
    reg.send("KICK #opchan opuser")
    resp = reg.read_for(0.4)
    if "482" in resp:
        ok("Non-operator is refused KICK (482)")
    else:
        fail("Non-operator is refused KICK (482)", f"got: {resp!r}")

    # Operator kicks the regular user -> should succeed
    op.send("KICK #opchan reguser :bye")
    resp_op = op.read_for(0.3)
    resp_reg = reg.read_for(0.3)
    if "KICK" in resp_op or "KICK" in resp_reg:
        ok("Operator can KICK a member")
    else:
        fail("Operator can KICK a member", f"op got: {resp_op!r} / reg got: {resp_reg!r}")

    op.close()
    reg.close()


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

    # Send a command split across two writes with no \r\n in between the pieces
    a.send_raw("PRIV")
    time.sleep(0.2)
    a.send_raw("MSG #fragtest :fragmented message\r\n")
    resp = a.read_for(0.5)

    # alice sent to herself; check the server didn't crash and stayed responsive
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

    # Kill the connection with no QUIT, mid-way through a command
    victim.send_raw("JOIN #another")  # no trailing \r\n on purpose
    victim.sock.close()

    time.sleep(0.3)

    # A fresh client should still be able to connect and register normally
    fresh = IRCClient()
    resp = fresh.register("survivor")
    if "001" in resp:
        ok("New client can still register after another client was killed abruptly")
    else:
        fail("New client can still register after another client was killed abruptly", f"got: {resp!r}")
    fresh.close()


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

        # server still alive?
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


def main():
    print("=== ft_irc functional test suite ===")
    proc = start_server()
    print(f"Server started on port {PORT} (pid {proc.pid})")

    try:
        test_registration()
        test_wrong_password()
        test_join_and_broadcast()
        test_privmsg_without_join()
        test_operator_privileges()
        test_invite_only_channel()
        test_channel_key()
        test_partial_command()
        test_abrupt_disconnect()
        test_notice()
        test_concurrent_clients()
    finally:
        stop_server(proc)

    print("\n=== Summary ===")
    print(f"  {passed} passed, {failed} failed, {warned} warnings")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
