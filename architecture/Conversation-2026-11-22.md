# Conversation 2026-11-22

(Continued from Conversation-2026-11-21.md; the session ran past midnight.)

- User: installer works on Windows 10; pushed and tagged v1.2.1.
- Release run 44958833305: tag check, four builds, publish — "LMW-Synth 1.2.1" with setup.exe, -windows.zip, -macos, -ubuntu-24.04, -fedora-45, SHA256SUMS.txt. CI run 44958813697 (b5fce3c): every installer page listed ok.
- Noticed: the 1.2.0 release is no longer on GitHub (its tag v1.2.0 is still there); our workflows did not remove it (the cleanup last ran 2026-11-21 11:22, release.yml only creates). Asked the user whether they deleted it on purpose — yes (user: "I deleted 1.2.0"; 1.2.1 contains everything of it).
- Releases now: 1.2.1, 1.1.0, 1.0.0. No CI artifacts older than 14 days yet.
- User: commit the docs.

## Small items

- User: A) effects get colour bars — COMB the filter yellow, DELAY and REVERB a lighter yellow; B) show the preset name on Schematic and Matrix; C) inputs left, outputs right on FORMANT (move Voice to the left) and the effects.
- Asked: B — the header "Patch:" line is on every tab (user: "my error, I was looking in the wrong place": dropped); A — #FFF8D0, everywhere (title bars, Matrix, Schematic).
- Done: Theme Unit::Effect #FFF8D0, unitOf com → VCF, del/rev → Effect; Effects title bars filled; FORMANT Voice | Vowel | Reso | Mix with In under Voice; effect In sockets col 0, Out last. test_contrast updated; native ctest 31/31.
- User: no GitHub CI for these small things — build the local Linux (build-u24) for a preview, then continue with the MCP server.
- Proposed Phase 18 (MCP server): a program between the AI assistant (MCP, stdio) and the synth (9P), tools for finding synths, reading parameters/patch, setting parameters and cables, presets, playing notes, saving; a sound-design guide as background; C++ (ships in every download, recommended) or Python. Waiting for the user's choice after the preview.
- User: cables grey at rest, black only when the mouse is over a socket, so the focus is on the relevant cables. Asked: grey #6B6B6B; black for mouse-over socket and keyboard focus, Schematic too.
- Done: theme cable grey (5.3:1 on its white edge, raised with the preset, black at Max, in pairs()); Synth tab: hot cables (hover or focused socket) black, 4 px, drawn last; Schematic: same for the hovered port and the keyboard cursor, legend sample grey. Native ctest 31/31; Xvfb checked. build-u24 rebuilt for the preview (no GitHub CI for these small items, user).

## Phase 18 — MCP server

- User: C++, commit the docs (2fb7eb5, with the small items) and start Phase 18.
- Spec S11 written (program lmw-mcp, protocol, tools, resources, setup, safety); Plan 18 split into 18a core, 18b presets and guide, 18c delivery.
- Phase 18 built: src/mcp/Json.h, src/mcp/Server.h (12 tools, 2 resources, instructions), src/ninep/Session.h (ClientSession), tools/lmw-mcp.cpp (Documents folder as JUCE finds it), docs/ai/SoundDesign.md (+ cmake/GuideText.cmake). tests/test_mcp.cpp (fake synth) and host test 4i (real lmw-mcp against the plugin); by hand on Xvfb against the standalone. Fixed on the way: raw string ending at ")\"", the class name Session (server side) → ClientSession, a blank line before the new name in apply_instrument_preset, the 9P text "a effect" → "an effect". Delivery: CI Package, installer "Command-line tools", build-u24, README "AI assistant". Native ctest 32/32.
- User (during the work): what about making the cables also transparent when grey? — answered with a proposal (below in the reply).
- The Ubuntu 24 container build found T-S9.8-html failing on the README's new link to docs/ai/SoundDesign.md on GitHub: the test now allows .md links to GitHub (local links must still point to the HTML copies). Container 32/32; build-u24 has lmw-mcp.
- User ideas during the work: cables transparent when grey; About and the contrast settings into the Options top-bar menu; hide/show the keyboard; a Theme menu in Options for personal colours. Noted as Plan Phase 21 (to discuss).
- User: commit Phase 18; then work out Phase 21; expects C++ was chosen for speed — answered: mainly no install dependency and reuse of the C++ 9P and preset code (speed hardly matters for tool calls).

## Phase 21 worked out

- Chosen (user): a header Menu button (standalone and DAW) plus the same items in the standalone Options menu; cables at rest 55 % opaque; keyboard hidden/shown by menu item, a small button and Ctrl+K; themes as presets plus own colours.
- Spec S6.11 (menu), S6.12 (keyboard), S6.13 (themes), S6.6 (see-through cables); Plan 21a–21d.
