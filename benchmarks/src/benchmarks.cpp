#include <shine/benchmark/memory_profiler.hpp>
#include <shine/engine/window.hpp>

#include <benchmark/benchmark.h>
#include <Windows.h>
#include <WebView2.h>
#include <wrl.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace Microsoft::WRL;

namespace {

constexpr auto kWebViewReadyTimeout = std::chrono::seconds(30);
constexpr auto kNavigationTimeout = std::chrono::seconds(30);
constexpr auto kProcessSettleTimeout = std::chrono::seconds(5);
constexpr auto kMemoryStabilizationDelay = std::chrono::seconds(2);

struct WebViewInstance {
    std::unique_ptr<shine::engine::Window> window;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
};

std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};

    const int size = MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), size);
    return result;
}

std::string HresultMessage(HRESULT hr) {
    char buffer[32]{};
    sprintf_s(buffer, "0x%08lX", static_cast<unsigned long>(hr));
    return buffer;
}

void PumpMessageLoopOnce() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

template <typename Predicate>
bool WaitUntil(Predicate predicate, std::chrono::steady_clock::duration timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        PumpMessageLoopOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    PumpMessageLoopOnce();
    return true;
}

void ApplyBenchmarkWebViewSettings(ICoreWebView2* webview) {
    ComPtr<ICoreWebView2Settings> settings;
    if (FAILED(webview->get_Settings(&settings)) || !settings) return;

    settings->put_AreDevToolsEnabled(FALSE);
    settings->put_AreDefaultScriptDialogsEnabled(FALSE);
    settings->put_IsStatusBarEnabled(FALSE);
    settings->put_AreDefaultContextMenusEnabled(FALSE);
    settings->put_IsBuiltInErrorPageEnabled(FALSE);

    ComPtr<ICoreWebView2Settings3> settings3;
    if (SUCCEEDED(settings.As(&settings3))) {
        settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
    }

    ComPtr<ICoreWebView2Settings4> settings4;
    if (SUCCEEDED(settings.As(&settings4))) {
        settings4->put_IsPasswordAutosaveEnabled(FALSE);
        settings4->put_IsGeneralAutofillEnabled(FALSE);
    }

    ComPtr<ICoreWebView2Settings5> settings5;
    if (SUCCEEDED(settings.As(&settings5))) {
        settings5->put_IsPinchZoomEnabled(FALSE);
    }

    ComPtr<ICoreWebView2Settings6> settings6;
    if (SUCCEEDED(settings.As(&settings6))) {
        settings6->put_IsSwipeNavigationEnabled(FALSE);
    }
}

WebViewInstance CreateHiddenWebView(const std::string& title, std::string_view scenario_name) {
    shine::engine::WindowConfig config;
    config.title = title;
    config.width = 800;
    config.height = 600;
    config.resizable = false;
    config.frameless = true;

    auto window = std::make_unique<shine::engine::Window>(config);
    auto user_data_dir = std::filesystem::temp_directory_path() / "Shine_Bench_WebView_Data" / scenario_name;
    const std::wstring user_data_folder = user_data_dir.wstring();
    const HWND hwnd = static_cast<HWND>(window->GetNativeHandle());

    SetWindowPos(
        hwnd,
        nullptr,
        -32000,
        -32000,
        static_cast<int>(config.width),
        static_cast<int>(config.height),
        SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(hwnd, SW_SHOWNA);

    std::atomic<bool> ready{false};
    HRESULT async_result = S_OK;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;

    const HRESULT create_result = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        user_data_folder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [&ready, &async_result, &controller, &webview, hwnd](
                HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                async_result = result;
                if (FAILED(result)) {
                    ready.store(true);
                    return result;
                }

                environment->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [&ready, &async_result, &controller, &webview](
                            HRESULT result, ICoreWebView2Controller* new_controller) -> HRESULT {
                            async_result = result;
                            if (SUCCEEDED(result) && new_controller) {
                                controller = new_controller;
                                controller->get_CoreWebView2(&webview);
                                if (webview) {
                                    ApplyBenchmarkWebViewSettings(webview.Get());
                                }

                                RECT hidden_bounds{0, 0, 0, 0};
                                controller->put_Bounds(hidden_bounds);
                            }

                            ready.store(true);
                            return result;
                        })
                        .Get());
                return S_OK;
            })
            .Get());

    if (FAILED(create_result)) {
        throw std::runtime_error("CreateCoreWebView2EnvironmentWithOptions failed: " + HresultMessage(create_result));
    }

    if (!WaitUntil([&ready] { return ready.load(); }, kWebViewReadyTimeout)) {
        throw std::runtime_error("Timed out while creating WebView2 environment");
    }

    if (FAILED(async_result) || !controller || !webview) {
        throw std::runtime_error("WebView2 initialization failed: " + HresultMessage(async_result));
    }

    return {std::move(window), controller, webview};
}

