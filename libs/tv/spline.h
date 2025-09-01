// ReSharper disable CppUE4CodingStandardNamingViolationWarning
#pragma once
#include <vector>
#include <algorithm>
#include "tvmath.h"

namespace TV::Math {
    // 16-16 scheme is imprecise, since it can easily overflow integral part
    // while calculating "d" coefficient for cubic splines for small intervals by x.
    // To mitigate this wider type, e.g. fixed<int64, int128, 24> can be used

    class SplineFunction {
    public:
        virtual ~SplineFunction() = default;

        // returns [x,y] for given coord value
        [[nodiscard]] virtual std::pair<Dec16, Dec16> value(Dec16 coord) const = 0;

        [[nodiscard]] virtual Dec16 getCoordMin() const = 0;

        [[nodiscard]] virtual Dec16 getCoordMax() const = 0;

        [[nodiscard]] virtual int getClosestKnotIndex(Dec16 coord) const = 0;
    };

    // Function that perform interpolation of a point by polynome coefficients
    class PolynomialSplineFunction final : public SplineFunction {
    public:
        PolynomialSplineFunction(std::vector<Dec16> knots, std::vector<std::vector<Dec16>> polynomials,
                                 const Dec16 xScale, const Dec16 yScale,
                                 const Dec16 origXMin, const Dec16 origXMax,
                                 const Dec16 origYMin, const Dec16 origYMax)
            : segmentNum(polynomials.size()),
              knots(std::move(knots)),
              polynomials(std::move(polynomials)),
              mXScale(xScale),
              mYScale(yScale),
              mOrigXMin(origXMin),
              mOrigXMax(origXMax),
              mOrigYMin(origYMin),
              mOrigYMax(origYMax) {
        }

        [[nodiscard]] std::pair<Dec16, Dec16> value(const Dec16 coord) const override {
            const Dec16 xNorm = rescale(coord, mOrigXMin, mOrigXMax, Dec16{0}, mXScale);
            return std::pair{coord, valueNorm(xNorm, mOrigYMin, mOrigYMax)};
        }

        [[nodiscard]] Dec16 valueNorm(const Dec16 xNorm, const Dec16 resMin, const Dec16 resMax) const {
            assert(xNorm >= knots[0]);
            assert(xNorm <= knots[segmentNum]);

            int i = binSearch(knots, xNorm);
            assert(i >= 0);
            if (i > 0) {
                --i;
            }

            const std::vector<Dec16>& spline = polynomials[i];
            const Dec16 r = interpPolynomial(spline, xNorm - knots[i]);
            return rescale(r, Dec16{0}, mYScale, resMin, resMax);
        }

        [[nodiscard]] Dec16 getCoordMin() const override {
            return mOrigXMin;
        }

        [[nodiscard]] Dec16 getCoordMax() const override {
            return mOrigXMax;
        }

        [[nodiscard]] int getClosestKnotIndex(const Dec16 coord) const override {
            const Dec16 coordNorm = rescale(coord, mOrigXMin, mOrigXMax, Dec16{0}, mXScale);
            return binSearch(knots, coordNorm);
        }

    private:
        /**
         * Number of spline segments
         */
        const int segmentNum;
        /**
         * Segment delimiter points. Size segmentNum + 1
         */
        const std::vector<Dec16> knots;
        /**
         * Segment spline function params. Size segmentNum
         */
        const std::vector<std::vector<Dec16>> polynomials;

        // Normalized scale max value
        const Dec16 mXScale;
        const Dec16 mYScale;

        // Original scale boundaries
        const Dec16 mOrigXMin;
        const Dec16 mOrigXMax;
        const Dec16 mOrigYMin;
        const Dec16 mOrigYMax;

        // Horner's scheme for polynomial evaluation
        [[nodiscard]] static Dec16 interpPolynomial(const std::vector<Dec16>& coefficients, const Dec16 t) {
            const int n = coefficients.size();
            Dec16 result = coefficients[n - 1];
            for (int j = n - 2; j >= 0; j--) {
                result = t * result + coefficients[j];
            }
            return result;
        }
    };

