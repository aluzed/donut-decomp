#pragma once

#include <memory>

namespace Donut
{

class FrontendProject;
class ResourceManager;
class Window;

// The game's loading screen (backend.p3d's Loading.pag): the level's
// newspaper, "LOADING", and a fuse burning down to the dynamite as the level
// loads. The load holds the game up, so each step of it draws a frame.
class LoadingScreen
{
public:
	LoadingScreen(Window& window, ResourceManager& resources);
	~LoadingScreen();

	bool IsLoaded() const { return _page != nullptr; }

	// Level `level`'s picture (dynaload/images/loading<n>.p3d), the fuse whole.
	void Show(int level);
	// The load `fraction` done, 0 to 1: the fuse burnt that far.
	void SetProgress(float fraction);

private:
	void draw();

	Window& _window;
	ResourceManager& _resources;
	std::unique_ptr<FrontendProject> _page;
	// The wick and the flame at its end, as the page has them, the fuse whole.
	float _wickX = 0.0f, _wickY = 0.0f, _wickWidth = 0.0f, _wickHeight = 0.0f;
	// How far the flame has been moved along it.
	float _fireMoved = 0.0f;
	bool _hasFuse = false;
	float _progress = 0.0f;
};

} // namespace Donut
