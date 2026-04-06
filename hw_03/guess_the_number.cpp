#include <iostream>
#include <ctime>
#include <random>
#include <fstream>
#include <string>
#include <map>
#include <sstream>


// Результаты парсинга аргументов
struct Args {
    bool success; // Флаг успешного чтения аргументов
    bool only_table; // Вывод таблицы без игры
    int max_target_value; // Максимальное значение загаданного числа
};

// Функция парсинга аргументов
Args parse_args(int argc, char* argv[]) {
    bool only_table = false; // Вывод таблицы без игры
    int level = 0; // Уровень (0 - значение не заданно)
    int max_target_value = 0; // Максимальное значение загаданного числа

    // Цикл по аргументам
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-table") {
            only_table = true;
             std::cout << "Argument 'table' given - show table without a game" << std::endl;
            break;
        } 
        else if (arg == "-level") {
            if (max_target_value !=0 ) {
                std::cout << "Arguments 'max' and 'level' are specified simultaneously" << std::endl;
                return {false, only_table, max_target_value};
            } // Ошибка, одноврменно заданы аргументы level и max (max_target_value уже определен)
            if (i + 1 < argc) {
                // Присвоение максимального значения загаданного числа в зависимости от заданного уровня
                try {
                    level = std::stoi(argv[++i]); 
                    switch (level) {
                        case 1:
                            max_target_value = 10;
                            break;
                        case 2:
                            max_target_value = 50;
                            break;
                        case 3:
                            max_target_value = 100;
                            break;
                        default:
                            max_target_value = 0;
                    }
                    std::cout << "Maximum value of the hidden number is determined by the argument 'level' " << std::endl;
                } catch (const std::invalid_argument&) {
                    std::cout <<  "Error: argument 'level' requires int value" << std::endl;
                    return {false, only_table, max_target_value};
                }
            }
            else {
                std::cout <<  "Error: argument 'level' requires int  value" << std::endl;
                return {false, only_table, max_target_value};
            }
        } 
        else if (arg == "-max") {
            if (max_target_value !=0 ) {
                std::cout << "Arguments 'max' and 'level' are specified simultaneously" << std::endl;
                return {false, only_table, max_target_value};
            } // Ошибка, одноврменно заданы аргументы level и max (max_target_value уже определен)
            if (i + 1 < argc) {
                // Присвоение максимального значения загаданного числа по соответствущему аргументу
                try {
                    max_target_value = std::stoi(argv[++i]);
                    std::cout << "Maximum value of the hidden number is determined by the argument 'max' " << std::endl;
                } catch (const std::invalid_argument&) {
                    std::cout <<  "Error: argument 'max' requires int value" << std::endl;
                    return {false, only_table, max_target_value};
                }
            } else {
                std::cout <<  "Error: argument 'max' requires int value" << std::endl;
                return {false, only_table, max_target_value};
            }
        }
    }
    // Если максимальное значение загаданного числа не задано аргументами, присвоение по умолчанию
    if (max_target_value == 0) {
        max_target_value = 10;
    }

    return {true, only_table, max_target_value};
}

// Функция записи таблицы результатов
void write_high_scores_table(const std::string high_scores_filename, 
                  const std::string username, 
                  const int score) {
    std::map<std::string, int> high_scores_table; // Словарь с таблицей результатов

    // Построчное чтение существующих записей из файла в словарь с таблицей
    std::ifstream readFile(high_scores_filename);
    if (readFile.is_open()) {
        std::string line;
        while (std::getline(readFile, line)) {
            std::istringstream string_stream(line);
            std::string current_username;
            int current_score;
            if (string_stream >> current_username >> current_score) {
                high_scores_table[current_username] = current_score;
            }
        }
        readFile.close();
    }
    // Добавление или обновление записи пользователя
    auto it = high_scores_table.find(username);
    // Если пользователя нет в файле или счет лучше - запись
    if (it == high_scores_table.end() || score > it->second) {
        high_scores_table[username] = score;
    }
    // Перезапись таблицы в файл
    std::ofstream writeFile(high_scores_filename);
    if (writeFile.is_open()) {
        for (const auto& record : high_scores_table) {
            writeFile << record.first << " " << record.second << "\n";
        }
        writeFile.close();
    } else {
        std::cout << "Error: could not open file for writing:" << high_scores_filename << std::endl;
    }
}

// Функция чтения таблицы реультатов
void read_high_scores_table(const std::string high_scores_filename) {
    std::map<std::string, int> high_scores_table; // Словарь с таблицей результатов

    // Построчное чтение существующих записей из файла в словарь с таблицей
    std::ifstream readFile(high_scores_filename);
    if (readFile.is_open()) {
        std::string line;
        while (std::getline(readFile, line)) {
            std::istringstream string_stream(line);
            std::string current_username;
            int current_score;
            if (string_stream >> current_username >> current_score) {
                // Добавление или обновление записи пользователя
                auto it = high_scores_table.find(current_username);
                // Если пользователя нет в словаре с таблицей или счет лучше - запись
                if (it == high_scores_table.end() || current_score > it->second) {
                    high_scores_table[current_username] = current_score;
                }
            }
        }
        readFile.close();
    } else {
        std::cout << "Error: could not open file for read: " << high_scores_filename << std::endl;
        return;
    } // Ошибка чтения файла

    // Цикл вывода таблицы
    std::cout << "High scores table:" << std::endl;
    for (const auto& [key, value] : high_scores_table) {
        std::cout << key << ": " << value << "\n";
    }
}

// Функция игры
int game(int max_target_value) {
    int current_value = 0; // Текущее введенное значение
    int attempts_count = 0; // Итоговое число попыток за время игры

    // Определение загаданного числа алгоритмом случайных чисел, ограниченного максимальным значением
    std::srand(std::time(nullptr));
    int target_value = std::rand() % max_target_value + 1;
    // Для отладки - вывод загаданного числа
    // std::cout << "Target value: " << target_value<< std::endl;
    // Главный цикл игры
    while (current_value != target_value) {
        std::cin >> current_value;
        if (current_value < target_value) {
			std::cout << "target value is greater than " << current_value << std::endl;
            attempts_count += 1;
		}
		else if (current_value > target_value) {
			std::cout << "target value is less than " << current_value << std::endl;
            attempts_count += 1;
		}
		else {
			std::cout << "you win!" << std::endl;
			break;
		}
    }

    return attempts_count;
}

// Главаная функция
int main(int argc, char* argv[]) {
    std::string username; // Имя игрока
    int attempts_count; // Итоговое число попыток за время игры
    const std::string high_scores_filename = "high_scores.txt"; // Имя файла с таблицей результатов

    std::cout << "Welcome to 'Guess the Number' game!" << std::endl;
    // Парсинг аргументов
    Args result = parse_args(argc, argv);
    if (!result.success) {
        std::cout << "Incorrect launch arguments, program will be stopped" << std::endl;
        return 1;
    } // Ошибка парсинга аргументов, завершение программы
    // Если задан только вывод таблицы без игры
    if (result.only_table) {
        read_high_scores_table(high_scores_filename);
        return 0;
    }
    // Запрос имени пользователя
    std::cout << "Enter your name: ";
    std::cin >> username;
    // Вывод максимального значения загаданного числа
    std::cout << "Max target value: " << result.max_target_value << std::endl;
    // Игра
    attempts_count = game(result.max_target_value);
    // Запись результата в таблицы
    write_high_scores_table(high_scores_filename, username, attempts_count);
    // Чтение и отображение таблицы результатов
    read_high_scores_table(high_scores_filename);

    return 0;
}