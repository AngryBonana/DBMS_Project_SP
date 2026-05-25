#pragma once
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include "storage/database.h"

namespace cw_db {

class Dbms {
public:
    explicit Dbms(std::filesystem::path data_root);

    Database& create_database(const std::string& name);
    void drop_database(const std::string& name);
    void use_database(const std::string& name);

    bool has_database(const std::string& name) const;
    Database& require_database(const std::string& name);

    // Возвращает активную БД (выбранную через use_database) или бросает исключение
    Database& active_db();
    const std::string& active_name() const noexcept { return active_; }

    const std::unordered_map<std::string, std::unique_ptr<Database>>& databases() const noexcept {
        return databases_;
    }

    // Персистентность
    void save_all() const;
    void save_database(const std::string& name) const;
    void load_all();

    // Мьютекс для внешней синхронизации
    std::mutex& mutex() noexcept { return mu_; }

private:
    std::filesystem::path root_;
    std::unordered_map<std::string, std::unique_ptr<Database>> databases_;
    std::string active_;
    mutable std::mutex mu_;
};

} // namespace cw_db
