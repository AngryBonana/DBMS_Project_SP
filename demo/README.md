# Demo: тестовая БД и SQL-сценарии

## Сборка

Из корня репозитория (WSL/Linux, нужен Boost):

```bash
cmake -S . -B build
cmake --build build --target server client -j4
```

## Запуск сервера

```bash
./build/server/server 5555 ./data
```

Каталог `./data` создаётся автоматически; в git не коммитится.

## Пакетный прогон SQL

В другом терминале, из корня репозитория:

```bash
./build/client/client 5555 demo/sql/setup.sql
./build/client/client 5555 demo/sql/queries.sql
```

Клиент автоматически ждёт завершения async-команд (CREATE, INSERT и т.д.).

## Интерактивный режим

```bash
./build/client/client 5555
```

Примеры:

```sql
USE demo;
SELECT * FROM users;
CREATE TABLE t (id INT NOT_NULL INDEXED, title STRING NOT_NULL);
GET STATUS <request_id>;
GET RESULT <request_id>;
```

В DDL используется модификатор `NOT_NULL` (одно ключевое слово), не `NOT NULL`.

## Автоматический прогон

```bash
sed -i 's/\r$//' demo/run_demo.sh   # если файл редактировали в Windows
chmod +x demo/run_demo.sh
bash demo/run_demo.sh
```

Или одной командой из корня (WSL):

```bash
./build/server/server 5555 data/demo_run &
sleep 1
./build/client/client 5555 demo/sql/setup.sql
./build/client/client 5555 demo/sql/queries.sql
```

## Файлы

| Файл | Назначение |
|------|------------|
| `sql/setup.sql` | Создание БД `demo`, таблиц, начальные данные |
| `sql/queries.sql` | SELECT, UPDATE, BETWEEN, LIKE |
| `sql/async_demo.sql` | Отдельная БД для проверки async DDL/DML |

## Unit-тесты

```bash
ctest --test-dir build --output-on-failure -R 'ExecutorTests|StorageTests|ParserTests'
```
