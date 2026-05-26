#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include "sql/schema.h"
#include "storage/table.h"

namespace cw_db {

// Класс Database: коллекция таблиц и персистентность на уровне каталога
class Database {
public:
    Database() = default;
    explicit Database(std::string name) : name_(std::move(name)) {}

    const std::string& name() const noexcept { return name_; }

    bool has_table(const std::string& name) const {
        return tables_.find(name) != tables_.end();
    }

    // Возвращает указатель на таблицу или nullptr если отсутствует
    Table* get_table(const std::string& name) {
        auto it = tables_.find(name);
        return it == tables_.end() ? nullptr : it->second.get();
    }

    // Бросает исключение если таблицы нет
    Table& require_table(const std::string& name) {
        Table* t = get_table(name);
        if (!t) throw std::runtime_error("Unknown table '" + name + "'");
        return *t;
    }

    // Создать таблицу с заданной схемой; бросает если уже есть
    Table& create_table(const std::string& name, const TableSchema& schema) {
        if (has_table(name)) throw std::runtime_error("Table '" + name + "' already exists");
        auto ptr = std::make_unique<Table>(name, schema);
        Table& ref = *ptr;
        tables_.emplace(name, std::move(ptr));
        return ref;
    }

    // Удалить таблицу (если нет — бросить)
    void drop_table(const std::string& name) {
        if (!tables_.erase(name)) throw std::runtime_error("Unknown table '" + name + "'");
    }

    const std::unordered_map<std::string, std::unique_ptr<Table>>& tables() const noexcept { return tables_; }

    // Сохранить все таблицы в каталог dir (каждая таблица в name.dat)
    void save_to(const std::filesystem::path& dir) const;

    // Загрузить все .dat файлы из каталога в текущую Database (заменяет текущее содержимое)
    void load_from(const std::filesystem::path& dir);

private:
    std::string name_;
    std::unordered_map<std::string, std::unique_ptr<Table>> tables_;
};

} // namespace cw_db
