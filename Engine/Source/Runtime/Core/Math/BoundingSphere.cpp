#include "BoundingSphere.h"
#include "BoundingBox.h"
#include "Math.h"

namespace AtomEngine
{
    BoundingSphere BoundingSphere::Union(const BoundingSphere& rhs)
    {
        float radA = GetRadius();
        if (radA == 0.0f)
            return rhs;

        float radB = rhs.GetRadius();
        if (radB == 0.0f)
            return *this;

        Vector3 diff = GetCenter() - rhs.GetCenter();
        float dist = diff.Length();

        Vector3 extremeA = GetCenter() + diff * std::max(radA, radB - dist);
        Vector3 extremeB = rhs.GetCenter() - diff * std::max(radB, radA - dist);

        return BoundingSphere((extremeA + extremeB) * 0.5f, (extremeA - extremeB).Length() * 0.5f);
    }
    BoundingSphere::BoundingSphere(const AxisAlignedBox& box)
    {
        mRepr = Vector4(box.GetCenter(), box.GetRadius());
    }

}
