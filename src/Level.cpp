// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Render/imgui/imgui.h"
#include <algorithm>
#include <cctype>
#include <Core/File.h>
#include <Core/Log.h>
#include <Entity.h>
#include <Game.h>
#include <Level.h>
#include <P3D/P3D.generated.h>
#include <P3D/P3DFile.h>
#include <Physics/WorldPhysics.h>
#include <Render/BillboardBatch.h>

#include <cmath>
#include <cstring>
#include <Render/Font.h>
#include <Render/LineRenderer.h>
#include <Render/Mesh.h>
#include <Render/OpenGL/ShaderProgram.h>
#include <Render/Shader.h>
#include <Render/WorldSphere.h>
#include <ResourceManager.h>
#include <array>

namespace Donut
{

namespace
{
// A length-prefixed string at `offset` of a chunk's data.
std::string readLPString(const std::vector<uint8_t>& data, std::size_t offset)
{
	if (offset >= data.size())
		return {};
	const std::size_t length = std::min<std::size_t>(data[offset], data.size() - offset - 1);
	std::string text(reinterpret_cast<const char*>(data.data()) + offset + 1, length);
	return text.substr(0, text.find('\0'));
}

// An animated level object (`Anim`, `AnimCollision`: the windmill, the power
// plant's platforms): its composite drawable, plus what it names from the
// file around it -- the skeleton, the meshes of its props, and the pose
// animation its frame controller plays.
class CompositeModel_Anim: public ICompositeModel
{
public:
	CompositeModel_Anim(const P3D::P3DChunk& anim, const std::map<std::string, const P3D::P3DChunk*>& skeletons,
	                    const std::map<std::string, const P3D::P3DChunk*>& geometries,
	                    const std::map<std::string, const P3D::P3DChunk*>& animations)
	{
		for (const auto& child : anim.GetChildren())
		{
			if (child->IsType(P3D::ChunkType::CompositeDrawable))
				_drawables.push_back(P3D::CompositeDrawable::Load(*child));
			// version, name, type ("PTRN" or "EFX\0"), frame offset, then the
			// hierarchy and the animation by name.
			else if (child->IsType(P3D::ChunkType::FrameController))
			{
				const auto& data = child->GetData();
				const std::string name = readLPString(data, 4);
				const std::size_t typeAt = 4 + 1 + data[4];
				if (typeAt + 8 >= data.size() || std::memcmp(data.data() + typeAt, "PTRN", 4) != 0)
					continue;
				const std::size_t hierarchyAt = typeAt + 8;
				const std::string animation = readLPString(data, hierarchyAt + 1 + data[hierarchyAt]);
				if (const auto found = animations.find(animation); found != animations.end() && _animations.empty())
					_animations.push_back(P3D::Animation::Load(*found->second));
			}
		}
		for (const auto& drawable : _drawables)
		{
			if (const auto found = skeletons.find(drawable->GetSkeletonName()); found != skeletons.end())
				_skeletons.push_back(P3D::Skeleton::Load(*found->second));
			if (const auto& props = drawable->GetPropList())
				for (const auto& prop : props->GetProps())
					if (const auto found = geometries.find(prop->GetName()); found != geometries.end())
						_meshes.push_back(P3D::Geometry::Load(*found->second));
		}
	}

