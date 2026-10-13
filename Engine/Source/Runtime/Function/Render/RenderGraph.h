#pragma once
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
namespace AtomEngine
{
enum class GraphAccess
{
    Read,
    Write,
    ReadWrite
};
struct GraphUse
{
    GpuResource *resource;
    GraphAccess access;
    D3D12_RESOURCE_STATES state;
};
// A frame-local graph of imported resources. Writable resources are owned by ViewBuffers.
class RenderGraph
{
    struct Pass
    {
        std::string name;
        std::vector<GraphUse> uses;
        std::function<void(GraphicsContext &)> draw;
    };
    std::vector<Pass> passes;
    std::map<GpuResource *, bool> imports;

  public:
    void Import(GpuResource &resource, bool initialized = true)
    {
        imports.try_emplace(&resource, initialized);
    }
    void AddPass(std::string name, std::vector<GraphUse> uses, std::function<void(GraphicsContext &)> draw)
    {
        passes.push_back({std::move(name), std::move(uses), std::move(draw)});
    }
    std::vector<size_t> Compile() const
    {
        auto initialized = imports;
        std::map<GpuResource *, size_t> writer;
        std::map<GpuResource *, std::set<size_t>> readers;
        std::vector<std::set<size_t>> dependencies(passes.size());
        for (size_t i = 0; i < passes.size(); ++i)
        {
            std::set<GpuResource *> unique;
            for (auto use : passes[i].uses)
            {
                if (!use.resource || !imports.contains(use.resource) || !unique.insert(use.resource).second)
                    throw std::runtime_error("Invalid graph resource declaration: " + passes[i].name);
                if (use.access != GraphAccess::Write && !initialized[use.resource])
                    throw std::runtime_error("Graph read before write: " + passes[i].name);
                if (writer.contains(use.resource))
                    dependencies[i].insert(writer.at(use.resource));
                if (use.access == GraphAccess::Read)
                    readers[use.resource].insert(i);
                else
                {
                    dependencies[i].insert(readers[use.resource].begin(), readers[use.resource].end());
                    readers[use.resource].clear();
                    writer[use.resource] = i;
                    initialized[use.resource] = true;
                }
            }
        }
        std::vector<size_t> order;
        std::set<size_t> completed;
        while (order.size() < passes.size())
        {
            bool progress = false;
            for (size_t i = 0; i < passes.size(); ++i)
                if (!completed.contains(i) && std::all_of(dependencies[i].begin(), dependencies[i].end(),
                                                          [&](size_t p) { return completed.contains(p); }))
                {
                    completed.insert(i);
                    order.push_back(i);
                    progress = true;
                }
            if (!progress)
                throw std::runtime_error("Render graph dependency cycle");
        }
        return order;
    }
    void Execute(GraphicsContext &context)
    {
        for (auto i : Compile())
        {
            for (auto use : passes[i].uses)
                context.TransitionResource(*use.resource, use.state);
            context.FlushResourceBarriers();
            passes[i].draw(context);
        }
    }
};
} // namespace AtomEngine
