#include <iostream>      // Для ввода-вывода (std::cout, std::endl)
#include <stdexcept>     // Для исключений (std::out_of_range)
#include <utility>       // Для std::move и семантики перемещения
#include <cstddef>       // Для size_t (тип для размеров и индексов)

// Последовательный контейнер
template<typename T>
class SequentialContainer {
private:
    T* m_data;                    // Указатель на динамический массив с данными
    size_t m_size;           // Текущее количество элементов в контейнере
    size_t m_capacity;       // Вместимость массива (сколько элементов можно хранить без реаллокации)
    static constexpr float m_growth_k = 1.5f; // Коэффициент роста для резервирования памяти (для доп. задания №1)
public:
    // Конструктор по умолчанию
    // Cоздание пустого контейнера
    SequentialContainer() : m_data(nullptr), m_size(0), m_capacity(0) {}
    // Конструктор с начальным размером
    // Резервирование места под n элементов
    explicit SequentialContainer(size_t n) : m_size(0), m_capacity(n) {
        m_data = new T[m_capacity];  // Выделение памяти под кол-во m_capacity элементов типа T
    }
    // Конструктор копирования
    // Cоздание глубокой копии другого контейнера
    SequentialContainer(const SequentialContainer& other) 
        : m_size(other.m_size), m_capacity(other.m_capacity) {
        m_data = new T[m_capacity];  // Выделение новой памяти
        for (size_t i = 0; i < m_size; ++i) {  // Копирование каждого элемента
            m_data[i] = other.m_data[i];  // Поэлементное копирование
        }
    }
    // Конструктор перемещения (для доп. задания №3)
    // Забирает ресурсы у временного объекта
    SequentialContainer(SequentialContainer&& other) noexcept 
        : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
        other.m_data = nullptr;      // Обнуление указателя источника, чтобы деструктор не освободил память
        other.m_size = 0;         
        other.m_capacity = 0;        
    }
    // Оператор присваивания копированием
    // Замена содержимого копией другого контейнера
    SequentialContainer& operator=(const SequentialContainer& other) {
        if (this != &other) {
            // Если вместимости достаточно, переиспользование памятя
            if (m_capacity >= other.m_size) {
                for (size_t i = 0; i < other.m_size; ++i) {
                    m_data[i] = other.m_data[i];
                }
                m_size = other.m_size;
            } else {
                // Если вместимости не хватает, выделение нового буфера
                delete[] m_data;
                m_size = other.m_size;
                m_capacity = other.m_capacity;
                m_data = new T[m_capacity];
                for (size_t i = 0; i < m_size; ++i) {
                    m_data[i] = other.m_data[i];
                }
            }
        }
        return *this;
    }
    // Оператор присваивания перемещением (для доп. задания №3):
    // Перенос ресурсов
    SequentialContainer& operator=(SequentialContainer&& other) noexcept {
        if (this != &other) {  // Защита от самоприсваивания
            SequentialContainer tmp(std::move(*this)); // Перемещение указателя на выделенную память во временный объект
            delete[] m_data;     // Освобождение текущей памяти
            m_data = other.m_data;        // Передача указателя на данные
            m_size = other.m_size;        // Передача  размера
            m_capacity = other.m_capacity; // Передача  вместимости
            other.m_data = nullptr;      // Обнуление источника
            other.m_size = 0; // Обнуление размера
            other.m_capacity = 0; // Обнуление вместимости
        }
        return *this;  // Возврат ссылки на текущий объект
    }
    // Деструктор
    // Освобождение динамически выделенной памяти
    ~SequentialContainer() {
        delete[] m_data;
    }
    // Функции контейнера
    //
    // Метод добавления элемента в конец контейнера (push_back)
    void push_back(const T& value) {
        if (m_size >= m_capacity) {  // Если место закончилось, увеличение вместимости
            reserve(static_cast<size_t>(m_capacity * m_growth_k) + 1);
        }
        m_data[m_size++] = value;  // Добавление элемента в конец
    }
    // Перегрузка push_back для rvalue-ссылок (для доп. задания №3)
    void push_back(T&& value) {
        if (m_size >= m_capacity) {  // Если место закончилось, увеличение вместимости
            reserve(static_cast<size_t>(m_capacity * m_growth_k) + 1);
        }
        m_data[m_size++] = std::move(value);  // Перемещение значения
    }
    // Метод резервирования памяти (для доп. задания №1)
    void reserve(size_t newCapacity) {
        if (newCapacity <= m_capacity) return;  // Если новая вместимость меньше текущей
        T* newData = new T[newCapacity];  // Выделение нового массива большей вместимости
        for (size_t i = 0; i < m_size; ++i) {  // Копирование элементов
            newData[i] = std::move_if_noexcept(m_data[i]);  // move_if_noexcept для эффективности
        }
        delete[] m_data;  // Удаление старого массива
        m_data = newData;  // Обновление указателя на новый массив
        m_capacity = newCapacity;  // Обновление значения вместимости
    }

    
    // Метод вставки элемента в произвольную позицию (insert)
    void insert(size_t index, const T& value) {
        if (index > m_size) {  
            throw std::out_of_range("Index out of range");
        } // Проверка на выход за границы
        if (m_size >= m_capacity) {  
            reserve(static_cast<size_t>(m_capacity * m_growth_k) + 1);
        } // Если контейнер заполнен, увеличение вместимости    
        for (size_t i = m_size; i > index; --i) {
            m_data[i] = std::move(m_data[i - 1]); // Перемещение
        }  // Сдвиг элементов перемещением вправо, начиная с позиции index, чтобы освободить место
        m_data[index] = value;  // Вставка нового элемента на освободившееся место
        ++m_size;  // Увеличение размера контейнера
    }
    // Метод удаления элемента по индексу (erase)
    void erase(size_t index) {
        if (index >= m_size) {  // Проверка на выход за границы
            throw std::out_of_range("Index out of range");
        }
        // Сдвиг элементов влево, начиная с позиции index+1, чтобы закрыть "пустые" элементы
        for (size_t i = index; i < m_size - 1; ++i) {
            m_data[i] = std::move(m_data[i + 1]);  
        } // Перемещение элементов
        --m_size;  // Уменьшение размера контейнера
    }
    // Метод получения текущего размера контейнера
    size_t size() const {
        return m_size;
    }
    // Оператор доступа по индексу (operator[])
    // Версия без константности
    T& operator[](size_t index) {
        if (index >= m_size) {  
            throw std::out_of_range("Index out of range");
        } // Проверка на выход за границы
        return m_data[index];  // Возврат ссылки на элемент
    }
    // Оператор доступа по индексу
    // Константная версия
    const T& operator[](size_t index) const {
        if (index >= m_size) {  
            throw std::out_of_range("Index out of range");
        } // // Проверка на выход за границы
        return m_data[index];  // Возврат константной ссылки на элемент
    }
    // Реализация итераторов (для доп. задания №4)
    //
    // Класс итератора для обхода контейнера
    class iterator {
    private:
        T* m_ptr;  // Указатель на текущий элемент в массиве       
    public:
        // Конструктор итератора
        // Инициализация указателя
        explicit iterator(T* ptr) : m_ptr(ptr) {}
        // Оператор разыменования
        // Возврат ссылки на текущий элемент
        T& operator*() { return *m_ptr; }
        // Префиксный инкремент
        // Переход к следующему элементу и возврат нового итератора
        iterator& operator++() {
            ++m_ptr;  // Увеличение указателя на размер одного элемента
            return *this;  // Возврат ссылки на текущий объект класса
        }
        // Постфиксный инкремент
        // Возврат старого значения, переход вперед
        iterator operator++(int) {
            iterator tmp = *this;  // Запись текущего состояния
            ++(*this);             // Вызов префиксного инкремента
            return tmp;            // Возврат старого значения
        }
        // Оператор сравнения указателей на неравенство
        bool operator!=(const iterator& other) const {
            return m_ptr != other.m_ptr;  // 
        }
        // Оператор сравнения указателей на равенство
        bool operator==(const iterator& other) const {
            return m_ptr == other.m_ptr; 
        }
        // Метод get() для получения значения (альтернатива operator)
        T& get() { return *m_ptr; }
    };
    // Метод begin()
    // Возврат итератора на первый элемент
    iterator begin() {
        return iterator(m_data);  // Итератор с указателем на начало массива
    }
    // Метод end()
    // Возврат итератора на позицию после последнего элемента
    iterator end() {
        return iterator(m_data + m_size);  // Указатель на ячейку после последнего элемента
    }
    // Класс итератора для обхода константного контейнера
    class const_iterator {
    private:
        const T* m_ptr;  // Константный указатель на текущий элемент в массиве    
    public:
        // Конструктор итератора
        // Инициализация указателя
        explicit const_iterator(const T* ptr) : m_ptr(ptr) {}
        // Оператор разыменования
        // Возврат константной ссылки на текущий элемент
        const T& operator*() const { return *m_ptr; }
        // Префиксный инкремент
         // Переход к следующему элементу и возврат нового итератора
        const_iterator& operator++() {
            ++m_ptr;
            return *this;
        }
        // Постфиксный инкремент
        // Возврат старого значения, переход вперед
        const_iterator operator++(int) {
            const_iterator tmp = *this; // Запись текущего состояния
            ++(*this); // Вызов префиксного инкремента
            return tmp; // Возврат старого значения
        }
        // Оператор сравнения указателей на неравенств
        bool operator!=(const const_iterator& other) const {
            return m_ptr != other.m_ptr;
        }
        // Оператор сравнения указателей на равенство
        bool operator==(const const_iterator& other) const {
            return m_ptr == other.m_ptr;
        }
        // Метод get() для получения значения (альтернатива operator)
        const T& get() const { return *m_ptr; }
    };
    // Метод cbegin()
    // Возврат итератора на первый константный элемент
    const_iterator cbegin() const {
        return const_iterator(m_data);
    }
    // Метод cend()
    // Возврат итератора на позицию после последнего константного элемента
    const_iterator cend() const {
        return const_iterator(m_data + m_size);
    }
};

