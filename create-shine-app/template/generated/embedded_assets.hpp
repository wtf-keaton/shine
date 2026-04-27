#pragma once

// Placeholder file.
// This will be overwritten by:
//   npm run shine build

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace shine_app::embedded_assets {
  struct AssetEntry {
    const char* path;
    const char* mime;
    const std::uint8_t* data;
    std::size_t size;
  };

  inline std::optional<AssetEntry> Find(std::string_view) { return std::nullopt; }
}