	const std::vector<std::unique_ptr<P3D::CompositeDrawable>>& GetDrawables() const override { return _drawables; }
	const std::vector<std::unique_ptr<P3D::Skeleton>>& GetSkeletons() const override { return _skeletons; }
	const std::vector<std::unique_ptr<P3D::Geometry>>& GetMeshes() const override { return _meshes; }
	const std::vector<std::unique_ptr<P3D::Shader>>& GetShaders() const override { return _shaders; }
	const std::vector<std::unique_ptr<P3D::Texture>>& GetTextures() const override { return _textures; }
	const std::vector<std::unique_ptr<P3D::Animation>>& GetAnimations() const override { return _animations; }

private:
	std::vector<std::unique_ptr<P3D::CompositeDrawable>> _drawables;
	std::vector<std::unique_ptr<P3D::Skeleton>> _skeletons;
	std::vector<std::unique_ptr<P3D::Geometry>> _meshes;
	// The file's own shaders and textures are loaded as the level is.
	std::vector<std::unique_ptr<P3D::Shader>> _shaders;
	std::vector<std::unique_ptr<P3D::Texture>> _textures;
	std::vector<std::unique_ptr<P3D::Animation>> _animations;
};
} // namespace

Level::Level()
{
	const auto worldVertSrc = File::ReadAll("shaders/world.vert");
	const auto worldFragSrc = File::ReadAll("shaders/world.frag");
	const auto worldInstancedVertSrc = File::ReadAll("shaders/world_instanced.vert");
	const auto billboardBatchVertSrc = File::ReadAll("shaders/billboard_batch.vert");

	_worldShader = std::make_unique<GL::ShaderProgram>(worldVertSrc, worldFragSrc);
	_worldInstancedShader = std::make_unique<GL::ShaderProgram>(worldInstancedVertSrc, worldFragSrc);
	_billboardBatchShader = std::make_unique<GL::ShaderProgram>(billboardBatchVertSrc, worldFragSrc);

	// todo: move this into Game.cpp or something else ?
}

Level::~Level() = default;

void Level::LoadP3D(const std::string& filename)
{
	if (_loadedP3Ds.find(filename) != _loadedP3Ds.end())
		return;
	_loadedP3Ds.insert(filename);

	std::string fullpath = "./art/" + filename;
	if (!FileSystem::exists(fullpath))
	{
		fullpath = "./art/level/" + filename;
		if (!FileSystem::exists(fullpath))
		{
			fullpath = filename;
			if (!FileSystem::exists(fullpath))
			{
				Log::Warn("Level not found: {}", filename);
				return;
			}
		}
	}

	Log::Info("Loading level: {}", filename);

	const auto p3d = P3D::P3DFile(fullpath);

	auto& rm = Game::GetInstance().GetResourceManager();
	const auto& root = p3d.GetRoot();

	collectSplines(root);

	// This file's own locators, for an interior's entry and exit.
	std::map<std::string, Vector3> fileLocators;
	DrawRange range {{_entities.size(), 0}, {_instances.size(), 0}, {_compositeModels.size(), 0},
	                 {_billboardBatches.size(), 0}};

	// What the animated objects name: the file's skeletons, meshes and
	// animations, by name.
	std::map<std::string, const P3D::P3DChunk*> fileSkeletons, fileGeometries, fileAnimations;
	for (const auto& chunk : root.GetChildren())
	{
		if (chunk->IsType(P3D::ChunkType::Skeleton))
			fileSkeletons[readLPString(chunk->GetData(), 0)] = chunk.get();
		else if (chunk->IsType(P3D::ChunkType::Geometry))
			fileGeometries[readLPString(chunk->GetData(), 0)] = chunk.get();
		else if (chunk->IsType(P3D::ChunkType::Animation))
			fileAnimations[readLPString(chunk->GetData(), 4)] = chunk.get();
	}

	for (const auto& chunk : root.GetChildren())
	{
		switch (chunk->GetType())
		{
		case P3D::ChunkType::Animation:
			if (auto animation = TextureAnimation::FromChunk(*chunk))
				_textureAnimations.emplace_back(std::move(*animation), std::string());
			break;
		case P3D::ChunkType::Anim:
		case P3D::ChunkType::AnimCollision:
		{
			auto model = std::make_unique<CompositeModel>(
			    CompositeModel_Anim(*chunk, fileSkeletons, fileGeometries, fileAnimations));
			const std::string name = readLPString(chunk->GetData(), 0);
			// Doorbells and light switches wait to be worked; the rest -- the
			// windmill, signs, rivers, the power plant's platforms -- go round.
			std::string lower = name;
			std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
			const bool waits = lower.rfind("db_", 0) == 0 || lower.find("switch") != std::string::npos;
			if (model->IsAnimated())
				_animatedObjects.push_back({_compositeModels.size(), name, !waits});
			// Their rest pose is not where they stand: the first frame places
			// them (a switch's rest is the world's origin).
			model->Pose(0.0f, false);
			_compositeModels.push_back(std::move(model));
			break;
		}
		case P3D::ChunkType::Shader: rm.LoadShader(*P3D::Shader::Load(*chunk)); break;
		case P3D::ChunkType::Texture: rm.LoadTexture(*P3D::Texture::Load(*chunk)); break;
		case P3D::ChunkType::Set: rm.LoadSet(*P3D::Set::Load(*chunk)); break;
		case P3D::ChunkType::Geometry: rm.LoadGeometry(*P3D::Geometry::Load(*chunk)); break;
		case P3D::ChunkType::StaticEntity:
			_entities.emplace_back(std::make_unique<StaticEntity>(*P3D::StaticEntity::Load(*chunk)));
			break;
		case P3D::ChunkType::StaticPhysics:
		{
			const auto& ent = P3D::StaticPhysics::Load(*chunk);
			if (auto const& volume = ent->GetCollisionObject()->GetVolume())
				Game::GetInstance().GetWorldPhysics().AddCollisionVolume(*volume);
			break;
		}
		case P3D::ChunkType::InstancedStaticPhysics:
		{
			const auto& staticPhys = P3D::InstancedStaticPhysics::Load(*chunk);
			std::vector<P3D::SceneGraphDrawable*> drawables;
			std::vector<Matrix4x4> transforms;
			P3D::P3DUtil::GetDrawables(staticPhys->GetInstanceList(), drawables, transforms);

			const auto& geometries = staticPhys->GetGeometries();
			std::map<std::string, size_t> meshesNameIndex;
			for (const auto& geometry : geometries) { meshesNameIndex.insert({geometry->GetName(), meshesNameIndex.size()}); }

			std::unordered_map<std::string, std::vector<Matrix4x4>> meshTransforms;

			for (size_t i = 0; i < drawables.size(); ++i)
			{
				const auto& drawable = drawables.at(i);
				const auto& transform = transforms.at(i);
				const auto& meshName = drawable->GetName();

				auto& transforms = meshTransforms[meshName];
				transforms.push_back(transform);
			}

			for (const auto& meshTransformsPair : meshTransforms)
			{
				const auto index = meshesNameIndex.find(meshTransformsPair.first);
				if (index == meshesNameIndex.end())
				{
					Log::Warn("Level: no mesh '{}' in its physics object, skipped", meshTransformsPair.first);
					continue;
				}
				const auto& geometry = geometries.at(index->second);
				_instances.emplace_back(std::make_unique<InstancedStaticEntity>(*geometry, meshTransformsPair.second));
			}

			break;
		}
		case P3D::ChunkType::DynamicPhysics:
		{
			const auto& dynaPhys = P3D::DynamicPhysics::Load(*chunk);
			std::vector<P3D::SceneGraphDrawable*> drawables;
			std::vector<Matrix4x4> transforms;
			P3D::P3DUtil::GetDrawables(dynaPhys->GetInstanceList(), drawables, transforms);

			const auto& geometries = dynaPhys->GetGeometries();
			std::map<std::string, size_t> meshesNameIndex;
			for (const auto& geometry : geometries) { meshesNameIndex.insert({geometry->GetName(), meshesNameIndex.size()}); }

			std::unordered_map<std::string, std::vector<Matrix4x4>> meshTransforms;

			for (size_t i = 0; i < drawables.size(); ++i)
			{
				const auto& drawable = drawables.at(i);
				const auto& transform = transforms.at(i);
				const auto& meshName = drawable->GetName();

				auto& transforms = meshTransforms[meshName];
				transforms.push_back(transform);
			}

			// Level 2's old lamps name their mesh after the instance
			// ("l2_lampoldShape3") while the object carries it under its own
			// name ("l2_oldstreetlamp_Shape"); map::at threw and the load died.
			// An object with a single mesh draws it whatever the name.
			for (const auto& meshTransformsPair : meshTransforms)
			{
				auto index = meshesNameIndex.find(meshTransformsPair.first);
				if (index == meshesNameIndex.end() && meshesNameIndex.size() == 1)
					index = meshesNameIndex.begin();
				if (index == meshesNameIndex.end())
				{
					Log::Warn("Level: no mesh '{}' in its physics object, skipped", meshTransformsPair.first);
					continue;
				}
				const auto& mesh = geometries.at(index->second);
				_instances.emplace_back(std::make_unique<InstancedStaticEntity>(*mesh, meshTransformsPair.second));
			}

			break;
		}
		case P3D::ChunkType::AnimDynamicPhysics:
		{
			const auto& dynaPhys = P3D::AnimDynamicPhysics::Load(*chunk);
			std::vector<P3D::SceneGraphDrawable*> drawables;
			std::vector<Matrix4x4> transforms;
			P3D::P3DUtil::GetDrawables(dynaPhys->GetInstanceList(), drawables, transforms);

			const auto& animObjectWrapper = dynaPhys->GetAnimObjectWrapper();
			// Level 2 has some without one (l2r1.p3d); building a model from
			// nothing crashed the load.
			if (!animObjectWrapper)
			{
				Log::Warn("Level: animated physics object '{}' has no model, skipped", dynaPhys->GetName());
				break;
			}

			for (size_t i = 0; i < drawables.size(); ++i)
			{
				const auto& drawable = drawables.at(i);
				const auto& transform = transforms.at(i);

				auto compositeModel = std::make_unique<CompositeModel>(CompositeModel_AnimObjectWrapper(*animObjectWrapper));
				compositeModel->SetTransform(transform);
				if (compositeModel->IsAnimated())
				{
					compositeModel->Pose(0.0f, false);
					AnimatedObject object {_compositeModels.size(), dynaPhys->GetName(), false};
					object.breaks = true;
					object.position = transform.Translation();
					_animatedObjects.push_back(object);
				}
				_compositeModels.push_back(std::move(compositeModel));
			}

			break;
		}
		case P3D::ChunkType::Intersect:
		{
			auto intersect = P3D::Intersect::Load(*chunk);
			Game::GetInstance().GetWorldPhysics().AddIntersect(*intersect);
			break;
		}
		case P3D::ChunkType::WorldSphere: _worldSphere = std::make_unique<WorldSphere>(*P3D::WorldSphere::Load(*chunk)); break;
		case P3D::ChunkType::Locator2:
		{
			auto loc = P3D::Locator2::Load(*chunk);

			// Locator2 carries its own position; it used to be taken from the
			// first trigger's transform, which meant every trigger-less locator
			// was dropped outright -- and those are most of them (spawn points,
			// car starts, race_finish...).
			_locators.insert({loc->GetName(), loc->GetPosition()});
			fileLocators[loc->GetName()] = loc->GetPosition();

			// A car start's first data word is its heading as a float, in radians
			// about +Y: sr2's grid reads 1.571, facing +X down the street the race
			// starts on.
			if (static_cast<LocatorType>(loc->GetType()) == LocatorType::CarStart && !loc->GetData().empty())
			{
				float heading = 0.0f;
				const uint32_t word = loc->GetData().front();
				std::memcpy(&heading, &word, sizeof(heading));
				_locatorHeadings.insert({loc->GetName(), heading});
			}
			if (static_cast<LocatorType>(loc->GetType()) == LocatorType::Coin)
				_coinPositions.push_back(loc->GetPosition());
			// The collector cards: "card" and two digits, the level and the card.
			if (const std::string& name = loc->GetName();
			    name.size() == 6 && name.compare(0, 4, "card") == 0 && std::isdigit(name[4]) && std::isdigit(name[5]))
				_cards.push_back({name, loc->GetPosition()});
			Log::Debug("Level: locator '{}' at ({:.1f}, {:.1f}, {:.1f})", loc->GetName(), loc->GetPosition().X,
			           loc->GetPosition().Y, loc->GetPosition().Z);

			// The data is a run of 32-bit words; for the locators that carry text
			// (dynamic zones, interior entrances, script locators) it starts with a
			// NUL-terminated string.
			std::string data;
			for (uint32_t word : loc->GetData())
			{
				bool end = false;
				for (int byte = 0; byte < 4 && !end; ++byte)
				{
					const char c = static_cast<char>((word >> (8 * byte)) & 0xFF);
					if (c == '\0' || static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126)
						end = true;
					else
						data += c;
				}
				if (end)
					break;
			}

			// An action locator's data is three strings, each NUL-ended and
			// padded: the object, its joint or switch, and the action.
			std::string action, actionTarget;
			if (static_cast<LocatorType>(loc->GetType()) == LocatorType::Action)
			{
				std::vector<std::string> strings(1);
				for (uint32_t word : loc->GetData())
					for (int byte = 0; byte < 4; ++byte)
					{
						const char c = static_cast<char>((word >> (8 * byte)) & 0xFF);
						if (c != '\0')
							strings.back() += c;
						else if (!strings.back().empty())
							strings.emplace_back();
					}
				if (strings.size() >= 3)
				{
					actionTarget = strings[1];
					action = strings[2];
				}
			}

			for (const auto& trigger : loc->GetTriggers())
			{
				// Rows 0-2 are the volume's axes, row 3 its centre (the P3D
				// convention, as for RoadSegment).
				const Matrix4x4& m = trigger->GetTransform();
				Trigger tvol;
				tvol.name = trigger->GetName();
				tvol.locator = loc->GetName();
				tvol.type = static_cast<LocatorType>(loc->GetType());
				tvol.data = data;
				tvol.action = action;
				tvol.actionTarget = actionTarget;
				tvol.position = Vector3(m[3][0], m[3][1], m[3][2]);
				for (int axis = 0; axis < 3; ++axis)
				{
					tvol.axes[axis] = Vector3(m[axis][0], m[axis][1], m[axis][2]);
					tvol.axes[axis].Normalize();
				}
				tvol.bounds = trigger->GetBounds();
				tvol.isRect = trigger->GetIsRect() != 0;
				_triggers.push_back(tvol);
				_insideTrigger.push_back(false);
			}
			break;
		}
		case P3D::ChunkType::FenceWrapper:
		{
			auto const& fence = P3D::FenceWrapper::Load(*chunk);
			Game::GetInstance().GetWorldPhysics().AddP3DFence(*fence->GetFence());
			break;
		}
		case P3D::ChunkType::BillboardQuadGroup:
		{
			_billboardBatches.push_back(std::make_unique<BillboardBatch>(*P3D::BillboardQuadGroup::Load(*chunk)));

			break;
		}
		case P3D::ChunkType::Intersection:
		{
			auto inter = P3D::Intersection::Load(*chunk);
			_intersections.insert({inter->GetName(), inter->GetPosition()});

			break;
		}
		case P3D::ChunkType::Road:
		{
			auto road = P3D::Road::Load(*chunk);

			// Each RoadSegment child places one tile of tarmac. Its transform's
			// translation is the tile's local origin, which is a *corner*: using it
			// puts the road's centre line along one kerb and makes consecutive tiles
			// zig-zag. Place the node at the tile's middle instead.
			std::vector<Vector3> points;
			for (const auto& child : chunk->GetChildren())
			{
				if (!child->IsType(P3D::ChunkType::RoadSegment))
					continue;

				const auto seg = P3D::RoadSegment::Load(*child);
				const Matrix4x4& t = seg->GetTransform();

				Vector3 centre = Vector3::Zero;
				const auto tile = _roadTileCentres.find(seg->GetData());
				if (tile != _roadTileCentres.end())
					centre = tile->second;
				else
					++_unresolvedRoadTiles;

				// Not Matrix4x4::operator*: these P3D matrices carry their
				// translation in the last row (see Translation()), and that operator
				// reads it from the last column.
				points.emplace_back(centre.X * t[0][0] + centre.Y * t[1][0] + centre.Z * t[2][0] + t[3][0],
				                    centre.X * t[0][1] + centre.Y * t[1][1] + centre.Z * t[2][1] + t[3][1],
				                    centre.X * t[0][2] + centre.Y * t[1][2] + centre.Z * t[2][2] + t[3][2]);
			}

			// The codegen calls the flag byte `noReset`, but on L1 it is set on
			// exactly the 16 roads that carry no cars and a non-zero difficulty
			// byte (15-23, where streets have 0) -- the layout of a shortcut. The
			// other 83 have it clear.
			const bool shortcut = road->GetNoReset() != 0;
			_roads.push_back(
			    RoadLink {road->GetStartIntersection(), road->GetEndIntersection(), std::move(points), shortcut});

			break;
		}
		case P3D::ChunkType::RoadDataSegment:
		{
			// A RoadDataSegment is one tile of tarmac, given as three corners with
			// the fourth at the local origin. Only 495 of the level's 937 are
			// parallelograms -- the rest are trapezoids, the tiles that make up a
			// curve -- so the tile's middle is the quad centroid, not p1/2.
			const auto seg = P3D::RoadDataSegment::Load(*chunk);
			_roadTileCentres[seg->GetName()] = (seg->GetPosition0() + seg->GetPosition1() + seg->GetPosition2()) * 0.25f;

			break;
		}
		case P3D::ChunkType::Path:
		{
			auto path = P3D::Path::Load(*chunk);
			_paths.push_back(Path {path->GetPoints()});

			break;
		}
		default: break;
		}
	}

	range.entities[1] = _entities.size();
	range.instances[1] = _instances.size();
	range.composites[1] = _compositeModels.size();
	range.billboards[1] = _billboardBatches.size();
	_drawRanges[filename] = range;

	const auto entry = fileLocators.find("InteriorEntryStart");
	if (entry != fileLocators.end())
	{
		Interior interior {entry->second, entry->second, entry->second};
		if (const auto origin = fileLocators.find("InteriorOrigin"); origin != fileLocators.end())
			interior.origin = origin->second;
		for (const auto& [name, position] : fileLocators)
		{
			if (name.size() > 5 && name.compare(name.size() - 5, 5, "_exit") == 0)
				interior.exit = position;
			if (const std::size_t end = name.find('_', 4);
			    interior.tag.empty() && name.compare(0, 4, "int_") == 0 && end != std::string::npos && end > 4)
				interior.tag = name.substr(4, end - 4);
		}
		_interiors[filename] = interior;
		Log::Info("Level: interior {} ('{}') -- in at ({:.1f}, {:.1f}, {:.1f}), out at ({:.1f}, {:.1f}, {:.1f})", filename,
		          interior.tag, interior.entry.X, interior.entry.Y, interior.entry.Z, interior.exit.X, interior.exit.Y,
		          interior.exit.Z);
	}
}

bool Level::GetInterior(const std::string& file, Interior& interior) const
{
	const auto found = _interiors.find(file);
	if (found == _interiors.end())
		return false;
	interior = found->second;
	return true;
}

std::string Level::FindInteriorFile(const std::string& door) const
{
	std::string lowered = door;
	std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return std::tolower(c); });
	std::string best;
	std::size_t bestLength = 0;
	for (const auto& [file, interior] : _interiors)
		if (interior.tag.size() > bestLength && lowered.find(interior.tag) != std::string::npos)
		{
			best = file;
			bestLength = interior.tag.size();
		}
	return best;
}

