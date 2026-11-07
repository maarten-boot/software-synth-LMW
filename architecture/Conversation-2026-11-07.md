# Conversation log — 2026-11-07

## 1. Working rules (user)
- Keep a daily conversation log, Plan.md, Spec.md, test cases; keep them current; suggest Spec/Plan/code crosschecks after significant changes.
- Suggest corrections for unclear/erroneous sentences and confirm before proceeding.
- Files go in `architecture/` (did not exist → created). New project without git → git repo created.
- Explicit `Changes.md` to record changes between releases.

## 2. Request (user)
> could we build a simple software synthesyzer using c or c++ similar to Alsa Modular Synth usable on both Mac Windows and Linux with variable polyphony and a simple userinterface

Assistant: proposed corrections and open questions (language, modular vs fixed, audio/MIDI/GUI libraries, plugin vs standalone). Awaiting confirmation — no code written yet.

## 3. Question (user)
> What is the implication of GPL ?

Assistant: explained GPL/AGPL obligations (source disclosure on distribution, copyleft, compatibility) and compared with the permissive libraries (miniaudio, RtMidi, Dear ImGui). Licence choice still open.

## 4. Statement (user)
> the whole code will be in the public domain so i expect GPL is fine here

Assistant: pointed out that "public domain" and "GPL" are different (no-rights vs. copyleft); a GPL/AGPL dependency (e.g. JUCE) forces the distributed program to be (A)GPL, not public domain. Asked user to confirm intent: (a) truly public domain (CC0/Unlicense) with permissive libs, or (b) open source under GPL/AGPL (JUCE possible).

## 5. Statement (user)
> normally i coe with MIT license

Assistant: suggested correction "coe" -> "code". Noted MIT is permissive (not public domain, not GPL); compatible with miniaudio/RtMidi/Dear ImGui, not with JUCE under AGPL. Asked to confirm MIT for the project (replaces "public domain").

## 6. Confirmation (user)
> code

Correction confirmed: "Normally I code with the MIT license." Licence: MIT (recorded in Spec S8). Remaining decisions asked via multiple-choice.

## 7. Answers to decision questions (user)
- Architecture: "i like the partial prewire implementation of teh ARP 2600, it starts with prewired but allows you to customize; i have no problems with switching to AGPL if that gives us a better instrumennt. The goals is a instrument but also usable as a VST3 plugin (optionally later but as the end goal)"
- Stack: "depends on the previous AGPL switch, what is the impact of C++20, i have no preference of 17/20"
- Play/voices: MIDI + computer keyboard, polyphony 1–32, oldest-voice stealing (confirmed).
- Other: "see my earlier not on final goal to be a vst3 plugin next to standalone. teh rest is ok" -> patch save/load, architecture/ on main, holder Maarten Boot OK.

Assistant: suggested corrections (teh->the, instrumennt->instrument, not->note). Interpreted as: semi-modular (normalled default patch, cables override), standalone + VST3 (end goal). Compared JUCE/AGPL vs iPlug2/MIT, explained C++20 impact. Recommended JUCE 8 + AGPLv3 + C++20. Awaiting confirmation; licence S8 provisional.

## 8. Confirmation + new input (user)
> yes, JUCE 8 + AGPLv3 + C++20, go ahead; i alao investigated a split between GUI and instrument and we will use the 9P protocoll to communicate between gui and instrument, see also the feedback i got from a independant chat: [pasted external chat proposing 9P control plane + ZeroMQ data plane, mentions user's language "Kukel" with native 9P/ZMQ]. Feel free to comment and suggest aternatives.

Confirmed: JUCE 8, AGPLv3, C++20, semi-modular (prewired, cables override), standalone + VST3 (end goal).
Corrections suggested: alao->also, protocoll->protocol, independant->independent, aternatives->alternatives.
Assistant: updated Spec/Plan/Tests/Changes for confirmed items; reviewed the 9P/ZMQ proposal (RT-safety, VST3 in-process editor constraints, inter-process audio routing, inproc misuse) and proposed alternatives. 9P split recorded in Spec S10 as DRAFT pending answers.

## 9. Decisions (user)
> yes Kukel (mit licence but as it is a external process talking to a instrument i would expect no problem it hac be hosted also by the kukel project), 9P2000, write it yourself, rest as recommended

Decided: external GUI in Kukel (MIT, separate process); 9P2000; own 9P server implementation in C++; built-in JUCE GUI in plugin/standalone; 9P only for telemetry (ZMQ deferred); Unix socket default + optional TCP.
Corrections suggested: "a instrument" -> "an instrument", "hac" -> "can". Open: does the Kukel GUI live in the Kukel project repo instead of this repo?
Assistant: licence note — separate processes over 9P are separate works; MIT client + AGPL engine is fine. AGPL §13: remote users of the engine must be offered its source (satisfied by public repo; add an /about or /source file).

## 10. Confirmation (user)
> hac=can

Assistant: built Phase 0 — CMake + JUCE 8.0.4 (FetchContent), targets Standalone + VST3 (silent processor, white/black editor), pure-C++ WCAG contrast module (lmw_ui_core) with ctest `test_contrast`, GitHub Actions matrix CI, README, .gitignore. Linux: build OK, tests pass, standalone starts (no display available, GUI not visually verified). Kukel GUI repo location: interpreted as "may live in the Kukel project" — still to confirm.
