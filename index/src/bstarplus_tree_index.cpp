#include "bstarplus_tree_index.h"

#include <cstring>
#include <limits>
#include <type_traits>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>
#include <iostream>

#include "DiskManager.h"
#include "constants.h"

namespace db {
    
    namespace {

        template<typename T>
        void write_scalar(std::vector<char>& page, std::size_t& offset, const T& value) {
            static_assert(std::is_trivially_copyable_v<T>);

            if (offset + sizeof(T) > page.size()) {
                throw std::length_error("Index node does not fit into page");
            }

            std::memcpy(page.data() + offset, &value, sizeof(T));
            offset += sizeof(T);
        }

        template<typename T>
        T read_scalar(const std::vector<char>& page, std::size_t& offset) {
            static_assert(std::is_trivially_copyable_v<T>);

            if (offset + sizeof(T) > page.size()) {
                throw std::runtime_error("Index node page is truncated");
            }

            T value{};
            std::memcpy(&value, page.data() + offset, sizeof(T));
            offset += sizeof(T);
            return value;
        }

        struct IndexKeyLess {
            bool operator()(const IndexKey& left, const IndexKey& right) const {
                return key_less(left, right);
            }
        };

        void validate_node_shape(const IndexNode& node) {
            if (node.keys.size() > std::numeric_limits<std::uint32_t>::max()) {
                throw std::length_error("Index node has too many keys");
            }

            if (node.is_leaf) {
                if (!node.children.empty()) {
                    throw std::invalid_argument("Leaf index node cannot have child pages");
                }
                if (node.records.size() != node.keys.size()) {
                    throw std::invalid_argument("Leaf index node must have one record per key");
                }
                return;
            }

            if (!node.records.empty()) {
                throw std::invalid_argument("Internal index node cannot have records");
            }
            if (node.children.size() != node.keys.size() + 1) {
                throw std::invalid_argument("Internal index node must have keys.size() + 1 children");
            }
        }

        void validate_node_keys(const IndexNode& node, const IndexKeyKind key_kind) {
            for (const auto& key : node.keys) {
                validate_key_kind(key, key_kind);
            }
        }

        std::size_t serialized_node_size(const IndexNode& node, const IndexKeyKind key_kind) {
            const auto payload_count = node.is_leaf ? node.records.size() : node.children.size();

            if (payload_count > std::numeric_limits<std::uint32_t>::max()) {
                throw std::length_error("Index node payload is too large");
            }

            auto size = INDEX_NODE_HEADER_SIZE;
            for (const auto& key : node.keys) {
                validate_key_kind(key, key_kind);
                size += serialized_key_size(key);
            }
            if (node.is_leaf) {
                size += node.records.size() * (sizeof(PageId) + sizeof(SlotId));
            } else {
                size += node.children.size() * sizeof(PageId);
            }

            return size;
        }

        std::size_t child_index_for_key(const std::vector<IndexKey>& keys, const IndexKey& key) {
            return static_cast<std::size_t>(
                std::upper_bound(keys.begin(), keys.end(), key, IndexKeyLess{}) - keys.begin()
            );
        }

        std::size_t fixed_key_size(const IndexKeyKind key_kind) {
            return key_kind == IndexKeyKind::Int64
                ? sizeof(std::int64_t)
                : INDEX_STRING_KEY_SIZE;
        }

        std::size_t max_leaf_key_count(const IndexKeyKind key_kind) {
            const auto key_size = fixed_key_size(key_kind);
            const auto entry_size = key_size + sizeof(PageId) + sizeof(SlotId);
            return (PAGE_SIZE - INDEX_NODE_HEADER_SIZE) / entry_size;
        }

        std::size_t max_internal_key_count(const IndexKeyKind key_kind) {
            const auto key_size = fixed_key_size(key_kind);
            const auto entry_size = key_size + sizeof(PageId);
            return (PAGE_SIZE - INDEX_NODE_HEADER_SIZE - sizeof(PageId)) / entry_size;
        }

        std::size_t min_leaf_key_count(const IndexKeyKind key_kind) {
            return std::max<std::size_t>(1, (2 * max_leaf_key_count(key_kind) + 2) / 3);
        }

        std::size_t min_internal_key_count(const IndexKeyKind key_kind) {
            return std::max<std::size_t>(1, (2 * max_internal_key_count(key_kind) + 2) / 3);
        }

        std::array<std::size_t, 3> three_way_counts(const std::size_t total) {
            const auto first = (total + 2) / 3;
            const auto second = (total - first + 1) / 2;
            const auto third = total - first - second;
            return {first, second, third};
        }

    } // namespace

    bool key_matches_kind(const IndexKey& key, const IndexKeyKind kind) {
        switch (kind) {
            case IndexKeyKind::Int64:
                return std::holds_alternative<std::int64_t>(key);
            case IndexKeyKind::String:
                return std::holds_alternative<std::string>(key);
        }

        return false;
    }

    void validate_key_kind(const IndexKey& key, const IndexKeyKind kind) {
        if (!key_matches_kind(key, kind)) {
            throw std::invalid_argument("Index key type does not match index key kind");
        }
    }

    bool key_less(const IndexKey& left, const IndexKey& right) {
        if (left.index() != right.index()) {
            throw std::invalid_argument("Cannot compare index keys of different types");
        }

        if (std::holds_alternative<std::int64_t>(left)) {
            return std::get<std::int64_t>(left) < std::get<std::int64_t>(right);
        }

        return std::get<std::string>(left) < std::get<std::string>(right);
    }

    bool keys_equal(const IndexKey& left, const IndexKey& right) {
        if (left.index() != right.index()) {
            throw std::invalid_argument("Cannot compare index keys of different types");
        }

        if (std::holds_alternative<std::int64_t>(left)) {
            return std::get<std::int64_t>(left) == std::get<std::int64_t>(right);
        }

        return std::get<std::string>(left) == std::get<std::string>(right);
    }

    void validate_string_key(const std::string& string_key) {
        if (string_key.size() > INDEX_STRING_KEY_SIZE) {
            throw std::length_error("Index string key is too long");
        }

        if (string_key.find('\0') != std::string::npos) {
            throw std::invalid_argument("Index string key cannot contain null bytes");
        }
    }

