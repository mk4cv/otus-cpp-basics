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
#include <thread>
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
void process_files_worker(const std::vector<std::string> filenames, Counter& local_counter) {
    for (const auto& filename : filenames) {
        std::ifstream input{filename};
        if (!input.is_open()) {
            std::cerr << "Failed to open file " << filename << '\n';
            continue; 
        }
        
        std::for_each(std::istream_iterator<std::string>(input),
                      std::istream_iterator<std::string>(),
                      [&local_counter](const std::string &s) { 
                          ++local_counter[tolower(s)]; 
                      });
    }
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
            // При равной частоте - сортировка по алфавиту
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

    std::vector<std::string> all_files;
    for (int i = 1; i < argc; ++i) {
        all_files.push_back(argv[i]);
    }

    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 2;
    
    // Ограничение кол-ва потоков количеством файлов, если файлов мало
    if (num_threads > all_files.size()) num_threads = all_files.size();

    // Структуры для хранения потоков и их локальных результатов
    std::vector<std::thread> threads;
    std::vector<Counter> local_counters(num_threads);
    
    // Распределение файлов по батчам
    std::vector<std::vector<std::string>> batches(num_threads);
    for (size_t i = 0; i < all_files.size(); ++i) {
        batches[i % num_threads].push_back(all_files[i]);
    }

    // 1. Запуск потоков
    for (size_t i = 0; i < num_threads; ++i) {
        if (batches[i].empty()) continue;
        
        // Создание потка с передачей батча файлов и ссылки на соответствующий локальный словарь
        // Передача ссылки в конструктор std::thread осуществляетс с помощью std::ref
        threads.emplace_back(process_files_worker, std::move(batches[i]), std::ref(local_counters[i]));
    }

    // 2. Ожидание завершения всех потоков
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // 3. Сбор результатов в глобальный словарь
    Counter global_freq_dict;
    for (const auto& local_dict : local_counters) {
        for (auto const& [word, count] : local_dict) {
            global_freq_dict[word] += count;
        }
    }

    // Вывод финального результата 
    print_topk(std::cout, global_freq_dict, TOPK);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Elapsed time is " << elapsed_ms.count() << " us\n";

}