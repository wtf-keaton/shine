#include <shine/app.hpp>
#include <shine/components/fs.hpp>
#include <iostream>
#include <Windows.h>

int main() {
    std::cout << "Starting Shine Application" << std::endl;

    try {
        shine::App app;

        app.RegisterComponent<shine::components::FileSystem>();

        return app.Run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal framework error: " << e.what() << std::endl;
        return 1;
    }
}
