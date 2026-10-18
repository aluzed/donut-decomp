// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector3.h"
#include "RCL/RCFFile.h"
#include "RCL/RSDFile.h"

#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>

#include <cstdint>
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
	// A line queued or still being spoken.
	bool IsDialoguePlaying() const;

	void SetListenerPosition(const Vector3& position, const Vector3& direction, const Vector3& up);
	void SetVolume(float volume);
	void PlayRaw(const std::vector<uint8_t>& pcmData, int sampleRate, int channels, int bits);
	void PlayMusic(const std::string& name);
	// Plays these tracks one after another, round and round: the game's own
	// music is made of short segments chained together.
	void PlayMusicPlaylist(std::vector<std::string> tracks);
	void StopMusic();
	// Every sound the loaded RCFs hold whose name contains `part`, sorted.
	std::vector<std::string> FindSounds(const std::string& part) const;

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
	// The source speaking the current line, or -1.
	int _dialogueSource = -1;
	std::unique_ptr<MusicSource> _music;
	std::vector<std::string> _playlist;
	std::size_t _playlistIndex = 0;
	// Stops and frees the music source, if any.
	void releaseMusic();
	// Decodes `name` whole and plays it once on a fresh music source.
	bool startMusicTrack(const std::string& name, bool loop);
};

} // namespace Donut
