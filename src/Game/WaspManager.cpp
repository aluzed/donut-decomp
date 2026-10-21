#include "WaspManager.h"

#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Core/Math/Quaternion.h"
#include "Core/MemoryStream.h"
#include "Game.h"
#include "Level.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DFile.h"
#include "P3D/StateProp.h"
#include "Render/BillboardBatch.h"
#include "Render/CompositeModel.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/ParticleSystem.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
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
constexpr int kFiring = 4;
constexpr int kDestroyed = 5;
constexpr float kAlertDistance = 10.0f;
constexpr int kCoins = 20;
const char* const kBuzz = "sound\\soundfx\\wasp\\wasp_idle_03.rsd";
const char* const kFire = "sound\\soundfx\\wasp\\wasp_laser.rsd";
const char* const kHit = "sound\\soundfx\\wasp\\character_zapped_02.rsd";
const char* const kMiss = "sound\\soundfx\\wasp\\laser_miss.rsd";
const char* const kBlownUp = "sound\\soundfx\\wasp\\wasp_destroyed.rsd";
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
	// The shot's billboards, and the wasp's own (its lens, its signal), with
	// the shaders they draw with.
	std::set<std::string> glowShaders;
	for (const auto& chunk : root.GetChildren())
		if (chunk->IsType(P3D::ChunkType::BillboardQuadGroup))
		{
			auto group = P3D::BillboardQuadGroup::Load(*chunk);
			const std::string name = group->GetName();
			if (name.rfind("waspray_bb", 0) == 0)
				_shotSprites.push_back(std::make_unique<BillboardBatch>(*group));
			else if (name.rfind("wasp_bb", 0) == 0 || name == "lenseShape" || name == "signal_bbShape" ||
			         name.rfind("shockwave", 0) == 0)
				_glows.emplace_back(name, std::make_unique<BillboardBatch>(*group));
			else
				continue;
			glowShaders.insert(group->GetShader());
		}
	for (const auto& chunk : root.GetChildren())
		if (chunk->IsType(P3D::ChunkType::Animation))
			if (auto animation = BillboardAnimation::FromChunk(*chunk))
				for (auto& quad : animation->quads)
					_glowAnimations.push_back(std::move(quad));
	auto& resources = Game::GetInstance().GetResourceManager();
	for (const auto& chunk : root.GetChildren())
		if (chunk->IsType(P3D::ChunkType::Shader))
		{
			auto shader = P3D::Shader::Load(*chunk);
			if (glowShaders.count(shader->GetName()) && !resources.GetShader(shader->GetName()))
				resources.LoadShader(*shader);
		}
	// Its particle effects: the factories, the systems naming them, and the
	// beecamera's effect list (each effect: name, translucent, joint).
	std::map<std::string, std::string> systems;
	for (const auto& chunk : root.GetChildren())
	{
		if (chunk->IsType(P3D::ChunkType::ParticleSystemFactory))
			if (auto factory = ParticleFactory::FromChunk(*chunk); !factory->GetEmitters().empty())
				_factories.try_emplace(factory->GetName(), std::move(factory));
		if (chunk->IsType(P3D::ChunkType::ParticleSystem))
		{
			MemoryStream stream(chunk->GetData());
			stream.Read<uint32_t>();
			const std::string name = readLPString(stream);
			systems[name] = readLPString(stream);
		}
	}
	for (const auto& chunk : root.GetChildren())
	{
		if (!chunk->IsType(P3D::ChunkType::CompositeDrawable))
			continue;
		MemoryStream header(chunk->GetData());
		if (readLPString(header) != "beecamera")
			continue;
		for (const auto& list : chunk->GetChildren())
			if (list->IsType(P3D::ChunkType::CompositeDrawableEffectList))
				for (const auto& effect : list->GetChildren())
				{
					MemoryStream stream(effect->GetData());
					const std::string name = readLPString(stream);
					stream.Read<uint32_t>();
					const int joint = static_cast<int>(stream.Read<uint32_t>());
					if (const auto system = systems.find(name); system != systems.end() && _factories.count(system->second))
						_effectSlots.push_back({name, joint, system->second});
				}
	}

	if (_states.size() <= kDestroyed)
	{
		Log::Warn("WaspManager: no beecamera states in {}", fx);
		return;
	}

	readBehaviours(levelScript);

	// AddSpawnPointByLocatorScript("w_lemon", "beecamera", "Shelley", "w_lemon", ...).
	const std::set<std::string>& destroyed = Game::GetInstance().GetSave().Entries("wasp");
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
		if (args.size() >= 5)
			wasp.spawnRadius = std::max(5.0f, static_cast<float>(std::atof(args[4].c_str())));
		wasp.home = home;
		wasp.position = home;
		// Its own behaviour, by its spawn point's name, or the class's.
		const auto own = _behaviours.find(args[0]);
		wasp.behaviour = own != _behaviours.end() ? own->second : _behaviours["beecamera"];
		wasp.destroyed = wasp.gone = destroyed.count(wasp.id) > 0;
		if (!wasp.gone)
		{
			wasp.model = std::make_unique<CompositeModel>(CompositeModel_Named(root, "beecamera", _wasps.empty()));
			for (const auto& slot : _effectSlots)
				wasp.effects.push_back({slot.name, slot.joint, std::make_unique<ParticleSystem>(*_factories[slot.factory])});
			enter(wasp, kHovering);
		}
		_wasps.push_back(std::move(wasp));
	}
	Log::Info("WaspManager: {} wasp cameras, {} destroyed before", _wasps.size(), GetDestroyed());
}

