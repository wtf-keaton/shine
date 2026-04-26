#pragma once

namespace shine {
    class App;

    class Component {
    public:
        virtual ~Component() = default;

        virtual void Init(App& app) = 0;
    };
}