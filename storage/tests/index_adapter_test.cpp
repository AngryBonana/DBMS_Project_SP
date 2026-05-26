#include <filesystem>

#include <gtest/gtest.h>

#include "index/bstarplus_adapter.h"

using namespace cw_db;

TEST(IndexAdapterTest, IntRoundtripThroughPartnerBTree) {
    auto path = std::filesystem::temp_directory_path() / "storage_index_adapter_test.idx";
    std::filesystem::remove(path);

    BStarPlusIndexAdapter adapter(path, db::IndexKeyKind::Int64);

    adapter.insert(Value::of_int(10), 42);
    EXPECT_EQ(adapter.find(Value::of_int(10)), 42u);
    EXPECT_EQ(adapter.range_search(Value::of_int(1), Value::of_int(20)).size(), 1u);
    EXPECT_TRUE(adapter.erase(Value::of_int(10)));
    EXPECT_EQ(adapter.find(Value::of_int(10)), std::nullopt);

    std::filesystem::remove(path);
}
