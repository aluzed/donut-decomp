#include "WaspManager.h"

#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Core/Math/Quaternion.h"
#include "Core/MemoryStream.h"
#include "Game.h"
#include "Level.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DFile.h"
#include "Render/CompositeModel.h"

#include <cmath>
#include <fstream>
#include <set>

namespace Donut
{

namespace
{
std::string readLPString(MemoryStream& stream)
{
	std::string text = stream.ReadLPString();
	return text.substr(0, text.find('\0'));
}

// One object of the effects file, by name: its drawable, its skeleton, the
// meshes its props name, and its pose animation (PTRN_<name>).
class CompositeModel_Named: public ICompositeModel
{
public:
	// `shaders`: also load its shaders (once is enough).
	CompositeModel_Named(const P3D::P3DChunk& root, const std::string& name, bool shaders)
	{
		std::set<std::string> meshes;
		for (const auto& chunk : root.GetChildren())
			if (chunk->IsType(P3D::ChunkType::CompositeDrawable))
			{
				auto drawable = P3D::CompositeDrawable::Load(*chunk);
				if (drawable->GetName() != name)
					continue;
				if (const auto& props = drawable->GetPropList())
					for (const auto& prop : props->GetProps())
						meshes.insert(prop->GetName());
				_drawables.push_back(std::move(drawable));
			}
		for (const auto& chunk : root.GetChildren())
		{
			if (chunk->IsType(P3D::ChunkType::Skeleton))
			{
				auto skeleton = P3D::Skeleton::Load(*chunk);
				if (skeleton->GetName() == name)
					_skeletons.push_back(std::move(skeleton));
			}
			else if (chunk->IsType(P3D::ChunkType::Geometry))
			{
				auto mesh = P3D::Geometry::Load(*chunk);
				if (meshes.count(mesh->GetName()))
					_meshes.push_back(std::move(mesh));
			}
			else if (chunk->IsType(P3D::ChunkType::Animation))
			{
				auto animation = P3D::Animation::Load(*chunk);
				if (animation->GetName() == "PTRN_" + name)
					_animations.push_back(std::move(animation));
			}
		}
		// Only the shaders its meshes draw with: others of the file could take
		// the names of the world's.
		std::set<std::string> used;
		for (const auto& mesh : _meshes)
			for (const auto& group : mesh->GetPrimitiveGroups())
				used.insert(group->GetShaderName());
		for (const auto& chunk : root.GetChildren())
			if (shaders && chunk->IsType(P3D::ChunkType::Shader))
			{
				auto shader = P3D::Shader::Load(*chunk);
				if (used.count(shader->GetName()))
					_shaders.push_back(std::move(shader));
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
	std::vector<std::unique_ptr<P3D::Shader>> _shaders;
	// Loaded with the level from the same file (Game::loadLevel).
	std::vector<std::unique_ptr<P3D::Texture>> _textures;
	std::vector<std::unique_ptr<P3D::Animation>> _animations;
};

constexpr int kHovering = 1;
constexpr int kAlerted = 3;
constexpr int kDestroyed = 5;
constexpr float kAlertDistance = 10.0f;
constexpr int kCoins = 20;
} // namespace

WaspManager::WaspManager(int level, const std::string& levelScript, const Level& world)
{
	const std::string fx = fmt::format("art/l{:02}_fx.p3d", level);
	if (!FileSystem::exists(fx))
		return;
	_effects = std::make_unique<P3D::P3DFile>(fx);
	const auto& root = _effects->GetRoot();
	for (const auto& chunk : root.GetChildren())
		if (chunk->IsType(P3D::ChunkType::StaticPropData))
		{
			MemoryStream stream(chunk->GetData());
			stream.Read<uint32_t>();
			if (readLPString(stream) == "beecamera")
				readStates(*chunk);
		}
	if (_states.size() <= kDestroyed)
	{
		Log::Warn("WaspManager: no beecamera states in {}", fx);
		return;
	}

	// AddSpawnPointByLocatorScript("w_lemon", "beecamera", "Shelley", "w_lemon", ...).
	std::set<std::string> destroyed;
	{
		std::ifstream saved("donut_wasps.dat");
		for (std::string id; std::getline(saved, id);)
			destroyed.insert(id);
	}
	std::ifstream script(levelScript);
	for (std::string line; std::getline(script, line);)
	{
		const std::size_t at = line.find("AddSpawnPointByLocatorScript(");
		const std::size_t comment = line.find("//");
		if (at == std::string::npos || (comment != std::string::npos && comment < at))
			continue;
		std::vector<std::string> args;
		for (std::size_t open = line.find('"', at); open != std::string::npos; open = line.find('"', open + 1))
		{
			const std::size_t close = line.find('"', open + 1);
			if (close == std::string::npos)
				break;
			args.push_back(line.substr(open + 1, close - open - 1));
			open = close;
		}
		if (args.size() < 4 || args[1] != "beecamera")
			continue;
		const Vector3 home = world.GetLocatorPosition(args[3]);
		if (home == Vector3::Zero)
			continue;
		Wasp wasp;
		wasp.id = fmt::format("L{} {}", level, args[3]);
		wasp.home = home;
		wasp.destroyed = wasp.gone = destroyed.count(wasp.id) > 0;
		if (!wasp.gone)
		{
			wasp.model = std::make_unique<CompositeModel>(CompositeModel_Named(root, "beecamera", _wasps.empty()));
			enter(wasp, kHovering);
			wasp.frame += static_cast<float>(_wasps.size() * 7 % 80);
		}
		_wasps.push_back(std::move(wasp));
	}
	Log::Info("WaspManager: {} wasp cameras, {} destroyed before", _wasps.size(), GetDestroyed());
}

WaspManager::~WaspManager() = default;

void WaspManager::readStates(const P3D::P3DChunk& stateProp)
{
	// Each state (StaticPropStateData): name, auto-transition, the state after,
	// counts, out frame; then per drawable its visibility (name, visible), and
	// per frame controller (name, cyclic, cycles, hold frame, first frame, last
	// frame, speed) -- of which the pose animation's is the one that matters.
	for (const auto& child : stateProp.GetChildren())
	{
		if (!child->IsType(P3D::ChunkType::StaticPropStateData))
			continue;
		MemoryStream stream(child->GetData());
		readLPString(stream);
		const bool autoTransition = stream.Read<uint32_t>() != 0;
		const int next = static_cast<int>(stream.Read<uint32_t>());
		State state;
		state.next = autoTransition ? next : -1;
		for (const auto& part : child->GetChildren())
		{
			MemoryStream data(part->GetData());
			const std::string name = readLPString(data);
			if (part->IsType(P3D::ChunkType::StaticPropVisibilityData))
				state.visible[name] = data.Read<uint32_t>() != 0;
			else if (part->IsType(P3D::ChunkType::StaticPropFrameControllerData) && name.rfind("PTRN_", 0) == 0)
			{
				state.loops = data.Read<uint32_t>() != 0;
				data.Read<uint32_t>();
				data.Read<uint32_t>();
				state.firstFrame = data.Read<float>();
				state.lastFrame = data.Read<float>();
				state.speed = data.Read<float>();
			}
		}
		_states.push_back(std::move(state));
	}
}

void WaspManager::enter(Wasp& wasp, int state)
{
	wasp.state = state;
	wasp.frame = _states[state].firstFrame;
	for (const auto& [prop, visible] : _states[state].visible)
		wasp.model->SetPropVisible(prop, visible);
}

WaspManager::Result WaspManager::Update(double dt, const Vector3& player, bool attacking, float reach)
{
	Result result;
	_time += dt;
	for (std::size_t i = 0; i < _wasps.size(); ++i)
	{
		Wasp& wasp = _wasps[i];
		if (wasp.gone)
			continue;

		// Hovering about its spot, bobbing.
		const Vector3 at = wasp.home + Vector3(0.0f, 0.3f * std::sin(static_cast<float>(_time) * 2.0f + i), 0.0f);
		const Vector3 toPlayer = player - at;
		const float distance = toPlayer.Length();

		if (!wasp.destroyed)
		{
			// Watching the player who comes near (the model faces -Z).
			if (distance < kAlertDistance * 2.0f)
				wasp.yaw = std::atan2(toPlayer.X, toPlayer.Z) + 3.14159265f;
			if (distance < reach && attacking)
			{
				wasp.destroyed = true;
				enter(wasp, kDestroyed);
				result.coins += kCoins;
				++result.destroyed;
				save();
				Log::Info("WaspManager: '{}' destroyed", wasp.id);
			}
			else if (wasp.state == kHovering && distance < kAlertDistance)
				enter(wasp, kAlerted);
			else if (wasp.state == kAlerted && distance > kAlertDistance * 1.5f)
				enter(wasp, kHovering);
		}

		// Its stretch of the animation: round and round, or through and on.
		const State& state = _states[wasp.state];
		wasp.frame += static_cast<float>(dt) * 30.0f * state.speed;
		if (wasp.frame >= state.lastFrame)
		{
			if (state.loops)
				wasp.frame = state.firstFrame + std::fmod(wasp.frame - state.firstFrame,
				                                          std::max(1.0f, state.lastFrame - state.firstFrame));
			else if (wasp.destroyed)
			{
				wasp.gone = true;
				continue;
			}
			else if (state.next >= 0 && state.next < static_cast<int>(_states.size()))
				enter(wasp, state.next);
			else
				wasp.frame = state.lastFrame;
		}
		wasp.model->Pose(wasp.frame, false);
		wasp.model->SetTransform(Matrix4x4::MakeTranslate(at) *
		                         Matrix4x4(Quaternion::MakeFromEuler(Vector3(0.0f, wasp.yaw, 0.0f))));
	}
	return result;
}

void WaspManager::Draw(GL::ShaderProgram& worldShader, const Matrix4x4& viewProj)
{
	for (const bool opaque : {true, false})
		for (const auto& wasp : _wasps)
			if (!wasp.gone)
				wasp.model->Draw(worldShader, viewProj, wasp.model->GetTransform(), opaque);
}

int WaspManager::GetDestroyed() const
{
	int destroyed = 0;
	for (const auto& wasp : _wasps)
		destroyed += wasp.destroyed ? 1 : 0;
	return destroyed;
}

void WaspManager::save() const
{
	if (Game::GetInstance().IsAutopilot())
		return;
	// Every level's: the others' as they were read.
	std::set<std::string> destroyed;
	{
		std::ifstream saved("donut_wasps.dat");
		for (std::string id; std::getline(saved, id);)
			if (!id.empty())
				destroyed.insert(id);
	}
	for (const auto& wasp : _wasps)
		if (wasp.destroyed)
			destroyed.insert(wasp.id);
	std::ofstream file("donut_wasps.dat");
	for (const auto& id : destroyed)
		file << id << "\n";
}

} // namespace Donut
