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
//   gravity (m/s/s, up: debris fall at -2 to -6, flames rise at 3), ALP alpha, CLR colour, TRA the emitter's offset; its generator
//   (GeneratorAnimation): WDT, HGT the area it emits from, HOSP and VESP the
//   spread of directions about straight up;
//   over a particle's life (ParticleAnimation, its frames spanning the life):
//   ALOL alpha (0-255), SZOL size, SPOL speed, as multipliers.
// Spin, weight and drag are not used.
class ParticleFactory
{
public:
	static std::unique_ptr<ParticleFactory> FromChunk(const P3D::P3DChunk& factory);
	// A one-emitter factory for bursts the game sets off (sparks off a crash,
	// dust under a landing): `count` sprites a burst, living `life` seconds,
	// leaving at `speed` m/s within `spread` radians of straight up, `size`
	// metres, falling at `gravity` m/s/s, fading out.
	static std::unique_ptr<ParticleFactory> Burst(const std::string& shader, float life, float speed, float spread,
	                                              float size, float gravity, const Vector4& color);

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
		// How fast a sprite turns, radians a frame (SPI, either way SPIV), times
		// SNOL over its life; and the angle it starts at: at random (RAND), the
		// emitter's own (SPEC), or none.
		Curve spin, spinVariance, spinOverLife;
		bool randomAngle = false;
		float startAngle = 0.0f;
		Vector4 color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		Vector3 offset = Vector3::Zero;
		float width = 0.0f, height = 0.0f, horizontalSpread = 0.0f, verticalSpread = 0.0f;
		// Its texture, a strip of frames side by side (a flame's four): how
		// many, and how a sprite goes through them, at so many a second --
		// at random (RAND), in order (LIN), or not at all, the first (NONE).
		enum class TextureAnimation
		{
			None,
			Random,
			Linear,
		};
		TextureAnimation textureAnimation = TextureAnimation::None;
		uint32_t textureFrames = 1;
		float textureFrameRate = 0.0f;
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
	// `emit` false lets those alive live out their lives, giving off no more.
	void Update(double dt, const Matrix4x4& placement, float timelineFrame = -1.0f, bool emit = true);
	// Gives off `count` sprites at once, at `position`.
	void Burst(int count, const Vector3& position);
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
		// Where it starts in its texture's frames, and what picks the next.
		uint32_t seed = 0;
		// How far it has turned, and how fast, radians a second.
		float angle = 0.0f;
		float spin = 0.0f;
	};
	struct Running
	{
		const ParticleFactory::Emitter* emitter;
		std::vector<Particle> particles;
		float owed = 0.0f;
		// Where its timeline stood at the last step; -1 before the first.
		float lastFrame = -1.0f;
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
