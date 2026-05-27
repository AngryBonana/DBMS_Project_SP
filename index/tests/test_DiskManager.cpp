// tests/disk_manager_test.cpp
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>

#include "DiskManager.h"
#include "constants.h"

using namespace db;

class DiskPageManagerTest : public ::testing::Test {
protected:
    std::filesystem::path test_path;
    
    void SetUp() override {
        static uint64_t counter = 0;
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        test_path = std::filesystem::temp_directory_path() 
            / ("mini_db_test_" + std::to_string(stamp) + "_" + std::to_string(counter++) + ".bin");
        std::filesystem::remove(test_path);
    }
    
    void TearDown() override {
        std::filesystem::remove(test_path);
    }
    
    std::vector<char> patterned_page() {
        auto page = std::vector<char>(PAGE_SIZE);
        for (size_t i = 0; i < page.size(); ++i) {
            page[i] = static_cast<char>(i % 127);
        }
        return page;
    }
};

// ==================== БАЗОВЫЕ ОПЕРАЦИИ ====================

TEST_F(DiskPageManagerTest, CreatesEmptyFileAndAllocatesFirstPage) {
    DiskPageManager manager(test_path);
    
    EXPECT_EQ(manager.page_count(), 0);
    
    PageId page_id = manager.allocate_page();
    EXPECT_EQ(page_id, 0);
    EXPECT_EQ(manager.page_count(), 1);
    
    auto page = manager.read_page(0);
    EXPECT_EQ(page, std::vector<char>(PAGE_SIZE, 0));
}

TEST_F(DiskPageManagerTest, WriteThenReadSamePage) {
    DiskPageManager manager(test_path);
    
    PageId page_id = manager.allocate_page();
    auto expected = patterned_page();
    
    manager.write_page(page_id, expected);
    auto actual = manager.read_page(page_id);
    
    EXPECT_EQ(actual, expected);
}

TEST_F(DiskPageManagerTest, MultiplePagesReadWrite) {
    DiskPageManager manager(test_path);
    
    const int N = 10;
    std::vector<std::vector<char>> pages;
    
    for (int i = 0; i < N; ++i) {
        PageId page_id = manager.allocate_page();
        EXPECT_EQ(page_id, i);
        
        auto data = patterned_page();
        data[0] = static_cast<char>(i);  // помечаем страницу
        pages.push_back(data);
        manager.write_page(page_id, data);
    }
    
    EXPECT_EQ(manager.page_count(), N);
    
    for (int i = 0; i < N; ++i) {
        auto actual = manager.read_page(i);
        EXPECT_EQ(actual, pages[i]);
    }
}

// ==================== ПЕРСИСТЕНТНОСТЬ ====================

TEST_F(DiskPageManagerTest, PersistsAfterReopen) {
    auto expected = patterned_page();
    
    {
        DiskPageManager manager(test_path);
        PageId page_id = manager.allocate_page();
        manager.write_page(page_id, expected);
        manager.flush();
    }
    
    {
        DiskPageManager manager(test_path);
        EXPECT_EQ(manager.page_count(), 1);
        EXPECT_EQ(manager.read_page(0), expected);
    }
}

TEST_F(DiskPageManagerTest, PersistsMultiplePages) {
    const int N = 5;
    std::vector<std::vector<char>> pages;
    
    {
        DiskPageManager manager(test_path);
        for (int i = 0; i < N; ++i) {
            manager.allocate_page();
            auto data = patterned_page();
            data[0] = static_cast<char>(i);
            pages.push_back(data);
            manager.write_page(i, data);
        }
        manager.flush();
    }
    
    {
        DiskPageManager manager(test_path);
        EXPECT_EQ(manager.page_count(), N);
        for (int i = 0; i < N; ++i) {
            EXPECT_EQ(manager.read_page(i), pages[i]);
        }
    }
}

// ==================== ПРОВЕРКА ОШИБОК ====================

TEST_F(DiskPageManagerTest, RejectsInvalidPageIds) {
    DiskPageManager manager(test_path);
    manager.allocate_page();
    
    auto valid_page = std::vector<char>(PAGE_SIZE, 0);
    
    EXPECT_THROW(manager.read_page(INVALID_PAGE_ID), std::out_of_range);
    EXPECT_THROW(manager.read_page(manager.page_count()), std::out_of_range);
    EXPECT_THROW(manager.write_page(manager.page_count(), valid_page), std::out_of_range);
}

TEST_F(DiskPageManagerTest, RejectsWrongPageSize) {
    DiskPageManager manager(test_path);
    manager.allocate_page();
    
    EXPECT_THROW(
        manager.write_page(0, std::vector<char>(PAGE_SIZE - 1)),
        std::invalid_argument
    );
    EXPECT_THROW(
        manager.write_page(0, std::vector<char>(PAGE_SIZE + 1)),
        std::invalid_argument
    );
}

TEST_F(DiskPageManagerTest, DetectsCorruptedFileSize) {
    // Создаём файл неправильного размера
    {
        std::ofstream file(test_path, std::ios::binary | std::ios::trunc);
        auto data = std::vector<char>(PAGE_SIZE + 1, 0);
        file.write(data.data(), data.size());
    }
    
    EXPECT_THROW(
        DiskPageManager manager(test_path),
        std::runtime_error
    );
}

