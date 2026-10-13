#pragma once
#include "Runtime/Function/Framework/ECS/World.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include <algorithm>
namespace AtomEngine
{
class UiSystem
{
  public:
    static std::vector<Entity> DrawOrder(World &world)
    {
        std::vector<Entity> order;
        for (auto e : world.View<UiElementComponent>())
            if (world.GetComponent<UiElementComponent>(e).visible)
                order.push_back(e);
        std::sort(order.begin(), order.end(), [&](Entity a, Entity b) {
            auto x = world.GetComponent<UiElementComponent>(a).layer,
                 y = world.GetComponent<UiElementComponent>(b).layer;
            return x == y ? uint32_t(a) < uint32_t(b) : x < y;
        });
        return order;
    }
    static Entity HitTest(World &world, float x, float y)
    {
        auto order = DrawOrder(world);
        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            auto &c = world.GetComponent<UiElementComponent>(*it);
            if (c.interactable && x >= c.position.x && y >= c.position.y && x < c.position.x + c.size.x &&
                y < c.position.y + c.size.y)
                return *it;
        }
        return entt::null;
    }
};
} // namespace AtomEngine