// Узел однонаправленного списка (для доп. задания №2)
template<typename T>
struct SinglyNode {
    T m_data;                    // Хранимые данные
    SinglyNode* m_next;          // Указатель на следующий узел
    
    // Конструктор (с инициализацией данных)
    explicit SinglyNode(const T& value) : m_data(value), m_next(nullptr) {}
    // Конструктор перемещения (для доп. задания №3)
    explicit SinglyNode(T&& value) : m_data(std::move(value)), m_next(nullptr) {}
};

// Однонаправленный список (для доп. задания №2)
template<typename T>
class SinglyLinkedList {
private:
    SinglyNode<T>* m_head;      // Указатель на первый узел списка
    SinglyNode<T>* m_tail;      // Указатель на последний узел
    size_t m_size;         // Текущее количество элементов
    // Метод полной очистки списка
    void clear() noexcept {
        while (m_head) {
            SinglyNode<T>* tmp = m_head; // Запись указателя на текущий узел
            m_head = m_head->m_next; // Переход к следующему узлу
            delete tmp; // Удаление текущего узла
        }
        m_tail = nullptr;
        m_size = 0;
    }
public:
    // Конструктор по умолчанию
    SinglyLinkedList() : m_head(nullptr), m_tail(nullptr), m_size(0) {}
    // Деструктор
    // Освобождение всех узлов списка
    ~SinglyLinkedList() {
        clear();  // Очистка текущего списка
    }
    // Конструктор копирования
    // Копирование каждого элемента
    SinglyLinkedList(const SinglyLinkedList& other) : m_head(nullptr), m_tail(nullptr), m_size(0) {
        for (SinglyNode<T>* curr = other.m_head; curr; curr = curr->m_next) {
            push_back(curr->m_data);  
        }
    }
    // Конструктор перемещения (для доп. задания №3)
    SinglyLinkedList(SinglyLinkedList&& other) noexcept 
        : m_head(other.m_head), m_tail(other.m_tail), m_size(other.m_size) {
        // Обнуление источника
        other.m_head = nullptr;  
        other.m_tail = nullptr;
        other.m_size = 0;
    }
    // Оператор присваивания копированием
    SinglyLinkedList& operator=(const SinglyLinkedList& other) {
        if (this != &other) {
            clear();  // Очистка текущнго списка
            for (SinglyNode<T>* curr = other.m_head; curr; curr = curr->m_next) {
                push_back(curr->m_data);
            } // Копирование элементов
        }
        return *this;
    }
    // Оператор присваивания перемещением
    SinglyLinkedList& operator=(SinglyLinkedList&& other) noexcept {
        if (this != &other) {
            SinglyLinkedList tmp(std::move(*this)); // Перемещение указателя на выделенную память во временный объект
            clear();  // Очистка текущнго списка
            // Передача ресурсов
            m_head = other.m_head;
            m_tail = other.m_tail;
            m_size = other.m_size;
            // Обнуление источника
            other.m_head = nullptr;
            other.m_tail = nullptr;
            other.m_size = 0;
        }
        return *this;
    }
    // Функции контейнера
    //
    // Метод добавления элемента в конец контейнера (push_back)
    void push_back(const T& value) {
        SinglyNode<T>* newNode = new SinglyNode<T>(value); 
        if (!m_head) {  // Если список пуст
            m_head = m_tail = newNode;  // Новый узел становится головой и хвостом
        } else {
            m_tail->m_next = newNode;  // Прикрепление нового узла к концу
            m_tail = newNode;        // Обновление указателя на хвост
        }
        ++m_size;  // Увеличение размера
    }
    // Метод добавления элемента в конец контейнера (push_back) с семантикой перемещения
    void push_back(T&& value) {
        SinglyNode<T>* newNode = new SinglyNode<T>(std::move(value));
        if (!m_head) {
            m_head = m_tail = newNode;
        } else {
            m_tail->m_next = newNode;
            m_tail = newNode;
        }
        ++m_size;
    }
    // Метод вставки элемента в произвольную позицию (insert)
    void insert(size_t index, const T& value) {
        if (index > m_size) throw std::out_of_range("Index out of range"); // Проверка на выход за границы
        if (index == 0) {  // Вставка в начало
            SinglyNode<T>* newNode = new SinglyNode<T>(value);
            newNode->m_next = m_head;
            m_head = newNode;
            if (!m_tail) m_tail = newNode;  // Если список был пуст
            ++m_size;
            return;
        }
        // Поиск узла перед позицией вставки
        SinglyNode<T>* curr = m_head;
        for (size_t i = 0; i < index - 1; ++i) {
            curr = curr->m_next;
        }
        SinglyNode<T>* newNode = new SinglyNode<T>(value);
        newNode->m_next = curr->m_next;  // Вставка нового узла
        curr->m_next = newNode;
        if (newNode->m_next == nullptr) m_tail = newNode;  // Если вставка произошла в конец
        ++m_size;
    }
    // Метод удаления элемента по индексу (erase)
    void erase(size_t index) {
        if (index >= m_size) throw std::out_of_range("Index out of range"); // Проверка на выход за границы
        SinglyNode<T>* toDelete;
        if (index == 0) {  // Удаление головы
            toDelete = m_head;
            m_head = m_head->m_next;
            if (!m_head) m_tail = nullptr; 
        } else {
            // Поиск узла перед удаляемым
            SinglyNode<T>* curr = m_head;
            for (size_t i = 0; i < index - 1; ++i) {
                curr = curr->m_next;
            }
            toDelete = curr->m_next;
            curr->m_next = toDelete->m_next;
            if (toDelete == m_tail) m_tail = curr;  // Если произведено удаления хвоста
        }
        delete toDelete;  // Освобождение памяти
        --m_size;
    }
    // Метод получения текущего размера контейнера
    size_t size() const { return m_size; }
    // Оператор доступа по индексу (operator[])
    // Версия без константности
    T& operator[](size_t index) {
        if (index >= m_size) throw std::out_of_range("Index out of range"); // Проверка на выход за границы
        SinglyNode<T>* curr = m_head;
        for (size_t i = 0; i < index; ++i) {
            curr = curr->m_next;
        }
        return curr->m_data;
    }
    // Оператор доступа по индексу
    // Константная версия
    const T& operator[](size_t index) const {
        if (index >= m_size) throw std::out_of_range("Index out of range"); // Проверка на выход за границы
        SinglyNode<T>* curr = m_head;
        for (size_t i = 0; i < index; ++i) {
            curr = curr->m_next;
        }
        return curr->m_data;
    }
    
