#pragma once

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>

#include <chrono>
#include <map>
#include <string>

namespace Donut
{

// The game's reverbs (its sound scripts' "create reverbSettings named
// pproom1": the power plant's rooms, the tunnels, Burns' mansion), through
// OpenAL's EFX: one effect in one slot, which the sounds placed in the world
// are sent to. The settings are the Xbox's, I3DL2 as EAX has them, which the
// EAX reverb takes as they are. Without EFX, nothing: the sounds stay dry.
class Reverb
{
public:
	// The EFX functions, an effect and its slot, on `device`'s current context.
	explicit Reverb(ALCdevice* device);
	~Reverb();

	bool IsAvailable() const { return _slot != 0; }

	// Reads every "create reverbSettings" block of a sound script's text.
	void LoadSettings(const std::string& script);
	bool HasSettings() const { return !_settings.empty(); }

	// Fades the reverb out, then `name`'s in, as its fade times say; an empty
	// or unknown name fades it out.
	void Set(const std::string& name);
	const std::string& GetCurrent() const { return _current; }
	// The fades, along the clock.
	void Update();

	// `source` heard through the reverb (`wet`), or dry.
	void Send(ALuint source, bool wet) const;

private:
	struct Settings
	{
		float gain = 1.0f;
		float fadeIn = 0.0f, fadeOut = 0.0f; // seconds
		float room = -10000.0f, roomHF = 0.0f, roomRolloff = 0.0f;
		float decayTime = 1.49f, decayHFRatio = 0.83f;
		float reflections = -10000.0f, reflectionsDelay = 0.007f;
		float reverb = -10000.0f, reverbDelay = 0.011f;
		float diffusion = 100.0f, density = 100.0f, hfReference = 5000.0f;
		float airAbsorptionHF = -5.0f;
	};
	void apply(const Settings& settings);
	void setSlotGain(float gain);

	ALuint _effect = 0, _slot = 0;
	std::map<std::string, Settings> _settings;
	std::string _current, _next;
	bool _switching = false;
	// The fade under way: from `_fadeFrom` to `_fadeTo` over `_fadeSeconds`.
	float _gain = 0.0f, _fadeFrom = 0.0f, _fadeTo = 0.0f, _fadeSeconds = 0.0f;
	std::chrono::steady_clock::time_point _fadeStart;

	// The EFX entry points, looked up at run time.
	LPALGENEFFECTS _genEffects = nullptr;
	LPALDELETEEFFECTS _deleteEffects = nullptr;
	LPALEFFECTI _effecti = nullptr;
	LPALEFFECTF _effectf = nullptr;
	LPALGENAUXILIARYEFFECTSLOTS _genSlots = nullptr;
	LPALDELETEAUXILIARYEFFECTSLOTS _deleteSlots = nullptr;
	LPALAUXILIARYEFFECTSLOTI _sloti = nullptr;
	LPALAUXILIARYEFFECTSLOTF _slotf = nullptr;
};

} // namespace Donut
