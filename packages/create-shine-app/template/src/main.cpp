#include <shine/command.hpp>
#include <shine/components/fs.hpp>
#include <shine/components/windows.hpp>
#include <shine_generated_app.hpp>

#include <iostream>

SHINE_COMMAND(greet) {
    std::string name_str = args["name"];

    return std::format("Hello, {}", name_str);
}

int main() {
    try {
        auto app = shine::CreateApp();

        app.RegisterComponent<shine::components::FileSystem>();
        app.RegisterComponent<shine::components::Window>();

        app.GetRouter().AddHandlers({SHINE_HANDLER(greet)});

        return app.Run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal framework error: " << e.what() << std::endl;
        return 1;
    }
}