    std::size_t serialized_key_size(const IndexKey& key) {
        if (std::holds_alternative<std::int64_t>(key)) {
            return sizeof(std::int64_t);
        }

        const auto& string_key = std::get<std::string>(key);
        validate_string_key(string_key);

        return INDEX_STRING_KEY_SIZE;
    }

    void write_key(std::vector<char>& page, std::size_t& offset, const IndexKey& key) {
        if (std::holds_alternative<std::int64_t>(key)) {
            write_scalar(page, offset, std::get<std::int64_t>(key));
            return;
        }

        const auto& string_key = std::get<std::string>(key);
        validate_string_key(string_key);

        if (offset + INDEX_STRING_KEY_SIZE > page.size()) {
            throw std::length_error("Index node does not fit into page");
        }

        std::array<char, INDEX_STRING_KEY_SIZE> buffer{};
        std::memcpy(buffer.data(), string_key.data(), string_key.size());

        std::memcpy(page.data() + offset, buffer.data(), INDEX_STRING_KEY_SIZE);
        offset += INDEX_STRING_KEY_SIZE;
    }

    IndexKey read_key(const std::vector<char>& page, std::size_t& offset, const IndexKeyKind key_kind) {
        switch (key_kind) {
            case IndexKeyKind::Int64:
                return read_scalar<std::int64_t>(page, offset);

            case IndexKeyKind::String: {
                if (offset + INDEX_STRING_KEY_SIZE > page.size()) {
                    throw std::runtime_error("Index string key is truncated");
                }

                const char* begin = page.data() + offset;
                const char* end = begin + INDEX_STRING_KEY_SIZE;
                const char* zero = std::find(begin, end, '\0');

                std::string value(begin, zero);
                offset += INDEX_STRING_KEY_SIZE;

                return value;
            }
        }

        throw std::invalid_argument("Unsupported index key kind");
    }

    std::vector<char> serialize_node(const IndexNode& node, const IndexKeyKind key_kind) {
        validate_node_shape(node);
        validate_node_keys(node, key_kind);

        if (serialized_node_size(node, key_kind) > db::PAGE_SIZE) {
            throw std::length_error("Index node does not fit into page");
        }

        std::vector<char> page(db::PAGE_SIZE, 0);
        std::size_t offset = 0;

        const auto node_type = node.is_leaf ? LEAF_NODE_TYPE : INTERNAL_NODE_TYPE;
        const auto reserved = std::uint8_t{0};
        const auto key_count = static_cast<std::uint32_t>(node.keys.size());
        const auto payload_count = static_cast<std::uint32_t>(node.is_leaf ? node.records.size() : node.children.size());

        write_scalar(page, offset, INDEX_NODE_MAGIC);
        write_scalar(page, offset, INDEX_NODE_VERSION);
        write_scalar(page, offset, node_type);
        write_scalar(page, offset, reserved);
        write_scalar(page, offset, node.page_id);
        write_scalar(page, offset, node.next_leaf_page_id);
        write_scalar(page, offset, key_count);
        write_scalar(page, offset, payload_count);

        for (const auto& key : node.keys) {
            write_key(page, offset, key);
        }

        if (node.is_leaf) {
            for (const auto& addr : node.records) {
                write_scalar(page, offset, addr);
            }
        } else {
            for (const auto child : node.children) {
                write_scalar(page, offset, child);
            }
        }

        return page;
    }

    IndexNode deserialize_node(const std::vector<char>& data, const IndexKeyKind key_kind) {
        if (data.size() != db::PAGE_SIZE) {
            throw std::invalid_argument("Index node page must have PAGE_SIZE bytes");
        }

        std::size_t offset = 0;

        const auto magic = read_scalar<std::uint32_t>(data, offset);
        if (magic != INDEX_NODE_MAGIC) {
            throw std::runtime_error("Invalid index node page magic");
        }

        const auto version = read_scalar<std::uint16_t>(data, offset);
        if (version != INDEX_NODE_VERSION) {
            throw std::runtime_error("Unsupported index node page version");
        }

        const auto node_type = read_scalar<std::uint8_t>(data, offset);
        read_scalar<std::uint8_t>(data, offset);

        if (node_type != INTERNAL_NODE_TYPE && node_type != LEAF_NODE_TYPE) {
            throw std::runtime_error("Invalid index node type");
        }

        IndexNode node;
        node.is_leaf = node_type == LEAF_NODE_TYPE;
        node.page_id = read_scalar<PageId>(data, offset);
        node.next_leaf_page_id = read_scalar<PageId>(data, offset);

        const auto key_count = read_scalar<std::uint32_t>(data, offset);
        const auto payload_count = read_scalar<std::uint32_t>(data, offset);

        if (key_count > data.size() || payload_count > data.size()) {
            throw std::runtime_error("Index node payload exceeds page size");
        }

        node.keys.reserve(key_count);
        for (std::uint32_t i = 0; i < key_count; ++i) {
            node.keys.push_back(read_key(data, offset, key_kind));
        }

        if (node.is_leaf) {
            if (payload_count != key_count) {
                throw std::runtime_error("Leaf index node payload count does not match key count");
            }

            node.records.reserve(payload_count);
            for (std::uint32_t i = 0; i < payload_count; ++i) {
                size_t d;
                d = read_scalar<size_t>(data, offset);
                node.records.push_back(d);
            }
        } else {
            if (payload_count != key_count + 1) {
                throw std::runtime_error("Internal index node payload count does not match child count");
            }

            node.children.reserve(payload_count);
            for (std::uint32_t i = 0; i < payload_count; ++i) {
                node.children.push_back(read_scalar<PageId>(data, offset));
            }
        }

        validate_node_shape(node);
        return node;
    }

    bool BStarPlusIndexMetadata::empty() const noexcept {
        return root_page_id == INVALID_PAGE_ID || height == 0;
    }

    BStarPlusIndex::BStarPlusIndex(std::filesystem::path p, IndexKeyKind k) 
            : _disk_manager(p) {
        BStarPlusIndexMetadata md;
        md.key_kind = k;
        _metadata = md;
        auto loaded_metadata = _disk_manager.read_metadata();
        // Используем загруженные метаданные только если они не пустые
        if (loaded_metadata.key_kind != IndexKeyKind::Unknown) {
            _metadata = loaded_metadata;
        } else {
            // Файл новый - проверяем, что метаданные переданы
            if (_metadata.key_kind == IndexKeyKind::Unknown) {
                throw std::invalid_argument("Metadata must be provided for new index");
            }
            // Сохраняем метаданные
            _disk_manager.write_metadata(_metadata);
        }
    }

