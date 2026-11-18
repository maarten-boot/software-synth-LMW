# Third-party code in LMW-Synth

| Component | Licence | Used for | Source |
|---|---|---|---|
| JUCE 9.0.5 | AGPL-3.0 (JUCE's open-source licence option) | the whole program: audio, plugin formats, GUI | https://github.com/juce-framework/JUCE (pinned commit, cmake/JuceVersion.cmake) |
| Freeverb (juce::Reverb, part of JUCE) | AGPL-3.0 with JUCE | the REVERB effect | in JUCE |
| VST3 SDK (part of JUCE) | MIT (Steinberg Media Technologies, 2025) | the VST3 plugin | in JUCE |
| NSIS 3.13 | zlib/libpng (the installer stub that becomes part of the setup program) | the Windows installer only | https://nsis.sourceforge.io; in CI from Chocolatey, pinned version |

LMW-Synth itself: GNU AGPL-3.0-or-later (LICENSE). The source of every release is at
https://github.com/maarten-boot/software-synth-LMW.
