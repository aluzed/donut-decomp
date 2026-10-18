#include "TextureAnimation.h"

#include "Core/MemoryStream.h"
#include "P3D/P3DChunk.h"

namespace Donut
{

std::optional<TextureAnimation> TextureAnimation::FromChunk(const P3D::P3DChunk& chunk)
{
	// version, name, type, frame count, rate; one group, one entity channel
	// ("TEX"): version, param, key count, key frames (u16), then each key's
	// texture name.
	MemoryStream stream(chunk.GetData());
	stream.Read<uint32_t>();
	const std::string name = stream.ReadLPString();
	if (stream.ReadString(4).rfind("TEX", 0) != 0 || name.rfind("TEX_", 0) != 0)
		return std::nullopt;

	TextureAnimation animation;
	animation.shader = name.substr(4);
	animation.frameCount = stream.Read<float>();
	animation.frameRate = stream.Read<float>();
	for (const auto& groups : chunk.GetChildren())
		for (const auto& group : groups->GetChildren())
			for (const auto& channel : group->GetChildren())
			{
				if (channel->GetType() != P3D::ChunkType::EntityChannel)
					continue;
				MemoryStream values(channel->GetData());
				values.Read<uint32_t>();
				values.ReadString(4);
				const uint32_t count = values.Read<uint32_t>();
				std::vector<uint16_t> frames(count);
				for (auto& frame : frames)
					frame = values.Read<uint16_t>();
				animation.keys.clear();
				for (const uint16_t frame : frames)
					animation.keys.emplace_back(static_cast<float>(frame), values.ReadLPString());
			}
	if (animation.keys.empty())
		return std::nullopt;
	return animation;
}

const std::string& TextureAnimation::TextureAt(float frame) const
{
	const std::string* texture = &keys.front().second;
	for (const auto& [at, name] : keys)
		if (at <= frame)
			texture = &name;
	return *texture;
}

} // namespace Donut