    class Parametric2DPolynomialSplineFunction final : public SplineFunction {
    public:
        Parametric2DPolynomialSplineFunction(PolynomialSplineFunction xFunc,
                                             PolynomialSplineFunction yFunc,
                                             std::vector<Dec16> tKnots,
                                             const Dec16 xMin, const Dec16 xMax,
                                             const Dec16 yMin, const Dec16 yMax)
            : mXFunc(std::move(xFunc)),
              mYFunc(std::move(yFunc)),
              mTKnots(std::move(tKnots)),
              mXMin(xMin),
              mXMax(xMax),
              mYMin(yMin),
              mYMax(yMax) {
        }

        [[nodiscard]] std::pair<Dec16, Dec16> value(const Dec16 coord) const override {
            return std::pair{valueX(coord), valueY(coord)};
        }

        [[nodiscard]] Dec16 valueX(const Dec16 t) const {
            return mXFunc.valueNorm(t, mXMin, mXMax);
        }

        [[nodiscard]] Dec16 valueY(const Dec16 t) const {
            return mYFunc.valueNorm(t, mYMin, mYMax);
        }

        [[nodiscard]] Dec16 getCoordMin() const override {
            return mTKnots[0];
        }

        [[nodiscard]] Dec16 getCoordMax() const override {
            return mTKnots[mTKnots.size() - 1];
        }

        [[nodiscard]] int getClosestKnotIndex(const Dec16 coord) const override {
            return binSearch(mTKnots, coord);
        }

    private:
        const PolynomialSplineFunction mXFunc;
        const PolynomialSplineFunction mYFunc;
        std::vector<Dec16> mTKnots;
        const Dec16 mXMin;
        const Dec16 mXMax;
        const Dec16 mYMin;
        const Dec16 mYMax;
    };

    // Interpolator is a class that generates interpolator functions,
    // which can be used to interpolate the data
    class Interpolator {
    public:
        // Constructs interpolator from given points.
        // Scale factors are used to rescale given values to [0...scale].
        // It is required to keep balance between precision of small numbers and the limit of large numbers
        Interpolator(const std::vector<Dec16>& xVals, const std::vector<Dec16>& yVals,
                     const Dec16 xScale = Dec16{15}, const Dec16 yScale = Dec16{15})
            : mXScale(xScale),
              mYScale(yScale),
              mXVals(xVals),
              mYVals(yVals),
              mXNormVals(std::vector<Dec16>(xVals.size())),
              mYNormVals(std::vector<Dec16>(yVals.size())),
              mUseFallback(true) {
            const auto [xm, xM] = std::minmax_element(xVals.begin(), xVals.end());
            mXMin = *xm;
            mXMax = *xM;

            const auto [ym, yM] = std::minmax_element(yVals.begin(), yVals.end());
            mYMin = *ym;
            mYMax = *yM;

            normalizeXYValues();
        }

        [[nodiscard]] PolynomialSplineFunction interpolateLinear() const {
            return interpolateLinear(mXNormVals, mYNormVals);
        }

        [[nodiscard]] PolynomialSplineFunction interpolateNatural() const {
            return interpolateNatural(mXNormVals, mYNormVals);
        }

        [[nodiscard]] PolynomialSplineFunction interpolateAkima() const {
            return interpolateAkima(mXNormVals, mYNormVals);
        }

        [[nodiscard]] Parametric2DPolynomialSplineFunction interpolate2D() const {
            return interpolate2D(mXNormVals, mYNormVals);
        }

    private:
        Dec16 mXScale;
        Dec16 mYScale;
        Dec16 mXMin;
        Dec16 mXMax;
        Dec16 mYMin;
        Dec16 mYMax;
        const std::vector<Dec16>& mXVals;
        const std::vector<Dec16>& mYVals;
        std::vector<Dec16> mXNormVals;
        std::vector<Dec16> mYNormVals;
        bool mUseFallback;

        void normalizeXYValues() {
            std::transform(mXVals.begin(), mXVals.end(), mXNormVals.begin(),
                           [this](const Dec16 x) {
                               if (mXMax != mXMin) {
                                   return rescale(x, mXMin, mXMax, Dec16{0}, mXScale);
                               }
                               return mXMin;
                           });
            std::transform(mYVals.begin(), mYVals.end(), mYNormVals.begin(),
                           [this](const Dec16 y) {
                               if (mYMax != mYMin) {
                                   return rescale(y, mYMin, mYMax, Dec16{0}, mYScale);
                               }
                               return mYMin;
                           });
        }

