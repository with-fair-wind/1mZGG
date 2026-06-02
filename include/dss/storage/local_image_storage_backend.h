#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <utility>

#include "dss/storage/i_storage_backend.h"

namespace Dss::Storage {

class LocalImageStorageBackend final : public IStorageBackend {
public:
    explicit LocalImageStorageBackend(std::filesystem::path baseDir)
        : m_baseDir(std::move(baseDir)) {}

    auto init(const std::filesystem::path& baseDir) -> std::expected<void, std::string> override {
        m_baseDir = baseDir;
        m_ready = true;
        return {};
    }

    [[nodiscard]] bool isReady() const override {
        return m_ready;
    }

    [[nodiscard]] auto baseDir() const -> const std::filesystem::path& {
        return m_baseDir;
    }

private:
    std::filesystem::path m_baseDir;
    bool m_ready = false;
};

}  // namespace Dss::Storage
