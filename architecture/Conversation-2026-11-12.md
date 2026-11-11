# Conversation 2026-11-12

## Validators in CI, continued
CI run 42914389824 for 082c6f7: Ubuntu, macOS and Fedora pass (Fedora fixed by dropping pluginval's -static-libstdc++). Windows: the forced include -FIchrono is applied by Visual Studio to C files too, so JUCE's juceaide failed (STL1003 on juce_graphics_Sheenbidi.c). Fix: the script adds `#include <chrono>` to the three pluginval sources that use ms literals, on all platforms. Local rebuild passes.
