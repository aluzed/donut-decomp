#include "P3D/StateProp.h"

#include "Core/MemoryStream.h"
#include "P3D/P3DChunk.h"

#include <cmath>

namespace Donut::P3D
{

namespace
{
std::string text(MemoryStream& stream)
{
	std::string read = stream.ReadLPString();
	return read.substr(0, read.find('\0'));
}
} // namespace

float StateProp::FrameController::FrameAt(float seconds) const
{
	const float frame = firstFrame + seconds * kFramesPerSecond * speed;
	const float length = lastFrame - firstFrame;
	if (length <= 0.0f)
		return firstFrame;
	if (cyclic)
		return firstFrame + std::fmod(frame - firstFrame, length);
	return std::min(frame, lastFrame);
}

const StateProp::FrameController* StateProp::State::Controller(const std::string& controller) const
{
	for (const auto& candidate : controllers)
		if (candidate.name == controller)
			return &candidate;
	return nullptr;
}

int StateProp::State::OnEvent(const std::string& event) const
{
	for (const auto& candidate : events)
		if (candidate.name == event)
			return static_cast<int>(candidate.toState);
	return -1;
}

StateProp StateProp::Load(const P3DChunk& chunk)
{
	StateProp prop;
	MemoryStream header(chunk.GetData());
	header.Read<uint32_t>();
	prop.name = text(header);
	prop.objectFactory = text(header);
	for (const auto& child : chunk.GetChildren())
	{
		if (!child->IsType(ChunkType::StaticPropStateData))
			continue;
		// Its name, whether it goes on by itself and to which, its counts,
		// and after how long.
		MemoryStream stream(child->GetData());
		State state;
		state.name = text(stream);
		state.autoTransition = stream.Read<uint32_t>() != 0;
		state.outState = stream.Read<uint32_t>();
		for (int count = 0; count < 4; ++count)
			stream.Read<uint32_t>();
		state.outSeconds = stream.Read<float>();
		for (const auto& part : child->GetChildren())
		{
			MemoryStream data(part->GetData());
			const std::string name = text(data);
			if (part->IsType(ChunkType::StaticPropVisibilityData))
				state.visibility.push_back({name, data.Read<uint32_t>() != 0});
			else if (part->IsType(ChunkType::StaticPropFrameControllerData))
			{
				FrameController controller;
				controller.name = name;
				controller.cyclic = data.Read<uint32_t>() != 0;
				controller.cycles = data.Read<uint32_t>();
				controller.holdFrame = data.Read<uint32_t>() != 0;
				controller.firstFrame = data.Read<float>();
				controller.lastFrame = data.Read<float>();
				controller.speed = data.Read<float>();
				state.controllers.push_back(controller);
			}
			else if (part->IsType(ChunkType::StaticPropEventData))
			{
				Event event;
				event.name = name;
				event.toState = data.Read<uint32_t>();
				event.event = data.Read<uint32_t>();
				state.events.push_back(event);
			}
			else if (part->IsType(ChunkType::StaticPropCallbackData))
			{
				CallbackAt callback;
				callback.name = name;
				callback.callback = data.Read<uint32_t>();
				callback.onSeconds = data.Read<float>();
				state.callbacks.push_back(callback);
			}
		}
		prop.states.push_back(std::move(state));
	}
	return prop;
}

int StateProp::Coins(uint32_t callback)
{
	switch (callback)
	{
	case Spawn1Coin: return 1;
	case Spawn5Coins:
	case Spawn5CoinsZ: return 5;
	case Spawn10Coins: return 10;
	case Spawn20Coins: return 20;
	default: return 0;
	}
}

} // namespace Donut::P3D
