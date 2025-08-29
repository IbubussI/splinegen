#include "app.h"
#include "imgui.h"
#include "imgui-SFML.h"
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include "pointTransformer.h"
#include "drawer.h"
#include "pointUtils.h"
#include "spline.h"
#include "textContainer.h"


App::App(sf::RenderWindow& window)
    : mUserCoords(TV::Math::Dec16{0}, TV::Math::Dec16{100},
                  TV::Math::Dec16{0}, TV::Math::Dec16{100}),
      mWindowCoords(TV::Math::Dec16{0}, TV::Math::Dec16{window.getSize().x},
                    TV::Math::Dec16{0}, TV::Math::Dec16{window.getSize().y}),
      mCoordsTransformer(mUserCoords, mWindowCoords),
      mWindowPoints(std::vector{
          WindowPoint(0, 0),
          WindowPoint(window.getSize().x, window.getSize().y),
      }),
      mWindow(window),
      mDrawer(Drawer(mWindow)) {
    mFrameContext.userPoints.push_back(Point{TV::Math::Dec16{0}, TV::Math::Dec16{0}});
    mFrameContext.userPoints.push_back(Point{TV::Math::Dec16{100}, TV::Math::Dec16{100}});
}

void App::run() {
    using namespace sf;
    using namespace TV::Math;

    while (mWindow.isOpen()) {
        const std::vector<Point>& userKnots = getUserPoints();
        const SplineFunction* spline = generateSpline(userKnots);
        // intermediate points
        std::vector<WindowPoint> points = generateIntermediatePoints(spline);

        ImGuiIO& io = ImGui::GetIO();
        while (const std::optional eventOpt = mWindow.pollEvent()) {
            if (eventOpt.has_value()) {
                Event event = eventOpt.value();
                ImGui::SFML::ProcessEvent(mWindow, event);
                if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
                    // pass events to GUI
                    continue;
                }
                processWindowEvent(event, spline);
            }
        }

        updateDragLocation(mFrameContext.dragPointIdx);

        processGuiWidgets();

        mDrawer.setDrawRefLines(mIsDrawRefLines);
        mDrawer.setConPointSize(mConPointSize);
        mDrawer.setPointSize(mPointSize);
        mDrawer.draw(mWindowPoints, points);

        ImGui::SFML::Render(mWindow);
        mWindow.display();

        delete spline;
    }
}

void App::processWindowEvent(const sf::Event& event, const TV::Math::SplineFunction* spline) {
    using namespace sf;

    if (event.is<Event::Closed>()) {
        mWindow.close();
    } else if (const auto* resized = event.getIf<Event::Resized>()) {
        // update the view to the new size of the window
        // incomplete: need to update knots to match new window size with the same scale
        // resize is disabled
        const FloatRect visibleArea(Vector2f(0, 0), Vector2f(resized->size.x, resized->size.y));
        mWindow.setView(View(visibleArea));
        mWindowCoords = BoundsRect(TV::Math::Dec16{0}, TV::Math::Dec16{mWindow.getSize().x},
                                   TV::Math::Dec16{0}, TV::Math::Dec16{mWindow.getSize().y});
        mCoordsTransformer = PointTransformer(mUserCoords, mWindowCoords);
        mFrameContext.isUserModifiedPoints = true;
    } else if (const auto* mousePressed = event.getIf<Event::MouseButtonPressed>()) {
        const Vector2i mousePos = Mouse::getPosition(mWindow);
        if (mousePressed->button == Mouse::Button::Left) {
            // check if click is on the point and start drag
            mFrameContext.dragPointIdx = findPointClicked(mousePos);

            // check if click is on the spline
            if (mFrameContext.dragPointIdx == -1) {
                std::pair<WindowPoint, int> result = findSplineClicked(spline, mousePos, mPointSize);
                if (result.second != -1) {
                    // insert new point
                    modifyPoints([this, result] {
                        mWindowPoints.insert(mWindowPoints.begin() + result.second, result.first);
                    });
                    // start drag
                    mFrameContext.dragPointIdx = result.second;
                }
            }
        } else if (mousePressed->button == Mouse::Button::Right) {
            if (mFrameContext.dragPointIdx == -1) {
                const int idx = findPointClicked(mousePos);
                removePoint(idx);
            }
        }
    } else if (const auto* mouseReleased = event.getIf<Event::MouseButtonReleased>()) {
        if (mouseReleased->button == Mouse::Button::Left) {
            // clear drag
            mFrameContext.dragPointIdx = -1;
        }
    }
}

