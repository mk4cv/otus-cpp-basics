#include <gtest/gtest.h>
#include "containers.cpp"
#include <stdexcept>

// Класс для отслеживания вызовов деструкторов (для доп. задания №2)
struct SC_DestructibleInt {
    static int destruct_count;
    int value;
    SC_DestructibleInt() : value(0) {} // Конструктор по умолчанию
    SC_DestructibleInt(int v) : value(v) {} // Конструктор с параметром
    ~SC_DestructibleInt() { ++destruct_count; } // Деструктор
};
int SC_DestructibleInt::destruct_count = 0;

// Тест 1. Проверка корректности работы конструктора по умолчанию.
// Ожидаемое поведение: создается пустой контейнер с размером равным 0, указатель на данные инициализирован корректно
TEST(SequentialContainer, default_constructor) {
    SequentialContainer<int> container;
    EXPECT_EQ(container.size(), 0);
}

// Тест 2. Добавление элементов в конец контейнера (push_back).
// Проверка корректности вставки, автоматического расширения памяти и правильного хранения порядка элементов.
TEST(SequentialContainer, push_back) {
    SequentialContainer<int> container;
    container.push_back(10);
    container.push_back(20);
    EXPECT_EQ(container.size(), 2);
    EXPECT_EQ(container[0], 10);
    EXPECT_EQ(container[1], 20);
}

// Тест 3. Вставка элемента в начало контейнера.
// Проверка сдвига существующих элементов вправо, корректное обновление размера и доступ к первому элементу.
TEST(SequentialContainer, insert_beginning) {
    SequentialContainer<int> container;
    container.push_back(10); container.push_back(20);
    container.insert(0, 5);
    EXPECT_EQ(container.size(), 3);
    EXPECT_EQ(container[0], 5);
    EXPECT_EQ(container[1], 10);
    EXPECT_EQ(container[2], 20);
}

// Тест 4. Вставка элемента в середину контейнера.
// Проверка корректности смещения элементов, сохранения исходного порядка и обновления индексов.
TEST(SequentialContainer, insert_middle) {
    SequentialContainer<int> container;
    for (int i = 0; i < 4; ++i) container.push_back(i); // [0, 1, 2, 3]
    container.insert(2, 99);
    EXPECT_EQ(container.size(), 5);
    EXPECT_EQ(container[0], 0); EXPECT_EQ(container[2], 99); EXPECT_EQ(container[3], 2);
}

// Тест 5. Удаление элемента из конца контейнера.
// Проверка уменьшения размера и отсутствия влияния на элементы, находящиеся перед удаляемым.
TEST(SequentialContainer, erase_end) {
    SequentialContainer<int> container;
    container.push_back(1); container.push_back(2); container.push_back(3);
    container.erase(2);
    EXPECT_EQ(container.size(), 2);
    EXPECT_EQ(container[0], 1); EXPECT_EQ(container[1], 2);
}

// Тест 6. Удаление элемента из начала контейнера.
// Проверка сдвига оставшихся элементов влево и корректности индексации после удаления.
TEST(SequentialContainer, erase_beginning) {
    SequentialContainer<int> container;
    container.push_back(1); container.push_back(2); container.push_back(3);
    container.erase(0);
    EXPECT_EQ(container.size(), 2);
    EXPECT_EQ(container[0], 2); EXPECT_EQ(container[1], 3);
}

// Тест 7. Удаление элемента из середины контейнера.
// Проверка целостности данных, корректности сдвига влево и обновления размера.
TEST(SequentialContainer, erase_middle) {
    SequentialContainer<int> container;
    for (int i = 0; i < 5; ++i) container.push_back(i); // [0,1,2,3,4]
    container.erase(2);
    EXPECT_EQ(container.size(), 4);
    EXPECT_EQ(container[0], 0); EXPECT_EQ(container[2], 3);
}

// Тест 8. Доступ к элементам контейнера по индексу (чтение и запись).
// Проверка работы константной и неконстантной версий operator[], а также возможности модификации данных.
TEST(SequentialContainer, element_access) {
    SequentialContainer<int> container;
    container.push_back(50); container.push_back(100);
    container[0] = 51; // Запись
    EXPECT_EQ(container[0], 51);
    const auto& c_const = container;
    EXPECT_EQ(c_const[1], 100); // Чтение через const-ссылку
}

// Тест 9. Динамическое отслеживание размера контейнера при операциях вставки и удаления.
// Проверка, что счетчик элементов корректно инкрементируется и декрементируется.
TEST(SequentialContainer, size_tracking) {
    SequentialContainer<int> container;
    EXPECT_EQ(container.size(), 0);
    container.push_back(1); EXPECT_EQ(container.size(), 1);
    container.insert(0, 0); EXPECT_EQ(container.size(), 2);
    container.erase(1); EXPECT_EQ(container.size(), 1);
}

// Тест 10. Проверка генерации исключения std::out_of_range при невалидных индексах.
// Проверка защиты от выхода за границы массива при доступе, вставке и удалении.
TEST(SequentialContainer, out_of_range_throws) {
    SequentialContainer<int> container;
    container.push_back(1);
    EXPECT_THROW(container[5], std::out_of_range);
    EXPECT_THROW(container.erase(5), std::out_of_range);
    EXPECT_THROW(container.insert(5, 0), std::out_of_range);
}

// Тест 11 (для доп. задания №1). Тестирование конструктора копирования (глубокая копия).
// Проверка идентичности содержимого после копирования и изоляции памяти (изменение копии не влияет на оригинал).
TEST(SequentialContainer, copy_correctness_and_deep_copy) {
    SequentialContainer<int> src_container;
    src_container.push_back(10); src_container.push_back(20);
    SequentialContainer<int> dst_container(src_container);
    EXPECT_EQ(dst_container.size(), src_container.size());
    EXPECT_EQ(dst_container[0], src_container[0]); EXPECT_EQ(dst_container[1], src_container[1]);
    dst_container[0] = 99;
    EXPECT_NE(src_container[0], 99); // Проверка независимости памяти
}

// Тест 12 (для доп. задания №2).  Проверка вызова деструкторов при уничтожении контейнера.
// Создание контейнера с элементами, далее сброс счетчика, далее выход контейнера из области видимости.
// Ожидаемое поведение: счетчик деструкторов должен равняться количеству добавленных элементов.
TEST(SequentialContainer, destructor_calls) {
    {
        SequentialContainer<SC_DestructibleInt> container;
        container.reserve(3); // Резервирование памяти для 3-x элементов, чтобы гарантировать, что m_capacity == m_size
        for (int i = 0; i < 3; ++i) container.push_back(SC_DestructibleInt(i));
        SC_DestructibleInt::destruct_count = 0; // Сброс счетчика перед уничтожением
    } 
    EXPECT_EQ(SC_DestructibleInt::destruct_count, 3);
}

// Тест 13 (для доп. задания №3). Тестирование перемещающего оператора присваивания.
// Проверка корректности переноса указателей и очистки состояния исходного объекта.
// Ожидаемое поведение: источник должен стать пустым.
TEST(SequentialContainer, move_assignment_operator) {
    SequentialContainer<int> src_container;
    src_container.push_back(10); src_container.push_back(20);
    SequentialContainer<int> dst_container;
    dst_container = std::move(src_container);
    EXPECT_EQ(dst_container.size(), 2);
    EXPECT_EQ(dst_container[0], 10); EXPECT_EQ(dst_container[1], 20);
    EXPECT_EQ(src_container.size(), 0); 
}