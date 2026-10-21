#include "VertexAnimation.h"

#include "Core/FileSystem.h"
#include "Core/MemoryStream.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DChunk.h"
#include "P3D/P3DFile.h"

namespace Donut
{

namespace
{
std::string readName(MemoryStream& stream)
{
	std::string text = stream.ReadLPString();
	return text.substr(0, text.find('\0'));
}
} // namespace

std::unique_ptr<VertexAnimation> VertexAnimation::FromFile(const std::string& path)
{
	if (!FileSystem::exists(path))
		return nullptr;
	const P3D::P3DFile file(path);
	auto animation = std::make_unique<VertexAnimation>();
	for (const auto& chunk : file.GetRoot().GetChildren())
	{
		// A key frame: its name, and every vertex's offset ("POS").
		if (chunk->IsType(P3D::ChunkType::VertexAnimKeyFrame))
		{
			const P3D::VertexAnimKeyFrame key(*chunk);
			const std::string& name = key.GetName();
			if (key.GetOffsets())
				animation->keys[name.substr(0, name.find('\0'))] = key.GetOffsets()->GetOffsets();
		}
		// The animation: version, name, type, frames, rate; per group an entity
		// channel ("VRTX": version, param, key count, frames u16, names).
		else if (chunk->IsType(P3D::ChunkType::Animation))
		{
			MemoryStream header(chunk->GetData());
			header.Read<uint32_t>();
			const std::string name = readName(header);
			if (header.ReadString(4).rfind("VRTX", 0) != 0 || name.rfind("VRTX_", 0) != 0)
				continue;
			header.Read<float>();
			animation->mesh = name.substr(5);
			animation->frameRate = header.Read<float>();
			for (const auto& groups : chunk->GetChildren())
				for (const auto& group : groups->GetChildren())
					for (const auto& channel : group->GetChildren())
					{
						if (!channel->IsType(P3D::ChunkType::EntityChannel))
							continue;
						MemoryStream stream(channel->GetData());
						stream.Read<uint32_t>();
						stream.ReadString(4);
						const uint32_t count = stream.Read<uint32_t>();
						std::vector<uint16_t> frames(count);
						for (auto& frame : frames)
							frame = stream.Read<uint16_t>();
						auto& keys = animation->groups.emplace_back();
						for (const uint16_t frame : frames)
							keys.emplace_back(static_cast<float>(frame), readName(stream));
					}
		}
	}
	if (animation->mesh.empty() || animation->groups.empty())
		return nullptr;
	return animation;
}

const std::vector<Vector3>* VertexAnimation::KeyAt(std::size_t group, float frame) const
{
	if (group >= groups.size() || groups[group].empty())
		return nullptr;
	const std::string* name = &groups[group].front().second;
	for (const auto& [at, key] : groups[group])
		if (at <= frame)
			name = &key;
	const auto found = keys.find(*name);
	return found != keys.end() ? &found->second : nullptr;
}

} // namespace Donut
