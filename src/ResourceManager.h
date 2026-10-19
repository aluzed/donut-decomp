// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Donut
{

namespace P3D
{
class Geometry;
class Texture;
class Sprite;
class Shader;
class Set;
} // namespace P3D

class Texture;
class Shader;
class Mesh;
class Font;

class ResourceManager
{
public:
	ResourceManager();
	~ResourceManager();

	// no copying
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(ResourceManager const&) = delete;

	void LoadTexture(const P3D::Texture&);
	void LoadTexture(const P3D::Sprite&);
	void LoadShader(const P3D::Shader&);
	void LoadSet(const P3D::Set&);
	void LoadGeometry(const P3D::Geometry&);

	void AddTexture(const std::string& name, std::unique_ptr<Texture> texture);
	void AddFont(const std::string& name, std::unique_ptr<Font> font);

	void ImGuiDebugWindow(bool* p_open) const;

	Shader* GetShader(const std::string& name) const;
	// Without the warning GetShader gives for a missing one.
	bool HasShader(const std::string& name) const { return _shaders.count(name) > 0; }
	Texture* GetTexture(const std::string& name) const;
	Font* GetFont(const std::string& name) const;
	Mesh* GetGeometry(const std::string& name) const;
	// Every shader whose own texture is `textureName` draws `shownAs` instead;
	// shownAs = textureName puts it back. False when `shownAs` is not loaded.
	bool ShowTextureAs(const std::string& textureName, const std::string& shownAs);
	// The shader named `shader` draws the texture named `texture`. False when
	// either is not loaded.
	bool ShowShaderTexture(const std::string& shader, const std::string& texture);

	const std::unordered_map<std::string, std::unique_ptr<Font>>& GetFonts() const { return _fonts; }

protected:
	std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
	std::unordered_map<std::string, std::unique_ptr<Shader>> _shaders;
	std::unordered_map<std::string, std::unique_ptr<Mesh>> _geometries;
	std::unordered_map<std::string, std::unique_ptr<Font>> _fonts;

	// Resources a later load of the same name replaced. They are kept, not freed:
	// shaders hold raw pointers to their textures and meshes to their shaders, so
	// freeing the old one left them dangling. Reloading a car on a mission restart
	// crashed the next frame in the sky's Texture::Bind.
	std::vector<std::unique_ptr<Texture>> _retiredTextures;
	std::vector<std::unique_ptr<Shader>> _retiredShaders;
	std::vector<std::unique_ptr<Mesh>> _retiredGeometries;
};

} // namespace Donut
