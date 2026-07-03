#!/usr/bin/env python3
"""
exam_run.py — exam-style command runner for ft_irc

Unlike test_ft_irc.py (which just asserts pass/fail), this script simulates
what an evaluator actually does by hand: connect, send ONE command at a
time, print the EXACT raw text the server sent back, and state in plain
words whether that looks like what's expected. You read the raw output
yourself, the same way a corrector would, and judge it — the script just
saves you from typing everything into nc manually and keeps a scoreboard.

Usage:
    python3 exam_run.py [port] [password]

Defaults: port=6667, password=password
Assumes ./ircserv is already built (run `make` first).
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

GREEN = "\033[32m"
RED = "\033[31m"
YELLOW = "\033[33m"
CYAN = "\033[36m"
DIM = "\033[2m"
BOLD = "\033[1m"
RESET = "\033[0m"

score_pass = 0
score_fail = 0


class Client:
    """A named raw IRC connection. Every send/recv is echoed to the screen
    exactly like you'd see it typed into nc, so you can eyeball it yourself."""

    def __init__(self, label, timeout=2.0):
        self.label = label
        self.sock = socket.create_connection((HOST, PORT), timeout=timeout)
        self.sock.settimeout(timeout)

    def send(self, line):
        wire = line if line.endswith("\r\n") else line + "\r\n"
        print(f"{DIM}[{self.label}] >> {line}{RESET}")
        self.sock.sendall(wire.encode())

    def send_raw(self, data):
        print(f"{DIM}[{self.label}] >> (raw bytes) {data!r}{RESET}")
        self.sock.sendall(data.encode() if isinstance(data, str) else data)

    def read(self, duration=0.5):
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
        text = data.decode(errors="replace")
        if text:
            for line in text.splitlines():
                print(f"{CYAN}[{self.label}] << {line}{RESET}")
        else:
            print(f"{DIM}[{self.label}] << (no response within {duration}s){RESET}")
        return text

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def step(title):
    print()
    print(f"{BOLD}--- {title} ---{RESET}")


def verdict(expected_desc, got_text, condition):
    global score_pass, score_fail
    print(f"          expected: {expected_desc}")
    if condition:
        score_pass += 1
        print(f"          {GREEN}=> LOOKS CORRECT{RESET}")
    else:
        score_fail += 1
        print(f"          {RED}=> DOES NOT MATCH EXPECTED — check manually{RESET}")
    return condition


def register(c, nick, user="user", password=PASSWORD):
    c.send(f"PASS {password}")
    c.send(f"NICK {nick}")
    c.send(f"USER {user} 0 * :{user}")
    return c.read(0.4)


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


