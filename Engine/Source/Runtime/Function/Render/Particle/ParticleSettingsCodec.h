#pragma once
#include "ParticleSettings.h"
#include "json.hpp"
#include <cmath>
namespace AtomEngine
{
	inline nlohmann::json ParticleJson(const ParticleSettings& s)
	{
		return
		{
			{ "version",2 },
            {"randomize",s.randomize},
            {"lifetimeRange",{s.lifetimeRange.x,s.lifetimeRange.y}},
            {"speedRange",{s.speedRange.x,s.speedRange.y}},
            {"sizeStartRange",{s.sizeStartRange.x,s.sizeStartRange.y}},
            {"sizeEndRange",{s.sizeEndRange.x,s.sizeEndRange.y}},
            {"colorStartMin",{s.colorStartMin.x,s.colorStartMin.y,s.colorStartMin.z,s.colorStartMin.w}},
            {"colorStartMax",{s.colorStartMax.x,s.colorStartMax.y,s.colorStartMax.z,s.colorStartMax.w}},
            {"colorEndMin",{s.colorEndMin.x,s.colorEndMin.y,s.colorEndMin.z,s.colorEndMin.w}},
            {"colorEndMax",{s.colorEndMax.x,s.colorEndMax.y,s.colorEndMax.z,s.colorEndMax.w}},
			{ "capacity",s.capacity },
			{ "burst",s.burst },
			{ "rate",s.rate },
			{ "duration",s.duration },
			{ "lifetime",s.lifetime },
			{ "sizeStart",s.sizeStart },
			{ "sizeEnd",s.sizeEnd },
			{ "speed",s.speed },
			{ "drag",s.drag },
			{ "radius",s.radius },
			{ "coneAngle",s.coneAngle },
			{ "shape",int(s.shape) },
			{ "blend",int(s.blend) },
			{ "billboard",s.billboard },
			{ "loop",s.loop },
			{ "texture",s.texture },
			{ "extent",{ s.extent.x,s.extent.y,s.extent.z } },
			{ "gravity",{ s.gravity.x,s.gravity.y,s.gravity.z } },
			{ "colorStart",{ s.colorStart.x,s.colorStart.y,s.colorStart.z,s.colorStart.w } },
			{ "colorEnd",{ s.colorEnd.x,s.colorEnd.y,s.colorEnd.z,s.colorEnd.w } }
		};
	}
	inline ParticleSettings ReadParticleSettings(const std::string& text)
	{
		auto j = nlohmann::json::parse(text);
		ParticleSettings s; if (j.value("version", 0) != 2)
			throw std::runtime_error("Particle preset requires version 2");
#define NUMBER(name) s.name=j.value(#name,s.name);
		NUMBER(capacity) NUMBER(burst) NUMBER(rate) NUMBER(duration) NUMBER(lifetime) NUMBER(sizeStart) NUMBER(sizeEnd) NUMBER(speed) NUMBER(drag) NUMBER(radius) NUMBER(coneAngle)
#undef NUMBER
			s.shape = EmitterShape(j.value("shape", 0));
		s.blend = ParticleBlend(j.value("blend", 1));
		s.billboard = j.value("billboard", true);
		s.loop = j.value("loop", false);
		s.texture = j.value("texture", "");
		auto vector = [&](const char* name, float* p, int n)
			{
				if (j.contains(name))
				{
					auto v = j.at(name);
					if (!v.is_array() || v.size() != n)
						throw std::runtime_error("Invalid particle vector");
					for (int i = 0; i < n; i++)
					{
						p[i] = v[i].get<float>();
						if (!std::isfinite(p[i]))
							throw std::runtime_error("Nonfinite particle setting");
					}
				}
			};
		vector("extent", &s.extent.x, 3); vector("gravity", &s.gravity.x, 3);
		vector("colorStart", &s.colorStart.x, 4);
		vector("colorEnd", &s.colorEnd.x, 4);
        s.randomize=j.value("randomize",false);
        vector("lifetimeRange",&s.lifetimeRange.x,2);vector("speedRange",&s.speedRange.x,2);
        vector("sizeStartRange",&s.sizeStartRange.x,2);vector("sizeEndRange",&s.sizeEndRange.x,2);
        vector("colorStartMin",&s.colorStartMin.x,4);vector("colorStartMax",&s.colorStartMax.x,4);
        vector("colorEndMin",&s.colorEndMin.x,4);vector("colorEndMax",&s.colorEndMax.x,4);
        for(auto range:{s.lifetimeRange,s.speedRange,s.sizeStartRange,s.sizeEndRange})
            if(range.x<0||range.x>range.y)throw std::runtime_error("Invalid particle random range: min must not exceed max");
        if(s.lifetimeRange.x<=0)throw std::runtime_error("Random lifetime must be positive");
        for(int i=0;i<4;i++)if((&s.colorStartMin.x)[i]<0||(&s.colorEndMin.x)[i]<0||(&s.colorStartMax.x)[i]>1||(&s.colorEndMax.x)[i]>1||(&s.colorStartMin.x)[i]>(&s.colorStartMax.x)[i]||(&s.colorEndMin.x)[i]>(&s.colorEndMax.x)[i])throw std::runtime_error("Invalid particle random color range");
		if (s.capacity < 1 || s.capacity>65536 || s.burst > s.capacity || uint32_t(s.shape) > 4 || uint32_t(s.blend) > 3)
			throw std::runtime_error("Particle setting out of range");
		for (float v : {s.rate, s.duration, s.lifetime, s.sizeStart, s.sizeEnd, s.speed, s.drag, s.radius, s.coneAngle})
			if (!std::isfinite(v) || v < 0)
				throw std::runtime_error("Invalid particle number");
		if (s.duration <= 0 || s.lifetime <= 0 || s.rate > 100000 || s.coneAngle > 89)
			throw std::runtime_error("Invalid particle lifetime or rate");
		return s;
	}
}
