#include <shine/benchmark/memory_profiler.hpp>
#include <shine/engine/window.hpp>

#include <Windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <shlwapi.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <chrono>
#include <atomic>
#include <thread>
#include <sstream>

using namespace Microsoft::WRL;

namespace shine::benchmark {

struct WebViewInstance {
    std::unique_ptr<engine::Window> window;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
};

static std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), &result[0], size);
    return result;
}

static void PumpMessageLoop(int max_iterations = 100) {
    for (int i = 0; i < max_iterations; ++i) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

static WebViewInstance CreateHiddenWindow(const std::string& title, uint32_t width = 800, uint32_t height = 600) {
    engine::WindowConfig config;
    config.title = title;
    config.width = width;
    config.height = height;
    config.resizable = false;
    config.frameless = true;

    auto window = std::make_unique<engine::Window>(config);

    auto tempDir = std::filesystem::temp_directory_path() / "Shine_Bench_WebView_Data";
    std::wstring userDataFolder = tempDir.wstring();

    std::atomic<bool> env_ready{false};
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;

    HWND hwnd = static_cast<HWND>(window->GetNativeHandle());

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        userDataFolder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [&controller, &webview, &env_ready, hwnd](HRESULT res, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(res)) return res;

                env->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [&controller, &webview, &env_ready](HRESULT res, ICoreWebView2Controller* ctrl) -> HRESULT {
                            if (FAILED(res)) return res;

                            controller = ctrl;
                            controller->get_CoreWebView2(&webview);

                            RECT bounds = {0, 0, 0, 0};
                            controller->put_Bounds(bounds);

                            env_ready.store(true);
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    while (!env_ready.load()) {
        PumpMessageLoop(1);
    }

    return {std::move(window), controller, webview};
}

static BenchmarkResult RunIdleBenchmark() {
    BenchmarkResult result;
    result.name = "Idle (blank page)";

    auto start_total = std::chrono::high_resolution_clock::now();

    WebViewInstance instance;
    try {
        instance = CreateHiddenWindow("Shine Idle Benchmark");
    } catch (const std::exception& e) {
        result.status = "failed";
        result.error = e.what();
        return result;
    }

    std::atomic<bool> navigation_done{false};
    auto nav_start = std::chrono::high_resolution_clock::now();

    EventRegistrationToken nav_token{};
    instance.webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [&nav_start, &navigation_done](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                nav_start = std::chrono::high_resolution_clock::now();
                navigation_done.store(true);
                return S_OK;
            }).Get(),
        &nav_token);

    instance.webview->Navigate(L"about:blank");

    while (!navigation_done.load()) {
        PumpMessageLoop(1);
    }

    auto nav_end = std::chrono::high_resolution_clock::now();
    result.startup_time_ms = std::chrono::duration<double, std::milli>(nav_end - nav_start).count();

    std::this_thread::sleep_for(std::chrono::seconds(3));

    result.memory = MemoryProfiler::CaptureSelf();

    auto end_total = std::chrono::high_resolution_clock::now();
    result.total_time_ms = std::chrono::duration<double, std::milli>(end_total - start_total).count();

    instance.webview->remove_NavigationCompleted(nav_token);

    return result;
}

static BenchmarkResult RunHeavyDomBenchmark() {
    BenchmarkResult result;
    result.name = "Heavy DOM (10k elements)";

    auto start_total = std::chrono::high_resolution_clock::now();

    WebViewInstance instance;
    try {
        instance = CreateHiddenWindow("Shine Heavy DOM Benchmark");
    } catch (const std::exception& e) {
        result.status = "failed";
        result.error = e.what();
        return result;
    }

    std::atomic<bool> navigation_done{false};
    auto nav_start = std::chrono::high_resolution_clock::now();

    EventRegistrationToken nav_token{};
    instance.webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [&nav_start, &navigation_done](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                nav_start = std::chrono::high_resolution_clock::now();
                navigation_done.store(true);
                return S_OK;
            }).Get(),
        &nav_token);

    std::string html = R"(<!DOCTYPE html>
<html>
<head><style>
table{border-collapse:collapse;width:100%}td,th{border:1px solid #ccc;padding:4px;font-size:11px}
th{background:#f0f0f0}tr:nth-child(even){background:#fafafa}
</style></head>
<body>
<table><thead><tr><th>ID</th><th>Name</th><th>Email</th><th>Status</th></tr></thead><tbody>
)";

    for (int i = 0; i < 10000; ++i) {
        html += "<tr><td>" + std::to_string(i) + "</td><td>User " + std::to_string(i) +
                "</td><td>user" + std::to_string(i) + "@bench.test</td><td>active</td></tr>\n";
    }

    html += "</tbody></table></body></html>";

    std::wstring wide_html = Utf8ToWide(html);
    instance.webview->NavigateToString(wide_html.c_str());

    while (!navigation_done.load()) {
        PumpMessageLoop(1);
    }

    auto nav_end = std::chrono::high_resolution_clock::now();
    result.startup_time_ms = std::chrono::duration<double, std::milli>(nav_end - nav_start).count();

    std::this_thread::sleep_for(std::chrono::seconds(3));

    result.memory = MemoryProfiler::CaptureSelf();

    auto end_total = std::chrono::high_resolution_clock::now();
    result.total_time_ms = std::chrono::duration<double, std::milli>(end_total - start_total).count();

    instance.webview->remove_NavigationCompleted(nav_token);

    return result;
}

