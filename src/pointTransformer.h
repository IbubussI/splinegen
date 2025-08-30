#pragma once
#include "boundsRect.h"
#include "point.h"
#include "../libs/tv/tvmath.h"

class PointTransformer {
public:
    PointTransformer(const BoundsRect<TV::Math::Dec16> userCoords,
                     const BoundsRect<TV::Math::Dec16> windowCoords)
        : mUserCoords(userCoords), mWindowCoords(windowCoords) {
    }

    [[nodiscard]] WindowPoint userToWindow(const Point point) const {
        return WindowPoint{
            TV::Math::toInt(TV::Math::rescale(point.x, mUserCoords.xMin, mUserCoords.xMax,
                                              mWindowCoords.xMin, mWindowCoords.xMax)),
            TV::Math::toInt(mWindowCoords.yMax - TV::Math::rescale(point.y, mUserCoords.yMin, mUserCoords.yMax,
                                                                   mWindowCoords.yMin, mWindowCoords.yMax))
        };
    }

    [[nodiscard]] Point windowToUser(const WindowPoint point) const {
        return Point{
            TV::Math::rescale(TV::Math::Dec16{point.x}, mWindowCoords.xMin, mWindowCoords.xMax,
                              mUserCoords.xMin, mUserCoords.xMax),
            TV::Math::rescale(mWindowCoords.yMax - TV::Math::Dec16{point.y}, mWindowCoords.yMin, mWindowCoords.yMax,
                              mUserCoords.yMin, mUserCoords.yMax),
        };
    }

private:
    BoundsRect<TV::Math::Dec16> mUserCoords;
    BoundsRect<TV::Math::Dec16> mWindowCoords;
};