void Level::DynaLoadData(const std::string& dynaLoadData)
{
	std::vector<std::string> regionsLoad, regionsUnload;

	std::size_t prev = 0, pos;
	while ((pos = dynaLoadData.find_first_of(";:@$", prev)) != std::string::npos)
	{
		const std::string file = dynaLoadData.substr(prev, pos - prev);
		switch (dynaLoadData.at(pos))
		{
		case ';': regionsLoad.push_back(file); break;
		case ':': regionsUnload.push_back(file); break;
		case '@': regionsLoad.push_back(file); Log::Info("Level: load interior {}", file); break;
		case '$': regionsUnload.push_back(file); Log::Info("Level: unload interior {}", file); break;
		}

		prev = pos + 1;
	}

	for (auto const& region : regionsUnload)
		unloadRegion(region);

	for (auto const& region : regionsLoad)
		loadRegion(region);
}

void Level::ImGuiDebugWindow(bool* p_open) const
{
	ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Level", p_open))
	{
		ImGui::End();
		return;
	}

	for (const auto& ent : _entities)
	{
		ImGui::TextDisabled("%s", ent->GetClassName().c_str());
		ImGui::SameLine();
		ImGui::TextUnformatted(ent->GetName().c_str());
	}

	ImGui::End();
}

bool Level::GetLocatorHeading(const std::string& name, float& heading) const
{
	const auto found = _locatorHeadings.find(name);
	if (found == _locatorHeadings.end())
		return false;
	heading = found->second;
	return true;
}

