#include "Vector3.h"
#include "Vector4.h"

namespace AtomEngine
{
    const Vector3 Vector3::ZERO(0, 0, 0);
    const Vector3 Vector3::RIGHT(1, 0, 0);
    const Vector3 Vector3::UP(0, 1, 0);
    const Vector3 Vector3::FORWARD(0, 0, 1);
    const Vector3 Vector3::UNIT_SCALE(1, 1, 1);

    Vector3::Vector3(const Vector4& v)
    {
        x = v.x;
        y = v.y;
        z = v.z;
    }
}
