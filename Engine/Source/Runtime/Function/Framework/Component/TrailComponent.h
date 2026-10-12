#pragma once
#include "Runtime/Core/Math/Vector3.h"
#include "Runtime/Core/Color/Color.h"
#include <deque>

namespace AtomEngine
{
	struct TrailComponent
	{
        struct Sample
        {
             Vector3 pos;
             Color color = Color::White;
            float age;
        };
        std::deque<Sample> samples;

        // Parameters
        float maxAge = 0.6f;            // トレイルが消えるまでの時間
        float sampleInterval = 0.02f;   // サンプリング間隔
        float timeSinceLast = 0.0f;
        float thickness = 0.2f;         // 基本厚さ（ワールド単位）
        uint32_t maxSamples = 48;

        Color startColor = Color(0xE491C9ff);
        Color endColor = Color(0x088395ff);
        uint32_t colorSegments = 4;
        bool useSegmentedColors = true;
		bool active = true;
	};
}