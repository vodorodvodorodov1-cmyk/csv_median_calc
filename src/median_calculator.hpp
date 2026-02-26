/**
 * \file median_calculator.hpp
 * \brief Точная инкрементальная медиана через две кучи
 */
#pragma once

#include <functional>
#include <queue>
#include <stdexcept>

namespace stats {

/**
 * \brief Точная инкрементальная медиана.
 *
 * Boost.Accumulators даёт только приближённую медиану (алгоритм P-Square),
 * поэтому используется структура из двух куч с гарантией точности.
 */
class median_calculator {
public:
    void add(double value_) noexcept {
        if (_lower.empty() || value_ <= _lower.top()) {
            _lower.push(value_);
        } else {
            _upper.push(value_);
        }
        _rebalance();
    }

    /**
     * \return точная медиана всех добавленных значений
     * \warning бросает std::logic_error если структура пуста
     */
    [[nodiscard]] double median() const {
        if (_lower.empty()) [[unlikely]] {
            throw std::logic_error("Нет данных для вычисления медианы");
        }
        if (_lower.size() == _upper.size()) {
            return (_lower.top() + _upper.top()) / 2.0;
        }
        return _lower.top();
    }

    [[nodiscard]] bool        empty() const noexcept { return _lower.empty(); }
    [[nodiscard]] std::size_t size()  const noexcept { return _lower.size() + _upper.size(); }

private:
    std::priority_queue<double>                                               _lower;
    std::priority_queue<double, std::vector<double>, std::greater<double>>   _upper;

    void _rebalance() noexcept {
        if (_lower.size() > _upper.size() + 1) {
            _upper.push(_lower.top());
            _lower.pop();
        } else if (_upper.size() > _lower.size()) {
            _lower.push(_upper.top());
            _upper.pop();
        }
    }
};

} // namespace stats
