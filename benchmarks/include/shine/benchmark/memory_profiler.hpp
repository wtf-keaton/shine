#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace shine::benchmark {

struct MemorySnapshot {
    uint64_t private_bytes = 0;
    uint64_t working_set_bytes = 0;
    uint64_t process_count = 0;

    double private_bytes_mb() const {
        return static_cast<double>(private_bytes) / (1024.0 * 1024.0);
    }

    double working_set_mb() const {
        return static_cast<double>(working_set_bytes) / (1024.0 * 1024.0);
    }
};

struct BenchmarkResult {
    std::string name;
    MemorySnapshot memory;
    double startup_time_ms = 0.0;
    double total_time_ms = 0.0;
    std::string status = "ok";
    std::string error;
};

class MemoryProfiler {
public:
    static MemorySnapshot CaptureSelf();

    static MemorySnapshot CaptureProcessTree(uint32_t root_pid);

    static std::vector<uint32_t> FindChildProcesses(uint32_t parent_pid);

    static MemorySnapshot CaptureProcess(uint32_t pid);
};

}
