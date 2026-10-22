#include "UI/LoadingScreen.h"

#include "Core/FileSystem.h"
#include "Core/Math/Matrix4x4.h"
#include "FrontendProject.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DFile.h"
#include "Render/OpenGL/glad/glad.h"
#include "ResourceManager.h"
#include "Window.h"

#include <SDL.h>
#include <fmt/format.h>

#include <algorithm>

namespace Donut
{

namespace
{
constexpr const char* kProject = "./art/frontend/scrooby2/backend.p3d";
} // namespace

LoadingScreen::LoadingScreen(Window& window, ResourceManager& resources): _window(window), _resources(resources)
{
	if (!FileSystem::exists(kProject))
		return;
	_page = std::make_unique<FrontendProject>();
	_page->LoadP3D(kProject, "Loading.pag");
	// The level's picture, which the game puts in at run time: the page holds
	// a blank in its place.
	_page->SetSpriteVisible("Loading", false);
	float fireX, fireY, fireWidth, fireHeight;
	_hasFuse = _page->GetSpriteBox("Wick", _wickX, _wickY, _wickWidth, _wickHeight) &&
	           _page->GetSpriteBox("Fire", fireX, fireY, fireWidth, fireHeight);
}

LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::Show(int level)
{
	if (!_page)
		return;
	// The level's picture, in the place the page leaves blank for it
	// (dynaload/images/loading<n>.p3d: a sprite in tiles).
	const std::string picture = fmt::format("./art/frontend/dynaload/images/loading{}.p3d", level);
	const std::string pictureName = fmt::format("loading{}.png", level);
	// Shown while the level it was loaded in is left and freed.
	const ResourceManager::PermanentScope permanent(_resources);
	if (!_resources.GetTexture(pictureName) && FileSystem::exists(picture))
	{
		const P3D::P3DFile file(picture);
		for (const auto& chunk : file.GetRoot().GetChildren())
			if (chunk->IsType(P3D::ChunkType::Sprite))
				_resources.LoadTexture(*P3D::Sprite::Load(*chunk));
	}
	if (Texture* texture = _resources.GetTexture(pictureName))
		_page->SetSpriteTexture("Loading", texture);
	else
		_page->SetSpriteVisible("Loading", false);
	SetProgress(0.0f);
}

void LoadingScreen::SetProgress(float fraction)
{
	if (!_page)
		return;
	_progress = std::clamp(fraction, 0.0f, 1.0f);
	// The flame at the wick's left end, burning its way right to the dynamite.
	if (_hasFuse)
	{
		const float burnt = _wickWidth * _progress;
		_page->SetSpriteBox("Wick", _wickX + burnt, _wickY, _wickWidth - burnt, _wickHeight);
		_page->MoveSprite("Fire", burnt - _fireMoved, 0.0f);
		_fireMoved = burnt;
	}
	draw();
}

void LoadingScreen::draw()
{
	// The window stays answerable while the load holds the loop up.
	SDL_PumpEvents();
	int width = 0, height = 0;
	SDL_GL_GetDrawableSize(_window, &width, &height);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, width, height);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	_page->Draw(Matrix4x4::MakeOrtho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f));
	_window.Swap();
	glEnable(GL_DEPTH_TEST);
}

} // namespace Donut
