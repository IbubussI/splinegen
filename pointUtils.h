#pragma once
#include <iostream>

#include "point.h"

namespace SplGen {
    inline WindowPoint pointToPointCollide(const WindowPoint movePoint,
                                           const WindowPoint anchorPoint,
                                           const int dist) {
        const WindowPoint diffPrev = movePoint - anchorPoint;
        const int rPrev = diffPrev.x * diffPrev.x + diffPrev.y * diffPrev.y;
        // avoid sqrt by comparing sqr-s
        if (rPrev < dist * dist) {
            // switch coordinate system origin to the center of prev point
            WindowPoint moveVec = diffPrev;
            if (moveVec.x == 0 && moveVec.y == 0) {
                moveVec = WindowPoint{movePoint.x + 1, movePoint.y + 1};
            }
            // normalize
            const float length = std::sqrt(moveVec.x * moveVec.x + moveVec.y * moveVec.y);
            // create point at dist from origin
            moveVec = moveVec * dist / length;
            // switch to original coordinate system
            return moveVec + anchorPoint;
        }
        return movePoint;
    }

    inline WindowPoint pointRestrictX(const WindowPoint point,
                                      const int xRight,
                                      const int xLeft,
                                      const int dist) {
        WindowPoint restrictedPoint(point);
        const bool isLeft = point.x - xLeft < dist;
        const bool isRight = xRight - point.x < dist;
        if (isLeft) {
            restrictedPoint.x = xLeft + dist;
        } else if (isRight) {
            restrictedPoint.x = xRight - dist;
        }
        return restrictedPoint;
    }

    /**
     * Check if a point is close enough to the line
     * taken from https://stackoverflow.com/a/48976071
     *
     * @param s0 line start point
     * @param s1 line end point
     * @param p point to collide
     * @param dist point size
     * @return true if line segment [s0, s1] collides with circle [c, r]
     */
    inline bool pointToLineSegmentCollide(const WindowPoint& s0,
                                          const WindowPoint& s1,
                                          const WindowPoint& p,
                                          const int dist) {
        using namespace TV::Math;
        const WindowPoint s0qp = p - s0;
        const WindowPoint s0s1 = s1 - s0;
        const int rSqr = dist * dist;

        // dot product of itself is a square of length
        const int a = s0s1.dot(s0s1);
        const int b = s0s1.dot(s0qp);
        const float t = (float) b / a; // length of projection of s0qp onto s0s1
        //std::cout << "t = " << t << std::endl;
        if (t >= 0 && t <= 1) {
            int c = s0qp.dot(s0qp);
            float r2 = c - a * t * t;
            //std::cout << "r2 = " << r2 << std::endl;
            return abs(r2) <= rSqr; // true if collides
        }
        return false;
    }
}
