#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>

#include "storage/table.h"

using namespace cw_db;

namespace {

class MockIndex : public IIndex {
public:
    MOCK_METHOD(void, insert, (const Value& key, RowId row_id), (override));
    MOCK_METHOD(void, erase, (const Value& key, RowId row_id), (override));
    MOCK_METHOD(std::vector<RowId>, find, (const Value& key), (const, override));
    MOCK_METHOD(void, clear, (), (override));
};

} // namespace

TEST(IndexHookTest, AttachRebuildsAndTracksInserts) {
    TableSchema schema({
        {"id", DataType::Int, true, true, std::nullopt},
        {"name", DataType::Str, false, false, std::nullopt}
    });

    Table table(schema);
    table.insert_row({Value::of_int(1), Value::of_str("one")});

    auto index = std::make_shared<MockIndex>();
    EXPECT_CALL(*index, insert(::testing::Truly([](const Value& v) { return v.is_int() && v.as_int() == 1; }), 0));
    table.attach_index(0, index, true);

    EXPECT_CALL(*index, insert(::testing::Truly([](const Value& v) { return v.is_int() && v.as_int() == 2; }), 1));
    table.insert_row({Value::of_int(2), Value::of_str("two")});

    EXPECT_TRUE(table.has_index(0));
    table.detach_index(0);
    EXPECT_FALSE(table.has_index(0));
}