static BenchmarkResult RunMultiWindowBenchmark() {
    BenchmarkResult result;
    result.name = "Multi-Window (5 instances)";

    auto start_total = std::chrono::high_resolution_clock::now();

    constexpr int kWindowCount = 5;
    std::vector<WebViewInstance> instances;
    instances.reserve(kWindowCount);

    std::atomic<int> loaded_count{0};
    auto nav_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < kWindowCount; ++i) {
        WebViewInstance instance;
        try {
            instance = CreateHiddenWindow("Shine Multi-Window " + std::to_string(i));
        } catch (const std::exception& e) {
            result.status = "failed";
            result.error = std::string("Window ") + std::to_string(i) + ": " + e.what();
            return result;
        }

        EventRegistrationToken nav_token{};
        instance.webview->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>(
                [&loaded_count](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                    loaded_count.fetch_add(1);
                    return S_OK;
                }).Get(),
            &nav_token);

        instance.webview->Navigate(L"about:blank");
        instances.push_back(std::move(instance));
    }

    while (loaded_count.load() < kWindowCount) {
        PumpMessageLoop(1);
    }

    auto nav_end = std::chrono::high_resolution_clock::now();
    result.startup_time_ms = std::chrono::duration<double, std::milli>(nav_end - nav_start).count();

    std::this_thread::sleep_for(std::chrono::seconds(3));

    result.memory = MemoryProfiler::CaptureSelf();

    auto end_total = std::chrono::high_resolution_clock::now();
    result.total_time_ms = std::chrono::duration<double, std::milli>(end_total - start_total).count();

    return result;
}

static void PrintSeparator() {
    std::cout << std::string(72, '-') << "\n";
}

static void PrintResults(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n";
    PrintSeparator();
    std::cout << "  SHINE FRAMEWORK - BENCHMARK RESULTS\n";
    PrintSeparator();
    std::cout << std::left;

    for (const auto& r : results) {
        std::cout << "\n  [" << (r.status == "ok" ? "PASS" : "FAIL") << "] " << r.name << "\n";

        if (r.status != "ok") {
            std::cout << "    Error: " << r.error << "\n";
            continue;
        }

        std::cout << std::fixed << std::setprecision(2);

        std::cout << "    Memory:\n";
        std::cout << "      Private Bytes:  " << std::setw(10) << r.memory.private_bytes_mb() << " MB\n";
        std::cout << "      Working Set:    " << std::setw(10) << r.memory.working_set_mb() << " MB\n";
        std::cout << "      Process Count:  " << std::setw(10) << r.memory.process_count << "\n";

        std::cout << "    Timing:\n";
        std::cout << "      Startup Time:   " << std::setw(10) << r.startup_time_ms << " ms\n";
        std::cout << "      Total Time:     " << std::setw(10) << r.total_time_ms << " ms\n";
    }

    PrintSeparator();
    std::cout << "\n";
}

static void PrintResultsJson(const std::vector<BenchmarkResult>& results) {
    std::cout << "{\n  \"benchmarks\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        std::cout << "    {\n";
        std::cout << "      \"name\": \"" << r.name << "\",\n";
        std::cout << "      \"status\": \"" << r.status << "\",\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "      \"private_bytes_mb\": " << r.memory.private_bytes_mb() << ",\n";
        std::cout << "      \"working_set_mb\": " << r.memory.working_set_mb() << ",\n";
        std::cout << "      \"process_count\": " << r.memory.process_count << ",\n";
        std::cout << "      \"startup_time_ms\": " << r.startup_time_ms << ",\n";
        std::cout << "      \"total_time_ms\": " << r.total_time_ms << "\n";
        std::cout << "    }";
        if (i < results.size() - 1) std::cout << ",";
        std::cout << "\n";
    }

    std::cout << "  ]\n}\n";
}

}

int main() {
    using namespace shine::benchmark;

    std::cout << "Shine Benchmark Suite v0.1.0\n";
    std::cout << "Measuring total memory footprint (host + all WebView2 child processes)\n\n";

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM: 0x" << std::hex << hr << "\n";
        return 1;
    }

    bool json_output = false;
    for (int i = 1; i < __argc; ++i) {
        std::string arg(__argv[i]);
        if (arg == "--json") json_output = true;
    }

    std::vector<BenchmarkResult> results;

    results.push_back(RunIdleBenchmark());
    results.push_back(RunHeavyDomBenchmark());
    results.push_back(RunMultiWindowBenchmark());

    if (json_output) {
        PrintResultsJson(results);
    } else {
        PrintResults(results);
    }

    CoUninitialize();

    int exit_code = 0;
    for (const auto& r : results) {
        if (r.status != "ok") {
            exit_code = 1;
            break;
        }
    }

    return exit_code;
}
