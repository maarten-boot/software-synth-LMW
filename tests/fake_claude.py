#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# T-S12-tab: a fake Claude Messages API for the Assistant tab (Spec S12), started with
#   fake_claude.py <port> <request log>   and the synth with LMW_ASSISTANT_URL=http://127.0.0.1:<port>.
# Answers by the last user message: "darker" -> set_parameters vcf1.cutoff 800 + play; "save" ->
# save_instrument_preset "Assistant test"; "slow" -> waits 5 s (for Stop); "busy" -> 529 once, then an answer; "bad key" when the key is not
# test-key; tool results -> a short end_turn text. Every request (headers without the key's value, body)
# is appended to the log as one JSON line, with "key_ok" and "key_in_body".
import json
import sys
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT, LOG = int(sys.argv[1]), sys.argv[2]
KEY = "test-key"
busy_done = {"n": 0}


def message(stop, content):
    return {"id": "msg_fake", "type": "message", "role": "assistant", "model": "claude-opus-5-5",
            "stop_reason": stop, "content": content,
            "usage": {"input_tokens": 2000, "output_tokens": 300, "cache_creation_input_tokens": 0,
                      "cache_read_input_tokens": 8000}}


def tool(id_, name, input_):
    return {"type": "tool_use", "id": id_, "name": name, "input": input_}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, status, obj, headers=()):
        data = json.dumps(obj).encode()
        self.send_response(status)
        self.send_header("content-type", "application/json")
        for k, v in headers:
            self.send_header(k, v)
        self.send_header("content-length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        ok = self.headers.get("x-api-key") == KEY
        self.reply(200 if ok else 401, {"data": []} if ok else {"error": {"message": "invalid x-api-key"}})

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("content-length", "0"))).decode()
        key_ok = self.headers.get("x-api-key") == KEY
        with open(LOG, "a") as f:
            f.write(json.dumps({"path": self.path, "key_ok": key_ok, "key_in_body": KEY in body,
                                "version": self.headers.get("anthropic-version"), "body": json.loads(body)}) + "\n")
        if not key_ok:
            return self.reply(401, {"type": "error", "error": {"type": "authentication_error", "message": "invalid x-api-key"}})
        last = json.loads(body)["messages"][-1]
        blocks = last["content"]
        if any(b.get("type") == "tool_result" for b in blocks):
            return self.reply(200, message("end_turn", [{"type": "text", "text": "Done: the cutoff is now 800 Hz, a darker sound."}]))
        text = " ".join(b.get("text", "") for b in blocks if b.get("type") == "text").lower()
        if "busy" in text and busy_done["n"] == 0:
            busy_done["n"] = 1
            return self.reply(529, {"error": {"message": "overloaded"}}, [("retry-after", "1")])
        if "slow" in text:                                  # Stop test: answers after 5 s
            time.sleep(5)
            return self.reply(200, message("tool_use", [tool("tu_9", "set_parameters", {"values": {"vcf1.cutoff": 500}})]))
        if "darker" in text:
            return self.reply(200, message("tool_use", [
                {"type": "text", "text": "I will lower the filter cutoff."},
                tool("tu_1", "set_parameters", {"values": {"vcf1.cutoff": 800}}),
                tool("tu_2", "play", {"notes": [48, 52, 55], "seconds": 0.5})]))
        if "save" in text:
            return self.reply(200, message("tool_use", [tool("tu_3", "save_instrument_preset", {"name": "Assistant test"})]))
        return self.reply(200, message("end_turn", [{"type": "text", "text": "Hello from the fake service."}]))


HTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
