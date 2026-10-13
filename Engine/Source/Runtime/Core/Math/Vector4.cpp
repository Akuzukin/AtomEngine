#include "Vector4.h"
#include "../Color/Color.h"

namespace AtomEngine
{
	Vector4::Vector4(const Color& color)
	{
		x = color.r;
        y = color.g;
        z = color.b;
        w = color.a;
	}
}
