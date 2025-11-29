#include "IntrinUtil.h"

__m128 cross_product_mm(__m128 a, __m128 b) 
{
    //(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x, 0)

    __m128 a_yzx = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 b_zxy = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2));

    __m128 c = _mm_mul_ps(a_yzx, b_zxy);

    __m128 a_zxy = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 b_yzx = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1));

    __m128 d = _mm_mul_ps(a_zxy, b_yzx);

    return _mm_sub_ps(c, d);
}

__m128 lerp_mm(__m128 a, __m128 b, __m128 alpha)
{
    //a * (1 - t) + b * t

    __m128 one_minus_alpha = _mm_sub_ps(_mm_set1_ps(1.f), alpha);

    __m128 scaled_a = _mm_mul_ps(a, one_minus_alpha);
    __m128 scaled_b = _mm_mul_ps(b, alpha);

    return _mm_add_ps(scaled_a, scaled_b);
}

__m128 sign_mm(__m128 x) 
{
    //Returns -1 or +1 based on sign of x (0 is positive)
    __m128 one = _mm_set1_ps(1.0f);
    __m128 sign_bit = _mm_and_ps(x, _mm_set1_ps(-0.0f));

    return _mm_or_ps(one, sign_bit);
}

__m128 is_negative_mm(__m128 x) 
{
    //Returns 0 for positive/0, 1 for negative

    __m128i x_int = _mm_castps_si128(x);
    __m128i sign_bits = _mm_srli_epi32(x_int, 31); //Shift sign bit to LSB
    __m128 result = _mm_cvtepi32_ps(sign_bits); //Convert 0 or 1 to float

    return result;
}