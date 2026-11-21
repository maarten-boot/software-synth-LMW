# Conversation 2026-11-22

(Continued from Conversation-2026-11-21.md; the session ran past midnight.)

- User: installer works on Windows 10; pushed and tagged v1.2.1.
- Release run 44958833305: tag check, four builds, publish — "LMW-Synth 1.2.1" with setup.exe, -windows.zip, -macos, -ubuntu-24.04, -fedora-45, SHA256SUMS.txt. CI run 44958813697 (b5fce3c): every installer page listed ok.
- Noticed: the 1.2.0 release is no longer on GitHub (its tag v1.2.0 is still there); our workflows did not remove it (the cleanup last ran 2026-11-21 11:22, release.yml only creates). Asked the user whether they deleted it on purpose — yes (user: "I deleted 1.2.0"; 1.2.1 contains everything of it).
- Releases now: 1.2.1, 1.1.0, 1.0.0. No CI artifacts older than 14 days yet.
- User: commit the docs.