    const BStarPlusIndexMetadata &BStarPlusIndex::metadata() const noexcept {
        return _metadata;
    }

    IndexNode BStarPlusIndex::read_node(const PageId page_id) {
        return deserialize_node(_disk_manager.read_page(page_id), _metadata.key_kind);
    }

    void BStarPlusIndex::write_node(const IndexNode& node) {
        _disk_manager.write_page(node.page_id, serialize_node(node, _metadata.key_kind));
    }

    bool BStarPlusIndex::node_fits_page(const IndexNode& node) {
        try {
            serialize_node(node, _metadata.key_kind);
            return true;
        } catch (const std::length_error&) {
            return false;
        }
    }

    IndexKey BStarPlusIndex::subtree_min_key(const PageId page_id) {
        auto node = read_node(page_id);

        while (!node.is_leaf) {
            if (node.children.empty()) {
                throw std::runtime_error("Index internal node has no children");
            }

            node = read_node(node.children.front());
        }

        if (node.keys.empty()) {
            throw std::runtime_error("Index subtree has no keys");
        }

        return node.keys.front();
    }

    PageId BStarPlusIndex::leftmost_leaf_page_id(const PageId page_id) {
        auto node = read_node(page_id);

        while (!node.is_leaf) {
            if (node.children.empty()) {
                throw std::runtime_error("Index internal node has no children");
            }

            node = read_node(node.children.front());
        }

        return node.page_id;
    }

    void BStarPlusIndex::rebuild_internal_keys(IndexNode& node) {
        if (node.is_leaf) {
            throw std::logic_error("Leaf node does not have internal separator keys");
        }

        if (node.children.empty()) {
            node.keys.clear();
            return;
        }

        node.keys.resize(node.children.size() - 1);
        for (std::size_t i = 1; i < node.children.size(); ++i) {
            node.keys[i - 1] = subtree_min_key(node.children[i]);
        }
    }

    void BStarPlusIndex::propagate_subtree_min_change(const PageId page_id, const std::vector<PageId>& path) {
        if (path.size() < 2 || path.back() != page_id) {
            return;
        }

        auto current_page_id = page_id;
        auto current_min_key = subtree_min_key(current_page_id);

        for (std::size_t i = path.size() - 1; i > 0; --i) {
            auto parent = read_node(path[i - 1]);
            if (parent.is_leaf) {
                throw std::logic_error("Leaf node cannot be a parent");
            }

            const auto child_it = std::find(parent.children.begin(), parent.children.end(), current_page_id);
            if (child_it == parent.children.end()) {
                return;
            }

            const auto child_index = static_cast<std::size_t>(child_it - parent.children.begin());
            if (child_index > 0) {
                parent.keys[child_index - 1] = current_min_key;
                write_node(parent);
                return;
            }

            current_page_id = parent.page_id;
            current_min_key = subtree_min_key(current_page_id);
        }
    }

    void BStarPlusIndex::promote_single_child_root(IndexNode& root) {
        if (root.is_leaf || root.children.size() != 1) {
            throw std::logic_error("Root can only promote a single child");
        }

        auto child = read_node(root.children.front());

        _metadata.root_page_id = child.page_id;
        if (_metadata.height > 1) {
            --_metadata.height;
        } else {
            _metadata.height = 1;
        }
        _metadata.first_leaf_page_id = child.is_leaf ? child.page_id : leftmost_leaf_page_id(child.page_id);
    }

    BStarPlusIndex::LeafSearchResult BStarPlusIndex::find_leaf(const IndexKey key) {
        PageId current_page_id = _metadata.root_page_id;
        std::vector<PageId> path;

        while (current_page_id != INVALID_PAGE_ID) {
            path.push_back(current_page_id);

            auto page = _disk_manager.read_page(current_page_id);
            auto node = deserialize_node(page, _metadata.key_kind);

            if (node.is_leaf) {
                return LeafSearchResult{std::move(node), std::move(path)};
            }

            const auto child_index = child_index_for_key(node.keys, key);
            if (child_index >= node.children.size()) {
                throw std::runtime_error("Index internal node child pointer is missing");
            }

            current_page_id = node.children[child_index];
        }

        throw std::runtime_error("Index leaf page was not found");
    }

    void BStarPlusIndex::insert_into_parent(IndexNode& left, const IndexKey separator, IndexNode& right,
                                            const std::vector<PageId>& left_path) {
        if (left_path.empty() || left_path.back() != left.page_id) {
            throw std::logic_error("Invalid path to left index node");
        }

        if (left_path.size() == 1) {
            const PageId new_root_page_id = _disk_manager.allocate_page();

            IndexNode new_root;
            new_root.is_leaf = false;
            new_root.page_id = new_root_page_id;
            new_root.keys = {separator};
            new_root.children = {left.page_id, right.page_id};

            write_node(left);
            write_node(right);
            write_node(new_root);

            _metadata.root_page_id = new_root_page_id;
            ++_metadata.height;
            return;
        }

        const auto parent_page_id = left_path[left_path.size() - 2];
        auto parent = read_node(parent_page_id);
        if (parent.is_leaf) {
            throw std::logic_error("Leaf node cannot be a parent");
        }

        auto child_it = std::find(parent.children.begin(), parent.children.end(), left.page_id);
        if (child_it == parent.children.end()) {
            throw std::logic_error("Parent does not reference split child");
        }

        const auto child_index = static_cast<std::size_t>(child_it - parent.children.begin());
        parent.keys.insert(parent.keys.begin() + static_cast<std::ptrdiff_t>(child_index), separator);
        parent.children.insert(parent.children.begin() + static_cast<std::ptrdiff_t>(child_index + 1), right.page_id);

        write_node(left);
        write_node(right);
        rebuild_internal_keys(parent);

        if (node_fits_page(parent)) {
            write_node(parent);
            return;
        }

        auto parent_path = left_path;
        parent_path.pop_back();
        split_internal(parent, parent_path);
    }

