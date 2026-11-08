# SPDX-License-Identifier: AGPL-3.0-or-later
# The one place that pins JUCE (Spec S2.3). The CI cache key is the hash of this file, so
# changing the version here also refreshes the cached archive.
set(LMW_JUCE_VERSION "9.0.5")
set(LMW_JUCE_COMMIT  "d60b7a78030661574a615b586d9325ba7a7fa7dd")
set(LMW_JUCE_SHA256  "8237b2976871982eb9107fd67b576d95bf02355d99540a0127c4e7c001ecd9f5")
# A shallow git clone cannot fetch a bare commit hash, so the commit's source archive is used.
set(LMW_JUCE_URL     "https://github.com/juce-framework/JUCE/archive/${LMW_JUCE_COMMIT}.tar.gz")
