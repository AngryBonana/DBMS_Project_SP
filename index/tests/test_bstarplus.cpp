// tests/btree_test.cpp
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "bstarplus_tree_index.h"
#include "DiskManager.h"

using namespace db;

class BStarPlusIndexTest : public ::testing::Test {
protected:
    std::filesystem::path test_path;
    
    void SetUp() override {
        test_path = std::filesystem::temp_directory_path() / "test_btree.idx";
        std::filesystem::remove(test_path);
    }
    
    void TearDown() override {
        std::filesystem::remove(test_path);
    }
    
    BStarPlusIndex create_index(IndexKeyKind kind = IndexKeyKind::Int64) {
        return BStarPlusIndex(test_path, kind);
    }
};

// ==================== ВСТАВКА И ПОИСК ====================

TEST_F(BStarPlusIndexTest, InsertAndFind_Single) {
    auto index = create_index();
    index.insert(42, 100);
    
    auto result = index.find(42);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 100);
}

TEST_F(BStarPlusIndexTest, InsertAndFind_Multiple) {
    auto index = create_index();
    index.insert(10, 100);
    index.insert(20, 200);
    index.insert(30, 300);
    
    EXPECT_EQ(index.find(10).value(), 100);
    EXPECT_EQ(index.find(20).value(), 200);
    EXPECT_EQ(index.find(30).value(), 300);
}

TEST_F(BStarPlusIndexTest, Find_NotFound) {
    auto index = create_index();
    index.insert(10, 100);
    
    auto result = index.find(99);
    EXPECT_FALSE(result.has_value());
}

TEST_F(BStarPlusIndexTest, Find_Empty) {
    auto index = create_index();
    
    auto result = index.find(10);
    EXPECT_FALSE(result.has_value());
}

TEST_F(BStarPlusIndexTest, Insert_DuplicateKey) {
    auto index = create_index();
    index.insert(10, 100);
    
    EXPECT_THROW(index.insert(10, 200), std::invalid_argument);
}

// ==================== УДАЛЕНИЕ ====================

TEST_F(BStarPlusIndexTest, Erase_Existing) {
    auto index = create_index();
    index.insert(10, 100);
    
    bool erased = index.erase(10);
    EXPECT_TRUE(erased);
    EXPECT_FALSE(index.find(10).has_value());
}

TEST_F(BStarPlusIndexTest, Erase_NotFound) {
    auto index = create_index();
    index.insert(10, 100);
    
    bool erased = index.erase(99);
    EXPECT_FALSE(erased);
    EXPECT_EQ(index.find(10).value(), 100);
}

TEST_F(BStarPlusIndexTest, Erase_Empty) {
    auto index = create_index();
    
    bool erased = index.erase(10);
    EXPECT_FALSE(erased);
}

TEST_F(BStarPlusIndexTest, InsertEraseInsert_SameKey) {
    auto index = create_index();
    index.insert(10, 100);
    index.erase(10);
    index.insert(10, 200);
    
    EXPECT_EQ(index.find(10).value(), 200);
}

// ==================== RANGE SEARCH ====================

TEST_F(BStarPlusIndexTest, RangeSearch_Normal) {
    auto index = create_index();
    index.insert(10, 100);
    index.insert(20, 200);
    index.insert(30, 300);
    index.insert(40, 400);
    index.insert(50, 500);
    
    auto results = index.range_search(20, 40);
    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(results[0], 200);
    EXPECT_EQ(results[1], 300);
    EXPECT_EQ(results[2], 400);
}

TEST_F(BStarPlusIndexTest, RangeSearch_Single) {
    auto index = create_index();
    index.insert(10, 100);
    index.insert(20, 200);
    
    auto results = index.range_search(20, 20);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], 200);
}

TEST_F(BStarPlusIndexTest, RangeSearch_EmptyResult) {
    auto index = create_index();
    index.insert(10, 100);
    index.insert(20, 200);
    
    auto results = index.range_search(30, 40);
    EXPECT_TRUE(results.empty());
}

TEST_F(BStarPlusIndexTest, RangeSearch_InvalidRange) {
    auto index = create_index();
    index.insert(10, 100);
    
    auto results = index.range_search(50, 10);  // beg > end
    EXPECT_TRUE(results.empty());
}

TEST_F(BStarPlusIndexTest, RangeSearch_EmptyIndex) {
    auto index = create_index();
    
    auto results = index.range_search(10, 50);
    EXPECT_TRUE(results.empty());
}

// ==================== БОЛЬШИЕ ДАННЫЕ ====================

TEST_F(BStarPlusIndexTest, ManyInserts_Sequential) {
    auto index = create_index();
    const int N = 1000;
    
    for (int i = 0; i < N; ++i) {
        index.insert(i, i * 10);
    }
    
    for (int i = 0; i < N; ++i) {
        auto result = index.find(i);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), i * 10);
    }
}

TEST_F(BStarPlusIndexTest, ManyInserts_Reverse) {
    auto index = create_index();
    const int N = 500;
    
    for (int i = N - 1; i >= 0; --i) {
        index.insert(i, i * 10);
    }
    
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(index.find(i).value(), i * 10);
    }
}

TEST_F(BStarPlusIndexTest, ManyInserts_Random) {
    auto index = create_index();
    std::vector<int> keys = {42, 17, 99, 3, 55, 88, 1, 77, 33, 66,
                             100, 200, 150, 250, 50, 75, 12, 8, 63, 91};
    
    for (int key : keys) {
        index.insert(key, key * 10);
    }
    
    for (int key : keys) {
        EXPECT_EQ(index.find(key).value(), key * 10);
    }
}

