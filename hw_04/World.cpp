#include "World.hpp"
#include "Painter.hpp"
#include <fstream>
#include <algorithm>

// Длительность одного тика симуляции.
// Без изменений
static constexpr double timePerTick = 0.001;

/**
 * Конструирует объект мира для симуляции
 * @param worldFilePath путь к файлу модели мира
 */
World::World(const std::string& worldFilePath) {
    std::ifstream stream(worldFilePath);

    // Чтение границ мира сразу в объекты Point (для доп. задания №1)
    stream >> topLeft >> bottomRight;
    physics.setWorldBox(topLeft, bottomRight);

    Point  center;
    Point  velocityVec;
    Color  color;
    double radius;
    bool   isCollidable;

    while (stream.peek(), stream.good()) {
        // Чтение параметров шаров разу в объекты Point и Color (для доп. задания №1)
        stream >> center >> velocityVec >> color >> radius;
        stream >> std::boolalpha >> isCollidable;
        // Содание шара и размещение его в контейнере
        Ball ball(center, Velocity(velocityVec), radius, color, isCollidable);
        balls.push_back(ball);
    }
}

/// @brief Отображает состояние мира
void World::show(Painter& painter) const {
    // Рисуем белый прямоугольник, отображающий границу
    // мира
    painter.draw(topLeft, bottomRight, Color(1, 1, 1));
    // Вызываем отрисовку каждого шара
    for (const Ball& ball : balls) {
        ball.draw(painter);
    }
    // Отрисовка частиц пыли
    for (const Dust& dust : dustParticles) {
        dust.draw(painter);
    }      
}

// @brief Обновляет состояние мира
void World::update(double time) {
    /**
     * В реальном мире время течет непрерывно. Однако
     * компьютеры дискретны по своей природе. Поэтому
     * симуляцию взаимодействия шаров выполняем дискретными
     * "тиками". Т.е. если с момента прошлой симуляции
     * прошло time секунд, time / timePerTick раз обновляем
     * состояние мира. Каждое такое обновление - тик -
     * в physics.update() перемещаем шары и обрабатываем
     * коллизии - ситуации, когда в результате перемещения
     * один шар пересекается с другим или с границей мира.
     * В общем случае время не делится нацело на
     * длительность тика, сохраняем остаток в restTime
     * и обрабатываем на следующей итерации.
     */
    // учитываем остаток времени, который мы не "доработали" при прошлом update
    time += restTime;
    const auto ticks = static_cast<size_t>(std::floor(time / timePerTick));
    restTime = time - double(ticks) * timePerTick;

    physics.update(balls, ticks);

    // Управление частицами пыли (для доп. задания №3)
    // Получение новых частиц пыли от физического движка и добавление их в массив
    auto fresh = physics.takeDust();
    dustParticles.insert(dustParticles.end(), fresh.begin(), fresh.end());
    // Обновление частиц
    for (auto& dust : dustParticles)
        dust.update(timePerTick);
    // Удаление "мертвых" частиц
    dustParticles.erase(
        std::remove_if(dustParticles.begin(), dustParticles.end(),
                    [](const Dust& dust) { return !dust.isAlive(); }),
        dustParticles.end());
}