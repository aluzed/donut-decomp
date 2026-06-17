// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AudioManager.h"

#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Render/imgui/imgui.h"

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

		alSource3f(src->id, AL_POSITION, position.X, position.Y, position.Z);
		alSourcei(src->id, AL_SOURCE_RELATIVE, AL_FALSE);

		playOnSource(src, rsdFile);
		return;
	}
}

void AudioManager::PlayDialogue(const std::string& name)
{
	PlayAudio(name);
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
		return;

	for (const auto& entry : FileSystem::directory_iterator(directory))
	{
		auto path = entry.path();
		if (path.extension() == extension)
			LoadRCF(path.string());
	}
}

} // namespace Donut
