// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Audio/MusicScript.h"
#include "Core/Math/Vector3.h"
#include "RCL/RCFFile.h"
#include "RCL/RSDFile.h"

#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>

#include <chrono>
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
	// Placed in the world; `gain` and `pitch` on top of the file's own.
	void PlayAudio(const std::string& name, const Vector3& position, float gain = 1.0f, float pitch = 1.0f);
	void PlayDialogue(const std::string& name);

	// The lines' text, heard by machine (tools/transcribe_dialogue.py: a sound's
	// name, a tab, its text, a line each, UTF-8); none without it.
	void LoadSubtitles(const std::string& path);
	// The line being said: who says it (empty when the file does not tell),
	// its text, in Latin-1 as the game's fonts are.
	struct Subtitle
	{
		std::string speaker;
		std::string text;
	};
	// The last line started, while it is heard and a moment after; none.
	const Subtitle* GetSubtitle() const;

	void QueueDialogue(const std::string& name, DialoguePriority priority);
	// A line queued or still being spoken.
	bool IsDialoguePlaying() const;
	// How open the speaker's mouth is now, 0 to 1, from the loudness of the
	// line being said (its envelope, a 30th of a second at a time, against
	// its loudest); `speaker` who says it ("homer", "marge"), empty when no
	// line plays or it is no one's.
	float GetMouthOpen(std::string& speaker) const;

	void SetListenerPosition(const Vector3& position, const Vector3& direction, const Vector3& up);
	void SetVolume(float volume);
	// The options' sound page: each kind of sound's volume, 0 to 1.
	enum class Channel
	{
		Music,
		Effects,
		Engine, // the cars: engines, tyres
		Voice,
	};
	void SetChannelVolume(Channel channel, float volume);
	float GetChannelVolume(Channel channel) const { return _channelVolumes[static_cast<int>(channel)]; }
	void PlayRaw(const std::vector<uint8_t>& pcmData, int sampleRate, int channels, int bits);
	void PlayMusic(const std::string& name);
	// Plays these tracks one after another, round and round: the game's own
	// music is made of short segments chained together.
	void PlayMusicPlaylist(std::vector<std::string> tracks);
	void StopMusic();
	// The level's music script (sound\\music\\l<n>_music.rms); false when
	// there is none, or nothing in it the archives hold.
	bool LoadMusicScript(int level);
	// The music a game event of the script calls for ("OF_explore_mission",
	// "M1_start", "M1_win"): its once tracks then its loop. A cue that loops
	// becomes the music to go back to, unless `keepBase`; one that does not, an
	// ending, gives the music back to it when it is over. False, the music
	// unchanged, when the script has nothing the archives hold for it.
	bool PlayMusicEvent(const std::string& event, bool keepBase = false);
	// The music to go back to becomes this event's, without playing it.
	void SetMusicBase(const std::string& event);
	// Back to the music to go back to, its loop.
	void ResumeMusicBase();

	// A car's sounds, from its sound script (sound\\scripts\\<car>.spt):
	// the engine loop and the horn it names. Empty when there is none.
	std::string CarSoundFile(const std::string& car, const std::string& suffix = "") const;
	// One of the files a sound script's resource names, at random:
	// ScriptSoundFile("collide", "car_hit_stopsign") is one of the stop sign's
	// hits (sound\\scripts\\collide.spt). Empty when there is none.
	std::string ScriptSoundFile(const std::string& script, const std::string& resource) const;
	// The same, with how the script plays it: its volume (SetTrim) and the
	// range its pitch is picked in (SetPitchRange), one at random.
	struct ScriptSound
	{
		std::string file;
		float trim = 1.0f;
		float pitch = 1.0f;
	};
	ScriptSound FindScriptSound(const std::string& script, const std::string& resource) const;
	// The engine of the car the player drives: one loop, pitched to the revs.
	void PlayEngine(const std::string& file);
	void SetEnginePitch(float pitch);
	void StopEngine();
	// A sound looped while something lasts, by name ("skid": the tyres of the
	// player's car squealing): `file` at `gain`; the same file again only
	// changes its gain, an empty one stops it.
	void SetLoop(const std::string& channel, const std::string& file, float gain = 1.0f);
	// The place's background sound, looped softly ("suburb_day_loop.rsd").
	void PlayAmbience(const std::string& part);
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
	std::unordered_map<std::string, std::string> _subtitleTexts;
	// The speakers' names by the codes their files carry ("lis": "lisa").
	std::unordered_map<std::string, std::string> _speakerCodes;
	Subtitle _subtitle;
	std::chrono::steady_clock::time_point _subtitleUntil;
	void showSubtitle(const std::string& name, const RCL::RSDFile& rsdFile);
	// The source speaking the current line, or -1.
	int _dialogueSource = -1;
	// The line being said: who says it, and its loudness a 30th of a second at
	// a time, 0 to 1 against its loudest. The last sound played's, to be taken
	// for a line.
	std::string _dialogueSpeaker;
	std::vector<float> _dialogueEnvelope, _lastEnvelope;
	// Who says line `name`: the folder of a character's own lines, or a
	// conversation's speaker by their code; empty for the generic people.
	std::string speakerOf(const std::string& name) const;
	std::unique_ptr<MusicSource> _music;
	float _channelVolumes[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	float volume(Channel channel) const { return _channelVolumes[static_cast<int>(channel)]; }
	// What each loop was asked to play at, before its channel's volume.
	std::unordered_map<std::string, float> _loopGains;
	// The music playing: its once tracks, then its loop; and the last cue that
	// looped, to go back to.
	MusicScript::Cue _cue;
	MusicScript::Cue _baseCue;
	std::size_t _cueStep = 0;
	std::unique_ptr<MusicScript> _musicScript;
	// Plays `cue` from its first track; false when none of it plays.
	bool startCue(MusicScript::Cue cue);
	ALuint _engineSource = 0;
	ALuint _ambienceSource = 0;
	ALuint _ambienceBuffer = 0;
	std::string _ambience;
	std::string _ambienceAsked; // the last `part`, to look it up only on a change
	ALuint _engineBuffer = 0;
	struct Loop
	{
		std::string file;
		ALuint source = 0;
		ALuint buffer = 0;
	};
	std::unordered_map<std::string, Loop> _loops;
	// The sound scripts' text, by name ("homer"), as read; empty when there is
	// no such script.
	mutable std::unordered_map<std::string, std::string> _scriptTexts;
	// Stops and frees the music source, if any.
	void releaseMusic();
	// Decodes `name` whole and plays it once on a fresh music source.
	bool startMusicTrack(const std::string& name, bool loop);
};

} // namespace Donut