    // Класс итератора для обхода однонаправленного списка
    class iterator {
    private:
        SinglyNode<T>* m_node;  // Указатель на текущий узел      
    public:
        explicit iterator(SinglyNode<T>* node) : m_node(node) {}
        T& operator*() { return m_node->m_data; }
        iterator& operator++() {
            m_node = m_node->m_next;  // Переход к следующему узлу
            return *this;
        }
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        // Оператор сравнения указателей на неравенство
        bool operator!=(const iterator& other) const {
            return m_node != other.m_node;
        }
        // Оператор сравнения указателей на равенство
        bool operator==(const iterator& other) const {
            return m_node == other.m_node;
        }
        // Метод get() для получения значения (альтернатива operator)
        T& get() { return m_node->m_data; }
    };
    // Метод begin()
    // Возврат итератора на первый элемент
    iterator begin() { return iterator(m_head); }
    // Метод end()
    // Возврат итератора на позицию после последнего элемента
    iterator end() { return iterator(nullptr); }  // Возврат nullptr как обозначения конца списка

    // Класс итератора для обхода константного однонаправленного списка
    class const_iterator {
    private:
        const SinglyNode<T>* m_node;  // Константный указатель на текущий узел
    public:
        explicit const_iterator(const SinglyNode<T>* node) : m_node(node) {}
        // Оператор разыменования
        // Возврат ссылки на текущий элемент
        const T& operator*() const { return m_node->m_data; }
        // Префиксный инкремент
        const_iterator& operator++() {
            m_node = m_node->m_next;
            return *this;
        }
        // Постфиксный инкремент
        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        // Операторы сравнения
        bool operator!=(const const_iterator& other) const {
            return m_node != other.m_node;
        }
        bool operator==(const const_iterator& other) const {
            return m_node == other.m_node;
        }
        // Метод get() для получения значения (альтернатива operator*)
        const T& get() const { return m_node->m_data; }
    };
    // Метод cbegin()
    // Возврат итератора на первый константный элемент
    const_iterator cbegin() const {
        return const_iterator(m_head);
    }
    // Метод cend()
    // Возврат итератора на позицию после последнего константного элемента
    const_iterator cend() const {
        return const_iterator(nullptr);  // Возврат nullptr как обозначения конца списка
    }
};

