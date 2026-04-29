#pragma once

#include <shine/component.hpp>

namespace shine::components {
    class Window final : public Component {
    public:
        void Init(App& app) override;
    };
}