    void BStarPlusIndex::split_leaf(IndexNode& leaf, const std::vector<PageId>& path) {
        if (!leaf.is_leaf) {
            throw std::logic_error("Only leaf node can be split by split_leaf");
        }

        if (path.empty() || path.back() != leaf.page_id) {
            throw std::logic_error("Invalid path to leaf index node");
        }

        if (path.size() > 1) {
            const auto parent_page_id = path[path.size() - 2];
            auto parent = read_node(parent_page_id);
            if (parent.is_leaf) {
                throw std::logic_error("Leaf node cannot be a parent");
            }

            const auto child_it = std::find(parent.children.begin(), parent.children.end(), leaf.page_id);
            if (child_it == parent.children.end()) {
                throw std::logic_error("Parent does not reference split leaf");
            }

            const auto child_index = static_cast<std::size_t>(child_it - parent.children.begin());
            if (try_give_to_right_leaf(leaf, parent, child_index)) {
                return;
            }
            if (try_give_to_left_leaf(leaf, parent, child_index)) {
                return;
            }

            auto parent_path = path;
            parent_path.pop_back();

            if (child_index + 1 < parent.children.size()) {
                auto right = read_node(parent.children[child_index + 1]);
                if (!right.is_leaf) {
                    throw std::logic_error("Leaf sibling has unexpected node type");
                }
                split_leaf_pair(leaf, right, parent, child_index, parent_path);
                return;
            }

            if (child_index > 0) {
                auto left = read_node(parent.children[child_index - 1]);
                if (!left.is_leaf) {
                    throw std::logic_error("Leaf sibling has unexpected node type");
                }
                split_leaf_pair(left, leaf, parent, child_index - 1, parent_path);
                return;
            }
        }

        const auto right_page_id = _disk_manager.allocate_page();
        IndexNode right;
        right.is_leaf = true;
        right.page_id = right_page_id;
        right.next_leaf_page_id = leaf.next_leaf_page_id;

        const auto split_index = leaf.keys.size() / 2;

        right.keys.assign(leaf.keys.begin() + static_cast<std::ptrdiff_t>(split_index), leaf.keys.end());
        right.records.assign(leaf.records.begin() + static_cast<std::ptrdiff_t>(split_index), leaf.records.end());

        leaf.keys.erase(leaf.keys.begin() + static_cast<std::ptrdiff_t>(split_index), leaf.keys.end());
        leaf.records.erase(leaf.records.begin() + static_cast<std::ptrdiff_t>(split_index), leaf.records.end());
        leaf.next_leaf_page_id = right.page_id;

        if (right.keys.empty()) {
            throw std::logic_error("Leaf split produced empty right node");
        }

        const auto separator = right.keys.front();
        insert_into_parent(leaf, separator, right, path);
    }

    bool BStarPlusIndex::try_give_to_right_leaf(IndexNode& leaf, IndexNode& parent, const std::size_t child_index) {
        if (child_index + 1 >= parent.children.size()) {
            return false;
        }

        auto right = read_node(parent.children[child_index + 1]);
        if (!right.is_leaf || leaf.keys.empty()) {
            return false;
        }

        auto candidate_leaf = leaf;
        auto candidate_right = right;

        const auto key = candidate_leaf.keys.back();
        const auto record = candidate_leaf.records.back();
        candidate_leaf.keys.pop_back();
        candidate_leaf.records.pop_back();
        candidate_right.keys.insert(candidate_right.keys.begin(), key);
        candidate_right.records.insert(candidate_right.records.begin(), record);

        if (candidate_leaf.keys.empty()) {
            return false;
        }

        if (!node_fits_page(candidate_right) || !node_fits_page(candidate_leaf)) {
            return false;
        }

        leaf = std::move(candidate_leaf);
        right = std::move(candidate_right);

        write_node(leaf);
        write_node(right);
        rebuild_internal_keys(parent);
        write_node(parent);
        return true;
    }

    bool BStarPlusIndex::try_give_to_left_leaf(IndexNode& leaf, IndexNode& parent, const std::size_t child_index) {
        if (child_index == 0 || leaf.keys.empty()) {
            return false;
        }

        auto left = read_node(parent.children[child_index - 1]);
        if (!left.is_leaf) {
            return false;
        }

        auto candidate_left = left;
        auto candidate_leaf = leaf;

        const auto key = candidate_leaf.keys.front();
        const auto record = candidate_leaf.records.front();
        candidate_leaf.keys.erase(candidate_leaf.keys.begin());
        candidate_leaf.records.erase(candidate_leaf.records.begin());
        candidate_left.keys.push_back(key);
        candidate_left.records.push_back(record);

        if (candidate_leaf.keys.empty()) {
            return false;
        }

        if (!node_fits_page(candidate_left) || !node_fits_page(candidate_leaf)) {
            return false;
        }

        left = std::move(candidate_left);
        leaf = std::move(candidate_leaf);

        write_node(left);
        write_node(leaf);
        rebuild_internal_keys(parent);
        write_node(parent);
        return true;
    }

    void BStarPlusIndex::redistribute_leaf_pair(IndexNode& left, IndexNode& right, IndexNode& parent,
                                                const std::vector<PageId>& parent_path) {
        if (!left.is_leaf || !right.is_leaf) {
            throw std::logic_error("Only leaf siblings can be redistributed by redistribute_leaf_pair");
        }

        std::vector<std::pair<IndexKey, size_t>> items;
        items.reserve(left.keys.size() + right.keys.size());
        for (std::size_t i = 0; i < left.keys.size(); ++i) {
            items.emplace_back(left.keys[i], left.records[i]);
        }
        for (std::size_t i = 0; i < right.keys.size(); ++i) {
            items.emplace_back(right.keys[i], right.records[i]);
        }

        std::sort(items.begin(), items.end(), [](const auto& lhs, const auto& rhs) {
            return key_less(lhs.first, rhs.first);
        });

        const auto left_count = (items.size() + 1) / 2;
        left.keys.clear();
        left.records.clear();
        right.keys.clear();
        right.records.clear();

        for (std::size_t i = 0; i < items.size(); ++i) {
            auto& target = i < left_count ? left : right;
            target.keys.push_back(items[i].first);
            target.records.push_back(items[i].second);
        }

        if (!node_fits_page(left) || !node_fits_page(right)) {
            throw std::length_error("Redistributed leaf siblings do not fit into pages");
        }

        left.next_leaf_page_id = right.page_id;

        write_node(left);
        write_node(right);
        rebuild_internal_keys(parent);
        write_node(parent);
        propagate_subtree_min_change(parent.page_id, parent_path);
    }

