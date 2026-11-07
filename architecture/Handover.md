# Handover — software-synth-LMW (2026-11-07)

Read this first when you continue the project in a new location or a new Claude Code session.
The full history is in `Conversation-2026-11-07.md`; requirements in `Spec.md`; phases in `Plan.md`;
tests in `Tests.md`; release notes in `Changes.md`.

## 1. What this project is
A semi-modular polyphonic software synthesizer (inspired by AlsaModularSynth and the ARP 2600
"prewired but patchable" idea; own design). Standalone app now, VST3 plugin as the end goal.
An external GUI written in **Kukel** (separate project) will talk to the engine over **9P2000**.

## 2. Decisions (all confirmed by the user)
| Topic | Decision |
|---|---|
| Language / build | C++20, CMake (≥ 3.22), Ninja |
| Framework | JUCE 8.0.4 via CMake FetchContent (downloaded at configure time) |
| Licence | GNU AGPLv3 (`LICENSE`); all dependencies must be AGPLv3-compatible |
| Targets | Standalone + VST3 (both already build) |
| Architecture | Semi-modular: default ("normalled") connections, patch cables override them |
| Polyphony | 1–32 voices at runtime (`voice.count`, default 8) |
| Input | MIDI keyboard + on-screen / computer (QWERTY) keyboard |
| Persistence | Patches saved/loaded to file (parameters already saved in host/standalone state) |
| GUI / engine split | Engine = 9P2000 server (own C++ implementation), single source of truth. Built-in JUCE GUI stays. Kukel GUI = extra 9P client, lives in the Kukel project. Unix socket default + optional TCP. Telemetry via 9P files; ZeroMQ deferred. Audio thread never does I/O, locks or allocation (lock-free queues). |
| Colours | White background, black text, every pair ≥ 4.5:1; presets AA/AAA/Max/custom via UI and config file (config/UI part not built yet) |
| Docs | `architecture/` folder on branch `main` |

## 3. Status
| Phase | Content | Status |
|---|---|---|
| 0 | Repo, docs, licence, CMake/JUCE skeleton, contrast module, CI workflow | done (Linux) |
| 1 | Single voice: polyBLEP VCO (sine/saw/square/triangle), linear ADSR, smoothed volume, knobs, keyboard | done (Linux) |
| 2 | Polyphony 1–32, voice stealing, Voices knob | done (Linux) |
| 3 | Module graph with normalled connections + cable override (adds VCF, LFO, Mixer, Noise) | **next** |
| 4–8 | GUI panel with cables + contrast settings, patch files, 9P server, Kukel GUI, VST3 host testing | pending |

Tests: `ctest` → 2 suites (`T-S6.2-contrast`, `T-S5-engine`), all passing.
**Not yet verified:** listening to the sound and seeing the GUI (the build machine had no display or
audio); macOS and Windows builds (CI has never run because nothing has been pushed yet).

## 4. Code map
```
CMakeLists.txt               targets: lmw_ui_core, lmw_dsp (header-only), LMWSynth (JUCE), tests
src/dsp/Oscillator.h         VCO
src/dsp/Adsr.h               envelope
src/dsp/Engine.h             Voice + polyphonic Engine with voice allocator (pure C++, RT-safe)
src/ui/Contrast.{h,cpp}      WCAG contrast ratio, presets, enforceContrast()
src/ui/Theme.h               default colours + list of all fg/bg pairs (tested)
src/app/PluginProcessor.*    JUCE AudioProcessor, parameters (IDs = future 9P paths), MIDI
src/app/PluginEditor.*       JUCE GUI: look-and-feel, knobs, keyboard
tests/test_contrast.cpp      contrast + theme tests
tests/test_engine.cpp        oscillator, ADSR, engine, polyphony tests
.github/workflows/build.yml  CI: Linux/macOS/Windows build + ctest
```

## 5. Open questions for the user
1. Voice stealing: prefer voices already in release over the oldest held voice (Spec S4.2) — implemented, awaiting confirmation.
2. Clipping with many voices: add a soft limiter or automatic gain scaling?
3. Exact default normalled patch (Spec S5.2).
4. Pending crosscheck of Spec ↔ Plan ↔ code (suggested after Phase 2).

## 6. Moving the project to a new path — checklist
1. **Copy/move the whole folder including `.git/`.** Everything is committed (last commit: Phase 2 + this handover).
2. **Delete `build/`** (≈ 500 MB) before or after moving. The CMake cache holds absolute paths and will
   refuse to work from the new location. Rebuild:
   ```sh
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```
   (The first configure downloads JUCE again: needs internet, about a minute.)
3. **Push:** remote `origin` = `https://github.com/maarten-boot/software-synth-LMW.git` is already set.
   Run `git push -u origin main`. The earlier push failed with **403** because the `gh` fine-grained token
   (`github_pat_…`) has no write access to this repo. If pushing from the new location still fails, give
   the token **Contents: Read and write** and **Workflows: Read and write** for this repo (Workflows is
   needed because of `.github/workflows/build.yml`), or use SSH:
   `git remote set-url origin git@github.com:maarten-boot/software-synth-LMW.git`.
4. **Global git config changed by the assistant:** `gh auth setup-git` added
   `credential.https://github.com.helper = !/usr/bin/gh auth git-credential` (also for gist.github.com and
   github.rl.lan) to `~/.gitconfig`. Remove it with
   `git config --global --unset-all credential.https://github.com.helper` (etc.) if you don't want it.
5. **Check CI** after the push: GitHub → Actions → "build" (3 jobs). Update `Tests.md` (T-S2.1) with the result.
6. **Claude Code context:** Claude's project memory is tied to the folder path, so a new session at the
   new path starts fresh. Start it with: *"Read architecture/Handover.md and continue."*
   Your global `~/.claude/CLAUDE.md` rules (conversation log, Spec/Plan/Tests, corrections first, contrast)
   still apply; this project additionally uses `architecture/` and keeps `Changes.md`.
7. Continue the daily log in a new `architecture/Conversation-YYYY-MM-DD.md` file.

## 7. Next step
Phase 3: module graph with normalled connections and cable override (Spec S1.2, S5) — after answering
open questions 3 (default patch) and ideally 1–2.
