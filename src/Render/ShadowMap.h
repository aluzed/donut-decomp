#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"

#include <memory>

namespace Donut
{

namespace GL
{
class FrameBuffer;
class ShaderProgram;
} // namespace GL

// The sun's shadows of what moves (FX-004): the characters and the cars, drawn
// from the sun into a depth map around the player, then looked up by the
// world's shader. The world's own shadows are in its vertex colours already.
// The game has none such (its characters and cars have blob shadows): off
// unless the Post-FX window sets a strength.
class ShadowMap
{
public:
	ShadowMap();
	~ShadowMap();

	float strength = 0.0f;
	// How far about the player the map reaches, metres.
	float range = 40.0f;

	bool IsOn() const { return strength > 0.0f; }

	// The casters are drawn from here with GetLightViewProj, into the depth only...
	void Begin(const Vector3& centre, const Vector3& sunDirection);
	// ...to here.
	void End();
	const Matrix4x4& GetLightViewProj() const { return _lightViewProj; }

	// The world's shader told where the map is and how dark it makes things,
	// the map on texture unit kUnit.
	void Apply(GL::ShaderProgram& worldShader) const;
	// No shadows on it.
	static void Disable(GL::ShaderProgram& worldShader);

	void DebugControls();

private:
	static constexpr int kSize = 2048;
	static constexpr int kUnit = 5;
	std::unique_ptr<GL::FrameBuffer> _depth;
	Matrix4x4 _lightViewProj = Matrix4x4::Identity;
	int _viewport[4] = {};
};

} // namespace Donut
