#include <shine/benchmark/memory_profiler.hpp>

#include <Windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <stdexcept>
#include <algorithm>

namespace shine::benchmark {

MemorySnapshot MemoryProfiler::CaptureSelf() {
    return CaptureProcessTree(GetCurrentProcessId());
}

std::vector<uint32_t> MemoryProfiler::FindChildProcesses(uint32_t parent_pid) {
    std::vector<uint32_t> children;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return children;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        return children;
    }

    do {
        if (entry.th32ParentProcessID == parent_pid) {
            children.push_back(entry.th32ProcessID);

            auto grandchildren = FindChildProcesses(entry.th32ProcessID);
            children.insert(children.end(), grandchildren.begin(), grandchildren.end());
        }
    } while (Process32NextW(snapshot, &entry));

    CloseHandle(snapshot);
    return children;
}

MemorySnapshot MemoryProfiler::CaptureProcess(uint32_t pid) {
    HANDLE process = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE,
        pid
    );

    if (!process) {
        return {};
    }

    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(PROCESS_MEMORY_COUNTERS_EX);

    MemorySnapshot snapshot{};

    if (GetProcessMemoryInfo(process, reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&pmc), sizeof(pmc))) {
        snapshot.private_bytes = pmc.PrivateUsage;
        snapshot.working_set_bytes = pmc.WorkingSetSize;
        snapshot.process_count = 1;
    }

    CloseHandle(process);
    return snapshot;
}

MemorySnapshot MemoryProfiler::CaptureProcessTree(uint32_t root_pid) {
    MemorySnapshot total = CaptureProcess(root_pid);

    auto children = FindChildProcesses(root_pid);

    for (uint32_t child_pid : children) {
        MemorySnapshot child = CaptureProcess(child_pid);
        if (child.process_count > 0) {
            total.private_bytes += child.private_bytes;
            total.working_set_bytes += child.working_set_bytes;
            total.process_count += child.process_count;
        }
    }

    return total;
}

}
