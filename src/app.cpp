#include "app.h"

#include <ranges>

#include "imgui.h"
#include "imgui-SFML.h"
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include "pointTransformer.h"
#include "drawer.h"
#include "pointUtils.h"
#include "tv/spline.h"
#include "textContainer.h"
#include "misc/cpp/imgui_stdlib.h"

App::App(sf::RenderWindow& window)
    : mUserCoords(TV::Math::Dec16{0}, TV::Math::Dec16{100},
                  TV::Math::Dec16{0}, TV::Math::Dec16{100}),
      mWindowCoords(TV::Math::Dec16{0}, TV::Math::Dec16{window.getSize().x},
                    TV::Math::Dec16{0}, TV::Math::Dec16{window.getSize().y}),
      mPointTransformer(mUserCoords, mWindowCoords),
      mWindowPoints(std::vector{
          WindowPoint(0, window.getSize().y),
          WindowPoint(window.getSize().x, 0),
      }),
      mWindow(window),
      mDrawer(Drawer(mWindow)) {
    mFrameContext.userPoints.push_back(Point{TV::Math::Dec16{0}, TV::Math::Dec16{0}});
    mFrameContext.userPoints.push_back(Point{TV::Math::Dec16{100}, TV::Math::Dec16{100}});
}

void App::initialPointsState() {
    mWindowPoints = std::vector{
        WindowPoint(0, mWindow.getSize().y),
        WindowPoint(mWindow.getSize().x, 0),
    };
    mFrameContext.userPoints.clear();
    mFrameContext.userPoints.push_back(Point{TV::Math::Dec16{mUserCoords.xMin}, TV::Math::Dec16{mUserCoords.yMin}});
    mFrameContext.userPoints.push_back(Point{TV::Math::Dec16{mUserCoords.xMax}, TV::Math::Dec16{mUserCoords.yMax}});
}

void App::initialSettingsState() {
    mUserCoords = BoundsRect{
        TV::Math::Dec16{0}, TV::Math::Dec16{100},
        TV::Math::Dec16{0}, TV::Math::Dec16{100}
    };
    mIsDrawRefLines = false;
    mPointSize = 6;
    mConPointSize = 3;
    mLineThickness = 1;
    mResolution = 100;
    mSplineType = CubicMonotone;
    refreshCoordinateSystem();
}

void App::run() {
    using namespace sf;
    using namespace TV::Math;

    while (mWindow.isOpen()) {
        const std::vector<Point>& userKnots = getUserPoints();
        const SplineFunction* spline = generateSpline(userKnots);
        // intermediate points
        std::vector<WindowPoint> points = generateIntermediatePoints(spline);
        const Vector2i mousePos = Mouse::getPosition(mWindow);
        const int hoveringPoint = findPointUnderCursor(mousePos);

        ImGuiIO& io = ImGui::GetIO();
        while (const std::optional eventOpt = mWindow.pollEvent()) {
            if (eventOpt.has_value()) {
                Event event = eventOpt.value();
                ImGui::SFML::ProcessEvent(mWindow, event);
                if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
                    // pass events to GUI
                    continue;
                }
                processWindowEvent(event, spline, hoveringPoint);
            }
        }

        updateDragLocation(mFrameContext.dragPointIdx);

        mDrawer.setDrawRefLines(mIsDrawRefLines);
        mDrawer.setConPointSize(mConPointSize);
        mDrawer.setPointSize(mPointSize);
        mDrawer.draw(mWindowPoints, points);

        ImGui::SFML::Update(mWindow, mFrameContext.deltaClock.restart());
        drawGuiWidgets();
        updateMouseTooltip(hoveringPoint, userKnots);
        //ImGui::ShowDemoWindow();
        ImGui::SFML::Render(mWindow);

        mWindow.display();

        delete spline;
    }
}