WaspManager::~WaspManager() = default;

void WaspManager::readBehaviours(const std::string& levelScript)
{
	// AddBehaviour("w_simpsons", "ATTRACTION", "2.0", "20", "-1"),
	// AddBehaviour("beecamera", "ATTACK_PLAYER", "10.0", "5.0", "10.0"),
	// SetActorRotationSpeed("beecamera", "40.0"), SetProjectileStats("waspray", "60.0", "5").
	std::ifstream script(levelScript);
	for (std::string line; std::getline(script, line);)
	{
		const std::size_t comment = line.find("//");
		const std::size_t open = line.find('(');
		if (open == std::string::npos || (comment != std::string::npos && comment < open))
			continue;
		std::vector<std::string> args;
		for (std::size_t q = line.find('"', open); q != std::string::npos; q = line.find('"', q + 1))
		{
			const std::size_t close = line.find('"', q + 1);
			if (close == std::string::npos)
				break;
			args.push_back(line.substr(q + 1, close - q - 1));
			q = close;
		}
		const auto number = [&](std::size_t i, float otherwise) {
			return i < args.size() ? static_cast<float>(std::atof(args[i].c_str())) : otherwise;
		};
		if (line.find("AddBehaviour") != std::string::npos && args.size() >= 5)
		{
			// A spawn point's own behaviour starts from the class's.
			auto found = _behaviours.find(args[0]);
			if (found == _behaviours.end())
				found = _behaviours.emplace(args[0], _behaviours["beecamera"]).first;
			Behaviour& b = found->second;
			if (args[1] == "ATTRACTION")
			{
				b.nearest = number(2, b.nearest);
				b.farthest = number(3, b.farthest);
				b.speed = number(4, b.speed);
			}
			else if (args[1] == "EVADE_PLAYER" && args.size() >= 7)
			{
				b.evadeNear = number(2, 1.0f);
				b.evadeFar = number(3, 5.0f);
				b.evadeLow = number(4, 1.0f);
				b.evadeHigh = number(5, 2.0f);
				b.evadeSpeed = number(6, 5.0f);
			}
			else if (args[1] == "ATTACK_PLAYER")
			{
				b.range = number(2, b.range);
				b.arc = number(3, b.arc);
				b.period = number(4, b.period);
			}
		}
		else if (line.find("SetActorRotationSpeed") != std::string::npos && args.size() >= 2)
			_turnRate = number(1, 85.0f) * 3.14159265f / 180.0f;
		else if (line.find("SetProjectileStats") != std::string::npos && args.size() >= 3)
		{
			_shotSpeed = number(1, 60.0f) / 3.6f;
			_shotDamage = number(2, _shotDamage);
		}
	}
}

