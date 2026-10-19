#include "UfoBoss.h"

#include "Core/Log.h"
#include "Core/Math/Quaternion.h"
#include "Core/FileSystem.h"
#include "Level.h"
#include "Physics/WorldPhysics.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DFile.h"
#include "Render/BillboardBatch.h"
#include "Render/CompositeModel.h"
#include "Render/OpenGL/ShaderProgram.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <regex>

namespace Donut
{

namespace
{
// How far from the beam's axis a car is still in it.
constexpr float kBeamRadius = 6.0f;
// The beam's light cone: its quad comes some 7mm by 30cm, stretched in the game
// by the beam's behaviour from the saucer to the ground; this wide.
constexpr float kBeamWidth = 10.0f;
// The saucer's turn, radians a second, and how far it bobs.
constexpr float kSpin = 0.4f;
constexpr float kBob = 0.6f;
} // namespace

std::unique_ptr<UfoBoss> UfoBoss::FromLevelScript(const std::string& levelScript, const Level& level,
                                                  const WorldPhysics& physics)
{
	// AddFlyingActorByLocator("spaceship","Planet Express Ship","l7_spaceship","NEVER_DESPAWN");
	std::ifstream script(levelScript);
	const std::regex actor(R"re(AddFlyingActorByLocator\s*\(\s*"spaceship"\s*,\s*"[^"]*"\s*,\s*"([^"]+)")re");
	std::string line;
	while (std::getline(script, line))
	{
		std::smatch match;
		if (!std::regex_search(line, match, actor))
			continue;
		const Vector3 at = level.GetLocatorPosition(match[1].str());
		if (at == Vector3::Zero)
		{
			Log::Warn("UfoBoss: no locator '{}'", match[1].str());
			return nullptr;
		}
		float ground = at.Y - 20.0f;
		physics.FindGroundHeight(at, -1.0f, 200.0f, ground, true);
		Log::Info("UfoBoss: the saucer at '{}' ({:.0f}, {:.0f}, {:.0f}), {:.0f}m up", match[1].str(), at.X, at.Y, at.Z,
		          at.Y - ground);
		return std::make_unique<UfoBoss>(at, ground);
	}
	return nullptr;
}

UfoBoss::UfoBoss(const Vector3& position, float ground): _position(position), _ground(ground)
{
	_saucer = CompositeModel::LoadP3D("art/missions/level07/UFO.p3d");
	_beam = CompositeModel::LoadP3D("art/missions/level07/ufobeam.p3d");
	loadGlows("art/missions/level07/UFO.p3d", _saucer.get());
	loadGlows("art/missions/level07/ufobeam.p3d", _beam.get());
}

void UfoBoss::loadGlows(const std::string& file, CompositeModel* model)
{
	if (!model || !FileSystem::exists(file))
		return;
	const P3D::P3DFile p3d(file);
	for (const auto& chunk : p3d.GetRoot().GetChildren())
	{
		if (chunk->IsType(P3D::ChunkType::Animation))
			if (auto animation = BillboardAnimation::FromChunk(*chunk))
				_glowAnimations.push_back(std::move(*animation));
		if (!chunk->IsType(P3D::ChunkType::BillboardQuadGroup))
			continue;
		const auto group = P3D::BillboardQuadGroup::Load(*chunk);
		_glows.push_back({model, group->GetName(), std::make_unique<BillboardBatch>(*group)});
	}
}

UfoBoss::~UfoBoss() = default;

