# Conversation 2026-11-21

(continued from 2026-11-20)
Local validators (Ubuntu 26 VM, no display): Steinberg 47 of 47, pluginval SUCCESS incl. "Fuzz parameters"; 10 more pluginval runs with random seeds (--skip-gui-tests) all pass — the Fedora crash does not reproduce here. (A first loop without --skip-gui-tests crashed in pluginval's Editor test for lack of a display: my mistake, not the plugin.) Proposed: re-run the failed Fedora job (needs the user's go-ahead) or reproduce in a Fedora 45 container.

**User:** cmake --build build failed in their shell (different path: the build folder belongs to the VM vibe-u26, reached as /net/192.168.122.1/...; the host has no /usr/bin/ninja). Explained: own build folder on the host (build-host) after installing ninja, or the container build.

**User:** do we have a make prepare (to install or list missing dependencies)? (correction: "dpendencies" → "dependencies")
No; offered scripts/prepare.sh.

**User:** yes, add prepare.sh
Done (S2.4): scripts/prepare.sh (list / --install / --optional / --quiet; apt, dnf, brew; Windows hint); CI Linux jobs and build-ubuntu24.sh use it; README "Prepare". Found on Ubuntu 26: libfontconfig1-dev and libgl1-mesa-dev are only transitional names — switched to libfontconfig-dev and libgl-dev (also on 24.04). Checked: VM 22 of 22 present; clean Ubuntu 24.04 and Fedora 45 containers install everything and then report nothing missing.

**User:** (validators on the host: 47 of 47, pluginval SUCCESS) — consistent: the crash only showed on Fedora.
**User:** yes, rerun the failed Fedora job
Not possible from this session: `gh run rerun 44829843368 --failed` → "Resource not accessible by personal access token" (read-only token); asked the user to start it.
Rebuilt with the NaN guard and the routing box fix: native 24 of 24; Xvfb: VCF 2 box "Parallel" (normal) and "Par" (compact) after restart from saved state. Ubuntu 24 container build started (it now installs through prepare.sh).

**User:** yes, commit them
Committed 16b (VCF routing), the NaN guard, prepare.sh with the CI / container change, docs and logs on main (the Ubuntu 24 container build was still running at commit time); the user pushes.