Vector3 Level::GetLocatorPosition(const std::string& name) const
{
	auto it = _locators.find(name);
	if (it != _locators.end())
		return it->second;
	return Vector3::Zero;
}

bool Level::CheckTrigger(const Vector3& pos, const std::string& name) const
{
	for (const auto& t : _triggers)
	{
		if (!name.empty() && t.name.find(name) == std::string::npos)
			continue;

		if (t.Contains(pos))
			return true;
	}
	return false;
}

bool Level::Trigger::Contains(const Vector3& point) const
{
	const Vector3 d = point - position;
	if (!isRect)
		return d.LengthSquared() <= bounds.X * bounds.X;

	// In the box's own frame. Comparing world-axis offsets, as this used to, is
	// only right for a box that is not turned.
	const float x = d.X * axes[0].X + d.Y * axes[0].Y + d.Z * axes[0].Z;
	const float y = d.X * axes[1].X + d.Y * axes[1].Y + d.Z * axes[1].Z;
	const float z = d.X * axes[2].X + d.Y * axes[2].Y + d.Z * axes[2].Z;
	return std::fabs(x) <= bounds.X && std::fabs(y) <= bounds.Y && std::fabs(z) <= bounds.Z;
}

std::vector<Level::TriggerEvent> Level::UpdateTriggers(const Vector3& pos)
{
	std::vector<TriggerEvent> events;
	for (std::size_t i = 0; i < _triggers.size(); ++i)
	{
		const bool inside = _triggers[i].Contains(pos);
		if (inside != _insideTrigger[i])
		{
			_insideTrigger[i] = inside;
			events.push_back({&_triggers[i], inside});
		}
	}
	return events;
}

