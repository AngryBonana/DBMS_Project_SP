#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>

namespace db {
    using PageId = std::uint64_t;
    using SlotId = std::uint16_t;

    using IndexKey = std::variant<std::int64_t, std::string>;

    constexpr std::size_t INDEX_METADATA_SIZE =
        sizeof(std::uint64_t) + // root_page_id (PageId)
        sizeof(std::uint64_t) + // first_leaf_page_id (PageId)
        sizeof(std::uint32_t) + // height
        sizeof(std::uint64_t) + // size
        sizeof(std::uint8_t); // key_kind

    inline constexpr PageId INVALID_PAGE_ID = UINT64_MAX;

    enum class IndexKeyKind : std::uint8_t {
        Int64,
        String,
        Unknown
    };

    struct BStarPlusIndexMetadata {
        PageId root_page_id = INVALID_PAGE_ID;
        PageId first_leaf_page_id = INVALID_PAGE_ID;
        std::uint32_t height = 0;
        std::uint64_t size = 0;
        IndexKeyKind key_kind = IndexKeyKind::Unknown;

        bool empty() const noexcept;
    };

    class DiskPageManager final {
    public:
        explicit DiskPageManager(const std::filesystem::path &file_path);

        ~DiskPageManager();

        PageId allocate_page();

        std::vector<char> read_page(PageId page_id);

        void write_page(PageId page_id, const std::vector<char> &data);

        void flush();

        PageId page_count() const noexcept;

        BStarPlusIndexMetadata read_metadata();

        void write_metadata(const BStarPlusIndexMetadata &metadata);

    private:
        std::streamoff page_offset(PageId page_id) const;

        std::filesystem::path _file_path;
        std::fstream _file;
        PageId _page_count = 0;
    };
} 