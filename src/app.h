#pragma once
#include "boundsRect.h"
#include "drawer.h"
#include "pointTransformer.h"
#include "../libs/tv/tvmath.h"
#include "SFML/System/Clock.hpp"
#include "SFML/System/Vector2.hpp"

class TextContainer;

namespace sf {
    class Event;
    class RenderWindow;
}

struct WindowPoint;

enum SplineType {
    Linear,
    Cubic,
    CubicMonotone,
    Parametric
};

// data that is not a part of state, but has to be shared between loop cycles
struct FrameContext {
    sf::Clock deltaClock;
    int dragPointIdx = -1;
    bool isUserModifiedPoints = false;
    std::vector<Point> userPoints;
};

class App {
public:
    explicit App(sf::RenderWindow& window);

    void run();

private:
    [[nodiscard]] TV::Math::SplineFunction* generateSpline(const std::vector<Point>& points) const;

    std::vector<WindowPoint> generateIntermediatePoints(const TV::Math::SplineFunction* spline) const;

    static bool isParametric(SplineType splineType);

    void updateDragLocation(int dragIdx);

    [[nodiscard]] int findPointUnderCursor(sf::Vector2i mousePos) const;

    std::pair<WindowPoint, int> findSplineClicked(const TV::Math::SplineFunction* spline,
                                                  sf::Vector2i mousePos, int dist) const;

    void tryInsertPoint(const TV::Math::SplineFunction* spline, sf::Vector2i mousePos);

    void removePoint(int idx);

    void processWindowEvent(const sf::Event& event, const TV::Math::SplineFunction* spline, int hoveringPoint);

    void modifyPoints(const std::function<void()>& modFunc);

    void setPoints(const std::vector<Point>& newPoints);

    std::vector<Point> getUserPoints();

    void initialPointsState();

    void initialSettingsState();

    void drawGuiWidgets();

    std::pair<std::string, std::string> captureCurrentPoints();

    static void writeToContainer(TextContainer& container,
                                 const std::string& xFormat, const std::string& xString,
                                 const std::string& yFormat, const std::string& yString);

    void refreshCoordinateSystem();

    std::vector<Point> parsePoints(const std::string& xStr, const std::string& yStr) const;

    void updateMouseTooltip(int hoveringPoint, const std::vector<Point>& userKnots) const;

    bool mIsDrawRefLines = false;
    int mPointSize = 6;
    int mConPointSize = 3;
    int mLineThickness = 1;
    // limits for spline normalized coordinates
    // changing it - bad idea: low values introduce error for little x deltas
    // high values overflow integral part during calculations
    int mScale = 15;
    // screen pixels withing which it would be impossible to move point closer
    // maybe there is a way to calculate it instead of hardcode
    int mXMinDelta = 5;
    // number of intermediate points on the screen
    int mResolution = 100;
    SplineType mSplineType = CubicMonotone;
    bool mIsRawValues = false;

    // app has 3 coordinate systems: user defined coords, screen coords and spline internal (normalized) coords
    BoundsRect<TV::Math::Dec16> mUserCoords;
    BoundsRect<TV::Math::Dec16> mWindowCoords;
    PointTransformer mPointTransformer;

    std::vector<WindowPoint> mWindowPoints;

    sf::RenderWindow& mWindow;
    Drawer mDrawer;

    FrameContext mFrameContext;
};
