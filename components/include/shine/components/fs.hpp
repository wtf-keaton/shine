#pragma once

#include <shine/component.hpp>

namespace shine::components {
    class FileSystem final : public Component {
    public:
        void Init(App& app) override;
    };
}