    void BStarPlusIndex::split_leaf_pair(IndexNode& left, IndexNode& right, IndexNode& parent,
                                         const std::size_t parent_index, const std::vector<PageId>& parent_path) {
        std::vector<IndexKey> keys;
        std::vector<size_t> records;
        keys.reserve(left.keys.size() + right.keys.size());
        records.reserve(left.records.size() + right.records.size());

        keys.insert(keys.end(), left.keys.begin(), left.keys.end());
        keys.insert(keys.end(), right.keys.begin(), right.keys.end());
        records.insert(records.end(), left.records.begin(), left.records.end());
        records.insert(records.end(), right.records.begin(), right.records.end());

        std::vector<std::pair<IndexKey, size_t>> items;
        items.reserve(keys.size());
        for (std::size_t i = 0; i < keys.size(); ++i) {
            items.emplace_back(keys[i], records[i]);
        }
        std::sort(items.begin(), items.end(), [](const auto& lhs, const auto& rhs) {
            return key_less(lhs.first, rhs.first);
        });

        const auto middle_page_id = _disk_manager.allocate_page();
        IndexNode middle;
        middle.is_leaf = true;
        middle.page_id = middle_page_id;
        middle.next_leaf_page_id = right.page_id;

        const auto counts = three_way_counts(items.size());
        const auto first_count = counts[0];
        const auto second_count = counts[1];

        left.keys.clear();
        left.records.clear();
        middle.keys.clear();
        middle.records.clear();
        right.keys.clear();
        right.records.clear();

        for (std::size_t i = 0; i < items.size(); ++i) {
            auto* target = &right;
            if (i < first_count) {
                target = &left;
            } else if (i < first_count + second_count) {
                target = &middle;
            }

            target->keys.push_back(items[i].first);
            target->records.push_back(items[i].second);
        }

        if (left.keys.empty() || middle.keys.empty() || right.keys.empty()) {
            throw std::logic_error("Leaf pair split produced an empty node");
        }

        left.next_leaf_page_id = middle.page_id;
        middle.next_leaf_page_id = right.page_id;

        write_node(left);
        write_node(middle);
        write_node(right);

        parent.children.insert(parent.children.begin() + static_cast<std::ptrdiff_t>(parent_index + 1), middle.page_id);
        rebuild_internal_keys(parent);

        if (node_fits_page(parent)) {
            write_node(parent);
            return;
        }

        split_internal(parent, parent_path);
    }

    void BStarPlusIndex::redistribute_internal_pair(IndexNode& left, IndexNode& right, IndexNode& parent,
                                                    const std::vector<PageId>& parent_path) {
        if (left.is_leaf || right.is_leaf) {
            throw std::logic_error("Only internal siblings can be redistributed by redistribute_internal_pair");
        }

        std::vector<PageId> children;
        children.reserve(left.children.size() + right.children.size());
        children.insert(children.end(), left.children.begin(), left.children.end());
        children.insert(children.end(), right.children.begin(), right.children.end());

        const auto left_count = (children.size() + 1) / 2;
        left.children.assign(children.begin(), children.begin() + static_cast<std::ptrdiff_t>(left_count));
        right.children.assign(children.begin() + static_cast<std::ptrdiff_t>(left_count), children.end());

        rebuild_internal_keys(left);
        rebuild_internal_keys(right);

        if (!node_fits_page(left) || !node_fits_page(right)) {
            throw std::length_error("Redistributed internal siblings do not fit into pages");
        }

        write_node(left);
        write_node(right);
        rebuild_internal_keys(parent);
        write_node(parent);
        propagate_subtree_min_change(parent.page_id, parent_path);
    }

    bool BStarPlusIndex::try_redistribute_internal_with_right(
        IndexNode& node,
        IndexNode& parent,
        const std::size_t child_index,
        const std::vector<PageId>& parent_path
    ) {
        if (child_index + 1 >= parent.children.size()) {
            return false;
        }

        auto right = read_node(parent.children[child_index + 1]);
        if (right.is_leaf) {
            return false;
        }

        auto candidate_node = node;
        auto candidate_right = right;
        std::vector<PageId> children;
        children.reserve(candidate_node.children.size() + candidate_right.children.size());
        children.insert(children.end(), candidate_node.children.begin(), candidate_node.children.end());
        children.insert(children.end(), candidate_right.children.begin(), candidate_right.children.end());

        const auto left_count = (children.size() + 1) / 2;
        candidate_node.children.assign(children.begin(), children.begin() + static_cast<std::ptrdiff_t>(left_count));
        candidate_right.children.assign(children.begin() + static_cast<std::ptrdiff_t>(left_count), children.end());
        rebuild_internal_keys(candidate_node);
        rebuild_internal_keys(candidate_right);

        if (!node_fits_page(candidate_node) || !node_fits_page(candidate_right)) {
            return false;
        }

        redistribute_internal_pair(node, right, parent, parent_path);
        return true;
    }

    bool BStarPlusIndex::try_redistribute_internal_with_left(
        IndexNode& node,
        IndexNode& parent,
        const std::size_t child_index,
        const std::vector<PageId>& parent_path
    ) {
        if (child_index == 0) {
            return false;
        }

        auto left = read_node(parent.children[child_index - 1]);
        if (left.is_leaf) {
            return false;
        }

        auto candidate_left = left;
        auto candidate_node = node;
        std::vector<PageId> children;
        children.reserve(candidate_left.children.size() + candidate_node.children.size());
        children.insert(children.end(), candidate_left.children.begin(), candidate_left.children.end());
        children.insert(children.end(), candidate_node.children.begin(), candidate_node.children.end());

        const auto left_count = (children.size() + 1) / 2;
        candidate_left.children.assign(children.begin(), children.begin() + static_cast<std::ptrdiff_t>(left_count));
        candidate_node.children.assign(children.begin() + static_cast<std::ptrdiff_t>(left_count), children.end());
        rebuild_internal_keys(candidate_left);
        rebuild_internal_keys(candidate_node);

        if (!node_fits_page(candidate_left) || !node_fits_page(candidate_node)) {
            return false;
        }

        redistribute_internal_pair(left, node, parent, parent_path);
        return true;
    }

