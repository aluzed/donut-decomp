#include "Render/PostProcess.h"

#include "Core/File.h"
#include "Core/Math/Vector2.h"
#include "Render/OpenGL/FrameBuffer.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/VertexBinding.h"
#include "Render/OpenGL/VertexBuffer.h"
#include "Render/OpenGL/glad/glad.h"
#include "Render/Texture.h"
#include "Render/imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Donut
{

namespace
{
std::unique_ptr<GL::ShaderProgram> loadProgram(const char* vertex, const char* fragment)
{
	return std::make_unique<GL::ShaderProgram>(File::ReadAll(vertex), File::ReadAll(fragment));
}
} // namespace

PostProcess::PostProcess()
{
	_final = loadProgram("shaders/postprocess.vert", "shaders/postprocess.frag");
	_bright = loadProgram("shaders/postprocess.vert", "shaders/bloom_bright.frag");
	_blur = loadProgram("shaders/postprocess.vert", "shaders/blur.frag");

	// Two triangles covering the screen, in normalised device coordinates;
	// the vertex shader makes the texture coordinates from them.
	const float quad[] = {
	    -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f,
	};
	_quad = std::make_shared<GL::VertexBuffer>(quad, 6, sizeof(float) * 3);
	_quadBinding = std::make_shared<GL::VertexBinding>();
	GL::ArrayElement element(_quad.get(), 0, 3, GL::AE_FLOAT, sizeof(float) * 3, 0);
	_quadBinding->Create(&element, 1, *_quad);
}

PostProcess::~PostProcess() = default;

const std::vector<std::string>& PostProcess::GetLooks()
{
	static const std::vector<std::string> looks = {"none", "warm", "cool", "sepia", "vivid", "night"};
	return looks;
}

void PostProcess::makeLook(int look)
{
	// The table: every colour on a 16-step grid, its blue slices side by side,
	// and what each becomes.
	std::vector<uint8_t> pixels(256 * 16 * 4);
	for (int b = 0; b < 16; ++b)
		for (int g = 0; g < 16; ++g)
			for (int r = 0; r < 16; ++r)
			{
				float red = r / 15.0f, green = g / 15.0f, blue = b / 15.0f;
				const float grey = 0.299f * red + 0.587f * green + 0.114f * blue;
				switch (look)
				{
				case 1: // warm: the evening's
					red = red * 1.08f + 0.02f, green = green * 1.02f, blue = blue * 0.88f;
					break;
				case 2: // cool
					red = red * 0.9f, green = green * 1.0f, blue = blue * 1.1f + 0.02f;
					break;
				case 3: // sepia
					red = grey * 1.07f + 0.05f, green = grey * 0.95f + 0.02f, blue = grey * 0.75f;
					break;
				case 4: // vivid: more colour, more contrast
					red = (grey + (red - grey) * 1.35f - 0.5f) * 1.15f + 0.5f;
					green = (grey + (green - grey) * 1.35f - 0.5f) * 1.15f + 0.5f;
					blue = (grey + (blue - grey) * 1.35f - 0.5f) * 1.15f + 0.5f;
					break;
				case 5: // night: dark and blue
					red = red * 0.45f, green = green * 0.55f, blue = blue * 0.8f + 0.05f;
					break;
				default: break;
				}
				uint8_t* pixel = &pixels[(g * 256 + b * 16 + r) * 4];
				pixel[0] = static_cast<uint8_t>(std::clamp(red, 0.0f, 1.0f) * 255.0f + 0.5f);
				pixel[1] = static_cast<uint8_t>(std::clamp(green, 0.0f, 1.0f) * 255.0f + 0.5f);
				pixel[2] = static_cast<uint8_t>(std::clamp(blue, 0.0f, 1.0f) * 255.0f + 0.5f);
				pixel[3] = 255;
			}
	_lut = std::make_unique<Texture>(pixels.data(), 256, 16, "post_lut");
	_lutLook = look;
}

void PostProcess::BeginScene(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;
	if (!_scene)
	{
		GL::FrameBuffer::Format format;
		format.EnableColourBuffer(true, 1);
		format.EnableDepthBuffer(true, true);
		_scene = std::make_unique<GL::FrameBuffer>(width, height, format);
	}
	else if (width != _width || height != _height)
		_scene->SetSize(width, height);
	_width = width;
	_height = height;
	_scene->Bind();
	glViewport(0, 0, width, height);
}

void PostProcess::drawQuad()
{
	_quadBinding->Bind();
	glDrawArrays(GL_TRIANGLES, 0, 6);
	_quadBinding->Unbind();
}

void PostProcess::bloom(int width, int height)
{
	// At half size: the bright parts, then blurred across and down, as many
	// times as it takes to spread them.
	const int w = std::max(1, width / 2), h = std::max(1, height / 2);
	if (!_bloomA)
	{
		GL::FrameBuffer::Format format;
		format.EnableColourBuffer(true, 1);
		format.EnableDepthBuffer(false, false);
		_bloomA = std::make_unique<GL::FrameBuffer>(w, h, format);
		_bloomB = std::make_unique<GL::FrameBuffer>(w, h, format);
	}
	else if (_bloomA->GetWidth() != w || _bloomA->GetHeight() != h)
	{
		_bloomA->SetSize(w, h);
		_bloomB->SetSize(w, h);
	}
	glViewport(0, 0, w, h);
	_bloomA->Bind();
	_bright->Bind();
	_scene->BindColorTexture(0);
	_bright->SetUniformValue("sceneTex", 0);
	_bright->SetUniformValue("threshold", _settings.bloomThreshold);
	drawQuad();
	_blur->Bind();
	_blur->SetUniformValue("sourceTex", 0);
	for (int pass = 0; pass < std::max(1, _settings.bloomPasses); ++pass)
	{
		_bloomB->Bind();
		_bloomA->BindColorTexture(0);
		_blur->SetUniformValue("direction", Vector2(1.0f / w, 0.0f));
		drawQuad();
		_bloomA->Bind();
		_bloomB->BindColorTexture(0);
		_blur->SetUniformValue("direction", Vector2(0.0f, 1.0f / h));
		drawQuad();
	}
}

void PostProcess::EndScene(int width, int height, const Matrix4x4& viewProj)
{
	if (!_scene || !_final)
		return;
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	// The world leaves a sampler on unit 0 asking for mipmaps, which these
	// single-level textures do not have: every fetch came back black.
	for (GLuint unit = 0; unit < 4; ++unit) glBindSampler(unit, 0);

	if (_settings.bloom > 0.0f)
		bloom(width, height);
	if (_settings.grading > 0.0f && _settings.look != _lutLook)
		makeLook(_settings.look);

	GL::FrameBuffer::Unbind();
	glViewport(0, 0, width, height);
	_final->Bind();
	glActiveTexture(GL_TEXTURE0);
	_scene->BindColorTexture(0);
	_final->SetUniformValue("sceneTex", 0);
	if (_bloomA)
	{
		glActiveTexture(GL_TEXTURE1);
		_bloomA->BindColorTexture(0);
		_final->SetUniformValue("bloomTex", 1);
	}
	glActiveTexture(GL_TEXTURE2);
	_scene->BindDepthTexture();
	_final->SetUniformValue("depthTex", 2);
	if (_lut)
	{
		_lut->Bind(3);
		_final->SetUniformValue("lutTex", 3);
	}
	glActiveTexture(GL_TEXTURE0);
	_final->SetUniformValue("screenSize", Vector2(static_cast<float>(width), static_cast<float>(height)));
	_final->SetUniformValue("bloomIntensity", _bloomA ? _settings.bloom : 0.0f);
	_final->SetUniformValue("vignetteIntensity", _settings.vignette);
	_final->SetUniformValue("motionBlur", _hasPrevious ? _settings.motionBlur : 0.0f);
	_final->SetUniformValue("grading", _lut ? _settings.grading : 0.0f);
	_final->SetUniformValue("inverseViewProj", viewProj.Inverse());
	_final->SetUniformValue("previousViewProj", _hasPrevious ? _previousViewProj : viewProj);
	drawQuad();

	_previousViewProj = viewProj;
	_hasPrevious = true;
	glEnable(GL_DEPTH_TEST);
}

void PostProcess::DebugControls()
{
	ImGui::SliderFloat("Vignette", &_settings.vignette, 0.0f, 1.0f);
	ImGui::SliderFloat("Bloom", &_settings.bloom, 0.0f, 2.0f);
	ImGui::SliderFloat("Bloom threshold", &_settings.bloomThreshold, 0.0f, 0.95f);
	ImGui::SliderInt("Bloom passes", &_settings.bloomPasses, 1, 6);
	ImGui::SliderFloat("Motion blur", &_settings.motionBlur, 0.0f, 1.0f);
	const auto& looks = GetLooks();
	if (ImGui::BeginCombo("Look", looks[static_cast<std::size_t>(_settings.look)].c_str()))
	{
		for (int i = 0; i < static_cast<int>(looks.size()); ++i)
			if (ImGui::Selectable(looks[static_cast<std::size_t>(i)].c_str(), i == _settings.look))
				_settings.look = i;
		ImGui::EndCombo();
	}
	ImGui::SliderFloat("Grading", &_settings.grading, 0.0f, 1.0f);
}

} // namespace Donut
