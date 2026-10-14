#pragma once
#include "Runtime/Function/Framework/ECS/World.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include <algorithm>
namespace AtomEngine
{
struct UiRect { Vector2 position,size; float fontScale=1; };
class UiSystem
{
  public:
    static std::vector<Entity> DrawOrder(World &world)
    {
        std::vector<Entity> order;
        for (auto e : world.View<UiElementComponent>())
            if (world.IsEnabled(e,ComponentKind::UI) && world.GetComponent<UiElementComponent>(e).visible)
                order.push_back(e);
        std::sort(order.begin(), order.end(), [&](Entity a, Entity b) {
            auto x = world.GetComponent<UiElementComponent>(a).layer,
                 y = world.GetComponent<UiElementComponent>(b).layer;
            return x == y ? uint32_t(a) < uint32_t(b) : x < y;
        });
        return order;
    }
    static UiRect Resolve(World &world,Entity e,int depth=0) {
        auto &c=world.GetComponent<UiElementComponent>(e);if(!c.responsive)return {c.position,c.size,1};
        auto viewport=world.uiViewportSize,render=world.uiRenderSize;
        float unit=std::min(viewport.x/1920.f,viewport.y/1080.f);Vector2 ratio{render.x/viewport.x,render.y/viewport.y};
        UiRect parent{{0,0},render,unit*ratio.y};Entity p=entt::null;
        if(depth<64)if(auto h=world.TryGet<HierarchyComponent>(e);h && !h->parentId.empty()){p=world.FindById(h->parentId);if(world.HasComponent<UiElementComponent>(p))parent=Resolve(world,p,depth+1);else p=entt::null;}
        Vector2 size{std::max(1.f,parent.size.x*(c.anchorMax.x-c.anchorMin.x)+c.size.x*unit*ratio.x),std::max(1.f,parent.size.y*(c.anchorMax.y-c.anchorMin.y)+c.size.y*unit*ratio.y)};
        Vector2 position{parent.position.x+parent.size.x*c.anchorMin.x+c.position.x*unit*ratio.x-size.x*c.pivot.x,parent.position.y+parent.size.y*c.anchorMin.y+c.position.y*unit*ratio.y-size.y*c.pivot.y};
        if(p!=entt::null) {
            auto &container=world.GetComponent<UiElementComponent>(p);
            if(container.layout!=UiLayout::None) {
                std::vector<Entity> children;auto parentId=world.GetComponent<PersistentIdComponent>(p).value;
                for(auto child:world.View<UiElementComponent,HierarchyComponent>())if(world.GetComponent<HierarchyComponent>(child).parentId==parentId && world.IsEnabled(child,ComponentKind::UI) && world.GetComponent<UiElementComponent>(child).visible)children.push_back(child);
                std::sort(children.begin(),children.end(),[&](Entity a,Entity b){auto x=world.GetComponent<UiElementComponent>(a).layoutOrder,y=world.GetComponent<UiElementComponent>(b).layoutOrder;return x==y?world.GetComponent<PersistentIdComponent>(a).value<world.GetComponent<PersistentIdComponent>(b).value:x<y;});
                position={parent.position.x+container.padding*unit*ratio.x,parent.position.y+container.padding*unit*ratio.y};
                for(auto child:children){if(child==e)break;auto &item=world.GetComponent<UiElementComponent>(child);if(container.layout==UiLayout::Horizontal)position.x+=(item.size.x+container.spacing)*unit*ratio.x;else position.y+=(item.size.y+container.spacing)*unit*ratio.y;}
            }
        }
        return {position,size,unit*ratio.y};
    }
    static Entity Navigate(World &world,int direction) {
        auto order=DrawOrder(world);std::erase_if(order,[&](Entity e){return !world.GetComponent<UiElementComponent>(e).interactable;});
        if(order.empty()){world.uiFocus=entt::null;return entt::null;}
        std::sort(order.begin(),order.end(),[&](Entity a,Entity b){auto x=Resolve(world,a),y=Resolve(world,b);return std::abs(x.position.y-y.position.y)<1.f?x.position.x<y.position.x:x.position.y<y.position.y;});
        auto it=std::find(order.begin(),order.end(),world.uiFocus);int index=it==order.end()?-1:int(it-order.begin());
        if(index<0)index=direction<0?int(order.size())-1:0;else if(direction)index=(index+direction+int(order.size()))%int(order.size());
        return world.uiFocus=order[index];
    }
    static Entity HitTest(World &world, float x, float y)
    {
        auto order = DrawOrder(world);
        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            auto &c = world.GetComponent<UiElementComponent>(*it);
            auto rect=Resolve(world,*it);
            if (c.interactable && x >= rect.position.x && y >= rect.position.y && x < rect.position.x + rect.size.x &&
                y < rect.position.y + rect.size.y)
                return *it;
        }
        return entt::null;
    }
};
} // namespace AtomEngine
