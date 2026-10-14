#
# test_aimon_live_e2e.py
#
# Real End-to-End Qualification against live aimon daemon (TCP port 3885)
# Verifies live tool registration, schema publication, and real RPC tool dispatch.
#
# Copyright (C) 2026, Charles Chiou
#

import json
import os
import signal
import socket
import subprocess
import sys
import time
import urllib.request

AIMON_HTTP_ENDPOINT = "http://127.0.0.1:3883/message"
EXPECTED_TOOLS = [
    "abc80_cancel_punching",
    "abc80_disassemble",
    "abc80_get_punch_status",
    "abc80_punch_keys",
    "abc80_read_cpu",
    "abc80_read_display",
    "abc80_read_memory",
]


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("", 0))
        return s.getsockname()[1]


def send_mcp_request(method: str, params: dict = None) -> dict:
    if params is None:
        params = {}
    payload = {
        "jsonrpc": "2.0",
        "id": int(time.time() * 1000) % 1000000,
        "method": method,
        "params": params,
    }
    req = urllib.request.Request(
        AIMON_HTTP_ENDPOINT,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=10) as resp:
        body = resp.read().decode("utf-8")
        return json.loads(body)


def call_mcp_tool(tool_name: str, arguments: dict = None) -> str:
    if arguments is None:
        arguments = {}
    resp = send_mcp_request("tools/call", {"name": tool_name, "arguments": arguments})
    assert "result" in resp, f"Tool call error in response: {resp}"
    result = resp["result"]
    assert "content" in result and len(result["content"]) > 0, f"No content in result: {result}"
    return result["content"][0]["text"]


