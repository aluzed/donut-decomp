#pragma once

#include "Core/Math/Matrix4x4.h"

#include <memory>
#include <string>
#include <vector>

namespace Donut
{

class Texture;
namespace GL
{
class FrameBuffer;
class ShaderProgram;
class VertexBuffer;
class VertexBinding;
} // namespace GL

// The 3D scene drawn into a frame buffer, then onto the screen through the
// post-process pass: bloom (FX-001), motion blur (FX-003), a colour grading
// table (FX-005) and a vignette (FX-002). The game has none of them: they are
// all off unless the Post-FX window turns them on, and the picture is then the
// scene as drawn.
class PostProcess
{
public:
	struct Settings
	{
		float vignette = 0.0f;
		// Bloom: how much of the glow is added, from how bright, how wide.
		float bloom = 0.0f;
		float bloomThreshold = 0.7f;
		int bloomPasses = 3;
		// Motion blur: how much of the camera's motion over a frame is smeared.
		float motionBlur = 0.0f;
		// The colour grading table: which look, how much of it.
		int look = 0;
		float grading = 0.0f;
	};

	PostProcess();
	~PostProcess();

	Settings& GetSettings() { return _settings; }

	// The scene is drawn into the frame buffer from here...
	void BeginScene(int width, int height);
	// ...and onto the screen through the effects from here. `viewProj` is the
	// camera's this frame (the motion blur compares it with the last one's).
	void EndScene(int width, int height, const Matrix4x4& viewProj);

	// The looks the grading table can take ("none", "warm", ...).
	static const std::vector<std::string>& GetLooks();
	// The Post-FX debug window's controls.
	void DebugControls();

private:
	void drawQuad();
	void bloom(int width, int height);
	void makeLook(int look);

	Settings _settings;
	std::unique_ptr<GL::ShaderProgram> _final, _bright, _blur;
	std::unique_ptr<GL::FrameBuffer> _scene, _bloomA, _bloomB;
	std::shared_ptr<GL::VertexBuffer> _quad;
	std::shared_ptr<GL::VertexBinding> _quadBinding;
	std::unique_ptr<Texture> _lut;
	int _lutLook = -1;
	int _width = 0, _height = 0;
	Matrix4x4 _previousViewProj;
	bool _hasPrevious = false;
};

} // namespace Donut
