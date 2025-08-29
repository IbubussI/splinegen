#pragma once
#include <algorithm>

template<typename T>
struct BoundsRect {
    T xMin;
    T xMax;
    T yMin;
    T yMax;

    T clampX(const T x) const {
        return std::max(std::min(x, xMax), xMin);
    }

    T clampY(const T y) const {
        return std::max(std::min(y, yMax), yMin);
    }
};
