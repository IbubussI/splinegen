#pragma once
#include <vector>

struct WindowPoint;

namespace TV::Math {
    class SplineFunction;
}

namespace sf {
    class RenderWindow;
}

struct Point;

class Drawer {
public:
    explicit Drawer(sf::RenderWindow& window);

    ~Drawer() = default;

    void draw(const std::vector<WindowPoint>& knotPoints, const std::vector<WindowPoint>& intermediatePoints) const;

    void setDrawRefLines(const bool isDrawRefLines) {
        mIsDrawRefLines = isDrawRefLines;
    }

    void setLineThickness(const int lineThickness) {
        mLineThickness = lineThickness;
    }

    void setPointSize(const int pointSize) {
        mPointSize = pointSize;
    }

    void setConPointSize(const int conPointSize) {
        mConPointSize = conPointSize;
    }

private:
    sf::RenderWindow& mWindow;
    bool mIsDrawRefLines = false;
    int mLineThickness = 1;
    int mPointSize = 6;
    int mConPointSize = 1;
};