    void BStarPlusIndex::split_internal_pair(IndexNode& left, IndexNode& right, IndexNode& parent,
                                             const std::size_t parent_index, const std::vector<PageId>& parent_path) {
        if (left.is_leaf || right.is_leaf) {
            throw std::logic_error("Only internal siblings can be split by split_internal_pair");
        }

        std::vector<PageId> children;
        children.reserve(left.children.size() + right.children.size());
        children.insert(children.end(), left.children.begin(), left.children.end());
        children.insert(children.end(), right.children.begin(), right.children.end());

        const auto middle_page_id = _disk_manager.allocate_page();
        IndexNode middle;
        middle.is_leaf = false;
        middle.page_id = middle_page_id;

        const auto counts = three_way_counts(children.size());
        const auto first_end = counts[0];
        const auto second_end = counts[0] + counts[1];

        left.children.assign(children.begin(), children.begin() + static_cast<std::ptrdiff_t>(first_end));
        middle.children.assign(
            children.begin() + static_cast<std::ptrdiff_t>(first_end),
            children.begin() + static_cast<std::ptrdiff_t>(second_end)
        );
        right.children.assign(children.begin() + static_cast<std::ptrdiff_t>(second_end), children.end());

        rebuild_internal_keys(left);
        rebuild_internal_keys(middle);
        rebuild_internal_keys(right);

        if (!node_fits_page(left) || !node_fits_page(middle) || !node_fits_page(right)) {
            throw std::length_error("Internal pair split produced a node that does not fit into a page");
        }

        write_node(left);
        write_node(middle);
        write_node(right);

        parent.children.insert(parent.children.begin() + static_cast<std::ptrdiff_t>(parent_index + 1), middle.page_id);
        rebuild_internal_keys(parent);

        if (node_fits_page(parent)) {
            write_node(parent);
            return;
        }

        split_internal(parent, parent_path);
    }

    void BStarPlusIndex::split_internal(IndexNode& node, const std::vector<PageId>& path) {
        if (node.is_leaf) {
            throw std::logic_error("Leaf node cannot be split by split_internal");
        }

        if (path.empty() || path.back() != node.page_id) {
            throw std::logic_error("Invalid path to internal index node");
        }

        if (path.size() > 1) {
            const auto parent_page_id = path[path.size() - 2];
            auto parent = read_node(parent_page_id);
            if (parent.is_leaf) {
                throw std::logic_error("Leaf node cannot be a parent");
            }

            const auto child_it = std::find(parent.children.begin(), parent.children.end(), node.page_id);
            if (child_it == parent.children.end()) {
                throw std::logic_error("Parent does not reference split internal node");
            }

            const auto child_index = static_cast<std::size_t>(child_it - parent.children.begin());
            std::vector<PageId> parent_path(path.begin(), path.end() - 1);

            if (try_redistribute_internal_with_right(node, parent, child_index, parent_path)) {
                return;
            }
            if (try_redistribute_internal_with_left(node, parent, child_index, parent_path)) {
                return;
            }

            if (child_index + 1 < parent.children.size()) {
                auto right = read_node(parent.children[child_index + 1]);
                split_internal_pair(node, right, parent, child_index, parent_path);
                return;
            }

            if (child_index > 0) {
                auto left = read_node(parent.children[child_index - 1]);
                split_internal_pair(left, node, parent, child_index - 1, parent_path);
                return;
            }
        }

        if (node.children.size() < 2) {
            throw std::logic_error("Cannot split internal node with fewer than two children");
        }

        const auto split_index = node.children.size() / 2;

        const auto right_page_id = _disk_manager.allocate_page();
        IndexNode right;
        right.is_leaf = false;
        right.page_id = right_page_id;
        right.children.assign(node.children.begin() + static_cast<std::ptrdiff_t>(split_index), node.children.end());

        node.children.erase(node.children.begin() + static_cast<std::ptrdiff_t>(split_index), node.children.end());

        rebuild_internal_keys(node);
        rebuild_internal_keys(right);

        const auto separator = subtree_min_key(right.children.front());
        insert_into_parent(node, separator, right, path);
    }

    void BStarPlusIndex::handle_internal_after_child_removed(IndexNode& node, const std::vector<PageId>& path) {
        if (node.is_leaf) {
            throw std::logic_error("Leaf node cannot handle removed children");
        }
        if (path.empty() || path.back() != node.page_id) {
            throw std::logic_error("Invalid path to internal index node");
        }

        rebuild_internal_keys(node);

        if (node.page_id == _metadata.root_page_id) {
            if (node.children.size() == 1) {
                promote_single_child_root(node);
                return;
            }

            write_node(node);
            return;
        }

        if (node.keys.size() < min_internal_key_count(_metadata.key_kind)) {
            fix_internal_underflow(node, path);
            return;
        }

        write_node(node);
        propagate_subtree_min_change(node.page_id, path);
    }

