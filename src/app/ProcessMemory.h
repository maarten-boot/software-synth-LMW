// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.28: the resident memory of this process in MB (0 when it cannot be read). Not for the audio thread.
#pragma once

namespace lmw::app {
double residentMemoryMB();
}
