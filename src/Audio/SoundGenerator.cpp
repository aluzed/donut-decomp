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

std::vector<uint8_t> SoundGenerator::Ambient(float duration, int sampleRate)
{
	int numSamples = static_cast<int>(sampleRate * duration);
	std::vector<uint8_t> data(numSamples * 2);

	float phases[3] = {0.0f, 0.3f, 0.7f};
	float freqs[3] = {110.0f, 138.59f, 164.81f};

	for (int i = 0; i < numSamples; ++i)
	{
		float t = static_cast<float>(i) / sampleRate;
		float tremolo = 0.5f + 0.5f * sinf(2.0f * 3.14159f * 0.25f * t);
		float sample = 0.0f;

		for (int ch = 0; ch < 3; ++ch)
		{
			phases[ch] += freqs[ch] / sampleRate;
			if (phases[ch] > 1.0f) phases[ch] -= 1.0f;
			sample += sinf(2.0f * 3.14159f * phases[ch]) * 0.04f;
		}

		sample *= tremolo;

		int16_t val = static_cast<int16_t>(sample * 32767.0f);
		data[i * 2] = val & 0xFF;
		data[i * 2 + 1] = (val >> 8) & 0xFF;
	}

	return data;
}

std::vector<uint8_t> SoundGenerator::Engine(float rpm, float duration, int sampleRate)
{
	int numSamples = static_cast<int>(sampleRate * duration);
	std::vector<uint8_t> data(numSamples * 2);

	float freq = 30.0f + rpm * 0.5f;
	float phase = 0.0f;

	for (int i = 0; i < numSamples; ++i)
	{
		float t = static_cast<float>(i) / sampleRate;
		float modulation = sinf(2.0f * 3.14159f * 4.0f * t) * 0.5f + 0.5f;
		freq = 30.0f + rpm * 0.5f * modulation;
		phase += freq / sampleRate;
		if (phase > 1.0f) phase -= 1.0f;

		float sample = sinf(2.0f * 3.14159f * phase) * 0.03f;
		sample += (sinf(2.0f * 3.14159f * freq * 0.5f * t) * 0.01f);

		int16_t val = static_cast<int16_t>(sample * 32767.0f);
		data[i * 2] = val & 0xFF;
		data[i * 2 + 1] = (val >> 8) & 0xFF;
	}

	return data;
}

std::vector<uint8_t> SoundGenerator::Horn(float duration, int sampleRate)
{
	int numSamples = static_cast<int>(sampleRate * duration);
	std::vector<uint8_t> data(numSamples * 2);

	// Two-tone klaxon (~440 Hz + 587 Hz) with a soft attack and decay so it
	// doesn't click. Mono 16-bit PCM matches the rest of SoundGenerator.
	const float freqA = 440.0f;
	const float freqB = 587.33f;

	for (int i = 0; i < numSamples; ++i)
	{
		float t = static_cast<float>(i) / sampleRate;
		float attack = std::min(1.0f, t / 0.02f);
		float decay = std::max(0.0f, 1.0f - (t / duration));
		float envelope = attack * decay;
		float sample = (sinf(2.0f * 3.14159f * freqA * t) + sinf(2.0f * 3.14159f * freqB * t)) * 0.15f * envelope;

		int16_t val = static_cast<int16_t>(std::max(-1.0f, std::min(1.0f, sample)) * 32767.0f);
		data[i * 2] = val & 0xFF;
		data[i * 2 + 1] = (val >> 8) & 0xFF;
	}

	return data;
}

} // namespace Donut