def main():
    print(f"{BOLD}=== ft_irc exam-style command run ==={RESET}")
    print("Every command below is what an evaluator would type by hand.")
    print("Read the raw '<<' lines yourself — the verdict is a hint, not a substitute for judgment.")

    proc = start_server()
    print(f"\nServer started on port {PORT} (pid {proc.pid})")

    try:
        # ---------------------------------------------------------------
        step("Connect and register (PASS/NICK/USER)")
        alice = Client("alice")
        resp = register(alice, "alice")
        verdict("a numeric 001 welcome reply", resp, "001" in resp)

        # ---------------------------------------------------------------
        step("Wrong password")
        badc = Client("badpw")
        badc.send(f"PASS wrongpass")
        r1 = badc.read(0.3)
        try:
            badc.send("NICK badnick")
            badc.send("USER badnick 0 * :badnick")
            r1 += badc.read(0.3)
        except (BrokenPipeError, OSError):
            print(f"{DIM}[badpw] (connection closed by server){RESET}")
        verdict("no 001 welcome, client should be rejected/closed", r1, "001" not in r1)
        badc.close()

        # ---------------------------------------------------------------
        step("Second client registers (bob)")
        bob = Client("bob")
        resp = register(bob, "bob")
        verdict("a numeric 001 welcome reply", resp, "001" in resp)

        # ---------------------------------------------------------------
        step("JOIN a channel (alice)")
        alice.send("JOIN #exam")
        resp = alice.read(0.4)
        verdict("JOIN echo and/or NAMES list (353/366)", resp, "JOIN" in resp or "353" in resp or "366" in resp)

        step("JOIN the same channel (bob)")
        bob.send("JOIN #exam")
        resp = bob.read(0.4)
        verdict("JOIN echo and/or NAMES list (353/366)", resp, "JOIN" in resp or "353" in resp or "366" in resp)

        # ---------------------------------------------------------------
        step("PRIVMSG to channel (alice -> #exam)")
        alice.send("PRIVMSG #exam :hello from alice")
        time.sleep(0.2)
        print("  (checking what bob receives)")
        resp_bob = bob.read(0.4)
        verdict("bob receives the PRIVMSG text", resp_bob, "hello from alice" in resp_bob)
        print("  (checking whether alice gets an echo — she should NOT)")
        resp_alice = alice.read(0.3)
        verdict("alice receives nothing back (no self-echo)", resp_alice, "hello from alice" not in resp_alice)

        # ---------------------------------------------------------------
        step("NOTICE to channel (bob -> #exam)")
        bob.send("NOTICE #exam :fyi from bob")
        time.sleep(0.2)
        resp_alice = alice.read(0.4)
        verdict("alice receives the NOTICE text", resp_alice, "fyi from bob" in resp_alice)

        # ---------------------------------------------------------------
        step("Direct PRIVMSG to a nickname (alice -> bob)")
        alice.send("PRIVMSG bob :direct hello")
        time.sleep(0.2)
        resp_bob = bob.read(0.4)
        verdict("bob receives the direct message", resp_bob, "direct hello" in resp_bob)

        # ---------------------------------------------------------------
        step("PRIVMSG to a channel you haven't joined (carol)")
        carol = Client("carol")
        register(carol, "carol")
        carol.send("PRIVMSG #exam :sneaky")
        resp = carol.read(0.4)
        verdict("rejected, typically 442 (not on channel)", resp, "442" in resp)

        # ---------------------------------------------------------------
        step("Non-operator tries MODE (carol, not in channel, or a member without ops)")
        carol.send("JOIN #exam")
        carol.read(0.3)
        carol.send("MODE #exam +i")
        resp = carol.read(0.4)
        verdict("rejected, typically 482 (not channel operator)", resp, "482" in resp)

        # ---------------------------------------------------------------
        step("Operator (alice, channel creator) sets MODE +i")
        alice.send("MODE #exam +i")
        resp = alice.read(0.4)
        verdict("mode change accepted (no 482), possibly echoed to channel", resp, "482" not in resp)

        # ---------------------------------------------------------------
        step("Non-operator tries KICK (carol)")
        carol.send("KICK #exam bob")
        resp = carol.read(0.4)
        verdict("rejected, typically 482", resp, "482" in resp)

        # ---------------------------------------------------------------
        step("Operator KICKs a member (alice kicks carol)")
        alice.send("KICK #exam carol :bye")
        resp_alice = alice.read(0.3)
        resp_carol = carol.read(0.3)
        verdict("KICK message seen by operator and/or kicked user", resp_alice + resp_carol,
                "KICK" in resp_alice or "KICK" in resp_carol)

        # ---------------------------------------------------------------
        step("INVITE (alice invites carol back to +i channel)")
        alice.send("INVITE carol #exam")
        resp = alice.read(0.4)
        verdict("INVITE acknowledged (numeric 341 or similar, no error)", resp, "482" not in resp and "401" not in resp)

        step("Invited user JOINs the invite-only channel (carol)")
        carol.send("JOIN #exam")
        resp = carol.read(0.4)
        verdict("JOIN succeeds now that carol was invited (no 473)", resp, "473" not in resp)

        # ---------------------------------------------------------------
        step("TOPIC — view current topic")
        bob.send("TOPIC #exam")
        resp = bob.read(0.4)
        verdict("topic reply (332) or 'no topic set' (331)", resp, "332" in resp or "331" in resp)

        step("TOPIC — operator sets a new topic (alice)")
        alice.send("TOPIC #exam :exam session topic")
        resp = alice.read(0.4)
        time.sleep(0.2)
        resp_bob = bob.read(0.4)
        verdict("topic text appears (to alice and/or broadcast to channel)", resp + resp_bob,
                "exam session topic" in (resp + resp_bob))

        step("TOPIC — restrict to operators with MODE +t")
        alice.send("MODE #exam +t")
        alice.read(0.3)
        bob.send("TOPIC #exam :bob tries to change it")
        resp = bob.read(0.4)
        verdict("rejected with +t set and bob not operator, typically 482", resp, "482" in resp)

        # ---------------------------------------------------------------
        step("MODE +k — set a channel key (clearing +i first to test +k in isolation)")
        alice.send("MODE #exam -i")
        alice.read(0.3)
        alice.send("MODE #exam +k examkey")
        alice.read(0.3)
        dave = Client("dave")
        register(dave, "dave")
        dave.send("JOIN #exam wrongkey")
        resp = dave.read(0.4)
        verdict("wrong key rejected, typically 475", resp, "475" in resp)

        dave.send("JOIN #exam examkey")
        resp = dave.read(0.4)
        verdict("correct key accepted (no 475)", resp, "475" not in resp)

        # ---------------------------------------------------------------
        step("MODE +l — set a user limit (clearing +k first to test +l in isolation)")
        alice.send("MODE #exam -k examkey")
        alice.read(0.3)
        alice.send("MODE #exam +l 4")
        alice.read(0.3)
        # currently in channel: alice, bob, carol, dave = 4. one more should be refused.
        eve = Client("eve")
        register(eve, "eve")
        eve.send(f"JOIN #exam")
        resp = eve.read(0.4)
        verdict("limit reached, JOIN refused, typically 471", resp, "471" in resp)

        # ---------------------------------------------------------------
        step("MODE +o — give operator status (alice promotes bob)")
        alice.send("MODE #exam +o bob")
        alice.read(0.3)
        bob.send("KICK #exam dave :promoted-op test")
        resp_bob = bob.read(0.3)
        resp_dave = dave.read(0.3)
        verdict("promoted bob can now KICK (no 482)", resp_bob, "482" not in resp_bob)

        # ---------------------------------------------------------------
        step("Fragmented command (subject's nc example, split mid-command)")
        alice.send_raw("PRIV")
        time.sleep(0.2)
        alice.send_raw("MSG #exam :fragmented ok\r\n")
        resp = alice.read(0.5)
        alice.send("PING stillalive")
        resp2 = alice.read(0.4)
        verdict("server still answers after a split command (PONG or echoed PING)", resp2,
                "PONG" in resp2 or "stillalive" in resp2)

        # ---------------------------------------------------------------
        step("Abrupt client disconnect doesn't affect others")
        eve.send_raw("JOIN #partial")  # no trailing \r\n
        eve.sock.close()
        time.sleep(0.3)
        frank = Client("frank")
        resp = register(frank, "frank")
        verdict("a fresh client can still register normally", resp, "001" in resp)

        # ---------------------------------------------------------------
        step("Server still alive after full exam run")
        alice.send("PING finalcheck")
        resp = alice.read(0.4)
        verdict("server responds (PONG or echoed PING)", resp, "PONG" in resp or "finalcheck" in resp)

        for c in (alice, bob, carol, dave, frank):
            c.close()

    finally:
        stop_server(proc)

    print()
    print(f"{BOLD}=== Scoreboard ==={RESET}")
    print(f"  {GREEN}{score_pass} looked correct{RESET}, {RED}{score_fail} did not match expected{RESET}")
    print("  This is a guide, not a grade — re-read any '<<' lines above for anything flagged,")
    print("  and re-run individual commands manually with nc if you want to double check.")


if __name__ == "__main__":
    main()
