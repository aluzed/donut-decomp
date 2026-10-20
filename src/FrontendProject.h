// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Render/OpenGL/glad/glad.h"
#include <Render/SpriteBatch.h>
#include <cstdint>
#include <memory>
#include <string>

namespace Donut
{
class ResourceManager;
class Texture;

class Font;

namespace P3D
{
class P3DChunk;
class FrontendGroup;
class FrontendMultiSprite;
class FrontendMultiText;
class FrontendPage;
class FrontendPolygon;
} // namespace P3D

class FrontendProject
{
public:
	FrontendProject();
	~FrontendProject();

	// Loads every page in the project, or only the one named `pageName` when given
	// (page names are the .pag entries inside the frontend P3D).
	void LoadP3D(const std::string& filename, const std::string& pageName = "");
	void Draw(const Matrix4x4& proj);

	// A text of the page, by name ("MainMenu"): shows `text` in place of its
	// own strings; empty, its own again.
	void SetText(const std::string& name, const std::string& text);
	// Its text in lines as wide as its box, each as high as the box: a card's
	// description, a quote.
	void SetTextWrapped(const std::string& name, const std::string& text);
	// A text's colour, in place of its own (an entry highlighted); its own
	// again with `reset`.
	void SetTextColor(const std::string& name, const Vector4& color);
	void ResetTextColor(const std::string& name);
	// Shows or hides a sprite of the page (one the game fills at run time, as
	// the loading screen's level picture; its text "Loading" is another).
	void SetSpriteVisible(const std::string& name, bool visible);
	// Draws only its left `fraction`, 0 to 1: a slider's bar at its value.
	void SetSpriteFill(const std::string& name, float fraction);
	// Its box in the page (top left, y down), and drawing it there, at that
	// size, rather than at its image's.
	bool GetSpriteBox(const std::string& name, float& x, float& y, float& width, float& height) const;
	void SetSpriteBox(const std::string& name, float x, float y, float width, float height);
	// Drawn last, over the others: a slider's knob over its bar.
	void BringSpriteToFront(const std::string& name);
	// Its texts shrunk to their boxes' height too, not only their width: the
	// keyboard page's, 15 high, in a font twice that.
	void SetTextsFitHeight(bool fit) { _fitHeight = fit; }
	// Shows `texture` in a sprite's place, filling its box (the loading
	// screen's level picture, which the page leaves blank).
	void SetSpriteTexture(const std::string& name, Texture* texture);
	// Where an element of the page is drawn, in the page's pixels (top left,
	// y down; the page is drawn at `scale`): false when it has none of that
	// name.
	bool GetRect(const std::string& name, float& x, float& y, float& width, float& height) const;
	// The page's 640 x 480 on the screen: as large as it fits, centred. Kept
	// from the last draw, for the mouse.
	mutable float _scale = 2.0f;
	bool _fitHeight = false;
	mutable Vector2 _offset = Vector2::Zero;

private:
	void AddGroup(const P3D::FrontendGroup& group, int32_t resX, int32_t resY);
	void AddMultiSprite(const P3D::FrontendMultiSprite& multiSprite, int32_t resX, int32_t resY);
	void AddMultiText(const P3D::FrontendMultiText& multiText, int32_t resX, int32_t resY);
	// A polygon (the pause screen's dark overlay, the loading screen's fuse):
	// drawn as the rectangle its points span, in their first colour.
	void AddPolygon(const P3D::FrontendPolygon& polygon, int32_t resX, int32_t resY);
	std::unique_ptr<Texture> _white;
	// The page's images (images\LArrowG.png), from the frontend's resource
	// directory beside the project file, by their names.
	void loadImages(const P3D::FrontendPage& page);
	// The text styles its pages name (FrontendTextStyleResource, 0x18104:
	// "fontB_16" is fonts\\font0_16.p3d), registered as fonts by that name.
	void loadTextStyles(const P3D::P3DChunk& chunk);
	std::string _resourceDir;

	struct Sprite
	{
		Texture* texture;
		int32_t positionX;
		int32_t positionY;
		int32_t width;
		int32_t height;
		Vector4 color;
		std::string name;
		bool visible = true;
		// Its box in the page, top left, y down.
		int32_t boxX = 0, boxY = 0, boxWidth = 0, boxHeight = 0;
		// How much of it, from the left, is drawn: a slider's bar.
		float fill = 1.0f;
	};
	// A text: its strings (the text bible's, by key) and which shows, in its
	// font, aligned in its box (top left, y down).
	struct Text
	{
		std::string name;
		const Font* font;
		std::vector<std::string> strings;
		std::size_t current;
		int32_t x, y, width, height;
		uint32_t alignX, alignY;
		Vector4 color;
		std::string shown;
		Vector4 ownColor;
		// In lines (SetTextWrapped), each its box's height.
		bool wrapped = false;
	};
	std::vector<Text> _texts;

	SpriteBatch _spriteBatch;
	std::vector<Sprite> _sprites;

	GLuint _sampler;
};
} // namespace Donut
