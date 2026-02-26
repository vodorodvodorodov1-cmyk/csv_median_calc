#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <ranges>
#include <vector>

#include "median_calculator.hpp"

using stats::median_calculator;
using Catch::Matchers::WithinAbs;

TEST_CASE("median_calculator: пустая структура бросает исключение") {
    median_calculator calc;
    REQUIRE(calc.empty());
    REQUIRE_THROWS_AS(calc.median(), std::logic_error);
}

TEST_CASE("median_calculator: один элемент") {
    median_calculator calc;
    calc.add(42.0);
    REQUIRE(calc.size() == 1);
    REQUIRE_THAT(calc.median(), WithinAbs(42.0, 1e-9));
}

TEST_CASE("median_calculator: два элемента — среднее") {
    median_calculator calc;
    calc.add(10.0);
    calc.add(20.0);
    REQUIRE_THAT(calc.median(), WithinAbs(15.0, 1e-9));
}

TEST_CASE("median_calculator: нечётное количество — центральный элемент") {
    median_calculator calc;
    for (double v : {3.0, 1.0, 2.0}) calc.add(v);
    REQUIRE_THAT(calc.median(), WithinAbs(2.0, 1e-9));
}

TEST_CASE("median_calculator: пример из ТЗ") {
    median_calculator calc;
    calc.add(100.0); REQUIRE_THAT(calc.median(), WithinAbs(100.0, 1e-9));
    calc.add(101.0); REQUIRE_THAT(calc.median(), WithinAbs(100.5, 1e-9));
    calc.add(102.0); REQUIRE_THAT(calc.median(), WithinAbs(101.0, 1e-9));
    calc.add(103.0); REQUIRE_THAT(calc.median(), WithinAbs(101.5, 1e-9));
}

TEST_CASE("median_calculator: убывающая последовательность") {
    median_calculator calc;
    calc.add(100.0); REQUIRE_THAT(calc.median(), WithinAbs(100.0, 1e-9));
    calc.add(90.0);  REQUIRE_THAT(calc.median(), WithinAbs(95.0,  1e-9));
    calc.add(80.0);  REQUIRE_THAT(calc.median(), WithinAbs(90.0,  1e-9));
    calc.add(70.0);  REQUIRE_THAT(calc.median(), WithinAbs(85.0,  1e-9));
}

TEST_CASE("median_calculator: все одинаковые значения") {
    median_calculator calc;
    for (int i = 0; i < 5; ++i) calc.add(50.0);
    REQUIRE_THAT(calc.median(), WithinAbs(50.0, 1e-9));
}

TEST_CASE("median_calculator: чередование нечётного и чётного числа элементов") {
    median_calculator calc;
    calc.add(10.0); REQUIRE_THAT(calc.median(), WithinAbs(10.0, 1e-9)); // 1 эл
    calc.add(20.0); REQUIRE_THAT(calc.median(), WithinAbs(15.0, 1e-9)); // 2 эл
    calc.add(30.0); REQUIRE_THAT(calc.median(), WithinAbs(20.0, 1e-9)); // 3 эл
    calc.add(40.0); REQUIRE_THAT(calc.median(), WithinAbs(25.0, 1e-9)); // 4 эл
    calc.add(50.0); REQUIRE_THAT(calc.median(), WithinAbs(30.0, 1e-9)); // 5 эл
}

TEST_CASE("median_calculator: результат совпадает с наивным алгоритмом сортировки") {
    median_calculator calc;
    std::vector<double> sorted;

    // Добавляем 99 нечётных чисел в произвольном порядке
    std::vector<double> vals;
    for (int i = 1; i <= 99; i += 2) vals.push_back(static_cast<double>(i));
    // Перемешиваем чтобы проверить независимость от порядка вставки
    std::ranges::reverse(vals);

    for (double v : vals) {
        calc.add(v);
        sorted.push_back(v);
        std::ranges::sort(sorted);

        const double naive = (sorted.size() % 2 == 1)
            ? sorted[sorted.size() / 2]
            : (sorted[sorted.size() / 2 - 1] + sorted[sorted.size() / 2]) / 2.0;

        REQUIRE_THAT(calc.median(), WithinAbs(naive, 1e-9));
    }
}
