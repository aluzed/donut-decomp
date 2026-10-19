#include "BillboardAnimation.h"

#include "Core/MemoryStream.h"
#include "P3D/P3DChunk.h"

#include <algorithm>

namespace Donut
{

namespace
{
std::string readName(MemoryStream& stream)
{
	std::string text = stream.ReadLPString();
	return text.substr(0, text.find('\0'));
}

template <class T>
T interpolate(const std::vector<std::pair<float, T>>& keys, float frame)
{
	if (frame <= keys.front().first)
		return keys.front().second;
	for (std::size_t i = 1; i < keys.size(); ++i)
		if (frame < keys[i].first)
		{
			const auto& [f0, v0] = keys[i - 1];
			const auto& [f1, v1] = keys[i];
			const float t = (frame - f0) / std::max(0.001f, f1 - f0);
			return v0 + (v1 - v0) * t;
		}
	return keys.back().second;
}
} // namespace

Vector2 BillboardAnimation::Quad::SizeAt(float frame, const Vector2& size) const
{
	return Vector2(width.empty() ? size.X : interpolate(width, frame), height.empty() ? size.Y : interpolate(height, frame));
}

std::optional<Vector4> BillboardAnimation::Quad::ColorAt(float frame) const
{
	if (color.empty())
		return std::nullopt;
	return interpolate(color, frame);
}

bool BillboardAnimation::Quad::VisibleAt(float frame) const
{
	bool visible = startsVisible;
	for (const float at : toggles)
		if (frame >= at)
			visible = !visible;
	return visible;
}

std::optional<BillboardAnimation> BillboardAnimation::FromChunk(const P3D::P3DChunk& chunk)
{
	// version, name, type, frame count, rate; groups (version, name, id,
	// channel count), each with its channels: version, param, key count, key
	// frames (u16), values. A bool channel: version, param, start state (u16),
	// toggle count, toggle frames (u16).
	MemoryStream header(chunk.GetData());
	header.Read<uint32_t>();
	readName(header);
	if (header.ReadString(4).rfind("BQG", 0) != 0)
		return std::nullopt;
	BillboardAnimation animation;
	animation.frames = header.Read<float>();
	animation.frameRate = header.Read<float>();

	for (const auto& groups : chunk.GetChildren())
		for (const auto& group : groups->GetChildren())
		{
			if (!group->IsType(P3D::ChunkType::AnimationGroup))
				continue;
			MemoryStream groupHeader(group->GetData());
			groupHeader.Read<uint32_t>();
			Quad quad;
			quad.name = readName(groupHeader);
			for (const auto& channel : group->GetChildren())
			{
				MemoryStream stream(channel->GetData());
				stream.Read<uint32_t>();
				std::string param = stream.ReadString(4);
				param = param.substr(0, param.find('\0'));
				if (channel->IsType(P3D::ChunkType::BoolChannel))
				{
					quad.startsVisible = stream.Read<uint16_t>() != 0;
					const uint32_t count = stream.Read<uint32_t>();
					for (uint32_t i = 0; i < count; ++i)
						quad.toggles.push_back(static_cast<float>(stream.Read<uint16_t>()));
					continue;
				}
				const uint32_t count = stream.Read<uint32_t>();
				std::vector<uint16_t> frames(count);
				for (auto& frame : frames)
					frame = stream.Read<uint16_t>();
				if (channel->IsType(P3D::ChunkType::Float1Channel) && (param == "WDT" || param == "HGT"))
				{
					auto& keys = param == "WDT" ? quad.width : quad.height;
					for (const uint16_t frame : frames)
						keys.emplace_back(static_cast<float>(frame), stream.Read<float>());
				}
				else if (channel->IsType(P3D::ChunkType::Color))
					for (const uint16_t frame : frames)
						quad.color.emplace_back(static_cast<float>(frame), P3D::P3DUtil::ConvertColor(stream.Read<uint32_t>()));
			}
			animation.quads.push_back(std::move(quad));
		}
	return animation;
}

} // namespace Donut
