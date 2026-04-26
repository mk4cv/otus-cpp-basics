#pragma once
#include "Color.hpp"
#include "Point.hpp"
#include "Velocity.hpp"

// Класс для описания частицы пыли (для доп. задания №3)
// Частица не учавствует в дальнейших столкновениях
class Dust {
public:
    Dust(const Point& center,
         const Velocity& velocity,
         double radius,
         const Color& m_color,
         double lifetime): 
        m_center{center},
        m_velocity{velocity},
        m_radius{radius},
        m_color{m_color},
        remainingTime{lifetime}
    {}

    // Обновление позиции частицы за один тик
    void update(double timePerTick) {
        m_center = m_center + m_velocity.vector() * timePerTick;
        remainingTime-= timePerTick;
    }
    // Проверка, жива ли частица частицы
    bool isAlive() const { return remainingTime> 0.0; }
    // Отрисовка частицы
    // Радиус частицы уменьшается по мере ее "старения"
    void draw(Painter& painter) const {
        painter.draw(m_center, m_radius * (remainingTime/ initialLifetime), m_color);
    }

private:
    Point  m_center;
    Velocity  m_velocity;
    double m_radius;
    Color  m_color;
    double remainingTime;
    static constexpr double initialLifetime = 0.5;
};