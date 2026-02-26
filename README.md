# csv_median_calculator

Консольное приложение для инкрементального расчёта медианы и других метрик цен из CSV-файлов биржевых торгов.

## Возможности

- Инкрементальная **точная** медиана через две кучи
- Дополнительные метрики через Boost.Accumulators: mean, std_dev, p50, p90, p95, p99 (7.2)
- Параллельное чтение файлов через `std::async` (7.1)
- Потоковый k-way merge для файлов >RAM (7.3)
- Полное покрытие unit-тестами (Catch2)

---

## Требования

| Зависимость | Версия | Поставка |
|---|---|---|
| CMake | ≥ 3.23 | — |
| GCC ≥ 13 / Clang ≥ 16 | C++23 | — |
| Boost (program_options, accumulators) | ≥ 1.74 | Системная |
| toml++ | 3.4.0 | FetchContent |
| spdlog | 1.13.0 | FetchContent |
| Catch2 | 3.4.0 | FetchContent |

```bash
sudo apt install libboost-all-dev
```

---

## Сборка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

---

## Запуск

```bash
./build/csv_median_calculator --config config.toml
./build/csv_median_calculator --cfg config.toml
./build/csv_median_calculator          # ищет config.toml рядом с бинарём
./build/csv_median_calculator --help
```

---

## Unit-тесты

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
# или напрямую:
./build/unit_tests
./build/unit_tests --reporter=compact
```

Покрытие:
- `test_median_calculator` — точность, нечётный/чётный размер, убывающий ввод, сравнение с наивным алгоритмом
- `test_stats_aggregator` — mean, std_dev, percentiles
- `test_csv_reader` — split, оба формата CSV, ошибки в данных
- `test_config_parser` — валидный/невалидный конфиг, все поля

---

## Формат конфигурации

```toml
[main]
input         = './examples/input'   # обязательный
output        = './examples/output'  # опциональный (по умолч. ./output)
filename_mask = ['level', 'trade']   # опциональный ([] = все CSV)

[metrics]
# median, mean, std_dev, p50, p90, p95, p99
enabled = ['median']

[performance]
parallel  = false  # 7.1: параллельное чтение через std::async
streaming = false  # 7.3: k-way merge без загрузки всего в память
```

### Примечания по режимам

**`streaming = true` (7.3):** предполагает, что каждый CSV-файл индивидуально
отсортирован по `receive_ts` (стандарт для биржевых данных). Память на записи — O(k),
где k — число файлов. Память на кучи медианы остаётся O(n): точная медиана требует
хранения всех значений. При `streaming = true` режим parallel игнорируется.

**`parallel = true` (7.1):** каждый файл разбирается в отдельном потоке, после чего
результаты merge-sort объединяются. Ускоряет обработку на многоядерных системах
при наличии нескольких входных файлов.

---

## Формат входных файлов

Разделитель — точка с запятой (`;`).

```
# level.csv
receive_ts;exchange_ts;price;quantity;side;rebuild

# trade.csv
receive_ts;exchange_ts;price;quantity;side
```

---

## Формат выходного файла

`median_result.csv` в `output_dir`. Колонки определяются включёнными метриками:

```csv
receive_ts;price_median
receive_ts;price_median;price_mean;price_std_dev;price_p90;price_p95;price_p99
```

Строка записывается только при изменении **любой** из включённых метрик.

---


## Структура проекта

```
csv_median_calculator/
├── CMakeLists.txt
├── README.md
├── specs.md
├── config.toml
├── src/
│   ├── main.cpp
│   ├── median_calculator.hpp  — точная медиана (две кучи)
│   ├── stats_aggregator.hpp   — Boost.Accumulators: mean/std_dev/percentiles
│   ├── config_parser.hpp      — TOML + метрики + performance
│   └── csv_reader.hpp         — batch / parallel / streaming
├── tests/
│   ├── unit/
│   │   ├── test_median_calculator.cpp
│   │   ├── test_stats_aggregator.cpp
│   │   ├── test_csv_reader.cpp
│   │   └── test_config_parser.cpp
│   └── (интеграционные CSV-данные)
└── examples/
    ├── input/
    └── output/
```
