#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
#include "DiskManager.h"

namespace db {
    constexpr std::uint32_t INDEX_NODE_MAGIC = 0x31495042;
    constexpr std::uint16_t INDEX_NODE_VERSION = 1;
    constexpr std::uint8_t INTERNAL_NODE_TYPE = 0;
    constexpr std::uint8_t LEAF_NODE_TYPE = 1;
    constexpr std::size_t INDEX_NODE_HEADER_SIZE =
        sizeof(std::uint32_t) + // magic
        sizeof(std::uint16_t) + // version
        sizeof(std::uint8_t)  + // node_type
        sizeof(std::uint8_t)  + // reserved
        sizeof(PageId)        + // page_id
        sizeof(PageId)        + // next_leaf_page_id
        sizeof(std::uint32_t) + // key_count
        sizeof(std::uint32_t);  // payload_count

    struct IndexNode {
        bool is_leaf = true;
        PageId page_id = INVALID_PAGE_ID;
        PageId next_leaf_page_id = INVALID_PAGE_ID;
        std::vector<IndexKey> keys;
        std::vector<PageId> children;
        std::vector<size_t> records;
    };

    bool key_matches_kind(const IndexKey& key, IndexKeyKind kind);
    void validate_key_kind(const IndexKey& key, IndexKeyKind kind);
    bool key_less(const IndexKey& left, const IndexKey& right);
    bool keys_equal(const IndexKey& left, const IndexKey& right);

    std::size_t serialized_key_size(const IndexKey& key);
    void write_key(std::vector<char>& page, std::size_t& offset, const IndexKey& key);
    IndexKey read_key(const std::vector<char>& page, std::size_t& offset, IndexKeyKind key_kind);

    std::vector<char> serialize_node(const IndexNode& node, IndexKeyKind key_kind = IndexKeyKind::Int64);

    IndexNode deserialize_node(const std::vector<char>& data, IndexKeyKind key_kind = IndexKeyKind::Int64);

    class BStarPlusIndex {
    public:
        // p - путь по которому будет хранится файл индекса
        explicit BStarPlusIndex(std::filesystem::path p, IndexKeyKind k = IndexKeyKind::Unknown);

        const BStarPlusIndexMetadata& metadata() const noexcept;

        std::optional<size_t> find(IndexKey key);
        std::vector<size_t> range_search(IndexKey beg, IndexKey end);

        // record - смещение записи от начала файла в котором он хранится 
        // если такой ключ уже есть -- исключение
        void insert(IndexKey key, size_t record);

        bool erase(IndexKey key);

    private:

        DiskPageManager _disk_manager;
        BStarPlusIndexMetadata _metadata;

        struct LeafSearchResult {
            IndexNode leaf;
            std::vector<PageId> path;
        };

        LeafSearchResult find_leaf(IndexKey key);
        IndexNode read_node(PageId page_id);
        void write_node(const IndexNode& node);
        bool node_fits_page(const IndexNode& node);
        void insert_into_parent(IndexNode& left, IndexKey separator, IndexNode& right, const std::vector<PageId>& left_path);
        void split_leaf(IndexNode& leaf, const std::vector<PageId>& path);
        bool try_give_to_right_leaf(IndexNode& leaf, IndexNode& parent, std::size_t child_index);
        bool try_give_to_left_leaf(IndexNode& leaf, IndexNode& parent, std::size_t child_index);
        void redistribute_leaf_pair(IndexNode& left, IndexNode& right, IndexNode& parent,
                                    const std::vector<PageId>& parent_path);
        void split_leaf_pair(IndexNode& left, IndexNode& right, IndexNode& parent, std::size_t parent_index,
                             const std::vector<PageId>& parent_path);
        bool try_redistribute_internal_with_right(IndexNode& node, IndexNode& parent, std::size_t child_index,
                                                  const std::vector<PageId>& parent_path);
        bool try_redistribute_internal_with_left(IndexNode& node, IndexNode& parent, std::size_t child_index,
                                                 const std::vector<PageId>& parent_path);
        void redistribute_internal_pair(IndexNode& left, IndexNode& right, IndexNode& parent,
                                        const std::vector<PageId>& parent_path);
        void split_internal_pair(IndexNode& left, IndexNode& right, IndexNode& parent, std::size_t parent_index,
                                 const std::vector<PageId>& parent_path);
        void split_internal(IndexNode& node, const std::vector<PageId>& path);
        void fix_leaf_underflow(IndexNode& leaf, const std::vector<PageId>& path);
        void fix_internal_underflow(IndexNode& node, const std::vector<PageId>& path);
        void handle_internal_after_child_removed(IndexNode& node, const std::vector<PageId>& path);
        void rebuild_internal_keys(IndexNode& node);
        void promote_single_child_root(IndexNode& root);
        IndexKey subtree_min_key(PageId page_id);
        PageId leftmost_leaf_page_id(PageId page_id);
        void propagate_subtree_min_change(PageId page_id, const std::vector<PageId>& path);
    };

} 