// ==================== ПЕРСИСТЕНТНОСТЬ ====================

TEST_F(BStarPlusIndexTest, Persistence_DataSurvivesReopen) {
    // Создаём и наполняем
    {
        auto index = create_index();
        index.insert(1, 100);
        index.insert(2, 200);
        index.insert(3, 300);
    }  // index уничтожается, файл остаётся
    
    // Открываем заново — файл существует
    {
        BStarPlusIndex index(test_path);  // key_kind загрузится из файла
        EXPECT_EQ(index.find(1).value(), 100);
        EXPECT_EQ(index.find(2).value(), 200);
        EXPECT_EQ(index.find(3).value(), 300);
    }
}

TEST_F(BStarPlusIndexTest, Persistence_EmptyFile) {
    // Файл не существует
    BStarPlusIndex index(test_path, IndexKeyKind::Int64);
    EXPECT_FALSE(index.find(1).has_value());
}

// ==================== СТРОКОВЫЕ КЛЮЧИ ====================

TEST_F(BStarPlusIndexTest, StringKeys_InsertAndFind) {
    std::filesystem::remove(test_path);
    BStarPlusIndex index(test_path, IndexKeyKind::String);
    
    index.insert(std::string("alice"), 100);
    index.insert(std::string("bob"), 200);
    index.insert(std::string("charlie"), 300);
    
    EXPECT_EQ(index.find(std::string("alice")).value(), 100);
    EXPECT_EQ(index.find(std::string("bob")).value(), 200);
    EXPECT_FALSE(index.find(std::string("dave")).has_value());
}

TEST_F(BStarPlusIndexTest, StringKeys_RangeSearch) {
    std::filesystem::remove(test_path);
    BStarPlusIndex index(test_path, IndexKeyKind::String);
    
    index.insert(std::string("apple"), 10);
    index.insert(std::string("banana"), 20);
    index.insert(std::string("cherry"), 30);
    index.insert(std::string("date"), 40);
    
    auto results = index.range_search(std::string("apple"), std::string("cherry"));
    EXPECT_EQ(results.size(), 3);
}

// ==================== МЕТАДАННЫЕ ====================

TEST_F(BStarPlusIndexTest, Metadata_Empty) {
    auto index = create_index();
    const auto& meta = index.metadata();
    
    EXPECT_TRUE(meta.empty());
    EXPECT_EQ(meta.size, 0);
    EXPECT_EQ(meta.height, 0);
}

TEST_F(BStarPlusIndexTest, Metadata_AfterInsert) {
    auto index = create_index();
    index.insert(10, 100);
    
    const auto& meta = index.metadata();
    EXPECT_FALSE(meta.empty());
    EXPECT_EQ(meta.size, 1);
    EXPECT_GT(meta.height, 0);
}

// ==================== ГРАНИЧНЫЕ СЛУЧАИ ====================

TEST_F(BStarPlusIndexTest, LargeOffset) {
    auto index = create_index();
    size_t large_offset = UINT64_MAX - 1;
    
    index.insert(1, large_offset);
    EXPECT_EQ(index.find(1).value(), large_offset);
}

TEST_F(BStarPlusIndexTest, NegativeKeys) {
    auto index = create_index();
    index.insert(-100, 100);
    index.insert(-50, 200);
    index.insert(0, 300);
    index.insert(50, 400);
    
    EXPECT_EQ(index.find(-100).value(), 100);
    EXPECT_EQ(index.find(0).value(), 300);
    
    auto results = index.range_search(-75, 25);
    EXPECT_EQ(results.size(), 2);
}

TEST_F(BStarPlusIndexTest, MinMaxKeys) {
    auto index = create_index();
    index.insert(INT64_MIN, 1);
    index.insert(INT64_MAX, 2);
    index.insert(0, 3);
    
    EXPECT_EQ(index.find(INT64_MIN).value(), 1);
    EXPECT_EQ(index.find(INT64_MAX).value(), 2);
    EXPECT_EQ(index.find(0).value(), 3);
}

// ==================== СТРЕСС-ТЕСТ НА УДАЛЕНИЕ ====================

TEST_F(BStarPlusIndexTest, EraseMany_All) {
    auto index = create_index();
    const int N = 100;
    
    for (int i = 0; i < N; ++i) {
        index.insert(i, i * 10);
    }
    
    for (int i = 0; i < N; ++i) {
        EXPECT_TRUE(index.erase(i));
    }
    
    EXPECT_TRUE(index.metadata().empty());
    EXPECT_FALSE(index.find(0).has_value());
}

TEST_F(BStarPlusIndexTest, EraseMany_Alternating) {
    auto index = create_index();
    const int N = 100;
    
    for (int i = 0; i < N; ++i) {
        index.insert(i, i * 10);
    }
    
    // Удаляем каждый второй
    for (int i = 0; i < N; i += 2) {
        EXPECT_TRUE(index.erase(i));
    }
    
    // Проверяем оставшиеся
    for (int i = 0; i < N; ++i) {
        auto result = index.find(i);
        if (i % 2 == 0) {
            EXPECT_FALSE(result.has_value());
        } else {
            EXPECT_EQ(result.value(), i * 10);
        }
    }
}