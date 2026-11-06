#
# test_aimon_live_e2e.py
#
# Real End-to-End Qualification against live aimon daemon (TCP port 3885)
# Verifies live tool registration, schema publication, and real RPC tool dispatch
# over MCP Streamable HTTP (2025-03-26).
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

AIMON_HTTP_ENDPOINT = "http://127.0.0.1:3883/mcp"
AIMON_MONITORS_ENDPOINT = "http://127.0.0.1:3883/api/monitors"
EXPECTED_TOOLS = [
    "abc80_cancel_punching",
    "abc80_disassemble",
    "abc80_get_punch_status",
    "abc80_punch_keys",
    "abc80_read_cpu",
    "abc80_read_display",
    "abc80_read_memory",
]


class McpStreamableHttpSession:
    """Manages an MCP Streamable HTTP (2025-03-26) session with session ID tracking."""

    def __init__(self, endpoint_url: str):
        self.endpoint_url = endpoint_url
        self.session_id = None

    def initialize(self, protocol_version: str = "2025-03-26") -> dict:
        payload = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "protocolVersion": protocol_version,
                "capabilities": {},
                "clientInfo": {"name": "test_e2e", "version": "1.0.0"},
            },
        }
        req = urllib.request.Request(
            self.endpoint_url,
            data=json.dumps(payload).encode("utf-8"),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            assert resp.status == 200, f"Expected 200 OK from initialize, got {resp.status}"
            self.session_id = resp.headers.get("Mcp-Session-Id")
            assert self.session_id, "Missing Mcp-Session-Id header in initialize response"
            data = json.loads(resp.read().decode("utf-8"))
            assert data.get("result", {}).get("protocolVersion") == protocol_version, (
                f"Protocol version mismatch: {data}"
            )
            return data

    def send_notification(self, method: str, params: dict = None) -> None:
        payload = {
            "jsonrpc": "2.0",
            "method": method,
        }
        if params is not None:
            payload["params"] = params
        headers = {"Content-Type": "application/json"}
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
        req = urllib.request.Request(
            self.endpoint_url,
            data=json.dumps(payload).encode("utf-8"),
            headers=headers,
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            assert resp.status == 202, f"Expected 202 Accepted on notification, got {resp.status}"
            body = resp.read()
            assert body == b"", f"Expected empty body on notification 202, got {body}"

    def send_request(self, method: str, params: dict = None) -> dict:
        if params is None:
            params = {}
        payload = {
            "jsonrpc": "2.0",
            "id": int(time.time() * 1000) % 1000000,
            "method": method,
            "params": params,
        }
        headers = {"Content-Type": "application/json"}
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
        req = urllib.request.Request(
            self.endpoint_url,
            data=json.dumps(payload).encode("utf-8"),
            headers=headers,
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            assert resp.status == 200, f"Expected 200 OK, got {resp.status}"
            body = resp.read().decode("utf-8")
            return json.loads(body)

    def call_tool(self, tool_name: str, arguments: dict = None) -> str:
        if arguments is None:
            arguments = {}
        resp = self.send_request("tools/call", {"name": tool_name, "arguments": arguments})
        assert "result" in resp, f"Tool call error in response: {resp}"
        result = resp["result"]
        assert "content" in result and len(result["content"]) > 0, f"No content in result: {result}"
        return result["content"][0]["text"]

    def close(self) -> None:
        if not self.session_id:
            return
        headers = {"Mcp-Session-Id": self.session_id}
        req = urllib.request.Request(
            self.endpoint_url,
            headers=headers,
            method="DELETE",
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            assert resp.status == 204, f"Expected 204 No Content on DELETE, got {resp.status}"
        self.session_id = None


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("", 0))
        return s.getsockname()[1]


def get_discovered_monitors() -> list:
    req = urllib.request.Request(AIMON_MONITORS_ENDPOINT, method="GET")
    with urllib.request.urlopen(req, timeout=10) as resp:
        data = json.loads(resp.read().decode("utf-8"))
        return data if isinstance(data, list) else data.get("monitors", [])


def test_aimon_live_e2e():
    port = find_free_port()
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    server_bin = os.path.join(repo_root, "build", "abc80_server")
    assert os.path.isfile(server_bin), f"Server binary not found at {server_bin}"

    print(f"[E2E] Starting resident abc80_server on port {port}...")
    server_proc = subprocess.Popen(
        [server_bin, "--port", str(port), "--root", repo_root, "--aimon"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    aimon_session = None
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

        # Verify ephemeral monitor is visible and connected in /api/monitors
        monitors = get_discovered_monitors()
        matching_ephemeral = [m for m in monitors if m.get("subsystem") == "abc80" and m.get("port") == port]
        assert len(matching_ephemeral) == 1 and matching_ephemeral[0].get("connected") is True, (
            f"Expected ephemeral abc80 on port {port} to be connected in /api/monitors; got {monitors}"
        )

        # 1. Initialize Streamable HTTP session against live aimon daemon
        print(f"[E2E] Step 1: Initializing MCP Streamable HTTP session against {AIMON_HTTP_ENDPOINT}...")
        aimon_session = McpStreamableHttpSession(AIMON_HTTP_ENDPOINT)
        init_data = aimon_session.initialize("2025-03-26")
        assert "protocolVersion" in init_data["result"]
        assert init_data["result"]["protocolVersion"] == "2025-03-26"
        assert aimon_session.session_id is not None
        print(f"  + Mcp-Session-Id: {aimon_session.session_id}")

        # Send notifications/initialized (202 Accepted, empty body)
        print("[E2E] Sending notifications/initialized...")
        aimon_session.send_notification("notifications/initialized")
        print("  + Received 202 Accepted with empty body")

        # Query tools/list from live aimon daemon
        print("[E2E] Querying tools/list from live aimon daemon...")
        tools_list_resp = aimon_session.send_request("tools/list")
        assert "result" in tools_list_resp, f"Invalid tools/list response: {tools_list_resp}"
        live_tools = {t["name"]: t for t in tools_list_resp["result"]["tools"]}

        for expected in EXPECTED_TOOLS:
            assert expected in live_tools, f"Expected tool '{expected}' not found in live aimon registry"
            print(f"  + Registered in aimon: {expected}")

        # 2. Call abc80_read_cpu
        print("[E2E] Step 2: Calling abc80_read_cpu via live aimon...")
        cpu_text = aimon_session.call_tool("abc80_read_cpu")
        cpu_data = json.loads(cpu_text)
        assert "registers" in cpu_data, f"Invalid CPU data: {cpu_data}"
        assert "flags" in cpu_data and "z" in cpu_data["flags"], f"Missing flags in CPU data: {cpu_data}"
        assert cpu_data["rom"] == "monitor", f"Expected monitor ROM, got {cpu_data.get('rom')}"
        assert cpu_data["paused"] is True, "Resident board should be auto-paused initially"
        assert cpu_data["autoPaused"] is True, "Resident board should report autoPaused=True initially"
        print(f"  + PC: 0x{cpu_data['registers']['pc']:04X}, SP: 0x{cpu_data['registers']['sp']:04X}, flags.z={cpu_data['flags']['z']}")

        # 3. Call abc80_read_memory
        print("[E2E] Step 3: Calling abc80_read_memory (addr=0, count=16) via live aimon...")
        mem_text = aimon_session.call_tool("abc80_read_memory", {"addr": 0, "count": 16})
        mem_data = json.loads(mem_text)
        assert mem_data["addr"] == 0
        assert mem_data["count"] == 16
        assert len(mem_data["bytes"]) == 16
        assert "hex_dump" in mem_data and mem_data["hex_dump"].startswith("0000:")
        print(f"  + Read 16 ROM bytes: {mem_data['hex_dump']}")

        # 4. Call abc80_disassemble
        print("[E2E] Step 4: Calling abc80_disassemble (addr=0, count=4) via live aimon...")
        disasm_text = aimon_session.call_tool("abc80_disassemble", {"addr": 0, "count": 4})
        disasm_data = json.loads(disasm_text)
        assert len(disasm_data["instructions"]) == 4
        print(f"  + First instruction: {disasm_data['instructions'][0]['mnemonic']}")

        # 5. Call abc80_read_display
        print("[E2E] Step 5: Calling abc80_read_display via live aimon...")
        disp_text = aimon_session.call_tool("abc80_read_display")
        disp_data = json.loads(disp_text)
        assert "digits" in disp_data
        assert "display_masks" in disp_data and len(disp_data["display_masks"]) == 6
        assert "leds" in disp_data and "speaker" in disp_data["leds"]
        print(f"  + Display digits: '{disp_data['digits']}', masks: {disp_data['display_masks']}, EP LED: {disp_data['leds']['ep']}")

        # 6. Call abc80_punch_keys synchronous (including RST tactile reset)
        print("[E2E] Step 6: Calling abc80_punch_keys (sync=true, keys=['RST', '1'])...")
        punch_t0 = time.time()
        punch_text = aimon_session.call_tool("abc80_punch_keys", {"keys": ["RST", "1"], "sync": True})
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
        async_text = aimon_session.call_tool("abc80_punch_keys", {
            "keys": ["A", "B", "C", "D", "E", "F"],
            "sync": False
        })
        async_data = json.loads(async_text)
        assert async_data["status"] == "enqueued"
        assert async_data["enqueued_count"] == 6
        assert "hint" in async_data

        # Check punch status
        status_text = aimon_session.call_tool("abc80_get_punch_status")
        status_data = json.loads(status_text)
        assert status_data["is_punching"] is True
        assert "remaining" in status_data
        print(f"  + Enqueued 6 keys; status reports is_punching=True, remaining={status_data['remaining']}")

        # Cancel punching
        cancel_text = aimon_session.call_tool("abc80_cancel_punching")
        cancel_data = json.loads(cancel_text)
        assert cancel_data["status"] == "cancelled"

        # Check punch status again
        time.sleep(0.1)
        post_cancel_status = json.loads(aimon_session.call_tool("abc80_get_punch_status"))
        assert post_cancel_status["is_punching"] is False
        assert post_cancel_status["queue_depth"] == 0
        assert post_cancel_status["remaining"] == 0
        print("  + Successfully cancelled punch sequence; status verified idle")

        # 8. Test abc80_server's own standalone http://127.0.0.1:<port>/mcp endpoint
        print(f"[E2E] Step 8: Testing abc80_server standalone MCP endpoint on http://127.0.0.1:{port}/mcp...")
        standalone_url = f"http://127.0.0.1:{port}/mcp"
        standalone_session = McpStreamableHttpSession(standalone_url)
        standalone_init = standalone_session.initialize("2025-03-26")
        assert standalone_init["result"]["serverInfo"]["name"] == "abc80"
        standalone_session.send_notification("notifications/initialized")
        standalone_tools = standalone_session.send_request("tools/list")
        assert len(standalone_tools["result"]["tools"]) == 7
        standalone_cpu = standalone_session.call_tool("abc80_read_cpu")
        standalone_cpu_data = json.loads(standalone_cpu)
        assert "registers" in standalone_cpu_data
        standalone_session.close()
        print("  + Standalone abc80_server MCP endpoint verified successfully (200, 202, 204)!")

        # 9. Cleanly close the aimon MCP session
        aimon_session.close()
        aimon_session = None
        print("  + Successfully closed aimon MCP session with DELETE /mcp (204 No Content)")

        print("[E2E] ALL LIVE AIMON MCP GATEWAY CHECKS PASSED WITH ZERO MOCKS!")

    finally:
        if aimon_session is not None:
            try:
                aimon_session.close()
            except Exception:
                pass

        print("[E2E] Cleaning up server process...")
        server_proc.send_signal(signal.SIGTERM)
        try:
            server_proc.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            server_proc.kill()
            server_proc.wait()

        # Verify clean deregistration of the ephemeral instance in aimon
        time.sleep(0.5)
        after_monitors = get_discovered_monitors()
        lingering_ephemeral = [m for m in after_monitors if m.get("subsystem") == "abc80" and m.get("port") == port]
        assert len(lingering_ephemeral) == 0, (
            f"Ephemeral abc80 on port {port} was not cleanly removed from /api/monitors: {lingering_ephemeral}"
        )

        active_abc_monitors = [
            m for m in after_monitors
            if m.get("subsystem") == "abc80" and m.get("connected") is True
        ]

        cleanup_session = McpStreamableHttpSession(AIMON_HTTP_ENDPOINT)
        cleanup_session.initialize("2025-03-26")
        cleanup_session.send_notification("notifications/initialized")
        after_tools_resp = cleanup_session.send_request("tools/list")
        after_tools = {t["name"] for t in after_tools_resp.get("result", {}).get("tools", [])}
        remaining_abc = [t for t in EXPECTED_TOOLS if t in after_tools]
        cleanup_session.close()

        if active_abc_monitors:
            assert len(remaining_abc) == len(EXPECTED_TOOLS), (
                f"Active resident abc80 monitor(s) {active_abc_monitors} lost tools after ephemeral exit: {remaining_abc}"
            )
            print("[E2E] Ephemeral server unregistered cleanly; resident abc80 monitor retained all 7 MCP tools.")
        else:
            assert len(remaining_abc) == 0, f"Tools not cleanly deregistered on exit: {remaining_abc}"
            print("[E2E] Server process terminated; all tools cleanly deregistered from live aimon registry.")


if __name__ == "__main__":
    test_aimon_live_e2e()
