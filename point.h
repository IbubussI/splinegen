#pragma once
#include "tvmath.h"

struct Point {
    TV::Math::Dec16 x;
    TV::Math::Dec16 y;

    bool operator<(const Point& other) const {
        return x < other.x;
    }
};

struct WindowPoint {
    int x;
    int y;

    constexpr WindowPoint() = default;

    constexpr WindowPoint(const int x, const int y)
        : x(x), y(y) {
    }

    constexpr explicit WindowPoint(const Point& point)
        : x(point.x), y(point.y) {
    }

    constexpr bool operator<(const WindowPoint& other) const {
        return x < other.x;
    }

    constexpr WindowPoint operator-(const WindowPoint other) const {
        return WindowPoint{x - other.x, y - other.y};
    }

    constexpr WindowPoint operator+(const WindowPoint other) const {
        return WindowPoint{x + other.x, y + other.y};
    }

    constexpr WindowPoint operator/(const int scalar) const {
        return WindowPoint{x / scalar, y / scalar};
    }

    constexpr WindowPoint operator*(const int scalar) const {
        return WindowPoint{x * scalar, y * scalar};
    }

    [[nodiscard]] constexpr int dot(const WindowPoint rhs) const {
        return x * rhs.x + y * rhs.y;
    }

    // true if given point is at a distance equal or less than r form this point
    [[nodiscard]] constexpr bool isInBounds(const int px, const int py, const int r) const {
        const int cX = x;
        const int cY = y;
        constexpr int fallOff = 2;
        // compare sqr-s to not count sqrt for length
        return (r + fallOff) * (r + fallOff) > (px - cX) * (px - cX) + (py - cY) * (py - cY);
    }
};
