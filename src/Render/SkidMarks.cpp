#include "SkidMarks.h"

#include "Game.h"
#include "Render/OpenGL/IndexBuffer.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/VertexBinding.h"
#include "Render/OpenGL/VertexBuffer.h"
#include "Render/Shader.h"
#include "ResourceManager.h"

#include <algorithm>
#include <vector>

namespace Donut
{

namespace
{
// Quads kept a shader.
constexpr std::size_t kQuads = 1024;
// How far a wheel goes before its strip takes another quad, and how far above
// the ground it is laid, against the ground showing through.
constexpr float kStep = 0.4f;
constexpr float kLift = 0.03f;
// The texture's length along the strip, in metres.
constexpr float kTextureLength = 1.0f;
} // namespace

SkidMarks::SkidMarks()
{
	std::vector<uint32_t> indices;
	indices.reserve(kQuads * 6);
	for (uint32_t q = 0; q < kQuads; ++q)
		for (const uint32_t corner : {0u, 1u, 2u, 2u, 1u, 3u})
			indices.push_back(q * 4 + corner);
	_indices = std::make_shared<GL::IndexBuffer>(indices.data(), indices.size(), GL_UNSIGNED_INT);
}

SkidMarks::~SkidMarks() = default;

void SkidMarks::Clear()
{
	_strips.clear();
	_tracks.clear();
}

void SkidMarks::Lay(uint64_t track, const Vector3& point, const Vector3& normal, const Vector3& axle, float width,
                    const std::string& shader, float strength)
{
	const Vector3 up = normal * kLift;
	const Vector3 half = axle.Normalized() * (width * 0.5f);
	Track here {shader, point + up - half, point + up + half, 0.0f, std::clamp(strength, 0.0f, 1.0f)};

	const auto found = _tracks.find(track);
	if (found == _tracks.end() || found->second.shader != shader)
	{
		// A new strip, from here.
		_tracks[track] = here;
		return;
	}
	Track& last = found->second;
	const Vector3 middle = (last.left + last.right) * 0.5f;
	const float gone = (point + up - middle).Length();
	if (gone < kStep)
		return;
	// Off too far in one step (put back on the road, a jump): start again.
	if (gone > kStep * 10.0f)
	{
		last = here;
		return;
	}
	here.v = last.v + gone / kTextureLength;
	addQuad(shader, last, here);
	last = here;
}

void SkidMarks::addQuad(const std::string& shader, const Track& from, const Track& to)
{
	Strip& strip = _strips[shader];
	if (strip.vertices.empty())
	{
		strip.vertices.resize(kQuads * 4);
		strip.buffer = std::make_shared<GL::VertexBuffer>(strip.vertices.data(), strip.vertices.size(), sizeof(Vertex),
		                                                  GL_DYNAMIC_DRAW);
		constexpr std::size_t stride = sizeof(Vertex);
		const GL::ArrayElement layout[] = {
		    GL::ArrayElement(strip.buffer.get(), 0, 3, GL::AE_FLOAT, stride, 0),
		    GL::ArrayElement(strip.buffer.get(), 1, 2, GL::AE_FLOAT, stride, 3 * sizeof(float)),
		    GL::ArrayElement(strip.buffer.get(), 2, 2, GL::AE_FLOAT, stride, 5 * sizeof(float)),
		    GL::ArrayElement(strip.buffer.get(), 3, 4, GL::AE_FLOAT, stride, 7 * sizeof(float)),
		};
		strip.binding = std::make_shared<GL::VertexBinding>();
		strip.binding->Create(layout, 4, *_indices, GL::ElementType::AE_UINT);
	}
	const std::size_t quad = strip.next;
	const Vector4 start(1.0f, 1.0f, 1.0f, from.strength), end(1.0f, 1.0f, 1.0f, to.strength);
	Vertex* v = &strip.vertices[quad * 4];
	v[0] = {from.left, Vector2(0.0f, from.v), Vector2(0.0f, 0.0f), start};
	v[1] = {from.right, Vector2(1.0f, from.v), Vector2(0.0f, 0.0f), start};
	v[2] = {to.left, Vector2(0.0f, to.v), Vector2(0.0f, 0.0f), end};
	v[3] = {to.right, Vector2(1.0f, to.v), Vector2(0.0f, 0.0f), end};
	strip.next = (quad + 1) % kQuads;
	strip.count = std::min(strip.count + 1, kQuads);
	strip.dirtyFrom = std::min(strip.dirtyFrom, quad);
	strip.dirtyTo = std::max(strip.dirtyTo, quad + 1);
}

void SkidMarks::Draw(GL::ShaderProgram& worldShader)
{
	if (_strips.empty())
		return;
	auto& resources = Game::GetInstance().GetResourceManager();
	worldShader.Bind();
	if (worldShader.HasUniform("alphaMask"))
		worldShader.SetUniformValue("alphaMask", 0.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	for (auto& [name, strip] : _strips)
	{
		const Shader* shader = resources.GetShader(name);
		if (!shader || strip.count == 0)
			continue;
		if (strip.dirtyFrom < strip.dirtyTo)
		{
			strip.buffer->UpdateBuffer(&strip.vertices[strip.dirtyFrom * 4], strip.dirtyFrom * 4 * sizeof(Vertex),
			                           (strip.dirtyTo - strip.dirtyFrom) * 4 * sizeof(Vertex));
			strip.dirtyFrom = SIZE_MAX;
			strip.dirtyTo = 0;
		}
		shader->Bind(0);
		strip.binding->Bind();
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(strip.count * 6), GL_UNSIGNED_INT, nullptr);
		strip.binding->Unbind();
	}
	glEnable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);
}

} // namespace Donut