    void BStarPlusIndex::fix_leaf_underflow(IndexNode& leaf, const std::vector<PageId>& path) {
        if (!leaf.is_leaf) {
            throw std::logic_error("Only leaf node can be fixed by fix_leaf_underflow");
        }
        if (path.empty() || path.back() != leaf.page_id) {
            throw std::logic_error("Invalid path to leaf index node");
        }

        if (leaf.page_id == _metadata.root_page_id) {
            write_node(leaf);
            return;
        }

        const auto parent_page_id = path[path.size() - 2];
        auto parent = read_node(parent_page_id);
        if (parent.is_leaf) {
            throw std::logic_error("Leaf node cannot be a parent");
        }

        const auto child_it = std::find(parent.children.begin(), parent.children.end(), leaf.page_id);
        if (child_it == parent.children.end()) {
            throw std::logic_error("Parent does not reference erased leaf");
        }

        const auto child_index = static_cast<std::size_t>(child_it - parent.children.begin());
        const auto min_keys = min_leaf_key_count(_metadata.key_kind);
        std::vector<PageId> parent_path(path.begin(), path.end() - 1);

        if (child_index > 0) {
            auto left = read_node(parent.children[child_index - 1]);
            if (!left.is_leaf) {
                throw std::logic_error("Leaf sibling has unexpected node type");
            }

            if (left.keys.size() > min_keys) {
                leaf.keys.insert(leaf.keys.begin(), left.keys.back());
                leaf.records.insert(leaf.records.begin(), left.records.back());
                left.keys.pop_back();
                left.records.pop_back();

                write_node(left);
                write_node(leaf);
                rebuild_internal_keys(parent);
                write_node(parent);
                propagate_subtree_min_change(parent.page_id, parent_path);
                return;
            }
        }

        if (child_index + 1 < parent.children.size()) {
            auto right = read_node(parent.children[child_index + 1]);
            if (!right.is_leaf) {
                throw std::logic_error("Leaf sibling has unexpected node type");
            }

            if (right.keys.size() > min_keys) {
                leaf.keys.push_back(right.keys.front());
                leaf.records.push_back(right.records.front());
                right.keys.erase(right.keys.begin());
                right.records.erase(right.records.begin());

                write_node(leaf);
                write_node(right);
                rebuild_internal_keys(parent);
                write_node(parent);
                propagate_subtree_min_change(parent.page_id, parent_path);
                return;
            }
        }

        if (child_index > 0) {
            auto left = read_node(parent.children[child_index - 1]);
            if (!left.is_leaf) {
                throw std::logic_error("Leaf sibling has unexpected node type");
            }

            auto merged_left = left;
            merged_left.keys.insert(merged_left.keys.end(), leaf.keys.begin(), leaf.keys.end());
            merged_left.records.insert(merged_left.records.end(), leaf.records.begin(), leaf.records.end());
            merged_left.next_leaf_page_id = leaf.next_leaf_page_id;

            if (!node_fits_page(merged_left)) {
                redistribute_leaf_pair(left, leaf, parent, parent_path);
                return;
            }

            left = std::move(merged_left);

            if (_metadata.first_leaf_page_id == leaf.page_id) {
                _metadata.first_leaf_page_id = left.page_id;
            }

            parent.children.erase(parent.children.begin() + static_cast<std::ptrdiff_t>(child_index));
            parent.keys.erase(parent.keys.begin() + static_cast<std::ptrdiff_t>(child_index - 1));

            write_node(left);
            handle_internal_after_child_removed(parent, parent_path);
            return;
        }

        if (child_index + 1 >= parent.children.size()) {
            throw std::logic_error("Leaf node has no sibling for merge");
        }

        auto right = read_node(parent.children[child_index + 1]);
        if (!right.is_leaf) {
            throw std::logic_error("Leaf sibling has unexpected node type");
        }

        auto merged_leaf = leaf;
        merged_leaf.keys.insert(merged_leaf.keys.end(), right.keys.begin(), right.keys.end());
        merged_leaf.records.insert(merged_leaf.records.end(), right.records.begin(), right.records.end());
        merged_leaf.next_leaf_page_id = right.next_leaf_page_id;

        if (!node_fits_page(merged_leaf)) {
            redistribute_leaf_pair(leaf, right, parent, parent_path);
            return;
        }

        leaf = std::move(merged_leaf);

        if (_metadata.first_leaf_page_id == right.page_id) {
            _metadata.first_leaf_page_id = leaf.page_id;
        }

        parent.children.erase(parent.children.begin() + static_cast<std::ptrdiff_t>(child_index + 1));
        parent.keys.erase(parent.keys.begin() + static_cast<std::ptrdiff_t>(child_index));

        write_node(leaf);
        handle_internal_after_child_removed(parent, parent_path);
    }

    void BStarPlusIndex::fix_internal_underflow(IndexNode& node, const std::vector<PageId>& path) {
        if (node.is_leaf) {
            throw std::logic_error("Leaf node cannot be fixed by fix_internal_underflow");
        }
        if (path.empty() || path.back() != node.page_id) {
            throw std::logic_error("Invalid path to internal index node");
        }

        if (node.page_id == _metadata.root_page_id) {
            if (node.children.size() == 1) {
                promote_single_child_root(node);
            } else {
                rebuild_internal_keys(node);
                write_node(node);
            }
            return;
        }

        const auto parent_page_id = path[path.size() - 2];
        auto parent = read_node(parent_page_id);
        if (parent.is_leaf) {
            throw std::logic_error("Leaf node cannot be a parent");
        }

        const auto child_it = std::find(parent.children.begin(), parent.children.end(), node.page_id);
        if (child_it == parent.children.end()) {
            throw std::logic_error("Parent does not reference underfilled internal node");
        }

        const auto child_index = static_cast<std::size_t>(child_it - parent.children.begin());
        const auto min_keys = min_internal_key_count(_metadata.key_kind);

        if (child_index > 0) {
            auto left = read_node(parent.children[child_index - 1]);
            if (left.is_leaf) {
                throw std::logic_error("Internal sibling has unexpected node type");
            }

            if (left.keys.size() > min_keys) {
                const auto moved_child_page_id = left.children.back();
                left.children.pop_back();
                node.children.insert(node.children.begin(), moved_child_page_id);

                rebuild_internal_keys(left);
                rebuild_internal_keys(node);
                write_node(left);
                write_node(node);
                rebuild_internal_keys(parent);
                write_node(parent);
                propagate_subtree_min_change(parent.page_id, std::vector<PageId>(path.begin(), path.end() - 1));
                return;
            }
        }

        if (child_index + 1 < parent.children.size()) {
            auto right = read_node(parent.children[child_index + 1]);
            if (right.is_leaf) {
                throw std::logic_error("Internal sibling has unexpected node type");
            }

            if (right.keys.size() > min_keys) {
                const auto moved_child_page_id = right.children.front();
                right.children.erase(right.children.begin());
                node.children.push_back(moved_child_page_id);

                rebuild_internal_keys(node);
                rebuild_internal_keys(right);
                write_node(node);
                write_node(right);
                rebuild_internal_keys(parent);
                write_node(parent);
                propagate_subtree_min_change(parent.page_id, std::vector<PageId>(path.begin(), path.end() - 1));
                return;
            }
        }

        auto parent_path = path;
        parent_path.pop_back();

        if (child_index > 0) {
            auto left = read_node(parent.children[child_index - 1]);
            if (left.is_leaf) {
                throw std::logic_error("Internal sibling has unexpected node type");
            }

            auto merged_left = left;
            for (const auto child_page_id : node.children) {
                merged_left.children.push_back(child_page_id);
            }
            rebuild_internal_keys(merged_left);

            if (!node_fits_page(merged_left)) {
                redistribute_internal_pair(left, node, parent, parent_path);
                return;
            }

            left = std::move(merged_left);

            rebuild_internal_keys(left);
            parent.children.erase(parent.children.begin() + static_cast<std::ptrdiff_t>(child_index));
            parent.keys.erase(parent.keys.begin() + static_cast<std::ptrdiff_t>(child_index - 1));

            write_node(left);
            handle_internal_after_child_removed(parent, parent_path);
            return;
        }

        if (child_index + 1 >= parent.children.size()) {
            throw std::logic_error("Internal node has no sibling for merge");
        }

        auto right = read_node(parent.children[child_index + 1]);
        if (right.is_leaf) {
            throw std::logic_error("Internal sibling has unexpected node type");
        }

        auto merged_node = node;
        for (const auto child_page_id : right.children) {
            merged_node.children.push_back(child_page_id);
        }
        rebuild_internal_keys(merged_node);

        if (!node_fits_page(merged_node)) {
            redistribute_internal_pair(node, right, parent, parent_path);
            return;
        }

        node = std::move(merged_node);

        rebuild_internal_keys(node);
        parent.children.erase(parent.children.begin() + static_cast<std::ptrdiff_t>(child_index + 1));
        parent.keys.erase(parent.keys.begin() + static_cast<std::ptrdiff_t>(child_index));

        write_node(node);
        handle_internal_after_child_removed(parent, parent_path);
    }