def test_aimon_live_e2e():
    port = find_free_port()
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    server_bin = os.path.join(repo_root, "build", "abc80_server")
    assert os.path.isfile(server_bin), f"Server binary not found at {server_bin}"

    print(f"[E2E] Starting resident abc80_server on port {port}...")
    server_proc = subprocess.Popen(
        [server_bin, "--port", str(port), "--root", repo_root],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    try:
        # Wait for server to report LISTENING
        started = False
        start_time = time.time()
        while time.time() - start_time < 5.0:
            line = server_proc.stdout.readline()
            if "LISTENING" in line:
                started = True
                print(f"[E2E] Server confirmed: {line.strip()}")
                break
            time.sleep(0.05)
        assert started, "Server failed to start within 5 seconds"

        # Allow TCP gateway registration to settle
        time.sleep(1.0)

        # 1. Query tools/list from live aimon daemon
        print("[E2E] Step 1: Querying tools/list from live aimon daemon...")
        tools_list_resp = send_mcp_request("tools/list")
        assert "result" in tools_list_resp, f"Invalid tools/list response: {tools_list_resp}"
        live_tools = {t["name"]: t for t in tools_list_resp["result"]["tools"]}

        for expected in EXPECTED_TOOLS:
            assert expected in live_tools, f"Expected tool '{expected}' not found in live aimon registry"
            print(f"  + Registered in aimon: {expected}")

        # 2. Call abc80_read_cpu
        print("[E2E] Step 2: Calling abc80_read_cpu via live aimon...")
        cpu_text = call_mcp_tool("abc80_read_cpu")
        cpu_data = json.loads(cpu_text)
        assert "registers" in cpu_data, f"Invalid CPU data: {cpu_data}"
        assert "flags" in cpu_data and "z" in cpu_data["flags"], f"Missing flags in CPU data: {cpu_data}"
        assert cpu_data["rom"] == "monitor", f"Expected monitor ROM, got {cpu_data.get('rom')}"
        assert cpu_data["paused"] is True, "Resident board should be auto-paused initially"
        assert cpu_data["autoPaused"] is True, "Resident board should report autoPaused=True initially"
        print(f"  + PC: 0x{cpu_data['registers']['pc']:04X}, SP: 0x{cpu_data['registers']['sp']:04X}, flags.z={cpu_data['flags']['z']}")

        # 3. Call abc80_read_memory
        print("[E2E] Step 3: Calling abc80_read_memory (addr=0, count=16) via live aimon...")
        mem_text = call_mcp_tool("abc80_read_memory", {"addr": 0, "count": 16})
        mem_data = json.loads(mem_text)
        assert mem_data["addr"] == 0
        assert mem_data["count"] == 16
        assert len(mem_data["bytes"]) == 16
        assert "hex_dump" in mem_data and mem_data["hex_dump"].startswith("0000:")
        print(f"  + Read 16 ROM bytes: {mem_data['hex_dump']}")

        # 4. Call abc80_disassemble
        print("[E2E] Step 4: Calling abc80_disassemble (addr=0, count=4) via live aimon...")
        disasm_text = call_mcp_tool("abc80_disassemble", {"addr": 0, "count": 4})
        disasm_data = json.loads(disasm_text)
        assert len(disasm_data["instructions"]) == 4
        print(f"  + First instruction: {disasm_data['instructions'][0]['mnemonic']}")

        # 5. Call abc80_read_display
        print("[E2E] Step 5: Calling abc80_read_display via live aimon...")
        disp_text = call_mcp_tool("abc80_read_display")
        disp_data = json.loads(disp_text)
        assert "digits" in disp_data
        assert "display_masks" in disp_data and len(disp_data["display_masks"]) == 6
        assert "leds" in disp_data and "speaker" in disp_data["leds"]
        print(f"  + Display digits: '{disp_data['digits']}', masks: {disp_data['display_masks']}, EP LED: {disp_data['leds']['ep']}")

        # 6. Call abc80_punch_keys synchronous (including RST tactile reset)
        print("[E2E] Step 6: Calling abc80_punch_keys (sync=true, keys=['RST', '1'])...")
        punch_t0 = time.time()
        punch_text = call_mcp_tool("abc80_punch_keys", {"keys": ["RST", "1"], "sync": True})
        punch_elapsed = time.time() - punch_t0
        punch_data = json.loads(punch_text)
        assert punch_data["status"] == "completed"
        assert punch_data["enqueued_count"] == 2
        assert punch_data["punched_count"] == 2
        assert "estimated_duration_sec" in punch_data
        assert "hint" in punch_data and "Keypad is locked to human input" in punch_data["hint"]
        assert punch_elapsed >= 0.8, f"Expected authentic 0.5s pacing (~1.0s), got {punch_elapsed:.2f}s"
        print(f"  + Punch completed in {punch_elapsed:.2f}s (authentic human pacing)")

        # 7. Call abc80_punch_keys async with cancel
        print("[E2E] Step 7: Testing async punch and cancellation via live aimon...")
        async_text = call_mcp_tool("abc80_punch_keys", {
            "keys": ["A", "B", "C", "D", "E", "F"],
            "sync": False
        })
        async_data = json.loads(async_text)
        assert async_data["status"] == "enqueued"
        assert async_data["enqueued_count"] == 6
        assert "hint" in async_data

        # Check punch status
        status_text = call_mcp_tool("abc80_get_punch_status")
        status_data = json.loads(status_text)
        assert status_data["is_punching"] is True
        assert "remaining" in status_data
        print(f"  + Enqueued 6 keys; status reports is_punching=True, remaining={status_data['remaining']}")

        # Cancel punching
        cancel_text = call_mcp_tool("abc80_cancel_punching")
        cancel_data = json.loads(cancel_text)
        assert cancel_data["status"] == "cancelled"

        # Check punch status again
        time.sleep(0.1)
        post_cancel_status = json.loads(call_mcp_tool("abc80_get_punch_status"))
        assert post_cancel_status["is_punching"] is False
        assert post_cancel_status["queue_depth"] == 0
        assert post_cancel_status["remaining"] == 0
        print("  + Successfully cancelled punch sequence; status verified idle")

        print("[E2E] ALL LIVE AIMON MCP GATEWAY CHECKS PASSED WITH ZERO MOCKS!")

    finally:
        print("[E2E] Cleaning up server process...")
        server_proc.send_signal(signal.SIGTERM)
        try:
            server_proc.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            server_proc.kill()
            server_proc.wait()

        # Verify clean deregistration in aimon
        time.sleep(0.5)
        after_tools_resp = send_mcp_request("tools/list")
        after_tools = {t["name"] for t in after_tools_resp.get("result", {}).get("tools", [])}
        remaining_abc = [t for t in EXPECTED_TOOLS if t in after_tools]
        assert len(remaining_abc) == 0, f"Tools not cleanly deregistered on exit: {remaining_abc}"
        print("[E2E] Server process terminated; all tools cleanly deregistered from live aimon registry.")


if __name__ == "__main__":
    test_aimon_live_e2e()
