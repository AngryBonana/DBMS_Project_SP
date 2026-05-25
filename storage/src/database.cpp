#include "storage/database.h"
#include <filesystem>
#include <system_error>

namespace cw_db {

void Database::save_to(const std::filesystem::path& dir) const {
    std::filesystem::create_directories(dir);
    std::error_code ec;
    // Удаляем .dat файлы таблиц, которых больше нет
    for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".dat") continue;
        std::string stem = entry.path().stem().string();
        if (tables_.find(stem) == tables_.end())
            std::filesystem::remove(entry.path(), ec);
    }

    for (const auto& [tname, table] : tables_) {
        auto p = dir / (tname + ".dat");
        table->save(p.string());
    }
}

void Database::load_from(const std::filesystem::path& dir) {
    tables_.clear();
    if (!std::filesystem::exists(dir)) return;
    for (auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".dat") continue;
        std::string tname = entry.path().stem().string();
        Table t = Table::load(entry.path().string());
        auto ptr = std::make_unique<Table>(t);
        tables_.emplace(tname, std::move(ptr));
    }
}

} // namespace cw_db