TEST_F(DiskPageManagerTest, DetectsFileTooSmall) {
    {
        std::ofstream file(test_path, std::ios::binary | std::ios::trunc);
        auto data = std::vector<char>(PAGE_SIZE / 2, 0);
        file.write(data.data(), data.size());
    }
    
    EXPECT_THROW(
        DiskPageManager manager(test_path),
        std::runtime_error
    );
}

// ==================== МЕТАДАННЫЕ ====================

TEST_F(DiskPageManagerTest, WriteAndReadMetadata) {
    DiskPageManager manager(test_path);
    
    BStarPlusIndexMetadata meta;
    meta.root_page_id = 42;
    meta.first_leaf_page_id = 10;
    meta.height = 3;
    meta.size = 1000;
    meta.key_kind = IndexKeyKind::Int64;
    
    manager.write_metadata(meta);
    manager.flush();
    
    auto loaded = manager.read_metadata();
    EXPECT_EQ(loaded.root_page_id, 42);
    EXPECT_EQ(loaded.first_leaf_page_id, 10);
    EXPECT_EQ(loaded.height, 3);
    EXPECT_EQ(loaded.size, 1000);
    EXPECT_EQ(loaded.key_kind, IndexKeyKind::Int64);
}

TEST_F(DiskPageManagerTest, DefaultMetadataForNewFile) {
    DiskPageManager manager(test_path);
    
    auto meta = manager.read_metadata();
    EXPECT_EQ(meta.root_page_id, INVALID_PAGE_ID);
    EXPECT_EQ(meta.first_leaf_page_id, INVALID_PAGE_ID);
    EXPECT_EQ(meta.height, 0);
    EXPECT_EQ(meta.size, 0);
    EXPECT_EQ(meta.key_kind, IndexKeyKind::Unknown);
}

TEST_F(DiskPageManagerTest, MetadataStringKeyKind) {
    DiskPageManager manager(test_path);
    
    BStarPlusIndexMetadata meta;
    meta.key_kind = IndexKeyKind::String;
    meta.root_page_id = 1;
    
    manager.write_metadata(meta);
    manager.flush();
    
    auto loaded = manager.read_metadata();
    EXPECT_EQ(loaded.key_kind, IndexKeyKind::String);
    EXPECT_EQ(loaded.root_page_id, 1);
}

// ==================== FLUSH ====================

TEST_F(DiskPageManagerTest, FlushWritesToDisk) {
    auto expected = patterned_page();
    
    {
        DiskPageManager manager(test_path);
        manager.allocate_page();
        manager.write_page(0, expected);
        manager.flush();
    }
    
    // Проверяем, что файл не пустой
    EXPECT_GT(std::filesystem::file_size(test_path), 0);
    
    // Читаем обратно
    {
        DiskPageManager manager(test_path);
        EXPECT_EQ(manager.read_page(0), expected);
    }
}

// ==================== СТРЕСС-ТЕСТ ====================

TEST_F(DiskPageManagerTest, ManyPagesAllocateReadWrite) {
    DiskPageManager manager(test_path);
    const int N = 100;
    
    std::vector<std::vector<char>> pages;
    
    // Пишем
    for (int i = 0; i < N; ++i) {
        PageId id = manager.allocate_page();
        EXPECT_EQ(id, i);
        
        auto data = patterned_page();
        data[0] = static_cast<char>(i % 256);
        data[PAGE_SIZE - 1] = static_cast<char>((i + 1) % 256);
        pages.push_back(data);
        manager.write_page(id, data);
    }
    
    // Читаем в случайном порядке
    std::vector<int> order(N);
    for (int i = 0; i < N; ++i) order[i] = i;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(order.begin(), order.end(), gen);
    
    for (int i : order) {
        auto actual = manager.read_page(i);
        EXPECT_EQ(actual, pages[i]);
    }
}

// ==================== ГРАНИЧНЫЕ СЛУЧАИ ====================

TEST_F(DiskPageManagerTest, ReadUnallocatedPageThrows) {
    DiskPageManager manager(test_path);
    manager.allocate_page();  // page 0
    manager.allocate_page();  // page 1
    
    // Страница 2 не выделена
    EXPECT_THROW(manager.read_page(2), std::out_of_range);
}

TEST_F(DiskPageManagerTest, WriteToUnallocatedPageThrows) {
    DiskPageManager manager(test_path);
    manager.allocate_page();  // page 0
    
    auto data = std::vector<char>(PAGE_SIZE, 0);
    EXPECT_THROW(manager.write_page(1, data), std::out_of_range);
}

TEST_F(DiskPageManagerTest, OpenNonexistentFileCreatesIt) {
    EXPECT_FALSE(std::filesystem::exists(test_path));
    
    {
        DiskPageManager manager(test_path);
        EXPECT_TRUE(std::filesystem::exists(test_path));
        EXPECT_EQ(manager.page_count(), 0);
    }
    
    EXPECT_TRUE(std::filesystem::exists(test_path));
}