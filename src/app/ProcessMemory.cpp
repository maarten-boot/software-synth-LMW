// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.28: the resident memory of this process (own file: windows.h stays out of the JUCE code).
#include "ProcessMemory.h"
#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <psapi.h>
#elif defined(__APPLE__)
  #include <mach/mach.h>
#else
  #include <cstdio>
  #include <unistd.h>
#endif

namespace lmw::app {

double residentMemoryMB() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    return 0.0;
#elif defined(__APPLE__)
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
        return static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
    return 0.0;
#else
    long pages = 0, resident = 0;
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (f == nullptr) return 0.0;
    const int got = std::fscanf(f, "%ld %ld", &pages, &resident);
    std::fclose(f);
    if (got != 2) return 0.0;
    return static_cast<double>(resident) * static_cast<double>(sysconf(_SC_PAGESIZE)) / (1024.0 * 1024.0);
#endif
}

} // namespace lmw::app
