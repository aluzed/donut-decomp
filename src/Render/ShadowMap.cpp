#include "Render/ShadowMap.h"

#include "Render/OpenGL/FrameBuffer.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/glad/glad.h"
#include "Render/imgui/imgui.h"

#include <cmath>

namespace Donut
{

ShadowMap::ShadowMap() = default;
ShadowMap::~ShadowMap() = default;

void ShadowMap::Begin(const Vector3& centre, const Vector3& sunDirection)
{
	if (!_depth)
	{
		GL::FrameBuffer::Format format;
		format.EnableColourBuffer(false);
		format.EnableDepthBuffer(true, true);
		_depth = std::make_unique<GL::FrameBuffer>(kSize, kSize, format);
	}
	// The sun's frame: looking along its light, square about the player. The
	// box moves a texel at a time, or the shadows' edges would crawl.
	Vector3 forward = sunDirection;
	forward.Normalize();
	Vector3 right = std::abs(forward.Y) > 0.99f ? Vector3(1.0f, 0.0f, 0.0f) : Vector3(0.0f, 1.0f, 0.0f).Cross(forward);
	right.Normalize();
	const Vector3 up = forward.Cross(right);
	const auto dot = [](const Vector3& a, const Vector3& b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; };
	const float texel = 2.0f * range / kSize;
	const auto snap = [texel](float value) { return std::floor(value / texel) * texel; };
	const float depth = range * 4.0f;
	const float x = snap(dot(right, centre)), y = snap(dot(up, centre)), z = dot(forward, centre);
	// Columns, as the matrices are kept: clip = (right, up, forward) . point,
	// less the centre's, over the box's half sizes.
	Matrix4x4& m = _lightViewProj;
	m = Matrix4x4::Identity;
	m.M[0][0] = right.X / range, m.M[1][0] = right.Y / range, m.M[2][0] = right.Z / range, m.M[3][0] = -x / range;
	m.M[0][1] = up.X / range, m.M[1][1] = up.Y / range, m.M[2][1] = up.Z / range, m.M[3][1] = -y / range;
	m.M[0][2] = forward.X / depth, m.M[1][2] = forward.Y / depth, m.M[2][2] = forward.Z / depth, m.M[3][2] = -z / depth;
	m.M[0][3] = 0.0f, m.M[1][3] = 0.0f, m.M[2][3] = 0.0f, m.M[3][3] = 1.0f;

	glGetIntegerv(GL_VIEWPORT, _viewport);
	// Not read from while it is drawn into.
	glActiveTexture(GL_TEXTURE0 + kUnit);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	_depth->Bind();
	glViewport(0, 0, kSize, kSize);
	glClear(GL_DEPTH_BUFFER_BIT);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	// Both sides: the characters' meshes are not all wound one way.
	glDisable(GL_CULL_FACE);
}

void ShadowMap::End()
{
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glEnable(GL_CULL_FACE);
	GL::FrameBuffer::Unbind();
	glViewport(_viewport[0], _viewport[1], _viewport[2], _viewport[3]);
}

void ShadowMap::Apply(GL::ShaderProgram& worldShader) const
{
	if (!_depth)
		return Disable(worldShader);
	glActiveTexture(GL_TEXTURE0 + kUnit);
	glBindSampler(kUnit, 0);
	_depth->BindDepthTexture();
	glActiveTexture(GL_TEXTURE0);
	worldShader.Bind();
	worldShader.SetUniformValue("shadowTex", kUnit);
	worldShader.SetUniformValue("lightViewProj", _lightViewProj);
	worldShader.SetUniformValue("shadowStrength", strength);
	worldShader.SetUniformValue("shadowTexel", 1.0f / kSize);
}

void ShadowMap::Disable(GL::ShaderProgram& worldShader)
{
	worldShader.Bind();
	worldShader.SetUniformValue("shadowStrength", 0.0f);
}

void ShadowMap::DebugControls()
{
	ImGui::SliderFloat("Shadows", &strength, 0.0f, 1.0f);
	ImGui::SliderFloat("Shadow range", &range, 10.0f, 120.0f);
}

} // namespace Donut
