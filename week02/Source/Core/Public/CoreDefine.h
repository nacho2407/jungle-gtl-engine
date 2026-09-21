#pragma once
#include <CoreTypes.h>
#include <stdexcept>

#define check(expr)				\
if (!(expr))					\
{								\
	throw std::runtime_error(	\
	FString("Error at ") +	__FILE__ + ":" + std::to_string(__LINE__) + #expr\
	);							\
}