# Библиотека с B-деревом
add_library(btree_lib STATIC
    src/bstarplus_tree_index.cpp
    src/DiskManager.cpp
)

target_include_directories(btree_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(btree_lib PUBLIC cxx_std_17)

# Отладочный исполняемый файл
add_executable(btree_debug main.cpp)

target_link_libraries(btree_debug PRIVATE
    btree_lib
)

target_compile_features(btree_debug PUBLIC cxx_std_17)