// Узел двунаправленного списка (для доп. задания №2)
template<typename T>
struct DoublyNode {
    T m_data;                    // Хранимые данные
    DoublyNode* m_prev;          // Указатель на предыдущий узел
    DoublyNode* m_next;          // Указатель на следующий узел
    
    // Конструктор (с инициализацией данных)
    explicit DoublyNode(const T& value) : m_data(value), m_prev(nullptr), m_next(nullptr) {}
    // Конструктор перемещения (для доп. задания №3)
    explicit DoublyNode(T&& value) : m_data(std::move(value)), m_prev(nullptr), m_next(nullptr) {}
};

// Двунаправленный список (для доп. задания №2)
template<typename T>
class DoublyLinkedList {
private:
    DoublyNode<T>* m_head;      // Указатель на первый узел
    DoublyNode<T>* m_tail;      // Указатель на последний узел
    size_t m_size;         // // Текущее количество элементов
    void clear() noexcept {
        while (m_head) {
            DoublyNode<T>* tmp = m_head; // Запись указателя на текущий узел
            m_head = m_head->m_next; // Переход к следующему узлу
            delete tmp; // Удаление текущего узла
        }
        m_tail = nullptr;
        m_size = 0;
    }
public:
    // Конструктор по умолчанию
    DoublyLinkedList() : m_head(nullptr), m_tail(nullptr), m_size(0) {}
    // Деструктор
    // Освобождение всех узлов списка
    ~DoublyLinkedList() {
        clear();
    }
    // Конструктор копирования
    // Копирование каждого элемента
    DoublyLinkedList(const DoublyLinkedList& other) : m_head(nullptr), m_tail(nullptr), m_size(0) {
        for (DoublyNode<T>* curr = other.m_head; curr; curr = curr->m_next) {
            push_back(curr->m_data);
        }
    }
    // Конструктор перемещения (для доп. задания №3)
    DoublyLinkedList(DoublyLinkedList&& other) noexcept 
        : m_head(other.m_head), m_tail(other.m_tail), m_size(other.m_size) {
        other.m_head = nullptr;
        other.m_tail = nullptr;
        other.m_size = 0;
    }
    // Оператор присваивания копированием
    DoublyLinkedList& operator=(const DoublyLinkedList& other) {
        if (this != &other) {
            clear(); // Очистка текущего списка
            m_tail = nullptr;
            m_size = 0;
            for (DoublyNode<T>* curr = other.m_head; curr; curr = curr->m_next) {
                push_back(curr->m_data);
            } // Копирование
        }
        return *this;
    }
    // Оператор присваивания перемещением
    DoublyLinkedList& operator=(DoublyLinkedList&& other) noexcept {
        if (this != &other) {
            DoublyLinkedList tmp(std::move(*this)); // Перемещение указателя на выделенную память во временный объект
            clear(); // Очистка текущего списка
            m_head = other.m_head;
            m_tail = other.m_tail;
            m_size = other.m_size;
            // Обнуление источника
            other.m_head = nullptr;
            other.m_tail = nullptr;
            other.m_size = 0;
        }
        return *this;
    }
    // Метод добавления элемента в конец контейнера (push_back)
    void push_back(const T& value) {
        DoublyNode<T>* newNode = new DoublyNode<T>(value);
        if (!m_head) { // Если список пуст
            m_head = m_tail = newNode; // Новый узел становится головой и хвостом
        } else {
            m_tail->m_next = newNode; // Прикрепление нового узла к концу
            newNode->m_prev = m_tail; // Обновление указателя на хвост
            m_tail = newNode;
        }
        ++m_size; // Увеличение размера
    }
    // Метод добавления элемента в конец контейнера (push_back) с семантикой перемещения
    void push_back(T&& value) {
        DoublyNode<T>* newNode = new DoublyNode<T>(std::move(value));
        if (!m_head) {
            m_head = m_tail = newNode;
        } else {
            m_tail->m_next = newNode;
            newNode->m_prev = m_tail;
            m_tail = newNode;
        }
        ++m_size;
    }
    // Метод вставки элемента в произвольную позицию (insert)
    void insert(size_t index, const T& value) {
        if (index > m_size) throw std::out_of_range("Index out of range"); // Проверка на выход за границы
        if (index == 0) {  // Вставка в начало
            DoublyNode<T>* newNode = new DoublyNode<T>(value);
            newNode->m_next = m_head;
            if (m_head) m_head->m_prev = newNode;
            m_head = newNode;
            if (!m_tail) m_tail = newNode; // Если список был пуст
            ++m_size;
            return;
        }
        if (index == m_size) {  
            push_back(value);
            return;
        } // Вставка в конец
        // Поиск позиции
        DoublyNode<T>* curr = m_head;
        for (size_t i = 0; i < index; ++i) {
            curr = curr->m_next;
        }
        DoublyNode<T>* newNode = new DoublyNode<T>(value);
        newNode->m_prev = curr->m_prev;
        newNode->m_next = curr;
        curr->m_prev->m_next = newNode;
        curr->m_prev = newNode;
        ++m_size;
    }
    // Метод удаления элемента по индексу (erase)
    void erase(size_t index) {
        if (index >= m_size) throw std::out_of_range("Index out of range"); // Проверка на выход за границы 
        DoublyNode<T>* toDelete;
        if (index == 0) {  // Удаление головы
            toDelete = m_head;
            m_head = m_head->m_next;
            if (m_head) m_head->m_prev = nullptr;
            else m_tail = nullptr;  // Список стал пустым
        } else {
            DoublyNode<T>* curr = m_head;
            for (size_t i = 0; i < index; ++i) {
                curr = curr->m_next;
            }
            toDelete = curr;
            curr->m_prev->m_next = curr->m_next;
            if (curr->m_next) curr->m_next->m_prev = curr->m_prev;
            else m_tail = curr->m_prev; 
        }
        delete toDelete; // Освобождение памяти
        --m_size;
    }
    // Метод получения текущего размера контейнера
    size_t size() const { return m_size; }
    // Оператор доступа по индексу (operator[])
    // Версия без константности
    T& operator[](size_t index) {
        if (index >= m_size) throw std::out_of_range("Index out of range"); // Проверка на выход за границы
        DoublyNode<T>* curr = m_head;
        for (size_t i = 0; i < index; ++i) {
            curr = curr->m_next;
        }
        return curr->m_data;
    }
    // Оператор доступа по индексу
    // Константная версия
    const T& operator[](size_t index) const {
        if (index >= m_size) throw std::out_of_range("Index out of range");
        DoublyNode<T>* curr = m_head;
        for (size_t i = 0; i < index; ++i) {
            curr = curr->m_next;
        }
        return curr->m_data;
    }
    
