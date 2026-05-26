#include <iostream>
#include <filesystem>

#include "bstarplus_tree_index.h"
#include "DiskManager.h"
#include "constants.h";
using namespace db;


void print(std::optional<size_t> r) {
    if(r.has_value()) {
        auto rec = r.value();
        std::cout << rec << "\n";
    } else {
        std::cout << "No such\n";
    }
}

int main() {
    try {
        std::filesystem::path p("./tt.txt");

        BStarPlusIndex b(p, IndexKeyKind::Int64);

        // Warning: при вставке одного и того же ключа -- исключение
        b.insert(123, 100);
        b.insert(190, 101);
        b.insert(9999, 116);

        
        print(b.find(10));
        print(b.find(11));
        print(b.find(12));
        print(b.find(15));

        std::cout << "\n";
        for (auto i: b.range_search(150, 10000000)) {
            std::cout << i << "\n";
        }

        std::cout << "Success!\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }
    catch (...) {
        std::cerr << "Unknown error\n";
    }
    return 0;
}