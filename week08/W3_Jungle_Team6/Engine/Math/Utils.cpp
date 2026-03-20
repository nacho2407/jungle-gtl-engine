#include "Utils.h"
float Clamp(float val, float lo, float hi) {
	if (val >= hi) {
		return hi;
	}
	else if (val <= lo) {
		return lo;
	}
	return val;
}