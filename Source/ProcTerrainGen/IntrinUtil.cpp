#include "IntrinUtil.h"

__m128 cross_product_mm(__m128 a, __m128 b) {
    // Cross product formula: (a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x, 0)

    // Shuffle to get: (a.y, a.z, a.x, a.w)
    __m128 a_yzx = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));

    // Shuffle to get: (b.z, b.x, b.y, b.w)
    __m128 b_zxy = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2));

    // First part: a_yzx * b_zxy = (a.y*b.z, a.z*b.x, a.x*b.y, a.w*b.w)
    __m128 c = _mm_mul_ps(a_yzx, b_zxy);

    // Shuffle to get: (a.z, a.x, a.y, a.w)
    __m128 a_zxy = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2));

    // Shuffle to get: (b.y, b.z, b.x, b.w)
    __m128 b_yzx = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1));

    // Second part: a_zxy * b_yzx = (a.z*b.y, a.x*b.z, a.y*b.x, a.w*b.w)
    __m128 d = _mm_mul_ps(a_zxy, b_yzx);

    // Subtract: c - d = cross product
    return _mm_sub_ps(c, d);
}