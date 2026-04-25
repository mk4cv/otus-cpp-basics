#include <gtest/gtest.h>
#include "containers.hpp"
#include <stdexcept>

// Класс для отслеживания вызовов деструкторов (для доп. задания №2)
struct DLL_DestructibleInt {
    static int destruct_count;
    int value;
    DLL_DestructibleInt() : value(0) {} // Конструктор по умолчанию
    DLL_DestructibleInt(int v) : value(v) {} // Конструктор с параметром
    ~DLL_DestructibleInt() { ++destruct_count; } // Деструктор
};
int DLL_DestructibleInt::destruct_count = 0;

// Тест 1. Проверка корректности работы конструктора по умолчанию.
// Ожидаемое поведение: указатели на голову и хвост равны nullptr, размер равен 0.
TEST(DoublyLinkedList, default_constructor) {
    DoublyLinkedList<int> list;
    EXPECT_EQ(list.size(), 0);
}

// Тест 2. Добавление элементов в конец списка (push_back).
// Проверка корректности обновления указателей m_tail/m_next и сохранение порядка.
TEST(DoublyLinkedList, push_back) {
    DoublyLinkedList<int> list;
    list.push_back(10); list.push_back(20);
    EXPECT_EQ(list.size(), 2);
    EXPECT_EQ(list[0], 10); EXPECT_EQ(list[1], 20);
}

// Тест 3. Вставка элемента в начало списка.
// Проверка корректности обновления m_head, m_prev у следующего узла и отсутствия разрывов в цепочке.
TEST(DoublyLinkedList, insert_beginning) {
    DoublyLinkedList<int> list;
    list.push_back(10); list.push_back(20);
    list.insert(0, 5);
    EXPECT_EQ(list.size(), 3);
    EXPECT_EQ(list[0], 5); EXPECT_EQ(list[1], 10); EXPECT_EQ(list[2], 20);
}

// Тест 4. Вставка элемента в середину списка.
// Проверка корректности перелинковки указателей prev/next у соседних узлов.
TEST(DoublyLinkedList, insert_middle) {
    DoublyLinkedList<int> list;
    for (int i = 0; i < 4; ++i) list.push_back(i); // [0, 1, 2, 3]
    list.insert(2, 99);
    EXPECT_EQ(list.size(), 5);
    EXPECT_EQ(list[0], 0); EXPECT_EQ(list[2], 99); EXPECT_EQ(list[3], 2);
}

// Тест 5. Удаление элемента из конца списка.
// Проверка корректности обновления m_tail и освобождения памяти последнего узла.
TEST(DoublyLinkedList, erase_end) {
    DoublyLinkedList<int> list;
    list.push_back(1); list.push_back(2); list.push_back(3);
    list.erase(2);
    EXPECT_EQ(list.size(), 2);
    EXPECT_EQ(list[0], 1); EXPECT_EQ(list[1], 2);
}

// Тест 6. Удаление элемента из начала списка.
// Проверка сдвига m_head и корректности обработки m_prev нового головного элемента.
TEST(DoublyLinkedList, erase_beginning) {
    DoublyLinkedList<int> list;
    list.push_back(1); list.push_back(2); list.push_back(3);
    list.erase(0);
    EXPECT_EQ(list.size(), 2);
    EXPECT_EQ(list[0], 2); EXPECT_EQ(list[1], 3);
}

// Тест 7. Удаление элемента из середины списка.
// Проверка сшивания предыдущего и следующего узлов, исключения удаляемого из цепочки.
TEST(DoublyLinkedList, erase_middle) {
    DoublyLinkedList<int> list;
    for (int i = 0; i < 5; ++i) list.push_back(i); // [0,1,2,3,4]
    list.erase(2);
    EXPECT_EQ(list.size(), 4);
    EXPECT_EQ(list[0], 0); EXPECT_EQ(list[2], 3);
}

// Тест 8. Доступ к элементам списка по индексу (чтение и запись).
// Проверка корректности линейного обхода при чтении/записи и константной версии operator[].
TEST(DoublyLinkedList, element_access) {
    DoublyLinkedList<int> list;
    list.push_back(50); list.push_back(100);
    list[0] = 51;
    EXPECT_EQ(list[0], 51);
    const auto& c_const = list;
    EXPECT_EQ(c_const[1], 100);
}

// Тест 9. Контроль изменения размера при модификациях.
// Проверка синхронизации внутреннего счетчика m_size с фактическим количеством узлов.
TEST(DoublyLinkedList, size_tracking) {
    DoublyLinkedList<int> list;
    EXPECT_EQ(list.size(), 0);
    list.push_back(1); EXPECT_EQ(list.size(), 1);
    list.insert(0, 0); EXPECT_EQ(list.size(), 2);
    list.erase(1); EXPECT_EQ(list.size(), 1);
}

// Тест 10. Генерация исключений при невалидных индексах.
// Проверка защиты от выхода за границы при операциях доступа, вставки и удаления.
TEST(DoublyLinkedList, out_of_range_throws) {
    DoublyLinkedList<int> list;
    list.push_back(1);
    EXPECT_THROW(list[5], std::out_of_range);
    EXPECT_THROW(list.erase(5), std::out_of_range);
    EXPECT_THROW(list.insert(5, 0), std::out_of_range);
}

// Тест 11 (для доп. задания №1). Проверка глубокого копирования двунаправленного списка.
// Проверка создания независимой копии узлов (изменение одной структуры не затрагивает другую).
TEST(DoublyLinkedList, copy_correctness_and_deep_copy) {
    DoublyLinkedList<int> src;
    src.push_back(10); src.push_back(20);
    DoublyLinkedList<int> dst(src);
    EXPECT_EQ(dst.size(), src.size());
    EXPECT_EQ(dst[0], src[0]); EXPECT_EQ(dst[1], src[1]);
    dst[0] = 99;
    EXPECT_NE(src[0], 99);
}

// Тест 12 (для доп. задания №2). Проверка вызова деструкторов узлов при уничтожении списка.
// Сброс счетчика перед выходом из области видимости и проверка точного количества вызовов.
TEST(DoublyLinkedList, destructor_calls) {
    {
        DoublyLinkedList<DLL_DestructibleInt> list;
        for (int i = 0; i < 3; ++i) list.push_back(DLL_DestructibleInt(i));
        DLL_DestructibleInt::destruct_count = 0;
    }
    EXPECT_EQ(DLL_DestructibleInt::destruct_count, 3);
}

// Тест 13 (для доп. задания №3). Проверка перемещающего присваивания.
// Проверка передачи указателей на узлы и полной очистки исходного списка.
TEST(DoublyLinkedList, move_assignment_operator) {
    DoublyLinkedList<int> src;
    src.push_back(10); src.push_back(20);
    DoublyLinkedList<int> dst;
    dst = std::move(src);
    EXPECT_EQ(dst.size(), 2);
    EXPECT_EQ(dst[0], 10); EXPECT_EQ(dst[1], 20);
    EXPECT_EQ(src.size(), 0);
}