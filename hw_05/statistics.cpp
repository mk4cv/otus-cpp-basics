#include <iostream>
#include <limits>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>

// Базовый класс для статистических вычислений
class IStatistics {

public:
    // Конструктор
    IStatistics(const char* name, size_t count = 0): m_name(name), m_count(count) {}
    // Деструктор !!!
    virtual ~IStatistics() {}
    // Виртуальная функция обработки каждого полученного числа из входной последовательности
    virtual void update(double next) = 0;
    // Виртуальная функция возврата результатов вычислений
    virtual double eval() const = 0;
    // Виртуальная функция возврата названия статистического вычисления
    virtual const char * name() const {
        return m_name;
    };

protected:
    const char* m_name; // Название статистического вычисления
    size_t m_count; // Число обработанных элементов последовательности    

};


// Класс Min
// Вычисление минимального значения в последовательности
class Min : public IStatistics {
public:
    // Конструктор
    // Инициализация m_min максимальным возможным значением для типа double
    // Любое значение из последовательности будет меньше или равно начальному значению m_min
    Min(): IStatistics("min"), m_min{std::numeric_limits<double>::max()} {}
    // Функция обработки каждого полученного числа из входной последовательности
    // Определение наименьшего элемента последовательности
    void update(double next) override {
        if (next < m_min) {
            m_min = next;
        } // Если полученное значение меньше текущего значения минимума - обновление минимума
        ++m_count;  // Увеличение счетчика обработанных элементов последовательности
    }
    // Функция  получения результата: возврат найденного значения минимума
    double eval() const override {
        // Если последовательность пустая, возврат 0.0
        return m_count > 0 ? m_min : 0.0;
    }
    
private:
    double m_min;   // Текущее минимальное значение
};

// Класс Max
// Вычисление максимального значения в последовательности
class Max : public IStatistics {
public:
    // Конструктор
    // Инициализация m_max минимально возможным значением для типа double
    // Любое значение из последовательности будет больше или равно начальному значению m_max
    Max(): IStatistics("max"), m_max{std::numeric_limits<double>::lowest()} {}
    // Функция обработки каждого полученного числа из входной последовательности
    // Определение наибольшего элемента последовательности
    void update(double next) override {
        if (next > m_max) {
            m_max = next;
        } // Если полученное значение больше текущего значения максимума - обновление максимума
        ++m_count; // Увеличение счетчика обработанных элементов последовательности
    }
    // Функция  получения результата: возврат найденного значение минимума
    double eval() const override {
         // Если последовательность пустая, возврат 0.0
        return m_count > 0 ? m_max : 0.0;
    }
    
private:
    double m_max;   // Текущее максимальное значение
};

// Класс Mean
// Вычисление арифметического среднего значения
class Mean : public IStatistics {
public:
    // Конструктор
    // Инициализация суммы и счетчика нулевыми значениями
    Mean(): IStatistics("mean"), m_sum{0.0} {}
    // Функция обработки каждого полученного числа из входной последовательности
    // Вычисление суммы всех элементов последовательности
    void update(double next) override {
        m_sum += next;  // Суммирование элементов
        ++m_count;  // Увеличение счетчика обработанных элементов последовательности
    }
    // Функция  получения результата: возврат среднего арифметического значения
    double eval() const override {
        // // Если последовательность пустая, возврат 0.0
        return m_count > 0 ? m_sum / m_count : 0.0;
    }

private:
    double m_sum;   // Сумма всех элементов последовательности
};

// Класс Std
// Вычисление среднеквадратического (стандартного) отклонения
class Std : public IStatistics {
public:
    // Конструктор
    // Инициализация суммы, суммы квадратов и счетчика нулевыми значениями
    Std(): IStatistics("std"), m_sum{0.0}, m_sum_sq{0.0} {}
    // Функция обработки каждого полученного числа из входной последовательности
    // Вычисление суммы и суммы квадратов всех элементов последовательности
    void update(double next) override {
        m_sum += next;        // Суммирование элементов
        m_sum_sq += next * next; // Суммирование квадратов элементов
        ++m_count;            // Увеличение счетчика обработанных элементов последовательности
    }
    // Функция  получения результата
    // Возврат среднеквадратического отклонения
    double eval() const override {
        if (m_count <= 1) {
            return 0.0;
        } // Если длина последовательности меньше 2-х, возврат 0.0
        double mean = m_sum / m_count; // Вычисление среднего арифметического значения
        double variance = (m_sum_sq / m_count) - (mean * mean); // Вычисление дисперсии 
        // Стандартное отклонение = квадратный корень из дисперсии
        // Проверка variance > 0 нужна для защиты от ошибок округления
        return variance > 0 ? std::sqrt(variance) : 0.0;
    }
    
private:
    double m_sum;    // Сумма всех элементов последовательности
    double m_sum_sq;  // Сумма квадратов всех элементов последовательности
};