void Level::collectSplines(const P3D::P3DChunk& chunk)
{
	// Spline chunks are not level-graph roots: they hang off the scene graph
	// (camera rails inside interiors, for instance), so the top-level switch in
	// LoadP3D never sees them. Walk the tree and key them by their own name.
	for (const auto& child : chunk.GetChildren())
	{
		if (child->IsType(P3D::ChunkType::Spline))
		{
			const auto spline = P3D::Spline::Load(*child);
			_splines[spline->GetName()] = spline->GetPoints();
			Log::Debug("Level: spline '{}' with {} points", spline->GetName(), spline->GetPoints().size());
		}

		collectSplines(*child);
	}
}

void Level::loadRegion(const std::string& filename)
{
	if (_loadedRegions.find(filename) != _loadedRegions.end())
		return;

	Log::Info("load region: {}", filename);
	_loadedRegions.insert(filename);
	LoadP3D(filename);
}

void Level::unloadRegion(const std::string& filename)
{
	Log::Info("unload region: {}", filename);
	_loadedRegions.erase(filename);
	_loadedP3Ds.erase(filename);
}

int Level::PlayAnimatedObject(const std::string& name)
{
	int started = 0;
	for (auto& object : _animatedObjects)
		if (!object.loops && object.name == name)
		{
			object.startedAt = _animationTime;
			++started;
		}
	return started;
}