std::string BuildHeavyDomHtml() {
    std::string html = R"(<!DOCTYPE html><html><head><meta charset="utf-8"><style>
body{margin:0;font:12px system-ui,sans-serif}
table{border-collapse:collapse;width:100%}
td,th{border:1px solid #ccc;padding:4px}
th{background:#f0f0f0}
tr:nth-child(even){background:#fafafa}
</style></head><body><table><thead><tr><th>ID</th><th>Name</th><th>Email</th><th>Status</th></tr></thead><tbody>)";

    for (int i = 0; i < 10000; ++i) {
        html += "<tr><td>" + std::to_string(i) + "</td><td>User " + std::to_string(i) +
            "</td><td>user" + std::to_string(i) + "@bench.test</td><td>active</td></tr>";
    }

    html += "</tbody></table></body></html>";
    return html;
}

shine::benchmark::MemorySnapshot Subtract(
    shine::benchmark::MemorySnapshot value,
    shine::benchmark::MemorySnapshot baseline) {
    shine::benchmark::MemorySnapshot result;
    result.private_bytes = value.private_bytes > baseline.private_bytes
        ? value.private_bytes - baseline.private_bytes
        : 0;
    result.working_set_bytes = value.working_set_bytes > baseline.working_set_bytes
        ? value.working_set_bytes - baseline.working_set_bytes
        : 0;
    result.process_count = value.process_count > baseline.process_count
        ? value.process_count - baseline.process_count
        : 0;
    return result;
}

void WaitForProcessSettle() {
    auto previous = shine::benchmark::MemoryProfiler::CaptureSelf();
    const auto deadline = std::chrono::steady_clock::now() + kProcessSettleTimeout;

    while (std::chrono::steady_clock::now() < deadline) {
        PumpMessageLoopOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto current = shine::benchmark::MemoryProfiler::CaptureSelf();
        if (current.process_count == previous.process_count) {
            return;
        }
        previous = current;
    }
}

double NavigateAndWait(ICoreWebView2* webview, const std::function<void()>& navigate) {
    std::atomic<bool> navigation_done{false};
    HRESULT navigation_result = S_OK;

    EventRegistrationToken nav_token{};
    const HRESULT add_result = webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [&navigation_done, &navigation_result](
                ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                BOOL success = FALSE;
                args->get_IsSuccess(&success);
                if (!success) navigation_result = E_FAIL;
                navigation_done.store(true);
                return S_OK;
            })
            .Get(),
        &nav_token);

    if (FAILED(add_result)) {
        throw std::runtime_error("Failed to register NavigationCompleted handler: " + HresultMessage(add_result));
    }

    const auto start = std::chrono::steady_clock::now();
    navigate();

    if (!WaitUntil([&navigation_done] { return navigation_done.load(); }, kNavigationTimeout)) {
        webview->remove_NavigationCompleted(nav_token);
        throw std::runtime_error("Timed out while waiting for navigation");
    }

    const auto end = std::chrono::steady_clock::now();
    webview->remove_NavigationCompleted(nav_token);

    if (FAILED(navigation_result)) {
        throw std::runtime_error("Navigation failed: " + HresultMessage(navigation_result));
    }

    return std::chrono::duration<double, std::milli>(end - start).count();
}

void AttachCounters(
    benchmark::State& state,
    const shine::benchmark::MemorySnapshot& delta,
    const shine::benchmark::MemorySnapshot& total,
    double startup_ms) {
    state.counters["Private_MB"] = delta.private_bytes_mb();
    state.counters["WorkingSet_MB"] = delta.working_set_mb();
    state.counters["Processes"] = static_cast<double>(delta.process_count);
    state.counters["Total_Private_MB"] = total.private_bytes_mb();
    state.counters["Total_WorkingSet_MB"] = total.working_set_mb();
    state.counters["Total_Processes"] = static_cast<double>(total.process_count);
    state.counters["Startup_ms"] = startup_ms;
}

