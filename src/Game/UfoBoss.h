#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"
#include "Render/BillboardAnimation.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Donut
{

class BillboardBatch;
class CompositeModel;
class Level;
class WorldPhysics;
namespace GL
{
class ShaderProgram;
}

// Kang and Kodos's saucer over level 7 (the "Planet Express Ship"): the level
// script puts it at a locator (AddFlyingActorByLocator("spaceship", ...,
// "l7_spaceship")), its tractor beam always on (UFO_BEAM_ALWAYS_ON). Four
// missions end on it ("destroyboss"): the player drives the barrel of waste
// they carry into its beam, up it goes and blows up against the hull; the
// fourth brings the saucer down.
class UfoBoss
{
public:
	// None when the level script has no saucer.
	static std::unique_ptr<UfoBoss> FromLevelScript(const std::string& levelScript, const Level& level,
	                                                const WorldPhysics& physics);
	// At `position`, over ground at the height `ground`.
	UfoBoss(const Vector3& position, float ground);
	~UfoBoss();

	void Update(double dt);
	void Draw(GL::ShaderProgram& worldShader, GL::ShaderProgram& billboardShader, const Matrix4x4& viewProj);

	// Whether `position` is in its beam: under the saucer, within the beam's
	// reach of its axis.
	bool InBeam(const Vector3& position) const;
	// A barrel gone up the beam and blown up against it; `last`, it comes
	// down. Where the blast is.
	Vector3 Hit(bool last);
	bool IsDestroyed() const { return _destroyed; }
	const Vector3& GetPosition() const { return _position; }
	Vector3 GetGroundPosition() const { return Vector3(_position.X, _ground, _position.Z); }

private:
	std::unique_ptr<CompositeModel> _saucer;
	std::unique_ptr<CompositeModel> _beam;
	// Their billboards (the beam's light cone, the saucer's glows), each hung
	// from the joint of its name: the model, the prop, the quads.
	struct Glow
	{
		CompositeModel* model;
		std::string name;
		std::unique_ptr<BillboardBatch> batch;
	};
	std::vector<Glow> _glows;
	// Their animations (BQG_*): size, colour, visibility over the frames,
	// round and round.
	std::vector<BillboardAnimation> _glowAnimations;
	void loadGlows(const std::string& file, CompositeModel* model);
	Vector3 _position;
	float _ground;
	double _time = 0.0;
	// Since the last hit: the saucer rocks, then (the last) falls away.
	double _sinceHit = -1.0;
	bool _destroyed = false;
};

} // namespace Donut