int Level::BreakObjectsNear(const Vector3& position, float radius)
{
	int broke = 0;
	for (auto& object : _animatedObjects)
		if (object.breaks && !object.broken && (object.position - position).Length() < radius)
		{
			object.broken = true;
			object.startedAt = _animationTime;
			++broke;
		}
	return broke;
}

void Level::Update(double deltatime)
{
	// The level's animated objects, round and round at their own rate.
	_animationTime += deltatime;
	auto& resources = Game::GetInstance().GetResourceManager();
	for (auto& [animation, showing] : _textureAnimations)
	{
		const float frames = std::max(animation.frameCount, animation.keys.back().first + 1.0f);
		const float frame = std::fmod(static_cast<float>(_animationTime) * animation.frameRate, frames);
		const std::string& texture = animation.TextureAt(frame);
		// Only on a change: a mesh caches its shader, so the texture set on it
		// stays.
		if (texture != showing && resources.ShowShaderTexture(animation.shader, texture))
			showing = texture;
	}
	for (auto& object : _animatedObjects)
	{
		auto& model = *_compositeModels[object.index];
		if (object.loops)
			model.Pose(static_cast<float>(_animationTime) * model.GetAnimationFrameRate(), true);
		else if (object.startedAt >= 0.0)
		{
			// Played once through, then back to where it stood -- or, broken,
			// left as it ends.
			const float frame = static_cast<float>(_animationTime - object.startedAt) * model.GetAnimationFrameRate();
			if (frame >= model.GetAnimationFrames())
			{
				object.startedAt = -1.0;
				model.Pose(object.breaks ? model.GetAnimationFrames() : 0.0f, false);
			}
			else
				model.Pose(frame, false);
		}
	}

	// draws debug shit
	_worldSphere->Update(deltatime);
}