void RunScenario(
    benchmark::State& state,
    const std::function<double(std::vector<WebViewInstance>&)>& scenario) {
    for (auto _ : state) {
        try {
            WaitForProcessSettle();
            const auto baseline = shine::benchmark::MemoryProfiler::CaptureSelf();

            std::vector<WebViewInstance> instances;
            const auto total_start = std::chrono::steady_clock::now();
            const double startup_ms = scenario(instances);

            std::this_thread::sleep_for(kMemoryStabilizationDelay);
            PumpMessageLoopOnce();

            const auto total_memory = shine::benchmark::MemoryProfiler::CaptureSelf();
            const auto delta_memory = Subtract(total_memory, baseline);
            const auto total_end = std::chrono::steady_clock::now();

            auto private_bytes = delta_memory.private_bytes;
            benchmark::DoNotOptimize(private_bytes);
            benchmark::ClobberMemory();

            AttachCounters(state, delta_memory, total_memory, startup_ms);
            state.SetIterationTime(std::chrono::duration<double>(total_end - total_start).count());
        } catch (const std::exception& error) {
            state.SkipWithError(error.what());
            return;
        }
    }
}

void BM_IdleMemory(benchmark::State& state) {
    RunScenario(state, [](std::vector<WebViewInstance>& instances) {
        instances.push_back(CreateHiddenWebView("Shine Idle Benchmark", "idle"));
        return NavigateAndWait(instances.back().webview.Get(), [&instances] {
            instances.back().webview->Navigate(L"about:blank");
        });
    });
}

void BM_HeavyDomMemory(benchmark::State& state) {
    RunScenario(state, [](std::vector<WebViewInstance>& instances) {
        instances.push_back(CreateHiddenWebView("Shine Heavy DOM Benchmark", "heavy-dom"));
        const std::wstring html = Utf8ToWide(BuildHeavyDomHtml());
        return NavigateAndWait(instances.back().webview.Get(), [&instances, &html] {
            instances.back().webview->NavigateToString(html.c_str());
        });
    });
}

void BM_MultiWindowMemory(benchmark::State& state) {
    constexpr int kWindowCount = 5;

    RunScenario(state, [](std::vector<WebViewInstance>& instances) {
        instances.reserve(kWindowCount);

        std::atomic<int> loaded_count{0};
        std::vector<EventRegistrationToken> nav_tokens;
        nav_tokens.reserve(kWindowCount);

        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < kWindowCount; ++i) {
            instances.push_back(CreateHiddenWebView("Shine Multi Benchmark " + std::to_string(i), "multi-window"));

            EventRegistrationToken token{};
            auto& webview = instances.back().webview;
            const HRESULT add_result = webview->add_NavigationCompleted(
                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                    [&loaded_count](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                    loaded_count.fetch_add(1);
                    return S_OK;
                })
                    .Get(),
                &token);

            if (FAILED(add_result)) {
                throw std::runtime_error("Failed to register NavigationCompleted handler: " +
                    HresultMessage(add_result));
            }

            nav_tokens.push_back(token);
            webview->Navigate(L"about:blank");
        }

        if (!WaitUntil([&loaded_count] { return loaded_count.load() >= kWindowCount; }, kNavigationTimeout)) {
            throw std::runtime_error("Timed out while waiting for multi-window navigation");
        }

        const auto end = std::chrono::steady_clock::now();

        for (size_t i = 0; i < instances.size(); ++i) {
            instances[i].webview->remove_NavigationCompleted(nav_tokens[i]);
        }

        return std::chrono::duration<double, std::milli>(end - start).count();
    });
}

}  // namespace

BENCHMARK(BM_IdleMemory)
    ->Name("Idle_Memory")
    ->Unit(benchmark::kMillisecond)
    ->UseManualTime()
    ->Iterations(1);
BENCHMARK(BM_HeavyDomMemory)
    ->Name("Heavy_DOM_Memory")
    ->Unit(benchmark::kMillisecond)
    ->UseManualTime()
    ->Iterations(1);
BENCHMARK(BM_MultiWindowMemory)
    ->Name("Multi_Window_Memory")
    ->Unit(benchmark::kMillisecond)
    ->UseManualTime()
    ->Iterations(1);

int main(int argc, char** argv) {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        fprintf(stderr, "Failed to initialize COM: %s\n", HresultMessage(hr).c_str());
        return 1;
    }

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        CoUninitialize();
        return 1;
    }

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    CoUninitialize();
    return 0;
}
