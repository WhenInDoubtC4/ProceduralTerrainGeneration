#pragma once

#include "CoreMinimal.h"
#include <immintrin.h>

__m128 mm128_cross_product(__m128 a, __m128 b);
__m128 mm128_lerp(__m128 a, __m128 b, __m128 alpha);
__m128 mm128_sign(__m128 x);
__m128 mm128_is_negative(__m128 x);
__m256 mm256_is_negative(__m256 x);
__m256 mm256_lerp(__m256 a, __m256 b, __m256 alpha);
float mm256_sum(__m256 x);
float mm128_sum(__m128 x);