void Level::Draw(Matrix4x4& viewProj, const Vector3& cameraPos)
{
	_worldShader->Bind();
	_worldShader->SetUniformValue("viewProj", viewProj);
	_worldShader->SetUniformValue("fogDensity", 0.0004f);
	_worldShader->SetUniformValue("fogColor", Vector4(0.62f, 0.78f, 1.0f, 1.0f));
	_worldShader->SetUniformValue("cameraPos", cameraPos);
	_worldShader->SetUniformValue("lightmapIntensity", 0.0f);
	_worldShader->SetUniformValue("lightmapTex", 1);

	glDisable(GL_DEPTH_TEST);

	if (_worldSphere != nullptr)
	{
		glDisable(GL_BLEND);
		_worldSphere->Draw(*_worldShader, viewProj, true);
		glEnable(GL_BLEND);
		_worldSphere->Draw(*_worldShader, viewProj, false);
	}

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	// Everything, or only the one file's share of each list (SetDrawOnly).
	const auto only = _drawRanges.find(_drawOnly);
	const bool restricted = !_drawOnly.empty() && only != _drawRanges.end();
	const auto shown = [&](std::size_t (DrawRange::*range)[2], std::size_t index) {
		if (!restricted)
			return true;
		const std::size_t(&bounds)[2] = only->second.*range;
		return index >= bounds[0] && index < bounds[1];
	};

	for (std::size_t i = 0; i < _entities.size(); ++i)
		if (shown(&DrawRange::entities, i))
			_entities[i]->Draw(*_worldShader, true);

	for (std::size_t i = 0; i < _compositeModels.size(); ++i)
		if (shown(&DrawRange::composites, i))
			_compositeModels[i]->Draw(*_worldShader, viewProj, _compositeModels[i]->GetTransform(), true);

	_billboardBatchShader->Bind();
	_billboardBatchShader->SetUniformValue("viewProj", viewProj);
	for (std::size_t i = 0; i < _billboardBatches.size(); ++i)
		if (shown(&DrawRange::billboards, i))
			_billboardBatches[i]->Draw(*_billboardBatchShader, true);

	_worldInstancedShader->Bind();
	_worldInstancedShader->SetUniformValue("viewProj", viewProj);

	for (std::size_t i = 0; i < _instances.size(); ++i)
		if (shown(&DrawRange::instances, i))
			_instances[i]->Draw(*_worldInstancedShader, true);

	glEnable(GL_BLEND);

	_worldShader->Bind();
	_worldShader->SetUniformValue("viewProj", viewProj);

	for (std::size_t i = 0; i < _entities.size(); ++i)
		if (shown(&DrawRange::entities, i))
			_entities[i]->Draw(*_worldShader, false);

	for (std::size_t i = 0; i < _compositeModels.size(); ++i)
		if (shown(&DrawRange::composites, i))
			_compositeModels[i]->Draw(*_worldShader, viewProj, _compositeModels[i]->GetTransform(), false);

	_billboardBatchShader->Bind();
	_billboardBatchShader->SetUniformValue("viewProj", viewProj);
	for (std::size_t i = 0; i < _billboardBatches.size(); ++i)
		if (shown(&DrawRange::billboards, i))
			_billboardBatches[i]->Draw(*_billboardBatchShader, false);

	_worldInstancedShader->Bind();
	_worldInstancedShader->SetUniformValue("viewProj", viewProj);

	for (std::size_t i = 0; i < _instances.size(); ++i)
		if (shown(&DrawRange::instances, i))
			_instances[i]->Draw(*_worldInstancedShader, false);
}

} // namespace Donut
