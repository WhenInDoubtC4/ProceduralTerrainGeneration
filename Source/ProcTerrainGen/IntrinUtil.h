#pragma once

#include "CoreMinimal.h"

__m128 cross_product_mm(__m128 a, __m128 b); 
__m128 lerp_mm(__m128 a, __m128 b, __m128 alpha);
__m128 sign_mm(__m128 x);
__m128 is_negative_mm(__m128 x);