void App::processGuiWidgets() {
    ImGui::SFML::Update(mWindow, mFrameContext.deltaClock.restart());
    ImGui::Begin("Settings");
    ImGui::Checkbox("Draw Reference Lines", &mIsDrawRefLines);
    ImGui::Combo("Spline Type", reinterpret_cast<int*>(&mSplineType),
                 "Linear\0Cubic\0Cubic Monotone\0Parametric\0");
    ImGui::InputInt("Point Size", &mPointSize);
    ImGui::InputInt("Scale", &mScale);
    ImGui::InputInt("Resolution", &mResolution);


    const auto xyStrings = captureCurrentPoints();
    TextContainer pointsText{};
    TextContainer pointsCode{};
    writeToContainer(pointsText,
                     "x: {}", xyStrings.first,
                     "y: {}", xyStrings.second);

    ImGui::Text("Points:");
    ImGui::Text(pointsText.getContent());
    if (ImGui::Button("Copy Points")) {
        ImGui::SetClipboardText(pointsText.getContent());
    }

    ImGui::End();

    //ImGui::ShowDemoWindow();
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
        const WindowPoint p = mCoordsTransformer.userToWindow(userPoint);
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
    constexpr int minDelta = 5; // screen pixels, maybe there is a way to calculate it instead of hardcode
    if (isParametric(mSplineType)) {
        if (dragIdx != 0) {
            const WindowPoint prev = mWindowPoints[dragIdx - 1];
            newDragPoint = SplGen::pointToPointCollide(newDragPoint, prev, minDelta);
        }
        if (dragIdx != mWindowPoints.size() - 1) {
            const WindowPoint next = mWindowPoints[dragIdx + 1];
            newDragPoint = SplGen::pointToPointCollide(newDragPoint, next, minDelta);
        }
    } else {
        // only move non-border points by X
        if (dragIdx != mWindowPoints.size() - 1 && dragIdx != 0) {
            newDragPoint = SplGen::pointRestrictX(newDragPoint, mWindowPoints[dragIdx + 1].x,
                                                  mWindowPoints[dragIdx - 1].x, minDelta);
        } else {
            newDragPoint.x = dragPoint.x;
        }
    }

    // update point
    modifyPoints([this, dragIdx, newDragPoint] {
        mWindowPoints[dragIdx] = newDragPoint;
    });
}

int App::findPointClicked(const sf::Vector2i mousePos) const {
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
    WindowPoint prev = mCoordsTransformer.userToWindow(userPrev);
    for (Dec16 ci = cMin; ci <= cMax; ci += step) {
        const auto [x, y] = spline->value(ci);
        const Point userCurr(
            mUserCoords.clampX(x),
            mUserCoords.clampY(y)
        );

        const WindowPoint curr = mCoordsTransformer.userToWindow(userCurr);
        const WindowPoint mouse = WindowPoint(mousePos.x, mousePos.y);
        if (SplGen::pointToLineSegmentCollide(prev, curr, mouse, dist)) {
            return std::pair{mouse, spline->getClosestKnotIndex(ci)};
        }
        prev = curr;
    }
    return std::pair{WindowPoint{}, -1};
}

void App::removePoint(const int idx) {
    if (idx != -1 && mWindowPoints.size() > 2) {
        if (isParametric(mSplineType) || (idx != mWindowPoints.size() - 1 && idx != 0)) {
            modifyPoints([this, idx] { mWindowPoints.erase(mWindowPoints.begin() + idx); });
        }
    }
}

void App::modifyPoints(const std::function<void()>& modFunc) {
    modFunc();
    mFrameContext.isUserModifiedPoints = true;
}

std::vector<Point> App::getUserPoints() {
    std::vector<Point>& userPoints = mFrameContext.userPoints;
    if (mFrameContext.isUserModifiedPoints) {
        userPoints.resize(mWindowPoints.size());
        std::transform(mWindowPoints.begin(), mWindowPoints.end(), userPoints.begin(),
                       [this](const WindowPoint p) {
                           return mCoordsTransformer.windowToUser(p);
                       });
        mFrameContext.isUserModifiedPoints = false;
    }
    return userPoints;
}
