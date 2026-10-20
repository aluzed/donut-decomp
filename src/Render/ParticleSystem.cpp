#include "ParticleSystem.h"

#include "Core/Log.h"
#include "Core/Math/Vector2.h"
#include "Core/MemoryStream.h"
#include "Game.h"
#include "P3D/P3DChunk.h"
#include "Render/OpenGL/IndexBuffer.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/VertexBinding.h"
#include "Render/OpenGL/VertexBuffer.h"
#include "Render/Shader.h"
#include "ResourceManager.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Donut
{

namespace
{
// As BillboardBatch lays a quad out: where, how big, what colour, its corners'
// texture coordinates.
struct SpriteInstance
{
	Vector3 pos;
	Vector2 size;
	Vector4 color;
	Vector2 uv0, uv1, uv2, uv3;
	// Turned about its middle, radians.
	float angle;
};

const std::array<Vector2, 4> kCorners = {Vector2(-0.5f, 0.5f), Vector2(0.5f, 0.5f), Vector2(0.5f, -0.5f),
                                         Vector2(-0.5f, -0.5f)};
const std::array<uint32_t, 6> kIndices = {0, 1, 2, 2, 3, 0};

// The channels of an animation chunk (and those under it), by their four-letter
// name: float and int channels as curves, the vector and colour ones apart.
struct Channels
{
	std::map<std::string, ParticleFactory::Curve> curves;
	std::map<std::string, Vector3> vectors;
	// Every key of each vector channel, by frame.
	std::map<std::string, std::vector<std::pair<float, Vector3>>> vectorKeys;
	std::map<std::string, std::vector<std::pair<float, uint32_t>>> colors;
	float frames = 0.0f;
};

void readChannels(const P3D::P3DChunk& chunk, Channels& out)
{
	const auto type = chunk.GetType();
	if (type == P3D::ChunkType::Animation)
	{
		MemoryStream stream(chunk.GetData());
		stream.Read<uint32_t>();
		stream.ReadLPString();
		stream.ReadString(4);
		out.frames = stream.Read<float>();
	}
	else if (type == P3D::ChunkType::Float1Channel || type == P3D::ChunkType::IntChannel ||
	         type == P3D::ChunkType::Vector3Channel || type == P3D::ChunkType::Color)
	{
		MemoryStream stream(chunk.GetData());
		stream.Read<uint32_t>();
		std::string param = stream.ReadString(4);
		param = param.substr(0, param.find('\0'));
		const uint32_t count = stream.Read<uint32_t>();
		std::vector<uint16_t> frames(count);
		for (auto& frame : frames)
			frame = stream.Read<uint16_t>();
		if (type == P3D::ChunkType::Vector3Channel)
		{
			auto& keys = out.vectorKeys[param];
			for (const uint16_t frame : frames)
				keys.emplace_back(static_cast<float>(frame), stream.Read<Vector3>());
			if (count > 0)
				out.vectors[param] = keys.front().second;
		}
		else if (type == P3D::ChunkType::Color)
		{
			auto& keys = out.colors[param];
			for (const uint16_t frame : frames)
				keys.emplace_back(static_cast<float>(frame), stream.Read<uint32_t>());
		}
		else
		{
			auto& curve = out.curves[param];
			for (const uint16_t frame : frames)
				curve.keys.emplace_back(static_cast<float>(frame), type == P3D::ChunkType::IntChannel
				                                                       ? static_cast<float>(stream.Read<int32_t>())
				                                                       : stream.Read<float>());
		}
		return;
	}
	for (const auto& child : chunk.GetChildren())
		readChannels(*child, out);
}

float uniform(std::mt19937& random, float from, float to)
{
	return std::uniform_real_distribution<float>(from, to)(random);
}
} // namespace

Vector3 ParticleFactory::Emitter::OffsetAt(float frame) const
{
	if (path.empty())
		return offset;
	if (frame <= path.front().first)
		return path.front().second;
	for (std::size_t i = 1; i < path.size(); ++i)
		if (frame < path[i].first)
		{
			const float t = (frame - path[i - 1].first) / std::max(1e-6f, path[i].first - path[i - 1].first);
			return path[i - 1].second + (path[i].second - path[i - 1].second) * t;
		}
	return path.back().second;
}

float ParticleFactory::Curve::At(float frame, float otherwise) const
{
	if (keys.empty())
		return otherwise;
	if (frame <= keys.front().first)
		return keys.front().second;
	for (std::size_t i = 1; i < keys.size(); ++i)
		if (frame < keys[i].first)
		{
			const auto& [f0, v0] = keys[i - 1];
			const auto& [f1, v1] = keys[i];
			return v0 + (v1 - v0) * (frame - f0) / (f1 - f0);
		}
	return keys.back().second;
}

std::unique_ptr<ParticleFactory> ParticleFactory::FromChunk(const P3D::P3DChunk& chunk)
{
	auto factory = std::make_unique<ParticleFactory>();
	MemoryStream header(chunk.GetData());
	header.Read<uint32_t>();
	factory->_name = header.ReadLPString();
	const float frameRate = header.Read<float>();

	for (const auto& child : chunk.GetChildren())
	{
		if (child->GetType() != P3D::ChunkType::SpriteEmitter)
			continue;
		// version, name, then the sprites' shader.
		MemoryStream sprite(child->GetData());
		sprite.Read<uint32_t>();
		sprite.ReadLPString();
		Emitter emitter;
		emitter.shader = sprite.ReadLPString();
		emitter.frameRate = frameRate > 0.0f ? frameRate : 30.0f;
		// How the sprites turn (SPEC, MOAL, RAND) and by how much; then their
		// texture's frames: how they go (NONE, RAND, LIN), how many, how fast.
		const std::string angleMode = sprite.ReadString(4);
		emitter.startAngle = sprite.Read<float>();
		emitter.randomAngle = angleMode.rfind("RAND", 0) == 0;
		// "MOAL" turns with the sprite's motion: not followed, upright.
		if (angleMode.rfind("SPEC", 0) != 0)
			emitter.startAngle = 0.0f;
		const std::string animation = sprite.ReadString(4);
		emitter.textureFrames = std::max<uint32_t>(1, sprite.Read<uint32_t>());
		emitter.textureFrameRate = static_cast<float>(sprite.Read<uint32_t>());
		if (animation.rfind("RAND", 0) == 0)
			emitter.textureAnimation = Emitter::TextureAnimation::Random;
		else if (animation.rfind("LIN", 0) == 0)
			emitter.textureAnimation = Emitter::TextureAnimation::Linear;

		for (const auto& base : child->GetChildren())
			for (const auto& part : base->GetChildren())
			{
				Channels channels;
				readChannels(*part, channels);
				const auto curve = [&](const char* name) {
					const auto found = channels.curves.find(name);
					return found != channels.curves.end() ? found->second : Curve {};
				};
				if (part->GetType() == P3D::ChunkType::ParticleAnimation)
				{
					emitter.lifeFrames = std::max(1.0f, channels.frames);
					emitter.alphaOverLife = curve("ALOL");
					emitter.sizeOverLife = curve("SZOL");
					emitter.speedOverLife = curve("SPOL");
					emitter.spinOverLife = curve("SNOL");
					if (const auto found = channels.colors.find("COOL"); found != channels.colors.end())
						for (const auto& [frame, color] : found->second)
							emitter.colorOverLife.emplace_back(frame, P3D::P3DUtil::ConvertColor(color));
				}
				else if (part->GetType() == P3D::ChunkType::EmitterAnimation)
				{
					emitter.count = curve("NUM");
					emitter.life = curve("LIF");
					emitter.lifeVariance = curve("LIFV");
					emitter.speed = curve("SPD");
					emitter.speedVariance = curve("SPDV");
					emitter.size = curve("SIZ");
					emitter.sizeVariance = curve("SIZV");
					emitter.gravity = curve("GRA");
					emitter.alpha = curve("ALP");
					emitter.spin = curve("SPI");
					emitter.spinVariance = curve("SPIV");
					if (const auto found = channels.vectors.find("TRA"); found != channels.vectors.end())
						emitter.offset = found->second;
					if (const auto found = channels.vectorKeys.find("TRA");
					    found != channels.vectorKeys.end() && found->second.size() > 1)
						emitter.path = found->second;
					if (const auto found = channels.colors.find("CLR"); found != channels.colors.end() && !found->second.empty())
						emitter.color = P3D::P3DUtil::ConvertColor(found->second.front().second);
					emitter.rate = curve("RAT");
					emitter.timelineFrames = channels.frames;
				}
				else if (part->GetType() == P3D::ChunkType::GeneratorAnimation)
				{
					emitter.width = curve("WDT").At(0.0f, 0.0f);
					emitter.height = curve("HGT").At(0.0f, 0.0f);
					emitter.horizontalSpread = curve("HOSP").At(0.0f, 0.0f);
					emitter.verticalSpread = curve("VESP").At(0.0f, 0.0f);
				}
			}
		factory->_emitters.push_back(std::move(emitter));
	}
	return factory;
}

std::unique_ptr<ParticleFactory> ParticleFactory::Burst(const std::string& shader, float life, float speed, float spread,
                                                       float size, float gravity, const Vector4& color)
{
	// In the file's terms: frames at 30 a second, speed per frame, gravity in
	// m/s/s, up; none given off but by Burst (NUM 0).
	auto factory = std::make_unique<ParticleFactory>();
	factory->_name = "burst_" + shader;
	Emitter e;
	e.shader = shader;
	e.frameRate = 30.0f;
	e.lifeFrames = 10.0f;
	e.life.keys = {{0.0f, life * 30.0f}};
	e.lifeVariance.keys = {{0.0f, life * 6.0f}};
	e.speed.keys = {{0.0f, speed / 30.0f}};
	e.speedVariance.keys = {{0.0f, speed / 90.0f}};
	e.size.keys = {{0.0f, size}};
	e.sizeVariance.keys = {{0.0f, size * 0.3f}};
	e.gravity.keys = {{0.0f, -gravity}};
	e.alphaOverLife.keys = {{0.0f, 255.0f}, {9.0f, 0.0f}};
	e.sizeOverLife.keys = {{0.0f, 1.0f}, {9.0f, 1.6f}};
	e.color = color;
	e.horizontalSpread = 3.14159265f;
	e.verticalSpread = spread;
	factory->_emitters.push_back(std::move(e));
	return factory;
}

void ParticleSystem::Burst(int count, const Vector3& position)
{
	for (auto& running : _running)
	{
		const auto& e = *running.emitter;
		const float rate = e.frameRate;
		for (int i = 0; i < count; ++i)
		{
			Particle p;
			p.position = position;
			const float yaw = uniform(_random, -e.horizontalSpread, e.horizontalSpread);
			const float tilt = uniform(_random, 0.0f, e.verticalSpread);
			p.velocity = Vector3(std::sin(tilt) * std::cos(yaw), std::cos(tilt), std::sin(tilt) * std::sin(yaw)) *
			             ((e.speed.At(0.0f, 0.0f) + uniform(_random, -1.0f, 1.0f) * e.speedVariance.At(0.0f, 0.0f)) * rate);
			p.age = 0.0f;
			p.life = std::max(0.1f, (e.life.At(0.0f, 30.0f) + uniform(_random, -1.0f, 1.0f) * e.lifeVariance.At(0.0f, 0.0f)) / rate);
			p.size = std::max(0.01f, e.size.At(0.0f, 1.0f) + uniform(_random, -1.0f, 1.0f) * e.sizeVariance.At(0.0f, 0.0f));
			p.seed = static_cast<uint32_t>(_random());
			p.angle = e.randomAngle ? uniform(_random, 0.0f, 6.2831853f) : e.startAngle;
			p.spin = (e.spin.At(0.0f, 0.0f) + uniform(_random, -1.0f, 1.0f) * e.spinVariance.At(0.0f, 0.0f)) * rate;
			running.particles.push_back(p);
		}
	}
}

ParticleSystem::ParticleSystem(const ParticleFactory& factory): _factory(factory), _random(std::random_device {}())
{
	_quad = std::make_shared<GL::VertexBuffer>(kCorners.data(), kCorners.size(), sizeof(Vector2));
	_indices = std::make_shared<GL::IndexBuffer>(kIndices.data(), kIndices.size(), GL_UNSIGNED_INT);
	for (const auto& emitter : factory.GetEmitters())
		_running.push_back({&emitter});
}

ParticleSystem::~ParticleSystem() = default;

void ParticleSystem::Update(double dt, const Matrix4x4& placement, float timelineFrame, bool emit)
{
	const float step = static_cast<float>(dt);
	_age += dt;
	for (auto& running : _running)
	{
		const auto& e = *running.emitter;
		const float rate = e.frameRate;
		const float lifeFrames = std::max(1.0f, e.life.At(0.0f, 30.0f));
		const std::size_t most = static_cast<std::size_t>(std::max(0.0f, e.count.At(0.0f, 0.0f)));

		// Moved on, and the dead dropped.
		for (auto& p : running.particles)
		{
			p.age += step;
			const float olFrame = std::min(p.age / p.life, 1.0f) * (e.lifeFrames - 1.0f);
			// GRA pulls up, in m/s/s: debris fall (a crate's bits at -2, glass at
			// -6), flames rise (3). Taken per frame per frame, it threw every
			// falling bit to the sky at hundreds of g.
			p.velocity.Y += e.gravity.At(0.0f, 0.0f) * step;
			p.angle += p.spin * e.spinOverLife.At(olFrame, 1.0f) * step;
			p.position += p.velocity * (e.speedOverLife.At(olFrame, 1.0f) * step);
		}
		running.particles.erase(std::remove_if(running.particles.begin(), running.particles.end(),
		                                       [](const Particle& p) { return p.age >= p.life; }),
		                        running.particles.end());

		// Given off RAT a frame, the timeline's rate where it stands, at most NUM
		// alive at once. The bursts say so: a bin's apples come at 7 a frame
		// for a NUM of 7, the phone booth's bits at 30 for 30. Read as a
		// multiplier of the rate that keeps NUM alive, a bin gave up an apple
		// or two, and a car's damage fire (two emitters of 100 flames, 3 a
		// frame, a sixth of a second each) was 200 flames added up to white.
		float frame = timelineFrame;
		if (frame < 0.0f)
			frame = e.timelineFrames > 0.0f ? std::fmod(static_cast<float>(_age) * rate, e.timelineFrames) : 0.0f;
		// The rate over the stretch of timeline this step covers, not where it
		// ends: a mailbox's letters come in a burst of five frames, which one
		// slow frame stepped right over.
		float from = running.lastFrame;
		running.lastFrame = frame;
		if (from < 0.0f || from > frame)
			from = frame;
		const int samples = std::clamp(static_cast<int>(std::ceil(frame - from)), 1, 64);
		float timelineRate = 0.0f;
		for (int i = 0; i < samples; ++i)
			timelineRate += std::max(0.0f, e.rate.At(from + (frame - from) * (i + 0.5f) / samples, 1.0f));
		timelineRate /= static_cast<float>(samples);
		if (emit)
			running.owed += timelineRate * rate * step;
		while (running.owed >= 1.0f)
		{
			running.owed -= 1.0f;
			if (running.particles.size() >= most)
				continue;
			Particle p;
			const Vector3 local = e.OffsetAt(frame) + Vector3(uniform(_random, -0.5f, 0.5f) * e.width, 0.0f,
			                                         uniform(_random, -0.5f, 0.5f) * e.height);
			p.position = placement * local;
			// Straight up, tipped within the spreads.
			const float yaw = uniform(_random, -e.horizontalSpread, e.horizontalSpread);
			const float tilt = uniform(_random, -e.verticalSpread, e.verticalSpread);
			// The emitter's up, as its joint turns it (a spring's steam, aimed
			// sideways): the placement's rotation alone.
			const Vector3 spread(std::sin(tilt) * std::cos(yaw), std::cos(tilt), std::sin(tilt) * std::sin(yaw));
			const Vector3 direction = (placement * spread - placement * Vector3::Zero).Normalized();
			const float speed =
			    (e.speed.At(0.0f, 0.0f) + uniform(_random, -1.0f, 1.0f) * e.speedVariance.At(0.0f, 0.0f)) * rate;
			p.velocity = direction * speed;
			p.age = 0.0f;
			p.life = std::max(0.1f, (lifeFrames + uniform(_random, -1.0f, 1.0f) * e.lifeVariance.At(0.0f, 0.0f)) / rate);
			p.size = std::max(0.01f, e.size.At(0.0f, 1.0f) + uniform(_random, -1.0f, 1.0f) * e.sizeVariance.At(0.0f, 0.0f));
			p.seed = static_cast<uint32_t>(_random());
			p.angle = e.randomAngle ? uniform(_random, 0.0f, 6.2831853f) : e.startAngle;
			p.spin = (e.spin.At(0.0f, 0.0f) + uniform(_random, -1.0f, 1.0f) * e.spinVariance.At(0.0f, 0.0f)) * rate;
			running.particles.push_back(p);
		}
	}
}

void ParticleSystem::Draw(GL::ShaderProgram& billboardShader)
{
	auto& resources = Game::GetInstance().GetResourceManager();
	for (auto& running : _running)
	{
		const auto& e = *running.emitter;
		if (running.particles.empty())
			continue;
		Shader* material = resources.GetShader(e.shader);
		if (!material)
			continue;

		std::vector<SpriteInstance> sprites;
		sprites.reserve(running.particles.size());
		const float baseAlpha = e.alpha.At(0.0f, 255.0f) / 255.0f;
		for (const auto& p : running.particles)
		{
			const float olFrame = std::min(p.age / p.life, 1.0f) * (e.lifeFrames - 1.0f);
			const float size = p.size * e.sizeOverLife.At(olFrame, 1.0f);
			Vector4 color = e.color;
			color.W *= baseAlpha * e.alphaOverLife.At(olFrame, 255.0f) / 255.0f;
			if (!e.colorOverLife.empty())
			{
				// Between the two keys about `olFrame`.
				Vector4 over = e.colorOverLife.back().second;
				for (std::size_t k = 1; k < e.colorOverLife.size(); ++k)
					if (olFrame < e.colorOverLife[k].first)
					{
						const auto& [f0, c0] = e.colorOverLife[k - 1];
						const auto& [f1, c1] = e.colorOverLife[k];
						const float t = std::clamp((olFrame - f0) / std::max(0.001f, f1 - f0), 0.0f, 1.0f);
						over = c0 + (c1 - c0) * t;
						break;
					}
				color = Vector4(color.X * over.X, color.Y * over.Y, color.Z * over.Z, color.W * over.W);
			}
			// Its frame of the strip: drawn whole, the four of a flame side by
			// side, a hundred of them added up to a white glare.
			uint32_t textureFrame = 0;
			const uint32_t tick = static_cast<uint32_t>(p.age * e.textureFrameRate);
			if (e.textureAnimation == ParticleFactory::Emitter::TextureAnimation::Random)
				textureFrame = (p.seed + tick * 2654435761u) % e.textureFrames;
			else if (e.textureAnimation == ParticleFactory::Emitter::TextureAnimation::Linear)
				textureFrame = tick % e.textureFrames;
			const float u0 = static_cast<float>(textureFrame) / static_cast<float>(e.textureFrames);
			const float u1 = static_cast<float>(textureFrame + 1) / static_cast<float>(e.textureFrames);
			sprites.push_back({p.position, Vector2(size, size), color, Vector2(u0, 0.0f), Vector2(u1, 0.0f),
			                   Vector2(u1, 1.0f), Vector2(u0, 1.0f), p.angle});
		}

		// The instance buffer, grown as needed and refilled every frame.
		if (sprites.size() > running.capacity)
		{
			running.capacity = std::max<std::size_t>(sprites.size() * 2, 64);
			std::vector<SpriteInstance> blank(running.capacity);
			running.instances =
			    std::make_shared<GL::VertexBuffer>(blank.data(), blank.size(), sizeof(SpriteInstance), GL_DYNAMIC_DRAW);
			constexpr std::size_t stride = sizeof(SpriteInstance);
			GL::ArrayElement layout[] = {
			    GL::ArrayElement(_quad.get(), 0, 2, GL::AE_FLOAT, sizeof(Vector2), 0),
			    GL::ArrayElement(running.instances.get(), 1, 3, GL::AE_FLOAT, stride, 0 * sizeof(float), 1),
			    GL::ArrayElement(running.instances.get(), 2, 2, GL::AE_FLOAT, stride, 3 * sizeof(float), 1),
			    GL::ArrayElement(running.instances.get(), 3, 4, GL::AE_FLOAT, stride, 5 * sizeof(float), 1),
			    GL::ArrayElement(running.instances.get(), 4, 2, GL::AE_FLOAT, stride, 9 * sizeof(float), 1),
			    GL::ArrayElement(running.instances.get(), 5, 2, GL::AE_FLOAT, stride, 11 * sizeof(float), 1),
			    GL::ArrayElement(running.instances.get(), 6, 2, GL::AE_FLOAT, stride, 13 * sizeof(float), 1),
			    GL::ArrayElement(running.instances.get(), 7, 2, GL::AE_FLOAT, stride, 15 * sizeof(float), 1),
			    GL::ArrayElement(running.instances.get(), 8, 1, GL::AE_FLOAT, stride, 17 * sizeof(float), 1),
			};
			running.binding = std::make_shared<GL::VertexBinding>();
			running.binding->Create(layout, 9, *_indices, GL::ElementType::AE_UINT);
		}
		running.instances->UpdateBuffer(sprites.data(), 0, sprites.size() * sizeof(SpriteInstance));

		// Blended as the material says, tested against the world but not
		// hiding what is behind.
		glEnable(GL_BLEND);
		// Additive, weighted by the alpha: a spark fades as it dies, and a
		// burst of them did not saturate into a white square.
		if (material->GetBlendMode() == BlendMode::Additive)
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		else
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		if (billboardShader.HasUniform("alphaMask"))
			billboardShader.SetUniformValue("alphaMask", 0.0f);
		running.binding->Bind();
		material->Bind(0);
		glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(sprites.size()));
		running.binding->Unbind();
		glDepthMask(GL_TRUE);
	}
}

} // namespace Donut