void App::processWindowEvent(const sf::Event& event, const TV::Math::SplineFunction* spline, const int hoveringPoint) {
    using namespace sf;

    if (event.is<Event::Closed>()) {
        mWindow.close();
    } else if (const auto* resized = event.getIf<Event::Resized>()) {
        // update the view to the new size of the window
        // incomplete: need to update knots to match new window size with the same scale
        // resize is disabled in main
        const FloatRect visibleArea(Vector2f(0, 0), Vector2f(resized->size.x, resized->size.y));
        mWindow.setView(View(visibleArea));
        mWindowCoords = BoundsRect(TV::Math::Dec16{0}, TV::Math::Dec16{mWindow.getSize().x},
                                   TV::Math::Dec16{0}, TV::Math::Dec16{mWindow.getSize().y});
        refreshCoordinateSystem();
    } else if (const auto* mousePressed = event.getIf<Event::MouseButtonPressed>()) {
        if (mousePressed->button == Mouse::Button::Left) {
            // if click is on the point - start drag, otherwise try to insert point
            if (hoveringPoint != -1) {
                mFrameContext.dragPointIdx = hoveringPoint;
            } else {
                tryInsertPoint(spline, Mouse::getPosition(mWindow));
            }
        } else if (mousePressed->button == Mouse::Button::Right) {
            if (mFrameContext.dragPointIdx == -1 && hoveringPoint != -1) {
                removePoint(hoveringPoint);
            }
        }
    } else if (const auto* mouseReleased = event.getIf<Event::MouseButtonReleased>()) {
        if (mouseReleased->button == Mouse::Button::Left) {
            // clear drag
            mFrameContext.dragPointIdx = -1;
        }
    }
}

