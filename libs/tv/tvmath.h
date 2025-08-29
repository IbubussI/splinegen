// ReSharper disable CppUE4CodingStandardNamingViolationWarning
#pragma once
#include <cassert>
#include <cstring>
#include <format>

#include "fpm/fixed.hpp"
#include "fpm/math.hpp"

namespace TV::Math {
    inline constexpr int FRACT_BITS = 10;
    inline constexpr int FRACT = 1024; // 2 ^ BITS
    inline constexpr int FRACT_HALF = 512;

    inline constexpr int FRACT_PRECISE_BITS = 24;
    inline constexpr int FRACT_PRECISE = 16777216;
    inline constexpr int FRACT_PRECISE_HALF = 8388608;

    inline constexpr int FRACT_16_BITS = 16;
    inline constexpr int FRACT_16 = 65536;

    // Representation of a deterministic decimal number.
    // It is a core data type in this math library.
    using Dec = fpm::fixed<int32_t, int64_t, FRACT_BITS>;
    // Precise version of Dec. Supports numbers in range of [-128, 128)
    using DecPrecise = fpm::fixed<int32_t, int64_t, FRACT_PRECISE_BITS>;
    // compromise between precision and range
    using Dec16 = fpm::fixed<std::int32_t, std::int64_t, FRACT_16_BITS>;

    inline constexpr Dec DEC_HALF = Dec::from_raw_value(FRACT_HALF);
    inline constexpr DecPrecise DEC_HALF_PRECISE = DecPrecise::from_raw_value(FRACT_PRECISE_HALF);

    // basic math

    template<typename B, typename I, unsigned int F>
    constexpr fpm::fixed<B, I, F> min(fpm::fixed<B, I, F> a, fpm::fixed<B, I, F> b) {
        return a < b ? a : b;
    }

    template<typename B, typename I, unsigned int F>
    constexpr fpm::fixed<B, I, F> max(fpm::fixed<B, I, F> a, fpm::fixed<B, I, F> b) {
        return a > b ? a : b;
    }

    constexpr Dec abs(Dec d) {
        return fpm::abs(d);
    }

    inline Dec floor(Dec d) {
        return fpm::floor(d);
    }

    inline Dec round(Dec d) {
        return fpm::round(d);
    }

    template<typename B, typename I, unsigned int F>
    constexpr int32_t toInt(fpm::fixed<B, I, F> d) {
        return fpm::round(d).raw_value() / (B(1) << F);
    }

    /**
     * Rescales given value to given range.
     *
     * @param val value to rescale
     * @param valMin min possible value of val
     * @param valMax max possible value of val
     * @param resMin min possible value of result
     * @param resMax max possible value of result
     * @return val rescaled to range [resMin...resMax]
     */
    template<typename B, typename I, unsigned int F>
    constexpr fpm::fixed<B, I, F> rescale(fpm::fixed<B, I, F> val,
                                          fpm::fixed<B, I, F> valMin,
                                          fpm::fixed<B, I, F> valMax,
                                          fpm::fixed<B, I, F> resMin,
                                          fpm::fixed<B, I, F> resMax) {
        // avoid overflow by dividing the largest num first
        if (valMax > resMax) {
            return resMin + (val - valMin) / (valMax - valMin) * (resMax - resMin);
        }
        return resMin + (resMax - resMin) / (valMax - valMin) * (val - valMin);
    }

    /**
     * Normalizes given [0...valMax] value.
     *
     * @param val value to normalize
     * @param valMax max possible positive value of val
     * @return val normalized to range [0...1]
     */
    constexpr Dec normalize(Dec val, Dec valMax) {
        return val / valMax;
    }

    /**
      * Normalizes given positive [valMin...valMax] value.
      *
      * @param val value to normalize
      * @param valMin min possible value of val
      * @param valMax max possible value of val
      * @return val normalized to range [0...1]
      */
    constexpr Dec normalize(Dec val, Dec valMin, Dec valMax) {
        return (val - valMin) / (valMax - valMin);
    }

    /**
       * Denormalizes given [0...1] to the given range boundaries
       *
       * @param val value to denormalize
       * @param resMin min possible value of val
       * @param resMax max possible value of val
       * @return value in range [resMin...resMax]
       */
    constexpr Dec denormalize(Dec val, Dec resMin, Dec resMax) {
        return val * (resMax - resMin) + resMin;
    }

    /**
     * Returns val if it does not exceed [resMin...resMax] and resMin/resMax otherwise
     *
     * @param val value to clamp
     * @param resMin min boundary
     * @param resMax max boundary
     * @return clamped value in range [resMin...resMax]
     */
    template<typename B, typename I, unsigned int F>
    constexpr fpm::fixed<B, I, F> clamp(fpm::fixed<B, I, F> val, fpm::fixed<B, I, F> resMin,
                                        fpm::fixed<B, I, F> resMax) {
        return max(min(val, resMax), resMin);
    }

    /**
     * Uniformly (almost) distributes given rnd number (that is expected to be full int range)
     * to the given range: [0...range). This version is fast but biased (some numbers more frequent than other).
     * Don't recommend to use it for range greater than 2^24 (it gives a bias > 0.5%)
     */
    constexpr uint32_t boundedRand(const uint32_t rnd, const uint32_t range) {
        const uint64_t m = static_cast<uint64_t>(rnd) * static_cast<uint64_t>(range);
        return m >> 32;
    }

    // interpolation

    constexpr Dec lerp(Dec a, Dec b, Dec t) { return a + t * (b - a); }

    constexpr Dec interpHermite(Dec t) { return t * t * (3 - 2 * t); }

    // known as fade
    constexpr Dec interpQuintic(Dec t) { return t * t * t * (t * (t * 6 - 15) + 10); }

