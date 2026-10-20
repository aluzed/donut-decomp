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
class FrontendGroup;
class FrontendMultiSprite;
class FrontendMultiText;
class FrontendPage;
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
	// Where an element of the page is drawn, in the page's pixels (top left,
	// y down; the page is drawn at `scale`): false when it has none of that
	// name.
	bool GetRect(const std::string& name, float& x, float& y, float& width, float& height) const;
	static constexpr float kScale = 2.0f;

private:
	void AddGroup(const P3D::FrontendGroup& group, int32_t resX, int32_t resY);
	void AddMultiSprite(const P3D::FrontendMultiSprite& multiSprite, int32_t resX, int32_t resY);
	void AddMultiText(const P3D::FrontendMultiText& multiText, int32_t resX, int32_t resY);
	// The page's images (images\LArrowG.png), from the frontend's resource
	// directory beside the project file, by their names.
	void loadImages(const P3D::FrontendPage& page);
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
	};
	std::vector<Text> _texts;

	SpriteBatch _spriteBatch;
	std::vector<Sprite> _sprites;

	GLuint _sampler;
};
} // namespace Donut
