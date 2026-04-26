#include "Physics.hpp"
#include <random>

double dot(const Point& lhs, const Point& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

Physics::Physics(double timePerTick) : timePerTick{timePerTick} {}

void Physics::setWorldBox(const Point& topLeft, const Point& bottomRight) {
    this->topLeft = topLeft;
    this->bottomRight = bottomRight;
}

void Physics::update(std::vector<Ball>& balls, const size_t ticks) const {

    for (size_t i = 0; i < ticks; ++i) {
        move(balls);
        collideWithBox(balls);
        collideBalls(balls);
    }
}

void Physics::collideBalls(std::vector<Ball>& balls) const {
    for (auto a = balls.begin(); a != balls.end(); ++a) {
        for (auto b = std::next(a); b != balls.end(); ++b) {
            // Пропуск призрачных шаров (для доп. задания №2)
            if (!a->isCollidable() || !b->isCollidable())
                continue;
            const double distanceBetweenCenters2 =
                distance2(a->getCenter(), b->getCenter());
            const double collisionDistance = a->getRadius() + b->getRadius();
            const double collisionDistance2 =
                collisionDistance * collisionDistance;

            if (distanceBetweenCenters2 < collisionDistance2) {
                processCollision(*a, *b, distanceBetweenCenters2);
            }
        }
    }
}

void Physics::collideWithBox(std::vector<Ball>& balls) const {
    for (Ball& ball : balls) {
        const Point p = ball.getCenter();
        const double r = ball.getRadius();
        // определяет, находится ли v в диапазоне (lo, hi) (не включая границы)
        auto isOutOfRange = [](double v, double lo, double hi) {
            return v < lo || v > hi;
        };

        if (isOutOfRange(p.x, topLeft.x + r, bottomRight.x - r)) {
            Point vector = ball.getVelocity().vector();
            vector.x = -vector.x;
            ball.setVelocity(vector);
        } else if (isOutOfRange(p.y, topLeft.y + r, bottomRight.y - r)) {
            Point vector = ball.getVelocity().vector();
            vector.y = -vector.y;
            ball.setVelocity(vector);
        }
    }
}

void Physics::move(std::vector<Ball>& balls) const {
    for (Ball& ball : balls) {
        Point newPos =
            ball.getCenter() + ball.getVelocity().vector() * timePerTick;
        ball.setCenter(newPos);
    }
}

void Physics::processCollision(Ball& a, Ball& b,
                               double distanceBetweenCenters2) const {
    // нормированный вектор столкновения
    const Point normal =
        (b.getCenter() - a.getCenter()) / std::sqrt(distanceBetweenCenters2);

    // получаем скорость в векторном виде
    const Point aV = a.getVelocity().vector();
    const Point bV = b.getVelocity().vector();

    // коэффициент p учитывает скорость обоих мячей
    const double p =
        2 * (dot(aV, normal) - dot(bV, normal)) / (a.getMass() + b.getMass());

    // задаем новые скорости мячей после столкновения
    a.setVelocity(Velocity(aV - normal * p * a.getMass()));
    b.setVelocity(Velocity(bV + normal * p * b.getMass()));

    // Генерация частиц пыли (для доп. задания №3)
    Point center_point = (a.getCenter() + b.getCenter()) * 0.5; // Центр столкновения (средняя точка между центрами шаров)
    double base_radius = 10.0; // Базовый радиус пыли
    auto len = [](const Point& p) {
        return std::sqrt(p.x * p.x + p.y * p.y);
    }; // Функция выяисления скорости шара
    double speed_a = len(a.getVelocity().vector()); // Скорость шара a
    double speed_b = len(b.getVelocity().vector()); // Скорость шара b
    double base_speed = (speed_a + speed_b) * 0.5 * 6; // Базовая скорость пыли (средняя скорость столкнувшихся шаров * коэффициент)    
    double lifetime = 0.5; // Время жизни частиц пыли
    Color color = (speed_a < speed_b) ? a.getColor() : b.getColor(); // Цвет пыли (цвет менее быстрого шара)
    // Случайный разброс параметров
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_real_distribution<double> radius_disp(0.6, 1.4);  // Отклонения радиуса, +/- 40% 
    static std::uniform_real_distribution<double> angle_disp(0.3, 0.3);  // Отклонения угла, +/- 3 рад
    static std::uniform_real_distribution<double> speed_disp(0.7, 1.3);  // Отклонения скорости, +/- 30% 
    for (int i = 0; i < 6; ++i) {
        double radius = base_radius * radius_disp(rng);
        double angle = i * (2.0 * M_PI / 6) + angle_disp(rng);
        double speed = base_speed * speed_disp(rng);
        Velocity velocity(speed, angle);
        newDust.emplace_back(center_point, velocity, radius, color, lifetime);
    }

}
