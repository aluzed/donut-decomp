#include "Render/BlobShadows.h"

#include "Render/OpenGL/IndexBuffer.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/VertexBinding.h"
#include "Render/OpenGL/VertexBuffer.h"
#include "Render/OpenGL/glad/glad.h"
#include "Render/Texture.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Donut
{

namespace
{
constexpr int kSize = 64;

// Black, its alpha falling off to nothing at the edge: a disc, or a rectangle
// with soft sides.
std::unique_ptr<Texture> makeTexture(bool disc, const char* name)
{
	std::vector<uint8_t> pixels(kSize * kSize * 4, 0);
	for (int y = 0; y < kSize; ++y)
		for (int x = 0; x < kSize; ++x)
		{
			const float u = (x + 0.5f) / kSize * 2.0f - 1.0f, v = (y + 0.5f) / kSize * 2.0f - 1.0f;
			float alpha;
			if (disc)
			{
				// Even inside, fading over its outer third.
				const float r = std::min(1.0f, std::sqrt(u * u + v * v));
				alpha = std::clamp((1.0f - r) / 0.35f, 0.0f, 1.0f);
			}
			else
			{
				const auto edge = [](float t) { return std::clamp((1.0f - std::abs(t)) / 0.35f, 0.0f, 1.0f); };
				alpha = edge(u) * edge(v);
			}
			pixels[(y * kSize + x) * 4 + 3] = static_cast<uint8_t>(alpha * 255.0f + 0.5f);
		}
	return std::make_unique<Texture>(pixels.data(), kSize, kSize, name);
}
} // namespace

BlobShadows::BlobShadows()
{
	_discTexture = makeTexture(true, "blob_shadow_disc");
	_boxTexture = makeTexture(false, "blob_shadow_box");
	std::vector<uint32_t> indices;
	indices.reserve(kQuads * 6);
	for (uint32_t q = 0; q < kQuads; ++q)
		for (const uint32_t corner : {0u, 1u, 2u, 2u, 1u, 3u}) indices.push_back(q * 4 + corner);
	_indices = std::make_shared<GL::IndexBuffer>(indices.data(), indices.size(), GL_UNSIGNED_INT);
	std::vector<Vertex> empty(kQuads * 4);
	_buffer = std::make_shared<GL::VertexBuffer>(empty.data(), empty.size(), sizeof(Vertex), GL_DYNAMIC_DRAW);
	constexpr std::size_t stride = sizeof(Vertex);
	const GL::ArrayElement layout[] = {
	    GL::ArrayElement(_buffer.get(), 0, 3, GL::AE_FLOAT, stride, offsetof(Vertex, pos)),
	    GL::ArrayElement(_buffer.get(), 1, 2, GL::AE_FLOAT, stride, offsetof(Vertex, uv)),
	    GL::ArrayElement(_buffer.get(), 2, 2, GL::AE_FLOAT, stride, offsetof(Vertex, lightmapUV)),
	    GL::ArrayElement(_buffer.get(), 3, 4, GL::AE_FLOAT, stride, offsetof(Vertex, color)),
	};
	_binding = std::make_shared<GL::VertexBinding>();
	_binding->Create(layout, 4, *_indices, GL::ElementType::AE_UINT);
}

BlobShadows::~BlobShadows() = default;

void BlobShadows::Clear()
{
	_discs.clear();
	_boxes.clear();
}

void BlobShadows::addQuad(std::vector<Vertex>& quads, const Vector3 corners[4], float darkness)
{
	if (_discs.size() + _boxes.size() >= kQuads * 4)
		return;
	const Vector4 colour(0.0f, 0.0f, 0.0f, std::clamp(darkness, 0.0f, 1.0f));
	const Vector2 uvs[4] = {Vector2(0.0f, 0.0f), Vector2(1.0f, 0.0f), Vector2(0.0f, 1.0f), Vector2(1.0f, 1.0f)};
	for (int i = 0; i < 4; ++i) quads.push_back({corners[i], uvs[i], Vector2(0.0f, 0.0f), colour});
}

void BlobShadows::AddDisc(const Vector3& ground, float radius, float darkness)
{
	const Vector3 corners[4] = {ground + Vector3(-radius, 0.0f, -radius), ground + Vector3(radius, 0.0f, -radius),
	                            ground + Vector3(-radius, 0.0f, radius), ground + Vector3(radius, 0.0f, radius)};
	addQuad(_discs, corners, darkness);
}

void BlobShadows::AddBox(const Vector3& ground, float yaw, float halfWidth, float halfLength, float darkness)
{
	const float c = std::cos(yaw), s = std::sin(yaw);
	const Vector3 across(c * halfWidth, 0.0f, -s * halfWidth), along(s * halfLength, 0.0f, c * halfLength);
	const Vector3 corners[4] = {ground - across - along, ground + across - along, ground - across + along,
	                            ground + across + along};
	addQuad(_boxes, corners, darkness);
}

void BlobShadows::Draw(GL::ShaderProgram& worldShader, const Matrix4x4& viewProj)
{
	if (!enabled || (_discs.empty() && _boxes.empty()))
		return;
	worldShader.Bind();
	worldShader.SetUniformValue("viewProj", viewProj);
	if (worldShader.HasUniform("alphaMask"))
		worldShader.SetUniformValue("alphaMask", 0.0f);
	if (worldShader.HasUniform("envColor"))
		worldShader.SetUniformValue("envColor", Vector3::Zero);
	worldShader.SetUniformValue("lighting", 0);
	worldShader.SetUniformValue("diffuseTex", 0);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	// Over the ground they lie on, not fighting it.
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-2.0f, -2.0f);
	glBindSampler(0, 0);
	_binding->Bind();
	std::size_t first = 0;
	for (const auto& [quads, texture] :
	     {std::make_pair(&_discs, _discTexture.get()), std::make_pair(&_boxes, _boxTexture.get())})
	{
		if (quads->empty())
			continue;
		_buffer->UpdateBuffer(quads->data(), first * sizeof(Vertex), quads->size() * sizeof(Vertex));
		texture->Bind(0);
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quads->size() / 4 * 6), GL_UNSIGNED_INT,
		               reinterpret_cast<const void*>(first / 4 * 6 * sizeof(uint32_t)));
		first += quads->size();
	}
	_binding->Unbind();
	glDisable(GL_POLYGON_OFFSET_FILL);
	glEnable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);
}

} // namespace Donut
