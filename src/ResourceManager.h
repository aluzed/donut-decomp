// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <cstdint>
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

	// Every texture, shader and geometry belongs to the section current when
	// it was loaded ("boot", "level 3", "minigame track 2"). Logs what each
	// section's resources weigh and which of them were asked for during the
	// section being left, frees that section's (the boot's stay), then makes
	// `name` the current one. Whatever holds a raw pointer to one of a level's
	// resources must be gone by then: the level, its characters and cars.
	void BeginSection(const std::string& name);

	// While one lives, what is loaded or asked for belongs to the boot's
	// section, kept for good: the menus, the HUD and the loading screen hold
	// raw pointers to theirs past a level's end.
	class PermanentScope
	{
	public:
		explicit PermanentScope(ResourceManager& resources): _resources(resources) { ++_resources._permanent; }
		~PermanentScope() { --_resources._permanent; }
		PermanentScope(const PermanentScope&) = delete;
		PermanentScope& operator=(const PermanentScope&) = delete;

	private:
		ResourceManager& _resources;
	};

	// `resource` kept for good, with the boot's: a page shows it.
	void Keep(const void* resource);

	// Counts the sections freed: a mesh's shaders, looked up once, are looked
	// up again when it changes.
	uint64_t GetGeneration() const { return _generation; }
	// That table, as of now: per section, its resources live and replaced,
	// and those asked for in the current section; then the process's memory.
	std::string SectionReport() const;

	Shader* GetShader(const std::string& name) const;
	// The shader of that name loaded nearest to `near` (a load index): the one
	// of the same file as what asks for it. The same names come back from
	// file to file -- the living room's "treeshadow_m" and its texture are
	// white, the levels' dark -- and the one loaded last took every file's
	// place.
	Shader* GetShader(const std::string& name, uint64_t near) const;
	// How many textures and shaders have been loaded: where a file's are.
	uint64_t GetLoadIndex() const { return _loadIndex; }
	// Without the warning GetShader gives for a missing one.
	bool HasShader(const std::string& name) const;
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
	// Every texture and shader loaded under each name, in load order, with
	// its load index; and the next index.
	std::unordered_map<std::string, std::vector<std::pair<uint64_t, Texture*>>> _textureLoads;
	std::unordered_map<std::string, std::vector<std::pair<uint64_t, Shader*>>> _shaderLoads;
	uint64_t _loadIndex = 0;
	// Each P3D texture's picture loaded under each name, and the texture made
	// of it.
	std::unordered_map<std::string, std::vector<std::pair<std::size_t, Texture*>>> _textureHashes;
	void addTexture(const std::string& name, std::unique_ptr<Texture> texture);
	// A shader's texture, once: the one of its name loaded nearest to it.
	void resolveTexture(Shader& shader) const;
	// What a shader with no texture, or one not loaded, draws on.
	mutable std::unique_ptr<Texture> _white;

	enum class Kind : uint8_t
	{
		Texture,
		Shader,
		Geometry
	};
	struct Record
	{
		Kind kind;
		std::size_t section;
		// The last section it was loaded or asked for in.
		std::size_t lastAsked;
		std::size_t cpuBytes, gpuBytes;
	};
	std::vector<std::string> _sections = {"boot"};
	mutable std::unordered_map<const void*, Record> _records;
	void record(const void* resource, Kind kind, std::size_t cpuBytes, std::size_t gpuBytes);
	void asked(const void* resource) const;
	void freeSection(std::size_t section);
	int _permanent = 0;
	uint64_t _generation = 0;
};

} // namespace Donut
