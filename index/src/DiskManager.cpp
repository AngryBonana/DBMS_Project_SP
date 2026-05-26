#include "DiskManager.h"

#include <fstream>
#include <limits>
#include <stdexcept>

#include "constants.h"

namespace db {

    namespace {

        void validate_existing_page_id(const PageId page_id, const PageId page_count) {
            if (page_id == INVALID_PAGE_ID || page_id >= page_count) {
                throw std::out_of_range("Index page id is out of range");
            }
        }

    } // namespace

    DiskPageManager::DiskPageManager(const std::filesystem::path& file_path)
        : _file_path(file_path) {
        if (!std::filesystem::exists(_file_path)) {
            std::ofstream create_file(_file_path, std::ios::binary | std::ios::out);
            if (!create_file) {
                throw std::runtime_error("Cannot create index page file");
            }

            // Записываем пустые метаданные в начало файла
            BStarPlusIndexMetadata empty_metadata{};
            create_file.write(reinterpret_cast<const char*>(&empty_metadata.root_page_id), sizeof(empty_metadata.root_page_id));
            create_file.write(reinterpret_cast<const char*>(&empty_metadata.first_leaf_page_id), sizeof(empty_metadata.first_leaf_page_id));
            create_file.write(reinterpret_cast<const char*>(&empty_metadata.height), sizeof(empty_metadata.height));
            create_file.write(reinterpret_cast<const char*>(&empty_metadata.size), sizeof(empty_metadata.size));
            create_file.write(reinterpret_cast<const char*>(&empty_metadata.key_kind), sizeof(empty_metadata.key_kind));

            // Дополняем до размера страницы нулями
            const auto padding_size = db::PAGE_SIZE - db::INDEX_METADATA_SIZE;
            const auto padding = std::vector<char>(padding_size, 0);
            create_file.write(padding.data(), static_cast<std::streamsize>(padding.size()));
        }

        _file.open(_file_path, std::ios::binary | std::ios::in | std::ios::out);
        if (!_file.is_open()) {
            throw std::runtime_error("Cannot open index page file");
        }

        const auto file_size = std::filesystem::file_size(_file_path);
        if (file_size < db::PAGE_SIZE || (file_size - db::PAGE_SIZE) % db::PAGE_SIZE != 0) {
            throw std::runtime_error("Index page file size is not aligned to PAGE_SIZE");
        }

        // Первая страница зарезервирована под метаданные
        _page_count = static_cast<PageId>((file_size - db::PAGE_SIZE) / db::PAGE_SIZE);
    }

    DiskPageManager::~DiskPageManager() {
        try {
            if (_file.is_open()) {
                flush();
            }
        } catch (...) {
        }
    }

    std::streamoff DiskPageManager::page_offset(const PageId page_id) const {
        constexpr auto max_offset = std::numeric_limits<std::streamoff>::max();
        if (page_id > static_cast<PageId>(max_offset / static_cast<std::streamoff>(db::PAGE_SIZE))) {
            throw std::out_of_range("Index page offset is out of range");
        }

        // Смещение = PAGE_SIZE (метаданные) + page_id * PAGE_SIZE
        return static_cast<std::streamoff>(db::PAGE_SIZE) +
               static_cast<std::streamoff>(page_id) * static_cast<std::streamoff>(db::PAGE_SIZE);
    }

    PageId DiskPageManager::allocate_page() {
        if (!_file.is_open()) {
            throw std::runtime_error("Index page file is not open");
        }

        const auto page_id = _page_count;
        const auto empty_page = std::vector<char>(db::PAGE_SIZE, 0);

        _file.clear();
        _file.seekp(page_offset(page_id), std::ios::beg);
        if (!_file) {
            throw std::runtime_error("Cannot seek to new index page");
        }

        _file.write(empty_page.data(), static_cast<std::streamsize>(empty_page.size()));
        if (!_file) {
            throw std::runtime_error("Cannot allocate index page");
        }

        ++_page_count;
        return page_id;
    }

    std::vector<char> DiskPageManager::read_page(const PageId page_id) {
        validate_existing_page_id(page_id, _page_count);

        auto page = std::vector<char>(db::PAGE_SIZE);
        _file.clear();
        _file.seekg(page_offset(page_id), std::ios::beg);
        if (!_file) {
            throw std::runtime_error("Cannot seek index page for read");
        }

        _file.read(page.data(), static_cast<std::streamsize>(page.size()));
        if (_file.gcount() != static_cast<std::streamsize>(db::PAGE_SIZE) || !_file) {
            throw std::runtime_error("Cannot read index page");
        }

        return page;
    }

    void DiskPageManager::write_page(const PageId page_id, const std::vector<char>& data) {
        validate_existing_page_id(page_id, _page_count);
        if (data.size() != db::PAGE_SIZE) {
            throw std::invalid_argument("Index page must have PAGE_SIZE bytes");
        }

        _file.clear();
        _file.seekp(page_offset(page_id), std::ios::beg);
        if (!_file) {
            throw std::runtime_error("Cannot seek index page for write");
        }

        _file.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!_file) {
            throw std::runtime_error("Cannot write index page");
        }
    }

    void DiskPageManager::flush() {
        if (!_file.is_open()) {
            throw std::runtime_error("Index page file is not open");
        }

        _file.flush();
        if (!_file) {
            throw std::runtime_error("Cannot flush index page file");
        }
    }

    PageId DiskPageManager::page_count() const noexcept {
        return _page_count;
    }

    BStarPlusIndexMetadata DiskPageManager::read_metadata() {
        if (!_file.is_open()) {
            throw std::runtime_error("Index page file is not open");
        }

        _file.clear();
        _file.seekg(0, std::ios::beg);
        if (!_file) {
            throw std::runtime_error("Cannot seek to metadata");
        }

        BStarPlusIndexMetadata metadata{};
        _file.read(reinterpret_cast<char*>(&metadata.root_page_id), sizeof(metadata.root_page_id));
        _file.read(reinterpret_cast<char*>(&metadata.first_leaf_page_id), sizeof(metadata.first_leaf_page_id));
        _file.read(reinterpret_cast<char*>(&metadata.height), sizeof(metadata.height));
        _file.read(reinterpret_cast<char*>(&metadata.size), sizeof(metadata.size));
        _file.read(reinterpret_cast<char*>(&metadata.key_kind), sizeof(metadata.key_kind));

        if (!_file) {
            // Если не удалось прочитать, возвращаем пустые метаданные
            return BStarPlusIndexMetadata{};
        }

        return metadata;
    }

    void DiskPageManager::write_metadata(const BStarPlusIndexMetadata& metadata) {
        if (!_file.is_open()) {
            throw std::runtime_error("Index page file is not open");
        }

        _file.clear();
        _file.seekp(0, std::ios::beg);
        if (!_file) {
            throw std::runtime_error("Cannot seek to metadata");
        }

        _file.write(reinterpret_cast<const char*>(&metadata.root_page_id), sizeof(metadata.root_page_id));
        _file.write(reinterpret_cast<const char*>(&metadata.first_leaf_page_id), sizeof(metadata.first_leaf_page_id));
        _file.write(reinterpret_cast<const char*>(&metadata.height), sizeof(metadata.height));
        _file.write(reinterpret_cast<const char*>(&metadata.size), sizeof(metadata.size));
        _file.write(reinterpret_cast<const char*>(&metadata.key_kind), sizeof(metadata.key_kind));

        if (!_file) {
            throw std::runtime_error("Cannot write metadata");
        }

        flush();
    }

} // namespace db
