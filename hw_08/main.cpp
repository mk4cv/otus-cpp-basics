#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>
#include <thread>
#include <chrono>
#include "CRC32.hpp"
#include "IO.hpp"


/**
 * Функция-обертка для запуска алгоритма в отдельном потоке.
 * @brief Поиск подходящего 4-байтового значения в заданном диапазоне.
 * Работает только с локальными переменными и выделенными ячейками вывода,
 * что гарантирует отсутствие гонок данных (без использования мьютексов).
 */
void searchRange(uint32_t start, uint32_t count, uint32_t prevState,
                 uint32_t targetCrc, uint32_t* outValue, uint8_t* outFound) {
    uint64_t end = static_cast<uint64_t>(start) + count;
    for (uint64_t val = start; val < end; ++val) {
        uint32_t candidate = static_cast<uint32_t>(val);
        // Вычисление CRC только для 4 изменяемых байт, продолжая от 
        // ранее вычисленного состояния неизменяемого префикса (для доп. задания №1).
        uint32_t currentCrc = crc32(reinterpret_cast<const char*>(&candidate), 4, prevState);
        if (currentCrc == targetCrc) {
            *outValue = candidate;
            *outFound = true;
            return; // Выход в случае успеха
        }
    }
}

/**
 * Функция динамической балансировки (для доп. задания №2).
 * @brief Определяет оптимальное количество потоков на основе бенчмарка.
 * Запускает короткие прогоны поиска с разным кол-ом потоков, 
 * замеряет время выполнения и выбирает конфигурацию с минимальным временем выполнения.
 */
size_t determineOptimalThreads(size_t maxThreads) {
    std::vector<size_t> threadCounts;
    for (size_t t = 1; t <= maxThreads; t *= 2) {
        threadCounts.push_back(t);
    }
    // Генерация данных для бенчмарка
    const uint32_t benchmark_size = 1 << 21;
    uint32_t dummyPrev = ~crc32("bench", 5, 0xFFFFFFFF);
    uint32_t dummyTarget = 0;
    // Последовательный замер для каждого количества потоков
    double bestTime = std::numeric_limits<double>::max();
    size_t bestT = 1;
    for (size_t t : threadCounts) {
        uint64_t chunk = benchmark_size / t;
        std::vector<std::thread> threads;
        threads.reserve(t);
        // Изолированные массивы для результатов.
        std::vector<uint32_t> res(t);
       std::vector<uint8_t> found(t, false);
        auto start = std::chrono::high_resolution_clock::now();
        // Запуск потоков
        for (size_t i = 0; i < t; ++i) {
            uint32_t s = static_cast<uint32_t>(i * chunk);
            uint32_t c = static_cast<uint32_t>(std::min(chunk, static_cast<uint64_t>(benchmark_size) - i * chunk));
            threads.emplace_back(searchRange, s, c, dummyPrev, dummyTarget, &res[i], &found[i]);
        }
        for (auto& th : threads) th.join();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        if (elapsed.count() < bestTime) {
            bestTime = elapsed.count();
            bestT = t;
        } // Запись наименьшего времени выполнения
    }
    return bestT;
}

/**
 * Основная функция взлома CRC32.
 * @brief Формирует новый вектор с тем же CRC32, добавляя в конец оригинального
 * строку injection и дополнительные 4 байта
 * @details При формировании нового вектора последние 4 байта не несут полезной
 * нагрузки и подбираются таким образом, чтобы CRC32 нового и оригинального
 * вектора совпадали
 * @param original оригинальный вектор
 * @param injection произвольная строка, которая будет добавлена после данных
 * оригинального вектора
 * @return новый вектор
 */
std::vector<char> hack(const std::vector<char> &original, const std::string &injection) {
    const uint32_t originalCrc32 = crc32(original.data(), original.size());
    /** Оптимизация (для доп. задания №1)
    * В исходной версии CRC32 пересчитывается для всего буфера на каждой итерации.
    * В оптимизированной версии состояние хеша вычисляется только для неизменяемой части (оригинал+инъекция) один раз. 
    * Функция crc32 возвращает инвертированное состояние, поэтому для 
    * продолжения вычислений применяется ещё одно инвертирование.
    */ 
    uint32_t prevState = 0xFFFFFFFF;
    prevState = ~crc32(original.data(), original.size(), prevState);
    prevState = ~crc32(injection.data(), injection.size(), prevState);
    // Динамическая балансировка (для доп. задания №2).
    size_t maxThreads = std::thread::hardware_concurrency();
    size_t t = determineOptimalThreads(maxThreads);
    std::cout << "Dynamic profiling complete. Optimal thread count: " << t << std::endl;
    // Многопоточный перебор.
    // Разделение полного пространства на t непересекающихся субдиапазонов.
    const uint64_t total = (uint64_t)1 << 32;
    const uint64_t chunkSize = total / t;
    std::vector<std::thread> threads;
    threads.reserve(t);
    // Изолированные массивы для результатов каждого потока (позволяет избежать использование мьютексов).
    std::vector<uint32_t> results(t, 0);
   std::vector<uint8_t> found(t, false);
    // Запуск потоков
    for (size_t i = 0; i < t; ++i) {
        uint32_t start = static_cast<uint32_t>(i * chunkSize);
        // Обработка остатка от деления для последнего потока
        uint32_t count = static_cast<uint32_t>(std::min(chunkSize, total - i * chunkSize));
        threads.emplace_back(searchRange, start, count, prevState, originalCrc32, &results[i], &found[i]);
    }
    // Ожидание завершения всех потоков.
    for (auto &th : threads) th.join();
    uint32_t solution = 0;
    bool success = false;
    // Поиск результата в изолированных массивах.
    for (size_t i = 0; i < t; ++i) {
        if (found[i]) {
            solution = results[i];
            success = true;
            break;
        }
    }
    if (!success) {
        throw std::logic_error("Can't hack: target CRC32 not found in 32-bit space");
    }
    std::cout << "Success\n";
    // Формирование итогового буфера
    size_t prefixLen = original.size() + injection.size();
    std::vector<char> result(prefixLen + 4);
    auto it = std::copy(original.begin(), original.end(), result.begin());
    std::copy(injection.begin(), injection.end(), it);
    std::copy_n(reinterpret_cast<const char*>(&solution), 4, result.end() - 4);
    
    return result;
}

// Главная функция.
int main(int argc, char **argv) {
    if (argc != 3) {
    std::cerr << "Call with two args: " << argv[0]
              << " <input file> <output file>\n";
    return 1;
    }

    try {
        const std::vector<char> data = readFromFile(argv[1]);
        const std::vector<char> badData = hack(data, "He-he-he");
        writeToFile(argv[2], badData);
    } catch (std::exception &ex) {
        std::cerr << ex.what() << '\n';
        return 2;
    }
    return 0;
}