    // Итератор для двунаправленного списка
    class iterator {
    private:
        DoublyNode<T>* m_node; // Указатель на текущий узел   
    public:
        explicit iterator(DoublyNode<T>* node) : m_node(node) {} 
        T& operator*() { return m_node->m_data; }
        iterator& operator++() {
            m_node = m_node->m_next;  // Переход к следующему узлу
            return *this;
        }
        iterator& operator--() {  // Обратный ход
            m_node = m_node->m_prev;
            return *this;
        }
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        // Оператор сравнения указателей на неравенство
        bool operator!=(const iterator& other) const {
            return m_node != other.m_node;
        }
        // Оператор сравнения указателей на равенство
        bool operator==(const iterator& other) const {
            return m_node == other.m_node;
        }
        // Метод get() для получения значения (альтернатива operator)
        T& get() { return m_node->m_data; }
    };
    // Метод begin()
    // Возврат итератора на первый элемент
    iterator begin() { return iterator(m_head); }
    // Метод end()
    // Возврат итератора на позицию после последнего элемента
    iterator end() { return iterator(nullptr); }

    // Класс итератора для обхода константного двунаправленного списка
    class const_iterator {
    private:
        const DoublyNode<T>* m_node;  // Константный указатель на текущий узел
    public:
        explicit const_iterator(const DoublyNode<T>* node) : m_node(node) {}
        // Оператор разыменования
        const T& operator*() const { return m_node->m_data; }
        // Префиксный инкремент
        const_iterator& operator++() {
            m_node = m_node->m_next;
            return *this;
        }
        // Префиксный декремент
        const_iterator& operator--() {
            m_node = m_node->m_prev;
            return *this;
        }
        // Постфиксный инкремент
        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        // Постфиксный декремент
        const_iterator operator--(int) {
            const_iterator tmp = *this;
            --(*this);
            return tmp;
        } 
        // Оператор сравнения указателей на неравенство
        bool operator!=(const const_iterator& other) const {
            return m_node != other.m_node;
        }
        // Оператор сравнения указателей на равенство
        bool operator==(const const_iterator& other) const {
            return m_node == other.m_node;
        }
        // Метод get() для получения значения
        const T& get() const { return m_node->m_data; }
    };
    // Метод cbegin()
    // Возврат итератора на первый константный элемент
    const_iterator cbegin() const {
        return const_iterator(m_head);
    }
    // Метод cend()
    // Возврат итератора на позицию после последнего константного элемента
    const_iterator cend() const {
        return const_iterator(nullptr);  // Возврат nullptr как обозначения конца списка
    }
};


// Функция вывода содержимого контейнера
template<typename Container>
void printContainer(const Container& container, const std::string& label) {
    std::cout << label << ": ";  // Вывод текстового примечания
    for (auto it = container.cbegin(); it != container.cend(); ++it) {  // Обход через константный итератор
        std::cout << *it << " ";  // Вывод значения элемента
    }
    std::cout << "\n";
    std::cout << "Размер: " << container.size() << "\n\n";  // Вывод размера контейнера
}
