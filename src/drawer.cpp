#include "drawer.h"
#include "tv/spline.h"
#include "point.h"

#include <SFML/Graphics.hpp>

Drawer::Drawer(sf::RenderWindow& window)
    : mWindow(window) {
}

void Drawer::draw(const std::vector<WindowPoint>& knotPoints, const std::vector<WindowPoint>& intermediatePoints) const {
    using namespace TV::Math;
    using namespace sf;

    // fill background
    mWindow.clear(Color(0x1f1f1fff));

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