void WaspManager::readStates(const P3D::P3DChunk& stateProp)
{
	// Each state: what it shows, the state after when it goes on by itself,
	// and its pose animation's frames (PTRN_), the one controller that
	// matters here.
	for (const auto& read : P3D::StateProp::Load(stateProp).states)
	{
		State state;
		state.next = read.autoTransition ? static_cast<int>(read.outState) : -1;
		for (const auto& shown : read.visibility)
			state.visible[shown.drawable] = shown.visible;
		for (const auto& controller : read.controllers)
			if (controller.name.rfind("PTRN_", 0) == 0)
			{
				state.loops = controller.cyclic;
				state.firstFrame = controller.firstFrame;
				state.lastFrame = controller.lastFrame;
				state.speed = controller.speed;
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

		// Coming in as the player arrives (state 0, which goes on to hovering
		// by itself: a fade-in, its sparkles and a shockwave); gone again,
		// unharmed, once they are well away.
		const float fromHome = (player - wasp.home).Length();
		if (!wasp.present)
		{
			if (wasp.destroyed || fromHome > wasp.spawnRadius)
				continue;
			wasp.present = true;
			wasp.position = wasp.home;
			wasp.evadeTo = wasp.home;
			enter(wasp, 0);
		}
		else if (!wasp.destroyed && fromHome > wasp.spawnRadius * 1.5f + 10.0f)
		{
			wasp.present = false;
			continue;
		}

		const Behaviour& b = wasp.behaviour;
		const Vector3 toPlayer = player - wasp.position;
		const float distance = toPlayer.Length();
		const float flat = std::sqrt(toPlayer.X * toPlayer.X + toPlayer.Z * toPlayer.Z);

		if (!wasp.destroyed)
		{
			// Turning to the player who comes near, at its own rate (the model
			// faces -Z).
			if (distance < b.farthest * 2.0f)
			{
				float delta = std::atan2(toPlayer.X, toPlayer.Z) + 3.14159265f - wasp.yaw;
				while (delta > 3.14159265f) delta -= 6.28318531f;
				while (delta < -3.14159265f) delta += 6.28318531f;
				const float turn = _turnRate * static_cast<float>(dt);
				wasp.yaw += std::clamp(delta, -turn, turn);
			}
			// Drawn to the player between its nearest and farthest, not too far
			// from home; back home otherwise. -1 never moves.
			// Alerted, it dodges about its spot now and then (EVADE_PLAYER),
			// rather than following the player.
			const bool evading = b.evadeSpeed > 0.0f && wasp.state == kAlerted;
			if (evading)
			{
				if (_time >= wasp.nextEvade)
				{
					static std::mt19937 random(std::random_device {}());
					std::uniform_real_distribution<float> unit(0.0f, 1.0f);
					const float angle = unit(random) * 6.28318531f;
					const float out = b.evadeNear + unit(random) * (b.evadeFar - b.evadeNear);
					const float up = b.evadeLow + unit(random) * (b.evadeHigh - b.evadeLow);
					wasp.evadeTo = wasp.home + Vector3(std::sin(angle) * out, up, std::cos(angle) * out);
					wasp.nextEvade = _time + (b.period > 0.0f ? b.period : 3.0f);
				}
				const Vector3 way = wasp.evadeTo - wasp.position;
				const float step = b.evadeSpeed / 3.6f * static_cast<float>(dt);
				if (way.Length() > step)
					wasp.position += way.Normalized() * step;
				else
					wasp.position = wasp.evadeTo;
			}
			else if (b.speed > 0.0f)
			{
				const float step = b.speed / 3.6f * static_cast<float>(dt);
				Vector3 goal = wasp.home;
				if (flat > b.nearest && distance < b.farthest * 2.0f && (player - wasp.home).Length() < 25.0f)
					goal = Vector3(player.X, wasp.home.Y + std::max(0.0f, player.Y - wasp.home.Y), player.Z);
				Vector3 way = goal - wasp.position;
				if (flat <= b.nearest && goal != wasp.home)
					way = Vector3::Zero;
				if (way.Length() > step)
					wasp.position += way.Normalized() * step;
			}

			if (distance < reach && attacking)
			{
				wasp.destroyed = true;
				enter(wasp, kDestroyed);
				result.coins += kCoins;
				++result.destroyed;
				save();
				Log::Info("WaspManager: '{}' destroyed", wasp.id);
				result.sounds.emplace_back(kBlownUp, wasp.position);
			}
			else if (wasp.state == kHovering && distance < kAlertDistance)
				enter(wasp, kAlerted);
			else if (wasp.state == kAlerted && distance > kAlertDistance * 1.5f)
				enter(wasp, kHovering);
			// Firing: in range, the player within its arc (a few degrees either
			// way, widened a little: it turns slower than a player walks), once a
			// period.
			else if (wasp.state == kAlerted && distance < b.range && _time >= wasp.nextShot)
			{
				float off = std::atan2(toPlayer.X, toPlayer.Z) + 3.14159265f - wasp.yaw;
				while (off > 3.14159265f) off -= 6.28318531f;
				while (off < -3.14159265f) off += 6.28318531f;
				if (std::abs(off) < (b.arc + 10.0f) * 3.14159265f / 180.0f)
				{
					enter(wasp, kFiring);
					wasp.nextShot = _time + (b.period > 0.0f ? b.period : 3.0f);
					// At where the player is now, chest high: a player who keeps
					// moving is missed.
					const Vector3 aim = player + Vector3(0.0f, 0.8f, 0.0f) - wasp.position;
					_shots.push_back({wasp.position, aim.Normalized() * _shotSpeed, 0.0f, b.range + 5.0f});
					result.sounds.emplace_back(kFire, wasp.position);
					Log::Info("WaspManager: '{}' fires at the player, {:.0f}m", wasp.id, distance);
				}
			}
		}

		// Buzzing, heard near.
		if (!wasp.destroyed && distance < 20.0f && _time >= wasp.nextBuzz)
		{
			result.sounds.emplace_back(kBuzz, wasp.position);
			wasp.nextBuzz = _time + 2.0;
		}

		// Hovering, bobbing.
		const Vector3 at = wasp.position + Vector3(0.0f, 0.3f * std::sin(static_cast<float>(_time) * 2.0f + i), 0.0f);

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
		for (auto& effect : wasp.effects)
			effect.system->Update(dt, wasp.model->GetTransform() * wasp.model->GetJointTransform(effect.joint),
			                      wasp.frame);
	}
	// The shots fly on; one that reaches the player hits.
	for (auto& shot : _shots)
	{
		const Vector3 step = shot.velocity * static_cast<float>(dt);
		shot.position += step;
		shot.travelled += step.Length();
		if ((shot.position - (player + Vector3(0.0f, 0.8f, 0.0f))).Length() < 1.2f)
		{
			result.damage += _shotDamage;
			result.sounds.emplace_back(kHit, shot.position);
			shot.travelled = shot.range;
		}
		else if (shot.travelled >= shot.range)
			result.sounds.emplace_back(kMiss, shot.position);
	}
	_shots.erase(std::remove_if(_shots.begin(), _shots.end(), [](const Shot& s) { return s.travelled >= s.range; }),
	             _shots.end());
	return result;
}

void WaspManager::Draw(GL::ShaderProgram& worldShader, GL::ShaderProgram& billboardShader, const Matrix4x4& viewProj)
{
	for (const bool opaque : {true, false})
		for (const auto& wasp : _wasps)
			if (!wasp.gone && wasp.present)
				wasp.model->Draw(worldShader, viewProj, wasp.model->GetTransform(), opaque);

	// The glows where their joints are, the shots where they fly. The blending
	// they set is put back after: the HUD drawn next relies on it, and left
	// additive (or off) the whole frame washed white (or its text came out as
	// solid boxes).
	const GLboolean blending = glIsEnabled(GL_BLEND);
	GLint source = GL_SRC_ALPHA, destination = GL_ONE_MINUS_SRC_ALPHA;
	glGetIntegerv(GL_BLEND_SRC_RGB, &source);
	glGetIntegerv(GL_BLEND_DST_RGB, &destination);
	billboardShader.Bind();
	for (const bool opaque : {true, false})
	{
		if (!opaque)
			glEnable(GL_BLEND);
		for (const auto& wasp : _wasps)
		{
			if (wasp.gone || !wasp.present)
				continue;
			const auto& visible = _states[wasp.state].visible;
			for (const auto& [name, glow] : _glows)
			{
				// As the state shows it, and as its animation has it at the
				// wasp's frame: the signal ring pulses, the shockwave grows and
				// fades as the wasp comes in.
				const int joint = wasp.model->FindPropJoint(name);
				const auto shown = visible.find(name);
				if (joint < 0 || (shown != visible.end() && !shown->second))
					continue;
				bool hidden = false;
				for (const auto& animation : _glowAnimations)
				{
					const int quad = glow->FindQuad(animation.name);
					if (quad < 0)
						continue;
					hidden = hidden || !animation.VisibleAt(wasp.frame);
					glow->SetQuadSize(quad, animation.SizeAt(wasp.frame, glow->GetQuadSize(quad)));
					if (const auto color = animation.ColorAt(wasp.frame))
						glow->SetQuadColor(quad, *color);
				}
				if (hidden)
					continue;
				billboardShader.SetUniformValue(
				    "viewProj", viewProj * wasp.model->GetTransform() * wasp.model->GetJointTransform(joint));
				glow->Draw(billboardShader, opaque);
			}
		}
		for (const auto& shot : _shots)
		{
			billboardShader.SetUniformValue("viewProj", viewProj * Matrix4x4::MakeTranslate(shot.position));
			for (const auto& sprite : _shotSprites)
				sprite->Draw(billboardShader, opaque);
		}
	}
	// Their particle effects, those their state shows.
	billboardShader.SetUniformValue("viewProj", viewProj);
	for (const auto& wasp : _wasps)
	{
		if (wasp.gone || !wasp.present)
			continue;
		const auto& visible = _states[wasp.state].visible;
		for (const auto& effect : wasp.effects)
			if (const auto shown = visible.find(effect.name); shown == visible.end() || shown->second)
				effect.system->Draw(billboardShader);
	}
	glBlendFunc(source, destination);
	if (blending)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}

bool WaspManager::AnyNear(const Vector3& position, float radius) const
{
	for (const auto& wasp : _wasps)
		if (wasp.present && !wasp.destroyed && !wasp.gone && (wasp.position - position).Length() < radius)
			return true;
	return false;
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
	// Every level's: this level's added to the others'.
	auto& save = Game::GetInstance().GetSave();
	for (const auto& wasp : _wasps)
		if (wasp.destroyed)
			save.Add("wasp", wasp.id);
	save.Save();
}

} // namespace Donut
