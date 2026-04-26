#pragma once
#include "Color.hpp"
#include "Painter.hpp"
#include "Point.hpp"
#include "Velocity.hpp"
#include "Color.hpp"

class Ball {
public:
    // Конструктор для создания шара с физическими параметрами
    Ball(const Point& center,
         const Velocity& velocity,
         double radius,
         const Color& color,
         bool isCollidable = true): 
        m_center{center},
        m_velocity{velocity},
        m_radius{radius},
        m_color{color},
        isCollidable_{isCollidable}
    {}

    // Физические параметры
    void setVelocity(const Velocity& velocity);
    Velocity getVelocity() const;
    void setCenter(const Point& center);
    Point getCenter() const;
    double getRadius() const;
    double getMass() const;
    Color getColor() const;
    // .Функция отрисовки
    void draw(Painter& painter) const;
    // Признак "призрачного" шара (для доп. задания №2)
    bool isCollidable() const { return isCollidable_; }

private:
    Point    m_center;
    Velocity m_velocity;
    double   m_radius;
    Color    m_color;
    bool     isCollidable_;
};