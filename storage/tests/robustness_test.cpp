#include <cstdint>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "sql/schema.h"
#include "storage/database.h"
#include "storage/table.h"

using namespace cw_db;

namespace {

void write_header(std::ofstream& out, const char magic[4], std::uint16_t version) {
    out.write(magic, 4);
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
}

} // namespace

TEST(RobustnessTest, TableLoadRejectsBadMagic) {
    auto path = std::filesystem::temp_directory_path() / "table_bad_magic.dat";
    {
        std::ofstream out(path, std::ios::binary);
        const char magic[4] = {'B', 'A', 'D', '!'};
        write_header(out, magic, 2);
    }

    EXPECT_THROW((void)Table::load(path.string()), std::runtime_error);
    std::filesystem::remove(path);
}

TEST(RobustnessTest, TableLoadRejectsUnsupportedVersion) {
    auto path = std::filesystem::temp_directory_path() / "table_bad_version.dat";
    {
        std::ofstream out(path, std::ios::binary);
        const char magic[4] = {'C', 'W', 'D', 'B'};
        write_header(out, magic, 0);
    }

    EXPECT_THROW((void)Table::load(path.string()), std::runtime_error);
    std::filesystem::remove(path);
}

TEST(RobustnessTest, TableRejectsTooManyValues) {
    TableSchema schema({
        {"id", DataType::Int, true, false, std::nullopt}
    });

    Table table(schema);
    EXPECT_THROW(table.insert_row({Value::of_int(1), Value::of_int(2)}), std::invalid_argument);
}

TEST(RobustnessTest, DatabaseLoadRejectsCorruptTableFile) {
    auto dir = std::filesystem::temp_directory_path() / "db_bad_file_dir";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    {
        std::ofstream out(dir / "broken.dat", std::ios::binary);
        const char magic[4] = {'C', 'W', 'D', 'B'};
        write_header(out, magic, 2);
        std::uint32_t pool_count = 1;
        out.write(reinterpret_cast<const char*>(&pool_count), sizeof(pool_count));
    }

    Database db("testdb");
    EXPECT_THROW(db.load_from(dir), std::runtime_error);

    std::filesystem::remove_all(dir);
}
