#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"

#include <map>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace Donut
{

namespace P3D
{
class P3DChunk;
}
namespace GL
{
class ShaderProgram;
class VertexBuffer;
class IndexBuffer;
class VertexBinding;
} // namespace GL

// A particle system factory (ParticleSystemFactory, 0x15800): the steam over a
// vent, the smoke of a tire fire. Each of its sprite emitters gives off
// textured sprites that rise, slow, grow and fade over their lives.
//
// Read from the emitter's animations, each channel a constant or a curve:
//   per emitter (EmitterAnimation): NUM particles alive at most, LIF life in
//   frames (LIFV either way), SPD speed per frame (SPDV), SIZ size (SIZV), GRA
//   gravity, ALP alpha, CLR colour, TRA the emitter's offset; its generator
//   (GeneratorAnimation): WDT, HGT the area it emits from, HOSP and VESP the
//   spread of directions about straight up;
//   over a particle's life (ParticleAnimation, its frames spanning the life):
//   ALOL alpha (0-255), SZOL size, SPOL speed, as multipliers.
// Spin, weight and drag are not used.
class ParticleFactory
{
public:
	static std::unique_ptr<ParticleFactory> FromChunk(const P3D::P3DChunk& factory);

	struct Curve
	{
		std::vector<std::pair<float, float>> keys;
		float At(float frame, float otherwise) const;
	};

	struct Emitter
	{
		std::string shader;
		float frameRate = 30.0f;
		// Over a particle's life.
		float lifeFrames = 20.0f;
		Curve alphaOverLife, sizeOverLife, speedOverLife;
		// Colour over a particle's life (COOL), multiplying its colour.
		std::vector<std::pair<float, Vector4>> colorOverLife;
		// The emission rate over the emitter's timeline (RAT), a multiplier:
		// an explosion's debris come in one burst. The timeline's length.
		Curve rate;
		float timelineFrames = 0.0f;
		// The emitter's.
		Curve count, life, lifeVariance, speed, speedVariance, size, sizeVariance, gravity, alpha;
		Vector4 color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		Vector3 offset = Vector3::Zero;
		float width = 0.0f, height = 0.0f, horizontalSpread = 0.0f, verticalSpread = 0.0f;
	};

	const std::string& GetName() const { return _name; }
	const std::vector<Emitter>& GetEmitters() const { return _emitters; }

private:
	std::string _name;
	std::vector<Emitter> _emitters;
};

// One running instance of a factory, placed in the world.
class ParticleSystem
{
public:
	explicit ParticleSystem(const ParticleFactory& factory);
	~ParticleSystem();

	// Moves the particles on, giving off new ones where `placement` puts the
	// system.
	// `timelineFrame`: where the emitters' timeline stands, for an effect keyed
	// to its owner's animation (a wasp's, whose explosion is frames 375-435);
	// < 0 runs it on its own clock, round and round.
	void Update(double dt, const Matrix4x4& placement, float timelineFrame = -1.0f);
	// With the billboard program, its viewProj already set.
	void Draw(GL::ShaderProgram& billboardShader);

private:
	struct Particle
	{
		Vector3 position;
		Vector3 velocity;
		float age;
		float life;
		float size;
	};
	struct Running
	{
		const ParticleFactory::Emitter* emitter;
		std::vector<Particle> particles;
		float owed = 0.0f;
		std::shared_ptr<GL::VertexBuffer> instances;
		std::shared_ptr<GL::VertexBinding> binding;
		std::size_t capacity = 0;
	};

	const ParticleFactory& _factory;
	std::vector<Running> _running;
	std::shared_ptr<GL::VertexBuffer> _quad;
	std::shared_ptr<GL::IndexBuffer> _indices;
	std::mt19937 _random;
	double _age = 0.0;
};

} // namespace Donut
