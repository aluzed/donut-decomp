// Copyright 2020 the donut authors. See AUTHORS.md

#include "Audio/SoundGenerator.h"
#include <cmath>

namespace Donut
{

std::vector<uint8_t> SoundGenerator::Beep(float frequency, float duration, int sampleRate)
{
	int numSamples = static_cast<int>(sampleRate * duration);
	std::vector<uint8_t> data(numSamples * 2);

	for (int i = 0; i < numSamples; ++i)
	{
		float t = static_cast<float>(i) / sampleRate;
		float envelope = 1.0f - (t / duration);
		if (envelope < 0.0f) envelope = 0.0f;
		float sample = sinf(2.0f * 3.14159f * frequency * t) * envelope * 0.3f;

		int16_t val = static_cast<int16_t>(sample * 32767.0f);
		data[i * 2] = val & 0xFF;
		data[i * 2 + 1] = (val >> 8) & 0xFF;
	}

	return data;
}

std::vector<uint8_t> SoundGenerator::Chirp(float startFreq, float endFreq, float duration, int sampleRate)
{
	int numSamples = static_cast<int>(sampleRate * duration);
	std::vector<uint8_t> data(numSamples * 2);

	for (int i = 0; i < numSamples; ++i)
	{
		float t = static_cast<float>(i) / sampleRate;
		float freq = startFreq + (endFreq - startFreq) * (t / duration);
		float envelope = 1.0f - (t / duration);
		if (envelope < 0.0f) envelope = 0.0f;
		float sample = sinf(2.0f * 3.14159f * freq * t) * envelope * 0.3f;

		int16_t val = static_cast<int16_t>(sample * 32767.0f);
		data[i * 2] = val & 0xFF;
		data[i * 2 + 1] = (val >> 8) & 0xFF;
	}

	return data;
}

} // namespace Donut
