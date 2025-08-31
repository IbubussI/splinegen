#include "drawer.h"
#include "tv/spline.h"
#include "point.h"

#include <SFML/Graphics.hpp>

Drawer::Drawer(sf::RenderWindow& window)
    : mWindow(window) {
}

void Drawer::draw(const std::vector<WindowPoint>& knotPoints,
                  const std::vector<WindowPoint>& intermediatePoints) const {
    using namespace TV::Math;
    using namespace sf;

    // fill background
    mWindow.clear(Color(0x1f1f1fff));

    // Draw grid
    drawGridBackground(100, 100, Color(0x323232ff));

    // draw reference lines
    if (mIsDrawRefLines) {
        VertexArray refLines(PrimitiveType::LineStrip);
        for (auto [x, y]: knotPoints) {
            refLines.append(Vertex(Vector2f(x, y)));
        }
        mWindow.draw(refLines);
    }

    // draw lines
    VertexArray lines(PrimitiveType::LineStrip);
    for (auto c: intermediatePoints) {
        lines.append(Vertex(Vector2f(c.x, c.y), Color(0xa1d7feff)));
    }
    mWindow.draw(lines);

    // draw connection points
    for (auto [x, y]: intermediatePoints) {
        CircleShape point(mConPointSize);
        point.setFillColor(Color(0xf55df2ff));
        point.setOrigin(Vector2f(mConPointSize, mConPointSize));
        point.setPosition(Vector2f(x, y));
        mWindow.draw(point);
    }

    // draw points
    for (auto [x, y]: knotPoints) {
        CircleShape point(mPointSize);
        point.setFillColor(Color(0xdd1d1dff));
        point.setOrigin(Vector2f(mPointSize, mPointSize));
        point.setPosition(Vector2f(x, y));
        mWindow.draw(point);
    }
}

void Drawer::drawGridBackground(const int cellWidth, const int cellHeight, const sf::Color color) const {
    using namespace sf;

    const int rowsNum = mWindow.getSize().x / cellWidth;
    const int colsNum = mWindow.getSize().y / cellHeight;

    // Draw horizontal lines
    for (int i = 0; i <= rowsNum; ++i) {
        RectangleShape line(Vector2f(rowsNum * cellWidth, 1));
        line.setPosition(Vector2f(0, i * cellWidth));
        line.setFillColor(color);
        mWindow.draw(line);
    }

    // Draw vertical lines
    for (int i = 0; i <= colsNum; ++i) {
        RectangleShape line(Vector2f(1, colsNum * cellHeight));
        line.setPosition(Vector2f(i * cellHeight, 0));
        line.setFillColor(color);
        mWindow.draw(line);
    }
}
