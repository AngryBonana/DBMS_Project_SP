# Чеклист сдачи

## Реализовано (п.0 ТЗ, кратко)

- Парсер SQL: DDL/DML/SELECT, модификаторы колонок `NOT_NULL`, `INDEXED`, `DEFAULT`
- Хранилище: БД/таблицы, типы INT/STRING, персистентность в `data_dir`
- Индекс B\*+ для INDEXED-колонок
- Исполнитель: sync SELECT, async DDL/DML с `request_id`, `GET STATUS` / `GET RESULT`
- TCP server/client, access-log, демо в `demo/`

## Воспроизведение за ~2 минуты

```bash
cmake -S . -B build && cmake --build build -j4
ctest --test-dir build --output-on-failure -R 'ExecutorTests|StorageTests|ParserTests'
./build/server/server 5555 data/demo_run &
sleep 1
./build/client/client 5555 demo/sql/setup.sql
./build/client/client 5555 demo/sql/queries.sql
```

Ожидание: ответы `OK` / JSON-массивы; после перезапуска server данные в `data/demo_run` сохраняются.

## Синтаксис

Во всех скриптах и тестах: **`NOT_NULL`**, не `NOT NULL`.