        static Dec16 differentiateThreePoint(const std::vector<Dec16>& xVals,
                                             const std::vector<Dec16>& yVals,
                                             const int indexOfDifferentiation,
                                             const int indexOfFirstSample,
                                             const int indexOfSecondSample,
                                             const int indexOfThirdSample) {
            const Dec16 x0 = yVals[indexOfFirstSample];
            const Dec16 x1 = yVals[indexOfSecondSample];
            const Dec16 x2 = yVals[indexOfThirdSample];

            const Dec16 t = xVals[indexOfDifferentiation] - xVals[indexOfFirstSample];
            const Dec16 t1 = xVals[indexOfSecondSample] - xVals[indexOfFirstSample];
            const Dec16 t2 = xVals[indexOfThirdSample] - xVals[indexOfFirstSample];

            const Dec16 a = (x2 - x0 - t2 / t1 * (x1 - x0)) / (t2 * t2 - t1 * t2);
            const Dec16 b = (x1 - x0 - a * t1 * t1) / t1;

            return 2 * a * t + b;
        }

        // Constructs linear interpolation function
        [[nodiscard]] PolynomialSplineFunction interpolateLinear(const std::vector<Dec16>& xVals,
                                                                 const std::vector<Dec16>& yVals) const {
            // number of points
            const int m = xVals.size();
            // number of segments
            const int n = m - 1;

            assert(m == yVals.size());
            assert(m >= 2);

            std::vector<Dec16> k(n);
            for (int i = 0; i < n; i++) {
                k[i] = (yVals[i + 1] - yVals[i]) / (xVals[i + 1] - xVals[i]);
            }

            std::vector<std::vector<Dec16>> spk(n);
            for (int i = 0; i < n; ++i) {
                spk[i] = std::vector{yVals[i], k[i]};
            }
            return PolynomialSplineFunction{xVals, spk, mXScale, mYScale, mXMin, mXMax, mYMin, mYMax};
        }

        // Constructs natural (with continuous 2nd derivative) cubic interpolation function
        [[nodiscard]] PolynomialSplineFunction interpolateNatural(const std::vector<Dec16>& xVals,
                                                                  const std::vector<Dec16>& yVals) const {
            // number of points
            const int m = xVals.size();
            // number of segments
            const int n = m - 1;

            assert(m == yVals.size());
            assert(m >= 3 || mUseFallback);
            // fallback to another type instead of failing assert
            if (mUseFallback && m < 3) {
                return interpolateLinear(xVals, yVals);
            }

            std::vector<Dec16> b(n);
            std::vector<Dec16> d(n);

            // Differences between knot points
            std::vector<Dec16> h;
            for (int i = 0; i < n; ++i) {
                h.push_back(xVals[i + 1] - xVals[i]);
            }

            std::vector<Dec16> alpha;
            alpha.emplace_back(0);
            for (int i = 1; i < n; ++i) {
                alpha.push_back(3 * (yVals[i + 1] - yVals[i]) / h[i] - 3 * (yVals[i] - yVals[i - 1]) / h[i - 1]);
            }

            std::vector<Dec16> c(n + 1);
            std::vector<Dec16> l(n + 1);
            std::vector<Dec16> mu(n);
            std::vector<Dec16> z(n + 1);
            l[0] = Dec16{1};
            mu[0] = Dec16{0};
            z[0] = Dec16{0};

            for (int i = 1; i < n; ++i) {
                l[i] = 2 * (xVals[i + 1] - xVals[i - 1]) - h[i - 1] * mu[i - 1];
                mu[i] = h[i] / l[i];
                z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
            }

            l[n] = Dec16{1};
            z[n] = Dec16{0};
            c[n] = Dec16{0};

            for (int j = n - 1; j >= 0; --j) {
                c[j] = z[j] - mu[j] * c[j + 1];
                Dec16 ba = (yVals[j + 1] - yVals[j]) / h[j];
                Dec16 bb = h[j] * (c[j + 1] + 2 * c[j]) / 3;
                b[j] = ba - bb;
                d[j] = (c[j + 1] - c[j]) / 3 / h[j];
            }

            std::vector<std::vector<Dec16>> spk(n);
            for (int i = 0; i < n; ++i) {
                spk[i] = std::vector{yVals[i], b[i], c[i], d[i]};
            }
            return PolynomialSplineFunction{xVals, spk, mXScale, mYScale, mXMin, mXMax, mYMin, mYMax};
        }

