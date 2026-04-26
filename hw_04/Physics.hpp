#pragma once
#include "Ball.hpp"
#include "Dust.hpp"
#include <vector>

class Physics {
  public:
    Physics(double timePerTick = 0.001);
    void setWorldBox(const Point& topLeft, const Point& bottomRight);
    void update(std::vector<Ball>& balls, size_t ticks) const;
    std::vector<Dust> takeDust() const {
        return std::move(newDust);
    } // Функция извлечения частиц

  private:
    void collideBalls(std::vector<Ball>& balls) const;
    void collideWithBox(std::vector<Ball>& balls) const;
    void move(std::vector<Ball>& balls) const;
    void processCollision(Ball& a, Ball& b,
                          double distanceBetweenCenters2) const;
    mutable std::vector<Dust> newDust; // Накопление частиц пыли за тик
    
  private:
    Point topLeft;
    Point bottomRight;
    double timePerTick;
    
};
