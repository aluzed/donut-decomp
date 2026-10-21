#include "Audio/Reverb.h"

#include "Core/Log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Donut
{

namespace
{
// Millibels to a gain: -2000 is a tenth.
float fromMillibels(float mB)
{
	return std::pow(10.0f, mB / 2000.0f);
}
} // namespace

Reverb::Reverb(ALCdevice* device)
{
	if (!device || !alcIsExtensionPresent(device, "ALC_EXT_EFX"))
	{
		Log::Info("Audio: no EFX, no reverb");
		return;
	}
	_genEffects = reinterpret_cast<LPALGENEFFECTS>(alGetProcAddress("alGenEffects"));
	_deleteEffects = reinterpret_cast<LPALDELETEEFFECTS>(alGetProcAddress("alDeleteEffects"));
	_effecti = reinterpret_cast<LPALEFFECTI>(alGetProcAddress("alEffecti"));
	_effectf = reinterpret_cast<LPALEFFECTF>(alGetProcAddress("alEffectf"));
	_genSlots = reinterpret_cast<LPALGENAUXILIARYEFFECTSLOTS>(alGetProcAddress("alGenAuxiliaryEffectSlots"));
	_deleteSlots = reinterpret_cast<LPALDELETEAUXILIARYEFFECTSLOTS>(alGetProcAddress("alDeleteAuxiliaryEffectSlots"));
	_sloti = reinterpret_cast<LPALAUXILIARYEFFECTSLOTI>(alGetProcAddress("alAuxiliaryEffectSloti"));
	_slotf = reinterpret_cast<LPALAUXILIARYEFFECTSLOTF>(alGetProcAddress("alAuxiliaryEffectSlotf"));
	if (!_genEffects || !_deleteEffects || !_effecti || !_effectf || !_genSlots || !_deleteSlots || !_sloti || !_slotf)
		return;

	alGetError();
	_genEffects(1, &_effect);
	_effecti(_effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
	if (alGetError() != AL_NO_ERROR)
	{
		Log::Info("Audio: no EAX reverb in EFX, no reverb");
		_deleteEffects(1, &_effect);
		_effect = 0;
		return;
	}
	_genSlots(1, &_slot);
	setSlotGain(0.0f);
	Log::Info("Audio: EFX reverb ready");
}

Reverb::~Reverb()
{
	if (_slot)
	{
		_sloti(_slot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
		_deleteSlots(1, &_slot);
	}
	if (_effect)
		_deleteEffects(1, &_effect);
}

void Reverb::LoadSettings(const std::string& script)
{
	// create reverbSettings named pproom1
	//     SetGain ( 0.640000 )
	//     SetXboxRoom ( -500 ) ...
	const std::string create = "create reverbSettings named ";
	for (std::size_t at = script.find(create); at != std::string::npos; at = script.find(create, at))
	{
		at += create.size();
		const std::size_t nameEnd = script.find_first_of(" \t\r\n", at);
		const std::string name = script.substr(at, nameEnd - at);
		const std::size_t end = std::min(script.find("create ", at), script.size());
		const std::string block = script.substr(at, end - at);
		const auto value = [&](const char* key, float& out)
		{
			const std::string call = std::string(key) + " (";
			if (const std::size_t found = block.find(call); found != std::string::npos)
				std::sscanf(block.c_str() + found + call.size(), "%f", &out);
		};
		Settings settings;
		value("SetGain", settings.gain);
		value("SetFadeInTime", settings.fadeIn);
		value("SetFadeOutTime", settings.fadeOut);
		settings.fadeIn /= 1000.0f;
		settings.fadeOut /= 1000.0f;
		value("SetXboxRoom", settings.room);
		value("SetXboxRoomHF", settings.roomHF);
		value("SetXboxRoomRolloffFactor", settings.roomRolloff);
		value("SetXboxDecayTime", settings.decayTime);
		value("SetXboxDecayHFRatio", settings.decayHFRatio);
		value("SetXboxReflections", settings.reflections);
		value("SetXboxReflectionsDelay", settings.reflectionsDelay);
		value("SetXboxReverb", settings.reverb);
		value("SetXboxReverbDelay", settings.reverbDelay);
		value("SetXboxDiffusion", settings.diffusion);
		value("SetXboxDensity", settings.density);
		value("SetXboxHFReference", settings.hfReference);
		value("SetWinAirAbsorptionHF", settings.airAbsorptionHF);
		_settings[name] = settings;
		at = end;
	}
}

void Reverb::Set(const std::string& name)
{
	if (!_slot)
		return;
	const std::string next = _settings.count(name) ? name : std::string();
	if (next == (_switching ? _next : _current))
		return;
	Log::Info("Audio: reverb '{}'", next.empty() ? "none" : next);
	// Out with the one there, as long as it says; then in with the next.
	const float fadeOut = _current.empty() ? 0.0f : _settings[_current].fadeOut;
	_next = next;
	_switching = true;
	_fadeFrom = _gain;
	_fadeTo = 0.0f;
	_fadeSeconds = fadeOut;
	_fadeStart = std::chrono::steady_clock::now();
	Update();
}

void Reverb::Update()
{
	if (!_slot)
		return;
	const float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - _fadeStart).count();
	const float along = _fadeSeconds > 0.0f ? std::clamp(seconds / _fadeSeconds, 0.0f, 1.0f) : 1.0f;
	setSlotGain(_fadeFrom + (_fadeTo - _fadeFrom) * along);
	if (along < 1.0f || !_switching)
		return;
	// Faded out: the next one's settings, then its fade in.
	_switching = false;
	_current = _next;
	if (_current.empty())
	{
		_sloti(_slot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
		return;
	}
	const Settings& settings = _settings[_current];
	apply(settings);
	_fadeFrom = 0.0f;
	_fadeTo = settings.gain;
	_fadeSeconds = settings.fadeIn;
	_fadeStart = std::chrono::steady_clock::now();
}

void Reverb::Send(ALuint source, bool wet) const
{
	if (!_slot || !source)
		return;
	alSource3i(source, AL_AUXILIARY_SEND_FILTER, wet ? static_cast<ALint>(_slot) : AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
}

void Reverb::apply(const Settings& settings)
{
	const auto set = [&](ALenum parameter, float value, float low, float high)
	{ _effectf(_effect, parameter, std::clamp(value, low, high)); };
	set(AL_EAXREVERB_DENSITY, settings.density / 100.0f, AL_EAXREVERB_MIN_DENSITY, AL_EAXREVERB_MAX_DENSITY);
	set(AL_EAXREVERB_DIFFUSION, settings.diffusion / 100.0f, AL_EAXREVERB_MIN_DIFFUSION, AL_EAXREVERB_MAX_DIFFUSION);
	set(AL_EAXREVERB_GAIN, fromMillibels(settings.room), AL_EAXREVERB_MIN_GAIN, AL_EAXREVERB_MAX_GAIN);
	set(AL_EAXREVERB_GAINHF, fromMillibels(settings.roomHF), AL_EAXREVERB_MIN_GAINHF, AL_EAXREVERB_MAX_GAINHF);
	set(AL_EAXREVERB_DECAY_TIME, settings.decayTime, AL_EAXREVERB_MIN_DECAY_TIME, AL_EAXREVERB_MAX_DECAY_TIME);
	set(AL_EAXREVERB_DECAY_HFRATIO, settings.decayHFRatio, AL_EAXREVERB_MIN_DECAY_HFRATIO, AL_EAXREVERB_MAX_DECAY_HFRATIO);
	set(AL_EAXREVERB_REFLECTIONS_GAIN, fromMillibels(settings.reflections), AL_EAXREVERB_MIN_REFLECTIONS_GAIN,
	    AL_EAXREVERB_MAX_REFLECTIONS_GAIN);
	set(AL_EAXREVERB_REFLECTIONS_DELAY, settings.reflectionsDelay, AL_EAXREVERB_MIN_REFLECTIONS_DELAY,
	    AL_EAXREVERB_MAX_REFLECTIONS_DELAY);
	set(AL_EAXREVERB_LATE_REVERB_GAIN, fromMillibels(settings.reverb), AL_EAXREVERB_MIN_LATE_REVERB_GAIN,
	    AL_EAXREVERB_MAX_LATE_REVERB_GAIN);
	set(AL_EAXREVERB_LATE_REVERB_DELAY, settings.reverbDelay, AL_EAXREVERB_MIN_LATE_REVERB_DELAY,
	    AL_EAXREVERB_MAX_LATE_REVERB_DELAY);
	set(AL_EAXREVERB_HFREFERENCE, settings.hfReference, AL_EAXREVERB_MIN_HFREFERENCE, AL_EAXREVERB_MAX_HFREFERENCE);
	set(AL_EAXREVERB_AIR_ABSORPTION_GAINHF, fromMillibels(settings.airAbsorptionHF), AL_EAXREVERB_MIN_AIR_ABSORPTION_GAINHF,
	    AL_EAXREVERB_MAX_AIR_ABSORPTION_GAINHF);
	set(AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, settings.roomRolloff, AL_EAXREVERB_MIN_ROOM_ROLLOFF_FACTOR,
	    AL_EAXREVERB_MAX_ROOM_ROLLOFF_FACTOR);
	_sloti(_slot, AL_EFFECTSLOT_EFFECT, static_cast<ALint>(_effect));
}

void Reverb::setSlotGain(float gain)
{
	_gain = gain;
	_slotf(_slot, AL_EFFECTSLOT_GAIN, std::clamp(gain, 0.0f, 1.0f));
}

} // namespace Donut
