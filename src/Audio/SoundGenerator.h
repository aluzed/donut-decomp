// Copyright 2020 the donut authors. See AUTHORS.md

#pragma once

#include <vector>
#include <cstdint>

namespace Donut
{

class SoundGenerator
{
public:
	static std::vector<uint8_t> Beep(float frequency, float duration, int sampleRate = 22050);
	static std::vector<uint8_t> Chirp(float startFreq, float endFreq, float duration, int sampleRate = 22050);
};

} // namespace Donut