    /**
     * Cubic interpolation between b and c using neighbour points: a - point before b; d - point after c
     *
     * @param a point 1
     * @param b point 2
     * @param c point 3
     * @param d point 4
     * @param t weight
     * @return interpolated value between b and c
     */
    constexpr Dec interpCubic(Dec a, Dec b, Dec c, Dec d, Dec t) {
        const Dec p = d - c - (a - b);
        return t * t * t * p + t * t * (a - b - p) + t * (c - a) + b;
    }

    inline Dec pingPong(Dec t) {
        const Dec r = t - floor(t / 2) * 2;
        return r < Dec{1} ? r : Dec{2} - r;
    }

    // Blur internal
    namespace Internal {
        // standard deviation, number of boxes
        inline void boxesForGauss(int* bxs, int sigma, int n) {
            // Ideal averaging filter width
            const Dec wIdeal = fpm::sqrt(Dec{12} * sigma * sigma / n + 1);
            int wl = toInt(floor(wIdeal));
            if (wl % 2 == 0) {
                wl--;
            }
            const int wu = wl + 2;

            const Dec mIdeal = (Dec{12} * sigma * sigma - n * wl * wl - 4 * n * wl - 3 * n) / (-4 * wl - 4);
            const int m = toInt(floor(mIdeal));
            // var sigmaActual = sqrt( (m*wl*wl + (n-m)*wu*wu - n)/12 );

            for (int i = 0; i < n; i++) {
                bxs[i] = i < m ? wl : wu;
            }
        }

        inline void boxBlurH(const int* in, int* buff, const int w, const int h, const int r) {
            int iArr = r + r + 1;
            assert(iArr != 0 && ((std::format("Precision too low for r {}", r)).data()));
            for (int i = 0; i < h; i++) {
                int ti = i * w;
                int li = ti;
                int ri = ti + r;
                int fv = in[ti];
                int lv = in[ti + w - 1];
                int val = (r + 1) * fv;
                for (int j = 0; j < r; j++) {
                    val += in[ti + j];
                }
                for (int j = 0; j <= r; j++) {
                    val += in[ri++] - fv;
                    buff[ti++] = val / iArr;
                }
                for (int j = r + 1; j < w - r; j++) {
                    val += in[ri++] - in[li++];
                    buff[ti++] = val / iArr;
                }
                for (int j = w - r; j < w; j++) {
                    val += lv - in[li++];
                    buff[ti++] = val / iArr;
                }
            }
        }

        inline void boxBlurT(const int* in, int* buff, const int w, const int h, const int r) {
            int iArr = r + r + 1;
            assert(iArr != 0 && ((std::format("Precision too low for r {}", r)).data()));
            for (int i = 0; i < w; i++) {
                int ti = i;
                int li = ti;
                int ri = ti + r * w;
                int fv = in[ti];
                int lv = in[ti + w * (h - 1)];
                int val = (r + 1) * fv;
                for (int j = 0; j < r; j++) {
                    val += in[ti + j * w];
                }
                for (int j = 0; j <= r; j++) {
                    val += in[ri] - fv;
                    buff[ti] = val / iArr;
                    ri += w;
                    ti += w;
                }
                for (int j = r + 1; j < h - r; j++) {
                    val += in[ri] - in[li];
                    buff[ti] = val / iArr;
                    li += w;
                    ri += w;
                    ti += w;
                }
                for (int j = h - r; j < h; j++) {
                    val += lv - in[li];
                    buff[ti] = val / iArr;
                    li += w;
                    ti += w;
                }
            }
        }

        inline void boxBlur(int* in, int* buff, const int size, const int w, const int h, const int r) {
            std::memcpy(buff, in, size * sizeof(int));
            boxBlurH(buff, in, w, h, r);
            boxBlurT(in, buff, w, h, r);
        }
    }

    /**
     * Performs Gaussian blur on the given data array.
      *
      * @param input input array of 2D data
      * @param xSize data width
      * @param ySize data height
      * @param k1 radius for 1st kernel box
      * @param k2 radius for 2nd kernel box
      * @param k3 radius for 3rd kernel box
      */
    inline void gaussBlur(int* input, const int xSize, const int ySize, const int k1, const int k2, const int k3) {
        const int size = xSize * ySize;
        int* buff = new int[size];
        Internal::boxBlur(input, buff, size, xSize, ySize, k1);
        Internal::boxBlur(buff, input, size, xSize, ySize, k2);
        Internal::boxBlur(input, buff, size, xSize, ySize, k3);
        std::memcpy(input, buff, size);
        delete[] buff;
    }

    /**
     * Performs Gaussian blur on the given data array for given kernel radius.
     *
     * @param input input array of 2D data
     * @param xSize data width
     * @param ySize data height
     * @param radius kernel radius
     */
    inline void gaussBlur(int* input, const int xSize, const int ySize, const int radius) {
        int bxs[3]{};
        Internal::boxesForGauss(bxs, radius, 3);
        gaussBlur(input, xSize, ySize, (bxs[0] - 1) / 2, (bxs[1] - 1) / 2, (bxs[2] - 1) / 2);
    }

    /**
     * Performs binary search of the given value on the given vector sorted in ascending order
     * and returns the position index where the given value should be inserted to keep the order.
     *
     * @param vec vector to search in
     * @param val value to search
     * @return the index of the first element that not less than val or -1 if none such element exist
     */
    template<typename T>
    int binSearch(const std::vector<T>& vec, T val) {
        auto it = std::lower_bound(vec.begin(), vec.end(), val);
        if (it == vec.end()) {
            return -1;
        }
        return std::distance(vec.begin(), it);
    }
}