void UfoBoss::Update(double dt)
{
	_time += dt;
	if (_sinceHit >= 0.0)
		_sinceHit += dt;

	Vector3 at = _position + Vector3(0.0f, kBob * std::sin(static_cast<float>(_time) * 1.3f), 0.0f);
	float tilt = 0.0f;
	if (_sinceHit >= 0.0 && _sinceHit < 2.0)
		// Rocked by the blast, settling.
		tilt = 0.25f * std::exp(-2.0f * static_cast<float>(_sinceHit)) * std::sin(static_cast<float>(_sinceHit) * 12.0f);
	if (_destroyed && _sinceHit >= 0.0)
	{
		// Down it goes, tipping, out of sight.
		const float t = static_cast<float>(_sinceHit);
		at.Y -= 4.0f * t * t;
		tilt = std::min(1.2f, 0.4f * t);
	}
	const Quaternion turn = Quaternion(Vector3(0.0f, 1.0f, 0.0f), static_cast<float>(_time) * kSpin);
	const Quaternion tip = Quaternion(Vector3(1.0f, 0.0f, 0.0f), tilt);
	const Matrix4x4 placement = Matrix4x4::MakeTranslate(at) * Matrix4x4(tip * turn);
	if (_saucer)
	{
		_saucer->SetTransform(placement);
		if (_saucer->IsAnimated())
			_saucer->Pose(static_cast<float>(_time) * _saucer->GetAnimationFrameRate(), true);
	}
	if (_beam)
	{
		_beam->SetTransform(Matrix4x4::MakeTranslate(at) * Matrix4x4(turn));
		if (_beam->IsAnimated())
			_beam->Pose(static_cast<float>(_time) * _beam->GetAnimationFrameRate(), true);
		_beam->SetVisible(!_destroyed);
	}
	if (_saucer && _destroyed && _sinceHit > 4.0)
		_saucer->SetVisible(false);
}

void UfoBoss::Draw(GL::ShaderProgram& worldShader, GL::ShaderProgram& billboardShader, const Matrix4x4& viewProj)
{
	for (const bool opaque : {true, false})
	{
		if (_saucer)
			_saucer->Draw(worldShader, viewProj, _saucer->GetTransform(), opaque);
		if (_beam)
			_beam->Draw(worldShader, viewProj, _beam->GetTransform(), opaque);
	}

	// The glows, where their joints are. The blending they set is put back
	// after, as the HUD drawn next relies on it.
	const GLboolean blending = glIsEnabled(GL_BLEND);
	GLint source = GL_SRC_ALPHA, destination = GL_ONE_MINUS_SRC_ALPHA;
	glGetIntegerv(GL_BLEND_SRC_RGB, &source);
	glGetIntegerv(GL_BLEND_DST_RGB, &destination);
	billboardShader.Bind();
	for (const bool opaque : {true, false})
	{
		if (!opaque)
			glEnable(GL_BLEND);
		for (const auto& glow : _glows)
		{
			if (!glow.model->IsVisible())
				continue;
			bool hidden = false;
			for (const auto& animation : _glowAnimations)
			{
				const float frames = std::max(1.0f, animation.frames);
				const float frame = std::fmod(static_cast<float>(_time) * animation.frameRate, frames);
				for (const auto& quad : animation.quads)
				{
					const int index = glow.batch->FindQuad(quad.name);
					if (index < 0)
						continue;
					hidden = hidden || !quad.VisibleAt(frame);
					glow.batch->SetQuadSize(index, quad.SizeAt(frame, glow.batch->GetQuadSize(index)));
					if (const auto color = quad.ColorAt(frame))
						glow.batch->SetQuadColor(index, *color);
				}
			}
			if (hidden)
				continue;
			const int joint = glow.model->FindPropJoint(glow.name);
			Matrix4x4 at = joint >= 0 ? glow.model->GetTransform() * glow.model->GetJointTransform(joint)
			                          : glow.model->GetTransform();
			if (glow.name == "beamShape")
			{
				// From the saucer down to the ground, centred between them.
				const float top = glow.model->GetTransform().Translation().Y;
				const float height = std::max(1.0f, top - _ground);
				glow.batch->SetQuadSize(0, Vector2(kBeamWidth, height));
				at = Matrix4x4::MakeTranslate(Vector3(_position.X, top - height * 0.5f, _position.Z));
			}
			billboardShader.SetUniformValue("viewProj", viewProj * at);
			glow.batch->Draw(billboardShader, opaque);
		}
	}
	glBlendFunc(source, destination);
	if (!blending)
		glDisable(GL_BLEND);
}

bool UfoBoss::InBeam(const Vector3& position) const
{
	if (_destroyed)
		return false;
	const Vector3 offset = position - _position;
	return offset.Y < 0.0f && offset.X * offset.X + offset.Z * offset.Z < kBeamRadius * kBeamRadius;
}

Vector3 UfoBoss::Hit(bool last)
{
	_sinceHit = 0.0;
	_destroyed = _destroyed || last;
	Log::Info("UfoBoss: hit{}", last ? ", and down it goes" : "");
	return _position;
}

} // namespace Donut