        // Constructs Akima cubic interpolation function
        [[nodiscard]] PolynomialSplineFunction interpolateAkima(const std::vector<Dec16>& xVals,
                                                                const std::vector<Dec16>& yVals) const {
            // number of points
            const int m = xVals.size();
            // number of segments
            const int n = m - 1;
            assert(m == yVals.size());
            assert(m >= 5 || mUseFallback);
            // fallback to another type instead of failing assert
            if (mUseFallback && m < 5) {
                return interpolateNatural(xVals, yVals);
            }

            std::vector<Dec16> differences(n);
            std::vector<Dec16> weights(n);

            for (int i = 0; i < n; i++) {
                differences[i] = (yVals[i + 1] - yVals[i]) / (xVals[i + 1] - xVals[i]);
            }

            for (int i = 1; i < n; i++) {
                weights[i] = abs(differences[i] - differences[i - 1]);
            }

            // Prepare Hermite interpolation scheme.
            std::vector<Dec16> firstDerivatives(m);

            for (int i = 2; i < m - 2; i++) {
                const Dec16 wP = weights[i + 1];
                const Dec16 wM = weights[i - 1];
                if (wP == Dec16{0} && wM == Dec16{0}) {
                    const Dec16 xv = xVals[i];
                    const Dec16 xvP = xVals[i + 1];
                    const Dec16 xvM = xVals[i - 1];
                    firstDerivatives[i] = ((xvP - xv) * differences[i - 1] + (xv - xvM) * differences[i]) / (xvP - xvM);
                } else {
                    firstDerivatives[i] = (wP * differences[i - 1] + wM * differences[i]) / (wP + wM);
                }
            }

            firstDerivatives[0] = differentiateThreePoint(xVals, yVals, 0, 0, 1, 2);
            firstDerivatives[1] = differentiateThreePoint(xVals, yVals, 1, 0, 1, 2);
            firstDerivatives[m - 2] = differentiateThreePoint(xVals, yVals, m - 2, m - 3, m - 2, m - 1);
            firstDerivatives[m - 1] = differentiateThreePoint(xVals, yVals, m - 1, m - 3, m - 2, m - 1);

            // hermite cubic spline interpolation
            std::vector<std::vector<Dec16>> spk(n);
            for (int i = 0; i < n; i++) {
                Dec16 w = xVals[i + 1] - xVals[i];
                Dec16 w2 = w * w;

                Dec16 yv = yVals[i];
                Dec16 yvP = yVals[i + 1];

                Dec16 fd = firstDerivatives[i];
                Dec16 fdP = firstDerivatives[i + 1];

                spk[i] = std::vector{
                    yv,
                    firstDerivatives[i],
                    (3 * (yvP - yv) / w - 2 * fd - fdP) / w,
                    (2 * (yv - yvP) / w + fd + fdP) / w2
                };
            }

            return PolynomialSplineFunction{
                xVals, spk, mXScale, mYScale,
                mXMin, mXMax, mYMin, mYMax
            };
        }

        // Constructs 2D (parametric) Akima cubic interpolation function. Uses length between knots as parameter.
        // Neighbour points must have different coordinates (non-zero interval length) to avoid zero-division.
        [[nodiscard]] Parametric2DPolynomialSplineFunction interpolate2D(const std::vector<Dec16>& xNormVals,
                                                                         const std::vector<Dec16>& yNormVals)
        const {
            // number of points
            const int m = xNormVals.size();

            std::vector<Dec16> chordLengths(m);
            Dec16 sum = Dec16{0};
            chordLengths[0] = sum;
            for (int i = 1; i < m; i++) {
                // gives less wiggly curves comparing to true length
                const Dec16 g = abs(xNormVals[i] - xNormVals[i - 1])
                                + abs(yNormVals[i] - yNormVals[i - 1]);
                const Dec16 length = sqrt(g);

                sum += length;
                chordLengths[i] = sum;
            }

            const PolynomialSplineFunction xFunc = interpolateAkima(chordLengths, xNormVals);
            const PolynomialSplineFunction yFunc = interpolateAkima(chordLengths, yNormVals);
            return Parametric2DPolynomialSplineFunction{
                xFunc, yFunc, chordLengths,
                mXMin, mXMax, mYMin, mYMax
            };
        }
    };
}
