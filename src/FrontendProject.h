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

namespace P3D
{
class FrontendGroup;
class FrontendMultiSprite;
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

private:
	void AddGroup(const P3D::FrontendGroup& group, int32_t resX, int32_t resY);
	void AddMultiSprite(const P3D::FrontendMultiSprite& multiSprite, int32_t resX, int32_t resY);

	struct Sprite
	{
		Texture* texture;
		int32_t positionX;
		int32_t positionY;
		int32_t width;
		int32_t height;
		Vector4 color;
	};

	SpriteBatch _spriteBatch;
	std::vector<Sprite> _sprites;

	GLuint _sampler;
};
} // namespace Donut
