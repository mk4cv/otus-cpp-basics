#include "Color.hpp"

Color::Color() = default;

Color::Color(double red, double green, double blue)
    : r{red}, g{green}, b{blue} {}

double Color::red() const {
    return r;
}

double Color::green() const {
    return g;
}

double Color::blue() const {
    return b;
}

// Функция чтения объектов из std::istream
std::istream& operator>>(std::istream& stream, Color& c) {
    double r, g, b;
    stream >> r >> g >> b;
    c = Color(r, g, b);
    return stream;
}