// Класс Percentile
// Базовый класс для вычисления процентилей
class Percentile : public IStatistics {
public:
    // Конструктор
    // Получение значений процентиля
    Percentile(const char* name, double percentile): IStatistics(name), m_percentile{percentile}, m_name{} {}
   // Функция обработки каждого полученного числа из входной последовательности
   // Добавление всех элементов последовательности в массив
    void update(double next) override {
        m_values.push_back(next);  // Добавляем значение в вектор
    }
    // Функция получения результата
    // Вычисление процентиля
    double eval() const override {
        if (m_values.empty()) {
            return 0.0;
        } // Если последовательность пустая, возврат 0.0
        std::vector<double> sorted_values = m_values; // Создание копии массива для сортировки (оригинал не модифицируем)
        std::sort(sorted_values.begin(), sorted_values.end()); // Сортировка значений по возрастанию
        double calc_index = (m_percentile / 100.0) * (sorted_values.size() - 1); // Вычисление индекса процентиля в отсортированном массиве
        size_t lower_index = static_cast<size_t>(std::floor(calc_index));  // Определение нижнего индекса (округление вниз)
        size_t upper_index = static_cast<size_t>(std::ceil(calc_index));   // Определение верхнего индекса (округление вверх)
        // std::cout << "lower index: " << lower_index  << " calc index: " << calc_index << " upper_index: " << upper_index << std::endl; // Для отладки
        // Если индексы совпали или верхний выходит за границы, возврат значения по нижнему индексу
        if (lower_index  == upper_index  || upper_index >= sorted_values.size()) {
            return sorted_values[lower_index];
        }
        // Если индексы не совпали
        // Вычисление линейной интерполяциии между значениями по нижнему и верхнему индексам
        double fraction = calc_index - lower_index;  // Дробная часть индекса
        return sorted_values[lower_index] + fraction * (sorted_values[upper_index] - sorted_values[lower_index]);
    }

private:
    double m_percentile;          // Значение процентиля
    std::vector<double> m_values; // Массив значений всех элементов последовательности
    mutable std::string m_name;   // Название статистистического вычисления (mutable, т.к. в базовом классе это константа)
};

// Класс Percentile_90
// Вычисление 90-го процентиля
class Percentile_90 : public Percentile {
public:
    // Конструктор
    // Передача названия статистического вычисления и значения процентиля в базовый класс Percentile
    Percentile_90() : Percentile("percentile_90", 90.0) {}
};

// Класс Percentile_95
// Вычисление 95-го процентиля
class Percentile_95 : public Percentile {
public:
    // Конструктор
    // Передача названия статистического вычисления и значения процентиля в базовый класс Percentile
    Percentile_95() : Percentile("percentile_95", 95.0) {}
};


int main() {
    const size_t statistics_count = 6; // Размер массива указателей на объекты наследников базового класса IStatistics
    IStatistics *statistics[statistics_count]; // Массив указателей на объекты наследников базового класса IStatistics
    // Создание объектов для всех видов статистический вычислений и запись указателей на них в массив
    statistics[0] = new Min{};    // Минимальное значение
    statistics[1] = new Max{};    // Максимальное значение
    statistics[2] = new Mean{};   // Среднее арифметическое
    statistics[3] = new Std{};    // Среднеквадратическое отклонение
    statistics[4] = new Percentile_90{};  // 90-й процентиль
    statistics[5] = new Percentile_95{};  // 95-й процентиль
    
    // Чтение чисел из стандартного ввода 
    std::cout << "Enter the number sequence: " << std::endl;
    double val = 0; // Переменная для записи числа из стандартного ввода
    while (std::cin >> val) {
        // Обработка полученного числа каждым объектом для статистических вычислений
        for (size_t i = 0; i < statistics_count; ++i) {
            statistics[i]->update(val);
        }
    }
    // Проверка корректности чтения
    if (!std::cin.eof() && !std::cin.good()) {
        std::cerr << "Invalid input data" << std::endl; 
        // Освобождение выделенной памяти через указатель базового класса
        for (size_t i = 0; i < statistics_count; ++i) {
            delete statistics[i];
        }
        return 1; 
    }
    
    // Вычисление статистик каждым объектом и вывод получившихся значений
    std::cout << std::endl;
    std::cout << "Statistics: " << std::endl;
    // Настройка формата отображения чисел типа float
    std::cout << std::fixed;              // Фиксированный формат (не экспоненциальный, для лучшего восприятия)
    std::cout << std::setprecision(4);    // Кол-во знаков после запятой
    for (size_t i = 0; i < statistics_count; ++i) {
        std::cout << statistics[i]->name() << " = " << statistics[i]->eval() << std::endl;
    }

    // Освобождение выделенной памяти через указатель базового класса
    for (size_t i = 0; i < statistics_count; ++i) {
        delete statistics[i];
    }
    
    return 0; 
}