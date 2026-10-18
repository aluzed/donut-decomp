// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <algorithm>
#include "AudioManager.h"

#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Render/imgui/imgui.h"

#include <cstdint>
#include <stdexcept>

namespace Donut
{

AudioManager::AudioManager()
{
	initializeOpenAL();

	discoverFiles("audio", ".rcf");
}

AudioManager::~AudioManager()
{
	for (auto& source : _sources)
	{
		if (source.buffer != 0)
			alDeleteBuffers(1, &source.buffer);
		if (source.id != 0)
			alDeleteSources(1, &source.id);
	}
	_sources.clear();

	shutdownOpenAL();
	_sounds.clear();
}

void AudioManager::LoadRCF(const std::string& filename)
{
	if (!FileSystem::exists(filename))
	{
		Log::Warn("Audio: RCF file not found: {}", filename);
		return;
	}

	auto rcf = std::make_unique<RCL::RCFFile>(filename);

	for (const auto& name : rcf->GetFilenames())
	{
		if (_sounds.find(name) == _sounds.end())
			_sounds[name] = rcf.get();
	}

	_rcfFiles.push_back(std::move(rcf));

	Log::Info("Audio: loaded RCF {} ({} files)", filename, _sounds.size());
}

void AudioManager::PlayAudio(const std::string& name)
{
	for (const auto& rcf : _rcfFiles)
	{
		auto rsdStream = rcf->GetFileStream(name);
		if (rsdStream == nullptr)
			continue;

		RCL::RSDFile rsdFile(*rsdStream);
		Source* src = findFreeSource();
		if (!src) return;

		if (src->buffer != 0)
			alDeleteBuffers(1, &src->buffer);
		alGenBuffers(1, &src->buffer);

		playOnSource(src, rsdFile);
		return;
	}
}

void AudioManager::PlayAudio(uint32_t hash)
{
	for (const auto& f : _rcfFiles)
	{
		auto stream = f->GetFileStream(hash);
		if (stream == nullptr) continue;

		RCL::RSDFile rsdFile(*stream);
		Source* src = findFreeSource();
		if (!src) return;

		if (src->buffer != 0)
			alDeleteBuffers(1, &src->buffer);
		alGenBuffers(1, &src->buffer);

		playOnSource(src, rsdFile);
		return;
	}
}

void AudioManager::PlayAudio(const std::string& name, const Vector3& position)
{
	for (const auto& rcf : _rcfFiles)
	{
		auto rsdStream = rcf->GetFileStream(name);
		if (rsdStream == nullptr) continue;

		RCL::RSDFile rsdFile(*rsdStream);
		Source* src = findFreeSource();
		if (!src) return;

		if (src->buffer != 0)
			alDeleteBuffers(1, &src->buffer);
		alGenBuffers(1, &src->buffer);

		playOnSource(src, rsdFile);
		// Placed in the world, after playOnSource has set it up as heard where
		// the listener is.
		alSourcei(src->id, AL_SOURCE_RELATIVE, AL_FALSE);
		alSource3f(src->id, AL_POSITION, position.X, position.Y, position.Z);
		return;
	}
}

void AudioManager::PlayDialogue(const std::string& name)
{
	QueueDialogue(name, DialoguePriority::Gameplay);
}

void AudioManager::QueueDialogue(const std::string& name, DialoguePriority priority)
{
	_dialogueQueue.push_back({name, priority});
}

bool AudioManager::IsDialoguePlaying() const
{
	if (!_dialogueQueue.empty())
		return true;
	if (_dialogueSource < 0)
		return false;
	ALint state = AL_STOPPED;
	alGetSourcei(_sources[_dialogueSource].id, AL_SOURCE_STATE, &state);
	return state == AL_PLAYING;
}

void AudioManager::processDialogueQueue()
{
	// One line at a time, each when the last has been said: every queued line
	// used to start at once, on as many free sources.
	if (_dialogueQueue.empty())
		return;
	if (_dialogueSource >= 0)
	{
		ALint state = AL_STOPPED;
		alGetSourcei(_sources[_dialogueSource].id, AL_SOURCE_STATE, &state);
		if (state == AL_PLAYING)
			return;
		_dialogueSource = -1;
	}

	Source* src = findFreeSource();
	if (src == nullptr)
		return;

	const std::string name = _dialogueQueue.front().name;
	_dialogueQueue.pop_front();
	PlayAudio(name);
	if (src->inUse)
		_dialogueSource = static_cast<int>(src - _sources.data());
}

void AudioManager::Update()
{
	cleanupFinished();
	processDialogueQueue();

	// The next segment when one ends.
	if (_music && !_playlist.empty())
	{
		ALint state = AL_STOPPED;
		alGetSourcei(_music->id, AL_SOURCE_STATE, &state);
		if (state == AL_STOPPED)
		{
			_playlistIndex = (_playlistIndex + 1) % _playlist.size();
			startMusicTrack(_playlist[_playlistIndex], false);
		}
	}
}

void AudioManager::cleanupFinished()
{
	for (auto& src : _sources)
	{
		if (!src.inUse) continue;
		ALint state;
		alGetSourcei(src.id, AL_SOURCE_STATE, &state);
		if (state != AL_PLAYING)
			src.inUse = false;
	}
}

ALenum AudioManager::getFormat(int channels, int bits) const
{
	if (channels == 1)
	{
		if (bits == 8) return AL_FORMAT_MONO8;
		if (bits == 16) return AL_FORMAT_MONO16;
	}
	else if (channels == 2)
	{
		if (bits == 8) return AL_FORMAT_STEREO8;
		if (bits == 16) return AL_FORMAT_STEREO16;
	}
	return AL_FORMAT_STEREO16;
}

void AudioManager::releaseMusic()
{
	if (!_music)
		return;
	alSourceStop(_music->id);
	alSourcei(_music->id, AL_BUFFER, 0);
	alDeleteSources(1, &_music->id);
	alDeleteBuffers(kStreamingBuffers, _music->buffers);
	_music.reset();
}

bool AudioManager::startMusicTrack(const std::string& name, bool loop)
{
	// The previous track's source and buffers used to be dropped without
	// being deleted.
	releaseMusic();

	auto soundIt = _sounds.find(name);
	if (soundIt == _sounds.end())
		return false;
	auto ms = soundIt->second->GetFileStream(name);
	if (!ms)
		return false;
	RCL::RSDFile rsdFile(*ms);
	const auto& data = rsdFile.GetData();
	if (data.empty())
		return false;

	_music = std::make_unique<MusicSource>();
	_music->sampleRate = rsdFile.GetSampleRate();
	_music->channels = rsdFile.GetNumChannels();
	_music->bits = rsdFile.GetBitsPerChannel();
	alGenSources(1, &_music->id);
	alGenBuffers(kStreamingBuffers, _music->buffers);
	// A segment is a few seconds: decoded whole, it fits one buffer.
	alBufferData(_music->buffers[0], getFormat(_music->channels, _music->bits), data.data(),
	             static_cast<ALsizei>(data.size()), _music->sampleRate);
	alSourcei(_music->id, AL_BUFFER, static_cast<ALint>(_music->buffers[0]));
	alSourcei(_music->id, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
	alSourcei(_music->id, AL_SOURCE_RELATIVE, AL_TRUE);
	alSourcef(_music->id, AL_GAIN, 0.45f);
	alSourcePlay(_music->id);
	_music->playing = true;
	return true;
}

void AudioManager::PlayMusic(const std::string& name)
{
	_playlist.clear();
	startMusicTrack(name, true);
}

void AudioManager::PlayMusicPlaylist(std::vector<std::string> tracks)
{
	_playlist = std::move(tracks);
	_playlistIndex = 0;
	if (_playlist.empty())
	{
		releaseMusic();
		return;
	}
	Log::Info("Audio: music, {} tracks from '{}'", _playlist.size(), _playlist.front());
	startMusicTrack(_playlist.front(), false);
}

void AudioManager::StopMusic()
{
	_playlist.clear();
	releaseMusic();
}

std::vector<std::string> AudioManager::FindSounds(const std::string& part) const
{
	std::vector<std::string> found;
	for (const auto& [name, rcf] : _sounds)
		if (name.find(part) != std::string::npos)
			found.push_back(name);
	std::sort(found.begin(), found.end());
	return found;
}

void AudioManager::SetListenerPosition(const Vector3& position, const Vector3& direction, const Vector3& up)
{
	alListener3f(AL_POSITION, position.X, position.Y, position.Z);
	ALfloat orientation[] = {direction.X, direction.Y, direction.Z, up.X, up.Y, up.Z};
	alListenerfv(AL_ORIENTATION, orientation);
}

void AudioManager::SetVolume(float volume)
{
	alListenerf(AL_GAIN, volume);
}

void AudioManager::PlayRaw(const std::vector<uint8_t>& pcmData, int sampleRate, int channels, int bits)
{
	Source* src = findFreeSource();
	if (!src) return;

	if (src->buffer != 0)
		alDeleteBuffers(1, &src->buffer);

	alGenBuffers(1, &src->buffer);

	ALenum format = AL_FORMAT_MONO16;
	if (channels == 2 && bits == 16)
		format = AL_FORMAT_STEREO16;
	else if (channels == 1 && bits == 16)
		format = AL_FORMAT_MONO16;
	else if (channels == 1 && bits == 8)
		format = AL_FORMAT_MONO8;

	alBufferData(src->buffer, format, pcmData.data(), static_cast<ALsizei>(pcmData.size()), sampleRate);
	alSourcei(src->id, AL_BUFFER, src->buffer);
	alSourcei(src->id, AL_LOOPING, AL_FALSE);
	// Not placed in the world: heard where the listener is, not from the
	// world's origin, metres away and faint.
	alSourcei(src->id, AL_SOURCE_RELATIVE, AL_TRUE);
	alSource3f(src->id, AL_POSITION, 0.0f, 0.0f, 0.0f);
	alSourcePlay(src->id);
	src->inUse = true;
}

void AudioManager::DebugGUI(bool* open)
{
	ImGui::SetNextWindowSize(ImVec2(330, 400), ImGuiCond_Once);
	ImGui::Begin("RADCORE CEMENT LIBRARY", open);

	for (const auto& rcf : _rcfFiles)
	{
		if (ImGui::TreeNode(rcf->GetFileName().c_str()))
		{
			for (const auto& filename : rcf->GetFilenames())
			{
				if (ImGui::Selectable(filename.c_str()))
				{
					PlayAudio(filename);
				}
			}

			ImGui::TreePop();
		}
	}

	ImGui::End();
}

void AudioManager::initializeOpenAL()
{
	_alDevice = alcOpenDevice(nullptr);
	if (!_alDevice)
		throw std::runtime_error("Failed to open OpenAL device.");

	_alContext = alcCreateContext(_alDevice, nullptr);
	if (!_alContext)
		throw std::runtime_error("Failed to create OpenAL context.");

	alcMakeContextCurrent(_alContext);

	_sources.resize(kMaxSources);
	for (auto& src : _sources)
	{
		src.id = 0;
		src.buffer = 0;
		src.inUse = false;
		alGenSources(1, &src.id);
	}
}

void AudioManager::shutdownOpenAL()
{
	releaseMusic();

	for (auto& src : _sources)
	{
		if (src.buffer) alDeleteBuffers(1, &src.buffer);
		if (src.id) alDeleteSources(1, &src.id);
	}

	if (_alContext)
	{
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(_alContext);
	}
	_alContext = nullptr;

	if (_alDevice)
	{
		alcCloseDevice(_alDevice);
	}
	_alDevice = nullptr;
}

ALenum AudioManager::getFormat(const RCL::RSDFile& file) const
{
	if (file.GetNumChannels() == 1)
	{
		if (file.GetBitsPerChannel() == 8)
			return AL_FORMAT_MONO8;
		else if (file.GetBitsPerChannel() == 16)
			return AL_FORMAT_MONO16;
	}
	else if (file.GetNumChannels() == 2)
	{
		if (file.GetBitsPerChannel() == 8)
			return AL_FORMAT_STEREO8;
		else if (file.GetBitsPerChannel() == 16)
			return AL_FORMAT_STEREO16;
	}

	return AL_FORMAT_STEREO16;
}

AudioManager::Source* AudioManager::findFreeSource()
{
	for (auto& src : _sources)
	{
		ALint state;
		alGetSourcei(src.id, AL_SOURCE_STATE, &state);
		if (state != AL_PLAYING)
		{
			src.inUse = false;
			return &src;
		}
	}
	return nullptr;
}

void AudioManager::playOnSource(Source* src, const RCL::RSDFile& rsdFile)
{
	const ALenum format = getFormat(rsdFile);
	const auto& data = rsdFile.GetData();

	alBufferData(src->buffer, format, data.data(), static_cast<ALsizei>(data.size()), rsdFile.GetSampleRate());
	alSourcei(src->id, AL_BUFFER, src->buffer);
	alSourcei(src->id, AL_LOOPING, AL_FALSE);
	alSourcePlay(src->id);
	src->inUse = true;
}

void AudioManager::discoverFiles(const std::string& directory, const std::string& extension)
{
	if (!FileSystem::exists(directory))
	{
		Log::Warn("Audio: directory '{}' not found, skipping scan", directory);
		return;
	}

	if (!FileSystem::is_directory(directory))
		return;

	// Recursive scan: any RCF anywhere under the audio tree is picked up,
	// so dropping a new file in a subdirectory requires no code change.
	std::error_code ec;
	for (const auto& entry : FileSystem::recursive_directory_iterator(directory, ec))
	{
		if (ec)
			break;
		if (!entry.is_regular_file())
			continue;
		auto path = entry.path();
		// DIALOGF.RCF, the game's voices, is in capitals.
		std::string found = path.extension().string();
		std::transform(found.begin(), found.end(), found.begin(), [](unsigned char c) { return std::tolower(c); });
		if (found == extension)
			LoadRCF(path.string());
	}
}

} // namespace Donut