void App::drawGuiWidgets() {
    using namespace TV::Math;

    ImGui::Begin("Settings");
    if (ImGui::Button("Reset Settings")) {
        initialSettingsState();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Points")) {
        initialPointsState();
    }
    ImGui::Checkbox("Draw Reference Lines", &mIsDrawRefLines);
    const bool isParametricBefore = isParametric(mSplineType);
    if (ImGui::Combo("Spline Type", reinterpret_cast<int*>(&mSplineType),
                     "Linear\0Cubic\0Cubic Monotone\0Parametric\0")) {
        const bool isParametricAfter = isParametric(mSplineType);
        if (isParametricBefore && !isParametricAfter) {
            initialPointsState();
        }
    }
    ImGui::InputInt("Point Size", &mPointSize);

    ImGui::Checkbox("Use Raw Values", &mIsRawValues);
    int yScale[2]{};
    int xScale[2]{};
    if (mIsRawValues) {
        xScale[0] = mUserCoords.xMin.raw_value();
        xScale[1] = mUserCoords.xMax.raw_value();
        yScale[0] = mUserCoords.yMin.raw_value();
        yScale[1] = mUserCoords.yMax.raw_value();
    } else {
        xScale[0] = toInt(mUserCoords.xMin);
        xScale[1] = toInt(mUserCoords.xMax);
        yScale[0] = toInt(mUserCoords.yMin);
        yScale[1] = toInt(mUserCoords.yMax);
    }
    if (ImGui::InputInt2("XScale", xScale)) {
        Dec16 newXMin;
        Dec16 newXMax;
        if (mIsRawValues) {
            newXMin = Dec16::from_raw_value(xScale[0]);
            newXMax = Dec16::from_raw_value(xScale[1]);
        } else {
            newXMin = Dec16{xScale[0]};
            newXMax = Dec16{xScale[1]};
        }

        // validate bounds
        if (newXMax - newXMin < Dec16{1}) {
            if (mUserCoords.xMin != newXMin) {
                newXMin = newXMax - Dec16{1};
            }
            if (mUserCoords.xMax != newXMax) {
                newXMax = newXMin + Dec16{1};
            }
        }

        mUserCoords.xMin = newXMin;
        mUserCoords.xMax = newXMax;
        refreshCoordinateSystem();
    }
    if (ImGui::InputInt2("YScale", yScale)) {
        Dec16 newYMin;
        Dec16 newYMax;
        if (mIsRawValues) {
            newYMin = Dec16::from_raw_value(yScale[0]);
            newYMax = Dec16::from_raw_value(yScale[1]);
        } else {
            newYMin = Dec16{yScale[0]};
            newYMax = Dec16{yScale[1]};
        }

        // validate bounds
        if (newYMax - newYMin < Dec16{1}) {
            if (mUserCoords.yMin != newYMin) {
                newYMin = newYMax - Dec16{1};
            } else {
                newYMax = newYMin + Dec16{1};
            }
        }

        mUserCoords.yMin = newYMin;
        mUserCoords.yMax = newYMax;
        refreshCoordinateSystem();
    }

    ImGui::InputInt("Resolution", &mResolution);
    const auto xyStrings = captureCurrentPoints();
    TextContainer pointsText{};
    TextContainer pointsCode{};
    writeToContainer(pointsText,
                     "x: {}", xyStrings.first,
                     "y: {}", xyStrings.second);

    ImGui::Text("Points:");
    ImGui::Indent();
    ImGui::Text(pointsText.getContent());
    ImGui::Unindent();
    if (ImGui::Button("Copy X")) {
        ImGui::SetClipboardText(xyStrings.first.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy Y")) {
        ImGui::SetClipboardText(xyStrings.second.c_str());
    }

    static std::string loadX;
    static std::string loadY;
    ImGui::InputText("Load X", &loadX);
    ImGui::InputText("Load Y", &loadY);
    if (ImGui::Button("Load Points")) {
        setPoints(parsePoints(loadX, loadY));
    }

    ImGui::End();
}

std::pair<std::string, std::string> App::captureCurrentPoints() {
    std::ostringstream xStream;
    std::ostringstream yStream;
    for (int i = 0; i < mFrameContext.userPoints.size(); i++) {
        auto [x, y] = mFrameContext.userPoints[i];
        const int xVal = mIsRawValues ? x.raw_value() : TV::Math::toInt(x);
        const int yVal = mIsRawValues ? y.raw_value() : TV::Math::toInt(y);

        xStream << xVal;
        yStream << yVal;
        if (i + 1 < mFrameContext.userPoints.size()) {
            xStream << ", ";
            yStream << ", ";
        }
    }
    return std::pair{xStream.str(), yStream.str()};
}

void App::writeToContainer(TextContainer& container,
                           const std::string& xFormat, const std::string& xString,
                           const std::string& yFormat, const std::string& yString) {
    const std::string formattedX = std::vformat(xFormat, std::make_format_args(xString));
    const std::string formattedY = std::vformat(yFormat, std::make_format_args(yString));
    container.update(formattedX + "\n" + formattedY);
}

void App::refreshCoordinateSystem() {
    mPointTransformer = PointTransformer(mUserCoords, mWindowCoords);
    mFrameContext.isUserModifiedPoints = true;
}

std::vector<Point> App::parsePoints(const std::string& xStr, const std::string& yStr) const {
    std::string_view xView(xStr);
    std::string_view yView(yStr);
    std::vector<Point> result;
    try {
        std::vector<int> xVec;
        for (const auto str: std::views::split(xView, ',')) {
            xVec.push_back(std::stoi(str.data()));
        }
        std::vector<int> yVec;
        for (const auto str: std::views::split(yView, ',')) {
            yVec.push_back(std::stoi(str.data()));
        }

        if (xVec.size() == yVec.size()) {
            for (int i = 0; i < xVec.size(); i++) {
                TV::Math::Dec16 x;
                TV::Math::Dec16 y;
                if (mIsRawValues) {
                    x = TV::Math::Dec16::from_raw_value(xVec[i]);
                    y = TV::Math::Dec16::from_raw_value(yVec[i]);
                } else {
                    x = TV::Math::Dec16{xVec[i]};
                    y = TV::Math::Dec16{yVec[i]};
                }
                result.emplace_back(x, y);
            }
        }
    } catch (...) {
    }
    return result;
}

void App::updateMouseTooltip(const int hoveringPoint, const std::vector<Point>& userKnots) const {
    const int pointIdx = mFrameContext.dragPointIdx != -1 ? mFrameContext.dragPointIdx : hoveringPoint;
    if (pointIdx != -1) {
        const Point point = userKnots[pointIdx];
        int x;
        int y;
        if (mIsRawValues) {
            x = point.x.raw_value();
            y = point.y.raw_value();
        } else {
            x = TV::Math::toInt(point.x);
            y = TV::Math::toInt(point.y);
        }
        const std::string tooltip = std::format("{}, {}", x, y);

        ImGui::BeginTooltip();
        ImGui::Text(tooltip.c_str());
        ImGui::EndTooltip();
    }

}

TV::Math::SplineFunction* App::generateSpline(const std::vector<Point>& points) const {
    using namespace TV::Math;

    // transform point array to x/y arrays
    std::vector<Dec16> x(points.size());
    std::vector<Dec16> y(points.size());
    std::transform(points.begin(), points.end(), x.begin(),
                   [](const Point p) { return p.x; });
    std::transform(points.begin(), points.end(), y.begin(),
                   [](const Point p) { return p.y; });

    const Interpolator interpolator(x, y, Dec16{mScale}, Dec16{mScale});

    switch (mSplineType) {
        case Cubic:
            return new PolynomialSplineFunction(interpolator.interpolateNatural());
        case CubicMonotone:
            return new PolynomialSplineFunction(interpolator.interpolateAkima());
        case Parametric:
            return new Parametric2DPolynomialSplineFunction(interpolator.interpolate2D());
        case Linear:
        default:
            return new PolynomialSplineFunction(interpolator.interpolateLinear());
    }
}

std::vector<WindowPoint> App::generateIntermediatePoints(const TV::Math::SplineFunction* spline) const {
    using namespace TV::Math;

    std::vector<WindowPoint> iPoints;
    const Dec16 cMin = spline->getCoordMin();
    const Dec16 cMax = spline->getCoordMax();
    const Dec16 step = (cMax - cMin) / mResolution;
    iPoints.reserve(mResolution);
    for (Dec16 ci = cMin; ci <= cMax; ci += step) {
        // get point in user coordinates with user spline
        const auto [x, y] = spline->value(ci);
        const Point userPoint(
            mUserCoords.clampX(x),
            mUserCoords.clampY(y)
        );
        // transform point to window coordinates
        const WindowPoint p = mPointTransformer.userToWindow(userPoint);
        iPoints.push_back(p);
    }
    return iPoints;
}

bool App::isParametric(const SplineType splineType) {
    return splineType == Parametric;
}

void App::updateDragLocation(const int dragIdx) {
    if (dragIdx == -1) {
        // nothing to update: no ongoing drag
        return;
    }

    const int windowXSize = mWindow.getSize().x;
    const int windowYSize = mWindow.getSize().y;
    const sf::Vector2i mousePos = sf::Mouse::getPosition(mWindow);

    const WindowPoint dragPoint = mWindowPoints[dragIdx];
    WindowPoint newDragPoint = dragPoint;
    newDragPoint.y = std::clamp(mousePos.y, 0, windowYSize);
    newDragPoint.x = std::clamp(mousePos.x, 0, windowXSize);

    // point movement restrictions
    if (isParametric(mSplineType)) {
        if (dragIdx != 0) {
            const WindowPoint prev = mWindowPoints[dragIdx - 1];
            newDragPoint = SplGen::pointToPointCollide(newDragPoint, prev, mXMinDelta);
        }
        if (dragIdx != mWindowPoints.size() - 1) {
            const WindowPoint next = mWindowPoints[dragIdx + 1];
            newDragPoint = SplGen::pointToPointCollide(newDragPoint, next, mXMinDelta);
        }
    } else {
        // only move non-border points by X
        if (dragIdx != mWindowPoints.size() - 1 && dragIdx != 0) {
            newDragPoint = SplGen::pointRestrictX(newDragPoint, mWindowPoints[dragIdx + 1].x,
                                                  mWindowPoints[dragIdx - 1].x, mXMinDelta);
        } else {
            newDragPoint.x = dragPoint.x;
        }
    }

    // update point
    modifyPoints([this, dragIdx, newDragPoint] {
        mWindowPoints[dragIdx] = newDragPoint;
    });
}

int App::findPointUnderCursor(const sf::Vector2i mousePos) const {
    using namespace sf;

    int result = -1;
    for (int i = 0; i < mWindowPoints.size(); i++) {
        if (mWindowPoints[i].isInBounds(mousePos.x, mousePos.y, mPointSize)) {
            result = i;
            break;
        }
    }
    return result;
}

// returns click position and the knot index after which the click occurred
// or -1 if the click was too far from the spline
std::pair<WindowPoint, int> App::findSplineClicked(const TV::Math::SplineFunction* spline,
                                                   const sf::Vector2i mousePos,
                                                   const int dist) const {
    using namespace TV::Math;

    const Dec16 cMin = spline->getCoordMin();
    const Dec16 cMax = spline->getCoordMax();
    const Dec16 step = (cMax - cMin) / mResolution;

    const auto [x, y] = spline->value(cMin);
    const Point userPrev(
        mUserCoords.clampX(x),
        mUserCoords.clampY(y)
    );
    WindowPoint prev = mPointTransformer.userToWindow(userPrev);
    for (Dec16 ci = cMin; ci <= cMax; ci += step) {
        const auto [x, y] = spline->value(ci);
        const Point userCurr(
            mUserCoords.clampX(x),
            mUserCoords.clampY(y)
        );

        const WindowPoint curr = mPointTransformer.userToWindow(userCurr);
        const WindowPoint mouse = WindowPoint(mousePos.x, mousePos.y);
        if (SplGen::pointToLineSegmentCollide(prev, curr, mouse, dist)) {
            return std::pair{mouse, spline->getClosestKnotIndex(ci)};
        }
        prev = curr;
    }
    return std::pair{WindowPoint{}, -1};
}

void App::tryInsertPoint(const TV::Math::SplineFunction* spline, const sf::Vector2i mousePos) {
    // check if click is on the spline
    std::pair<WindowPoint, int> result = findSplineClicked(spline, mousePos, mPointSize);
    if (result.second != -1) {
        const WindowPoint clickLocation = result.first;
        const int knotIndex = result.second;
        const bool isParametricSpline = isParametric(mSplineType);
        const bool prevAllows = clickLocation.x - mWindowPoints[knotIndex - 1].x >= mXMinDelta;
        const bool nextAllows = mWindowPoints[knotIndex].x - clickLocation.x >= mXMinDelta;
        if (isParametricSpline || prevAllows && nextAllows) {
            // insert new point
            modifyPoints([this, knotIndex, clickLocation] {
                mWindowPoints.insert(mWindowPoints.begin() + knotIndex, clickLocation);
            });
            // start drag
            mFrameContext.dragPointIdx = result.second;
        }
    }
}

void App::removePoint(const int idx) {
    if (mWindowPoints.size() > 2) {
        if (isParametric(mSplineType) || (idx != mWindowPoints.size() - 1 && idx != 0)) {
            modifyPoints([this, idx] { mWindowPoints.erase(mWindowPoints.begin() + idx); });
        }
    }
}

void App::modifyPoints(const std::function<void()>& modFunc) {
    modFunc();
    mFrameContext.isUserModifiedPoints = true;
}

void App::setPoints(const std::vector<Point>& newPoints) {
    mWindowPoints.resize(newPoints.size());
    std::transform(newPoints.begin(), newPoints.end(), mWindowPoints.begin(),
                   [this](const Point p) {
                       return mPointTransformer.userToWindow(p);
                   });
    mFrameContext.isUserModifiedPoints = true;
}

std::vector<Point> App::getUserPoints() {
    std::vector<Point>& userPoints = mFrameContext.userPoints;
    if (mFrameContext.isUserModifiedPoints) {
        userPoints.resize(mWindowPoints.size());
        std::transform(mWindowPoints.begin(), mWindowPoints.end(), userPoints.begin(),
                       [this](const WindowPoint p) {
                           return mPointTransformer.windowToUser(p);
                       });
        mFrameContext.isUserModifiedPoints = false;
    }
    return userPoints;
}
