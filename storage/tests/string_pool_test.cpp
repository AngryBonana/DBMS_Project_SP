#include <gtest/gtest.h>
#include "core/string_pool.h"

using namespace cw_db;

TEST(StringPoolTest, InternAndGet) {
    auto& pool = StringPool::instance();
    auto id1 = pool.intern("abc");
    auto id2 = pool.intern(std::string("abc"));
    EXPECT_EQ(id1, id2);
    EXPECT_EQ(pool.get(id1), std::string("abc"));
}

TEST(StringPoolTest, InvalidIdThrows) {
    auto& pool = StringPool::instance();
    EXPECT_THROW(pool.get(StringPool::kInvalid), std::runtime_error);
}
