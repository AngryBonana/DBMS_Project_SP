#include "storage/table.h"
#include <stdexcept>

namespace cw_db {

void Table::insert_row(const std::vector<std::optional<Value>>& values) {
    const auto cols = schema_.column_count();
    if (values.size() > cols) {
        throw std::invalid_argument("Table::insert_row: too many values");
    }

    std::vector<Value> row;
    row.reserve(cols);

    for (std::size_t i = 0; i < cols; ++i) {
        const auto& col = schema_.columns()[i];

        if (i < values.size() && values[i].has_value()) {
            const Value& v = *values[i];
            if (!v.matches(col.type)) {
                throw std::invalid_argument("Table::insert_row: type mismatch for column '" + col.name + "'");
            }
            row.push_back(v);
        } else {
            // отсутствующее значение — используем default, NULL или ошибку при NOT NULL
            if (col.default_value.has_value()) {
                row.push_back(*col.default_value);
            } else {
                if (col.not_null) {
                    throw std::invalid_argument("Table::insert_row: missing NOT NULL value for column '" + col.name + "'");
                }
                row.push_back(Value::null());
            }
        }
    }

    rows_.push_back(std::move(row));
}

} 