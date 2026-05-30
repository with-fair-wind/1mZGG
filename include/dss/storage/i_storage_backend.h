#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>

namespace Dss::Storage
{

class IStorageBackend
{
public:
    virtual ~IStorageBackend() = default;

    virtual auto init(const std::filesystem::path& baseDir) -> std::expected<void, std::string> = 0;
    [[nodiscard]] virtual bool isReady() const = 0;
};

} // namespace Dss::Storage
