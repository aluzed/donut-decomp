// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector3.h"
#include "RCL/RCFFile.h"
#include "RCL/RSDFile.h"

#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Donut
{

class AudioManager
{
public:
	enum class DialoguePriority
	{
		Cutscene = 0,
		Mission = 1,
		Gameplay = 2,
	};

	struct Dialogue
	{
		std::string name;
		DialoguePriority priority;
	};

	AudioManager();
	~AudioManager();

	void LoadRCF(const std::string& filename);

	void PlayAudio(const std::string& name);
	void PlayAudio(uint32_t hash);
	void PlayAudio(const std::string& name, const Vector3& position);
	void PlayDialogue(const std::string& name);

	void QueueDialogue(const std::string& name, DialoguePriority priority);

	void SetListenerPosition(const Vector3& position, const Vector3& direction, const Vector3& up);
	void SetVolume(float volume);
	void PlayRaw(const std::vector<uint8_t>& pcmData, int sampleRate, int channels, int bits);
	void PlayMusic(const std::string& name);

	void Update();
	void DebugGUI(bool* open);

private:
	static constexpr int kMaxSources = 24;
	static constexpr int kStreamingBuffers = 2;
	static constexpr int kStreamingBufferSize = 65536;

	struct Source
	{
		ALuint id;
		ALuint buffer;
		bool inUse;
		std::string dialogueName;
	};

	struct MusicSource
	{
		ALuint id;
		ALuint buffers[kStreamingBuffers];
		bool playing;
		int sampleRate;
		int channels;
		int bits;
	};

	void initializeOpenAL();
	void shutdownOpenAL();
	ALenum getFormat(const RCL::RSDFile& file) const;
	ALenum getFormat(int channels, int bits) const;

	Source* findFreeSource();

	void playOnSource(Source* src, const RCL::RSDFile& rsdFile);

	void discoverFiles(const std::string& directory, const std::string& extension);

	void cleanupFinished();
	void processDialogueQueue();

	ALCcontext* _alContext;
	ALCdevice* _alDevice;

	std::vector<std::unique_ptr<RCL::RCFFile>> _rcfFiles;
	std::unordered_map<std::string, RCL::RCFFile*> _sounds;

	std::vector<Source> _sources;
	std::deque<Dialogue> _dialogueQueue;
	std::unique_ptr<MusicSource> _music;
};

} // namespace Donut
