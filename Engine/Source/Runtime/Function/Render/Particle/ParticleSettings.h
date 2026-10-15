#pragma once
#include "Runtime/Core/Math/MathInclude.h"
#include <string>
namespace AtomEngine {
enum class EmitterShape { Point, Box, Sphere, Cone, Ring };
enum class ParticleBlend { Alpha, Additive, Multiply, Opaque };
struct ParticleSettings {
    uint32_t capacity=512,burst=32;float rate=80,duration=1,lifetime=1.5f,sizeStart=.12f,sizeEnd=0,speed=2,drag=0,radius=1,coneAngle=25;
    Vector3 extent{1,1,1},gravity{0,-1,0};Vector4 colorStart{1,.6f,.1f,1},colorEnd{1,.1f,0,0};
    EmitterShape shape=EmitterShape::Point;ParticleBlend blend=ParticleBlend::Additive;bool billboard=true,loop=false;
    std::string texture;
};
}
