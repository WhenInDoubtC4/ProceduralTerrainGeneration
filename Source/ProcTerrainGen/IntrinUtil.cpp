#include "IntrinUtil.h"

__m128 mm128_cross_product(__m128 a, __m128 b) 
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

__m128 mm128_lerp(__m128 a, __m128 b, __m128 alpha)
{
    //a * (1 - t) + b * t

    __m128 one_minus_alpha = _mm_sub_ps(_mm_set1_ps(1.f), alpha);

    __m128 scaled_a = _mm_mul_ps(a, one_minus_alpha);
    __m128 scaled_b = _mm_mul_ps(b, alpha);

    return _mm_add_ps(scaled_a, scaled_b);
}

__m128 mm128_sign(__m128 x) 
{
    //Returns -1 or +1 based on sign of x (0 is positive)
    __m128 one = _mm_set1_ps(1.0f);
    __m128 sign_bit = _mm_and_ps(x, _mm_set1_ps(-0.0f));

    return _mm_or_ps(one, sign_bit);
}

__m128 mm128_is_negative(__m128 x) 
{
    //Returns 0 for positive/0, 1 for negative

    __m128i x_int = _mm_castps_si128(x);
    __m128i sign_bits = _mm_srli_epi32(x_int, 31); //Shift sign bit to LSB
    return _mm_cvtepi32_ps(sign_bits); //Convert 0 or 1 to float
}

__m256 mm256_is_negative(__m256 x)
{
    __m256i x_int = _mm256_castps_si256(x);
    __m256i sign_bits = _mm256_srli_epi32(x_int, 31);
    return _mm256_cvtepi32_ps(sign_bits);
}

__m256 mm256_lerp(__m256 a, __m256 b, __m256 alpha)
{
    __m256 one_minus_alpha = _mm256_sub_ps(_mm256_set1_ps(1.f), alpha);
    __m256 scaled_a = _mm256_mul_ps(a, one_minus_alpha);
    __m256 scaled_b = _mm256_mul_ps(b, alpha);

    return _mm256_add_ps(scaled_a, scaled_b);
}

float mm256_sum(__m256 x)
{
    x = _mm256_hadd_ps(x, x);
    __m128 x128 = _mm256_extractf128_ps(x, 0);
    x128 = _mm_hadd_ps(x128, x128);
    x128 = _mm_hadd_ps(x128, x128);

    return x128.m128_f32[0];
}

float mm128_sum(__m128 x)
{
    __m128 result = _mm_hadd_ps(x, x);
    result = _mm_hadd_ps(result, result);
    return result.m128_f32[0];
}