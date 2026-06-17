// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector3.h"
#include "RCL/RCFFile.h"
#include "RCL/RSDFile.h"

#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Donut
{

class AudioManager
{
public:
	AudioManager();
	~AudioManager();

	void LoadRCF(const std::string& filename);

	void PlayAudio(const std::string& name);
	void PlayAudio(uint32_t hash);
	void PlayAudio(const std::string& name, const Vector3& position);
	void PlayDialogue(const std::string& name);

	void SetListenerPosition(const Vector3& position, const Vector3& direction, const Vector3& up);
	void SetVolume(float volume);

	void DebugGUI(bool* open);

private:
	static constexpr int kMaxSources = 24;

	struct Source
	{
		ALuint id;
		ALuint buffer;
		bool inUse;
	};

	void initializeOpenAL();
	void shutdownOpenAL();
	ALenum getFormat(const RCL::RSDFile& file) const;

	Source* findFreeSource();

	void playOnSource(Source* src, const RCL::RSDFile& rsdFile);

	void discoverFiles(const std::string& directory, const std::string& extension);

	ALCcontext* _alContext;
	ALCdevice* _alDevice;

	std::vector<std::unique_ptr<RCL::RCFFile>> _rcfFiles;
	std::unordered_map<std::string, RCL::RCFFile*> _sounds;

	std::vector<Source> _sources;
};

} // namespace Donut
