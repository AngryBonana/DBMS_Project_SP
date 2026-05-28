#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/value.h"
#include "sql/schema.h"

namespace cw_db::test {

class TempDir {
public:
    explicit TempDir(std::string name)
        : path_(std::filesystem::temp_directory_path() / std::move(name)) {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TempDir() { std::filesystem::remove_all(path_); }

    const std::filesystem::path& path() const { return path_; }
    std::string string() const { return path_.string(); }

private:
    std::filesystem::path path_;
};

inline TableSchema users_schema() {
    return TableSchema({
        {"id", DataType::Int, true, false, std::nullopt},
        {"name", DataType::Str, false, false, std::optional<Value>(Value::of_str("anon"))},
        {"age", DataType::Int, false, false, std::optional<Value>(Value::of_int(18))},
    });
}

inline TableSchema id_name_schema(bool name_not_null = false) {
    return TableSchema({
        {"id", DataType::Int, true, false, std::nullopt},
        {"name", DataType::Str, name_not_null, false, std::nullopt},
    });
}

inline TableSchema indexed_id_schema() {
    return TableSchema({
        {"id", DataType::Int, true, true, std::nullopt},
        {"name", DataType::Str, false, false, std::nullopt},
    });
}

inline void write_table_header(std::ofstream& out, const char magic[4], std::uint16_t version) {
    out.write(magic, 4);
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
}

} // namespace cw_db::test
