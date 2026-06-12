#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <vector>
#include <chrono>
#include <future>
#include <string>


const size_t TOPK = 10;

using Counter = std::map<std::string, std::size_t>;


// Функция приведения строки к нижнему регистру
std::string tolower(const std::string &str) {
    std::string lower_str;
    lower_str.reserve(str.size()); // Резервирование памяти
    std::transform(std::cbegin(str), std::cend(str),
                   std::back_inserter(lower_str),
                   [](unsigned char ch) { return std::tolower(ch); });
    return lower_str;
}

// Функция обработки файлов внутри одного потока
Counter process_files(const std::vector<std::string>& filenames) {
    Counter local_counter;
    for (const auto& filename : filenames) {
        std::ifstream input{filename};
        if (!input.is_open()) {
            std::cerr << "Failed to open file " << filename << '\n';
            continue; 
        }
        // Чтение слов и создание локального словаря
        std::for_each(std::istream_iterator<std::string>(input),
                      std::istream_iterator<std::string>(),
                      [&local_counter](const std::string &s) { 
                          // Инкремент счетчика для приведенного к нижнему регистру слова
                          ++local_counter[tolower(s)]; 
                      });
    }
    return local_counter;
}

// Функция сортировки и вывода K наиболее частых слов
void print_topk(std::ostream& stream, const Counter& counter, const size_t k) {
    if (counter.empty()) return;
    std::vector<Counter::const_iterator> words;
    words.reserve(counter.size());
    for (auto it = std::cbegin(counter); it != std::cend(counter); ++it) {
        words.push_back(it);
    }
    // Определение реального кол-ва элементов для вывода (для случая если слов меньше, чем K)
    size_t actual_k = std::min(k, words.size());

    std::partial_sort(
        std::begin(words), std::begin(words) + actual_k, std::end(words),
        [](auto lhs, auto &rhs) { 
            // Сортировка по убыванию частоты
            if (lhs->second != rhs->second)
                return lhs->second > rhs->second;
            // При равной частоте — сортировка по алфавиту
            return lhs->first < rhs->first;
        });

    std::for_each(
        std::begin(words), std::begin(words) + actual_k,
        [&stream](const Counter::const_iterator &pair) {
            stream << std::setw(4) << pair->second << " " << pair->first << '\n';
        });
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: topk_words [FILES...]\n";
        return EXIT_FAILURE;
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Получение списка имен файлов из аргументов командной строки
    std::vector<std::string> all_files;
    for (int i = 1; i < argc; ++i) {
        all_files.push_back(argv[i]);
    }

    // Определение кол-ва ядер процессора
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 2;
    
    // Распределение файлов по батчам равномерно для каждого потока 
    std::vector<std::vector<std::string>> batches(num_threads);
    for (size_t i = 0; i < all_files.size(); ++i) {
        batches[i % num_threads].push_back(all_files[i]);
    }

    // Запуск асинхронных задач
    std::vector<std::future<Counter>> futures; // Массив для результатов
    for (auto& batch : batches) {
        if (batch.empty()) continue;
        futures.push_back(std::async(std::launch::async, process_files, std::move(batch))); // Запуск задач в новом потоке
    }

    // Сбор и объединение результатов
    Counter global_freq_dict;
    for (auto& fut : futures) {
        Counter local_dict = fut.get(); // Блокировка выполнения до завершения потока
        // Запись локальных результатов в глобальный словарь
        for (auto const& [word, count] : local_dict) {
            global_freq_dict[word] += count;
        }
    }

    // Вывод финального результата 
    print_topk(std::cout, global_freq_dict, TOPK);

    // Вывод времени вычисления
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Elapsed time is " << elapsed_ms.count() << " us\n";

}