    void BStarPlusIndex::insert(IndexKey key, size_t record) {
        validate_key_kind(key, _metadata.key_kind);
        if (_metadata.empty()) {
            const PageId root_page_id = _disk_manager.allocate_page();

            IndexNode root;
            root.is_leaf = true;
            root.page_id = root_page_id;
            root.next_leaf_page_id = INVALID_PAGE_ID;
            root.keys.push_back(key);
            root.records.push_back(record);

            _disk_manager.write_page(root_page_id, serialize_node(root, _metadata.key_kind));

            _metadata.root_page_id = root_page_id;
            _metadata.first_leaf_page_id = root_page_id;
            _metadata.height = 1;
            _metadata.size = 1;

            // Сохраняем метаданные
            _disk_manager.write_metadata(_metadata);

            return;
        }

        auto leaf_search = find_leaf(key);
        auto& leaf = leaf_search.leaf;

        auto it = std::lower_bound(leaf.keys.begin(), leaf.keys.end(), key, IndexKeyLess{});
        const auto index = static_cast<std::size_t>(it - leaf.keys.begin());

        if (it != leaf.keys.end() && keys_equal(*it, key)) {
            throw std::invalid_argument("Duplicate index key");
        }

        leaf.keys.insert(leaf.keys.begin() + index, key);
        leaf.records.insert(leaf.records.begin() + index, record);

        if (node_fits_page(leaf)) {
            write_node(leaf);
        } else {
            split_leaf(leaf, leaf_search.path);
        }

        ++_metadata.size;

        // Сохраняем метаданные
        _disk_manager.write_metadata(_metadata);
    }

    bool BStarPlusIndex::erase(IndexKey key) {
        validate_key_kind(key, _metadata.key_kind);

        if (_metadata.empty()) {
            return false;
        }

        auto leaf_search = find_leaf(key);
        auto& leaf = leaf_search.leaf;

        auto it = std::lower_bound(leaf.keys.begin(), leaf.keys.end(), key, IndexKeyLess{});
        if (it == leaf.keys.end() || !keys_equal(*it, key)) {
            return false;
        }

        const auto erased_index = static_cast<std::size_t>(it - leaf.keys.begin());
        leaf.keys.erase(it);
        leaf.records.erase(leaf.records.begin() + static_cast<std::ptrdiff_t>(erased_index));

        if (_metadata.size > 0) {
            --_metadata.size;
        }

        if (_metadata.size == 0) {
            leaf.keys.clear();
            leaf.records.clear();
            write_node(leaf);

            _metadata.root_page_id = INVALID_PAGE_ID;
            _metadata.first_leaf_page_id = INVALID_PAGE_ID;
            _metadata.height = 0;

            // Сохраняем метаданные
            _disk_manager.write_metadata(_metadata);

            return true;
        }

        if (leaf.page_id == _metadata.root_page_id || leaf.keys.size() >= min_leaf_key_count(_metadata.key_kind)) {
            write_node(leaf);
            if (erased_index == 0 && !leaf.keys.empty()) {
                propagate_subtree_min_change(leaf.page_id, leaf_search.path);
            }

            // Сохраняем метаданные
            _disk_manager.write_metadata(_metadata);

            return true;
        }

        fix_leaf_underflow(leaf, leaf_search.path);

        // Сохраняем метаданные
        _disk_manager.write_metadata(_metadata);

        return true;
    }

    std::optional<size_t> BStarPlusIndex::find(IndexKey key) {
        validate_key_kind(key, _metadata.key_kind);

        if (_metadata.empty()) {
            return std::nullopt;
        }

        PageId current_page_id = _metadata.root_page_id;

        while (current_page_id != INVALID_PAGE_ID) {
            auto page = _disk_manager.read_page(current_page_id);
            auto node = deserialize_node(page, _metadata.key_kind);

            if (node.is_leaf) {
                auto it = std::lower_bound(node.keys.begin(), node.keys.end(), key, IndexKeyLess{});
                if (it != node.keys.end() && keys_equal(*it, key)) {
                    auto index = static_cast<std::size_t>(it - node.keys.begin());
                    return node.records[index];
                }

                return std::nullopt;
            }

            const auto child_index = child_index_for_key(node.keys, key);
            if (child_index >= node.children.size()) {
                throw std::runtime_error("Index internal node child pointer is missing");
            }

            current_page_id = node.children[child_index];
        }

        return std::nullopt;
    }


    std::vector<size_t> BStarPlusIndex::range_search(IndexKey beg, IndexKey end) {
        validate_key_kind(beg, _metadata.key_kind);
        validate_key_kind(end, _metadata.key_kind);
        if(_metadata.empty() || key_less(end, beg)) {
            return {};
        }

        std::vector<size_t> res;
        for (auto i = find_leaf(beg).leaf; i.page_id != INVALID_PAGE_ID && i.page_id < _disk_manager.page_count(); i = read_node(i.next_leaf_page_id)) {
            for (size_t j = 0; j < i.keys.size(); j++) {
                if (key_less(i.keys[j], beg)) continue;
                if (key_less(end, i.keys[j])) return res;
                res.push_back(i.records[j]);
            }
            if (i.next_leaf_page_id == INVALID_PAGE_ID) break;
        }

        return res;
    }
} // namespace db
