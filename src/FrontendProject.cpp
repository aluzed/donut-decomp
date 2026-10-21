// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <Core/Log.h>
#include <Entity.h>
#include <FrontendProject.h>
#include <Game.h>
#include <P3D/P3D.generated.h>
#include <P3D/P3DFile.h>
#include <Render/Font.h>
#include <Render/OpenGL/GLTexture2D.h>
#include <Render/Texture.h>
#include <ResourceManager.h>
#include "Core/FileSystem.h"
#include "Core/MemoryStream.h"
#include "ThirdParty/stb_image.h"
#include "UI/TextBible.h"

#include <set>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Donut
{
enum class Alignment
{
	Left,
	Right,
	Top,
	Bottom,
	Center
};

namespace
{
// The pages alive, for RefreshAllTexts.
std::set<FrontendProject*>& livePages()
{
	static std::set<FrontendProject*> pages;
	return pages;
}
} // namespace

FrontendProject::FrontendProject()
{
	livePages().insert(this);
	glGenSamplers(1, &_sampler);
	glSamplerParameteri(_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri(_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

FrontendProject::~FrontendProject()
{
	livePages().erase(this);
	if (_sampler != 0)
	{
		glDeleteSamplers(1, &_sampler);
	}
}

void FrontendProject::AddMultiSprite(const P3D::FrontendMultiSprite& multiSprite, int32_t resX, int32_t resY)
{
	auto texture = Game::GetInstance().GetResourceManager().GetTexture(multiSprite.GetImageNames()[0] + ".png");
	auto dimX = (int32_t)multiSprite.GetDimensionX();
	auto dimY = (int32_t)multiSprite.GetDimensionY();
	Alignment alignX = (Alignment)multiSprite.GetAlignX();
	Alignment alignY = (Alignment)multiSprite.GetAlignY();

	int32_t x = multiSprite.GetPositionX();
	int32_t y = resY - multiSprite.GetPositionY();
	int32_t w = texture->GetWidth();
	int32_t h = texture->GetHeight();

	if (alignX == Alignment::Right)
	{
		x += dimX - w;
	}
	if (alignX == Alignment::Center)
	{
		x += ((dimX - w) * 0.5f);
		if (dimX > w && alignY != Alignment::Center)
		{
			x -= (dimX * 0.5f) - w;
			w = dimX - w;
		}
	}

	if (alignY == Alignment::Top)
	{
		y -= dimY;
	}
	else if (alignY == Alignment::Bottom)
	{
		y -= h;
	}
	else if (alignY == Alignment::Center)
	{
		y -= dimY - ((dimY - h) * 0.5f);
		if (dimY > h && alignX != Alignment::Center)
		{
			y -= (dimY * 0.5f) - h;
			h = dimY - h;
		}
	}

	_sprites.push_back(Sprite {
	    texture,
	    x,
	    y,
	    w,
	    h,
	    P3D::P3DUtil::ConvertColor(multiSprite.GetColor()),
	    multiSprite.GetName(),
	    true,
	    multiSprite.GetPositionX(),
	    resY - multiSprite.GetPositionY() - dimY,
	    dimX,
	    dimY,
	});
}

void FrontendProject::AddMultiText(const P3D::FrontendMultiText& multiText, int32_t resX, int32_t resY)
{
	auto& resources = Game::GetInstance().GetResourceManager();
	// The page names its text style ("font0_24"), the font file its font
	// ("boulder_24"): both are registered.
	const Font* font = resources.GetFont(multiText.GetFontName());
	if (!font)
		return;
	Text text {multiText.GetName(), font, {}, multiText.GetCurrent()};
	const auto& bible = Game::GetInstance().GetTextBible();
	for (const auto& string : multiText.GetTextBibles())
	{
		const std::string& phrase = bible.Get(string->GetKey());
		text.strings.push_back(phrase.empty() ? string->GetKey() : phrase);
		text.keys.push_back(string->GetKey());
	}
	// Its box: the position is its bottom left, y up.
	text.width = static_cast<int32_t>(multiText.GetDimensionX());
	text.height = static_cast<int32_t>(multiText.GetDimensionY());
	text.x = multiText.GetPositionX();
	text.y = resY - multiText.GetPositionY() - text.height;
	text.alignX = multiText.GetAlignX();
	text.alignY = multiText.GetAlignY();
	text.color = P3D::P3DUtil::ConvertColor(multiText.GetColor());
	text.ownColor = text.color;
	_texts.push_back(std::move(text));
}

void FrontendProject::AddPolygon(const P3D::FrontendPolygon& polygon, int32_t resX, int32_t resY)
{
	const auto& points = polygon.GetPoints();
	if (points.empty())
		return;
	if (!_white)
	{
		const uint8_t pixel[4] = {255, 255, 255, 255};
		_white = std::make_unique<Texture>(pixel, 1, 1, "frontend_white");
	}
	float left = points[0].X, right = points[0].X, bottom = points[0].Y, top = points[0].Y;
	for (const auto& point : points)
	{
		left = std::min(left, point.X);
		right = std::max(right, point.X);
		bottom = std::min(bottom, point.Y);
		top = std::max(top, point.Y);
	}
	const Vector4 color = polygon.GetColors().empty() ? Vector4(1.0f, 1.0f, 1.0f, 1.0f)
	                                                  : P3D::P3DUtil::ConvertColor(polygon.GetColors()[0]);
	// Points y up, as the sprites'; its box where it is drawn (the loading
	// screen's wick burns down by it).
	Sprite sprite {_white.get(),
	               static_cast<int32_t>(left),
	               static_cast<int32_t>(resY - top),
	               static_cast<int32_t>(right - left),
	               static_cast<int32_t>(top - bottom),
	               color,
	               polygon.GetName()};
	sprite.boxX = sprite.positionX;
	sprite.boxY = sprite.positionY;
	sprite.boxWidth = sprite.width;
	sprite.boxHeight = sprite.height;
	_sprites.push_back(std::move(sprite));
}

void FrontendProject::loadImages(const P3D::FrontendPage& page)
{
	auto& resources = Game::GetInstance().GetResourceManager();
	for (const auto& image : page.GetImageResources())
	{
		const std::string name = image->GetName() + ".png";
		if (resources.GetTexture(name))
			continue;
		std::string path = image->GetFilepath();
		path = path.substr(0, path.find('\0'));
		std::replace(path.begin(), path.end(), '\\', '/');
		int width = 0, height = 0, channels = 0;
		uint8_t* pixels = stbi_load((_resourceDir + path).c_str(), &width, &height, &channels, 4);
		if (!pixels)
		{
			Log::Warn("Frontend: no image '{}'", _resourceDir + path);
			continue;
		}
		resources.AddTexture(name, std::make_unique<Texture>(pixels, width, height, name));
		stbi_image_free(pixels);
	}
}

void FrontendProject::SetSpriteTexture(const std::string& name, Texture* texture)
{
	for (auto& sprite : _sprites)
		if (sprite.name == name && texture)
		{
			sprite.texture = texture;
			sprite.positionX = sprite.boxX;
			sprite.positionY = sprite.boxY;
			sprite.width = sprite.boxWidth;
			sprite.height = sprite.boxHeight;
			sprite.visible = true;
		}
}

void FrontendProject::SetSpriteVisible(const std::string& name, bool visible)
{
	for (auto& sprite : _sprites)
		if (sprite.name == name)
			sprite.visible = visible;
}

void FrontendProject::SetSpriteColor(const std::string& name, const Vector4& color)
{
	for (auto& sprite : _sprites)
		if (sprite.name == name)
			sprite.color = color;
}

void FrontendProject::RenameSprite(const std::string& name, const std::string& newName)
{
	for (auto& sprite : _sprites)
		if (sprite.name == name)
		{
			sprite.name = newName;
			return;
		}
}

void FrontendProject::SetSpriteFill(const std::string& name, float fraction)
{
	for (auto& sprite : _sprites)
		if (sprite.name == name)
			sprite.fill = std::clamp(fraction, 0.0f, 1.0f);
}

bool FrontendProject::GetSpriteBox(const std::string& name, float& x, float& y, float& width, float& height) const
{
	for (const auto& sprite : _sprites)
		if (sprite.name == name)
		{
			x = static_cast<float>(sprite.boxX);
			y = static_cast<float>(sprite.boxY);
			width = static_cast<float>(sprite.boxWidth);
			height = static_cast<float>(sprite.boxHeight);
			return true;
		}
	return false;
}

void FrontendProject::SetSpriteBox(const std::string& name, float x, float y, float width, float height)
{
	for (auto& sprite : _sprites)
		if (sprite.name == name)
		{
			sprite.positionX = sprite.boxX = static_cast<int32_t>(std::lround(x));
			sprite.positionY = sprite.boxY = static_cast<int32_t>(std::lround(y));
			sprite.width = sprite.boxWidth = static_cast<int32_t>(std::lround(width));
			sprite.height = sprite.boxHeight = static_cast<int32_t>(std::lround(height));
		}
}

void FrontendProject::MoveSprite(const std::string& name, float dx, float dy)
{
	const auto x = static_cast<int32_t>(std::lround(dx)), y = static_cast<int32_t>(std::lround(dy));
	for (auto& sprite : _sprites)
		if (sprite.name == name)
		{
			sprite.positionX += x;
			sprite.boxX += x;
			sprite.positionY += y;
			sprite.boxY += y;
		}
}

void FrontendProject::BringSpriteToFront(const std::string& name)
{
	std::stable_partition(_sprites.begin(), _sprites.end(), [&](const Sprite& sprite) { return sprite.name != name; });
}

void FrontendProject::loadTextStyles(const P3D::P3DChunk& chunk)
{
	auto& resources = Game::GetInstance().GetResourceManager();
	for (const auto& child : chunk.GetChildren())
	{
		if (child->IsType(P3D::ChunkType::FrontendTextStyleResource))
		{
			// Name, version, font file, the font's own name.
			MemoryStream stream(child->GetData());
			std::string name = stream.ReadLPString();
			name = name.substr(0, name.find('\0'));
			stream.Read<uint32_t>();
			std::string file = stream.ReadLPString();
			file = file.substr(0, file.find('\0'));
			std::replace(file.begin(), file.end(), '\\', '/');
			if (resources.GetFont(name) || !FileSystem::exists(_resourceDir + file))
				continue;
			const P3D::P3DFile fonts(_resourceDir + file);
			for (const auto& font : fonts.GetRoot().GetChildren())
				if (font->IsType(P3D::ChunkType::TextureFont))
				{
					resources.AddFont(name, std::make_unique<Font>(*P3D::TextureFont::Load(*font)));
					break;
				}
		}
		loadTextStyles(*child);
	}
}

void FrontendProject::SetTextColor(const std::string& name, const Vector4& color)
{
	for (auto& text : _texts)
		if (text.name == name)
			text.color = color;
}

void FrontendProject::ResetTextColor(const std::string& name)
{
	for (auto& text : _texts)
		if (text.name == name)
			text.color = text.ownColor;
}

void FrontendProject::SetText(const std::string& name, const std::string& text)
{
	for (auto& entry : _texts)
		if (entry.name == name)
		{
			entry.shown = text;
			entry.wrapped = false;
		}
}

bool FrontendProject::SetTextPosition(const std::string& name, float x, float y)
{
	bool found = false;
	for (auto& entry : _texts)
		if (entry.name == name)
		{
			entry.x = static_cast<int32_t>(std::lround(x));
			entry.y = static_cast<int32_t>(std::lround(y));
			found = true;
		}
	return found;
}

bool FrontendProject::SetTextBox(const std::string& name, float x, float y, float width, float height, float lineHeight)
{
	bool found = false;
	for (auto& entry : _texts)
		if (entry.name == name)
		{
			entry.x = static_cast<int32_t>(std::lround(x));
			entry.y = static_cast<int32_t>(std::lround(y));
			entry.width = static_cast<int32_t>(std::lround(width));
			entry.height = static_cast<int32_t>(std::lround(height));
			entry.alignY = static_cast<uint32_t>(Alignment::Center);
			entry.lineHeight = lineHeight;
			found = true;
		}
	return found;
}

bool FrontendProject::GetTextBox(const std::string& name, float& x, float& y, float& width, float& height) const
{
	for (const auto& entry : _texts)
		if (entry.name == name)
		{
			x = static_cast<float>(entry.x);
			y = static_cast<float>(entry.y);
			width = static_cast<float>(entry.width);
			height = static_cast<float>(entry.height);
			return true;
		}
	return false;
}

void FrontendProject::SetTextIndex(const std::string& name, std::size_t index)
{
	for (auto& entry : _texts)
		if (entry.name == name)
		{
			entry.current = index;
			entry.shown.clear();
			entry.wrapped = false;
		}
}

void FrontendProject::SetTextWrapped(const std::string& name, const std::string& text)
{
	for (auto& entry : _texts)
		if (entry.name == name && entry.font && entry.height > 0)
		{
			entry.shown = text.empty() ? " " : text;
			entry.wrapped = true;
		}
}

namespace
{
// `text` broken into lines no wider than `most` in `font`, at word breaks (a
// word longer than that alone on its line).
std::vector<std::string> wrapLines(const Font& font, const std::string& text, float most)
{
	std::vector<std::string> lines;
	std::string line;
	std::size_t at = 0;
	while (at < text.size())
	{
		const std::size_t end = std::min(text.find(' ', at), text.size());
		const std::string word = text.substr(at, end - at);
		at = end + 1;
		const std::string longer = line.empty() ? word : line + " " + word;
		if (!line.empty() && font.MeasureWidth(longer) > most)
		{
			lines.push_back(line);
			line = word;
		}
		else
			line = longer;
	}
	if (!line.empty())
		lines.push_back(line);
	return lines;
}
} // namespace

void FrontendProject::RefreshAllTexts()
{
	const auto& bible = Game::GetInstance().GetTextBible();
	for (FrontendProject* page : livePages())
		for (auto& text : page->_texts)
			for (std::size_t i = 0; i < text.keys.size() && i < text.strings.size(); ++i)
			{
				const std::string& phrase = bible.Get(text.keys[i]);
				text.strings[i] = phrase.empty() ? text.keys[i] : phrase;
			}
}

bool FrontendProject::CopySprite(const std::string& name, const std::string& newName, float dy)
{
	for (std::size_t i = 0; i < _sprites.size(); ++i)
		if (_sprites[i].name == name)
		{
			Sprite copy = _sprites[i];
			copy.name = newName;
			copy.positionY += static_cast<int32_t>(std::lround(dy));
			copy.boxY += static_cast<int32_t>(std::lround(dy));
			_sprites.push_back(copy);
			return true;
		}
	return false;
}

bool FrontendProject::CopyText(const std::string& name, const std::string& newName, float dy)
{
	for (std::size_t i = 0; i < _texts.size(); ++i)
		if (_texts[i].name == name)
		{
			Text copy = _texts[i];
			copy.name = newName;
			copy.y += static_cast<int32_t>(std::lround(dy));
			_texts.push_back(copy);
			return true;
		}
	return false;
}

bool FrontendProject::GetRect(const std::string& name, float& x, float& y, float& width, float& height) const
{
	for (const auto& sprite : _sprites)
		if (sprite.name == name)
		{
			x = sprite.positionX * _scale + _offset.X;
			y = sprite.positionY * _scale + _offset.Y;
			width = sprite.width * _scale;
			height = sprite.height * _scale;
			return true;
		}
	for (const auto& text : _texts)
		if (text.name == name)
		{
			x = text.x * _scale + _offset.X;
			y = text.y * _scale + _offset.Y;
			width = text.width * _scale;
			height = text.height * _scale;
			return true;
		}
	return false;
}

void FrontendProject::AddGroup(const P3D::FrontendGroup& group, int32_t resX, int32_t resY)
{
	for (const auto& multiSprite : group.GetMultiSprites()) { AddMultiSprite(*multiSprite, resX, resY); }

	for (const auto& polygon : group.GetPolygons()) { AddPolygon(*polygon, resX, resY); }
	for (const auto& multiText : group.GetMultiTexts()) { AddMultiText(*multiText, resX, resY); }

	for (const auto& group : group.GetChildren()) { AddGroup(*group, resX, resY); }
}

void FrontendProject::LoadP3D(const std::string& filename, const std::string& pageName)
{
	if (!FileSystem::exists(filename))
	{
		Log::Info("Frontend Project not found: {}", filename);
		return;
	}

	Log::Info("Loading Frontend Project: {}", filename);

	// Its resources sit beside it (scrooby2/resource/): the pages' images, and
	// the fonts their texts name, by style and by font.
	_resourceDir = filename.substr(0, filename.find_last_of('/') + 1) + "resource/";
	if (FileSystem::exists(_resourceDir + "fonts"))
	{
		auto& resources = Game::GetInstance().GetResourceManager();
		for (const auto& entry : FileSystem::directory_iterator(_resourceDir + "fonts"))
		{
			if (entry.path().extension() != ".p3d")
				continue;
			const P3D::P3DFile fonts(entry.path().string());
			for (const auto& chunk : fonts.GetRoot().GetChildren())
				if (chunk->IsType(P3D::ChunkType::TextureFont))
				{
					const auto p3dFont = P3D::TextureFont::Load(*chunk);
					for (const std::string& name : {p3dFont->GetName(), entry.path().stem().string()})
						if (!resources.GetFont(name))
							resources.AddFont(name, std::make_unique<Font>(*p3dFont));
				}
		}
	}

	const auto p3d = P3D::P3DFile(filename);
	int pagesLoaded = 0;
	const auto& root = p3d.GetRoot();
	for (const auto& chunk : root.GetChildren())
	{
		switch (chunk->GetType())
		{
		case P3D::ChunkType::FrontendProject:
		{
			loadTextStyles(*chunk);
			auto project = P3D::FrontendProject::Load(*chunk);
			auto resX = (int32_t)project->GetResX();
			auto resY = (int32_t)project->GetResY();

			for (const auto& page : project->GetPages())
			{
				if (!pageName.empty() && page->GetName() != pageName)
					continue;

				loadImages(*page);
				for (const auto& layer : page->GetLayers())
				{
					for (const auto& polygon : layer->GetPolygons()) { AddPolygon(*polygon, resX, resY); }
					for (const auto& group : layer->GetGroups()) { AddGroup(*group, resX, resY); }

					for (const auto& multiSprite : layer->GetMultiSprites()) { AddMultiSprite(*multiSprite, resX, resY); }
					for (const auto& multiText : layer->GetMultiTexts()) { AddMultiText(*multiText, resX, resY); }
				}

				++pagesLoaded;
			}

			if (!pageName.empty() && pagesLoaded == 0)
				Log::Warn("Frontend: no page named '{}' in {}", pageName, filename);
			break;
		}
		case P3D::ChunkType::Sprite:
		{
			auto sprite = P3D::Sprite::Load(*chunk);
			Game::GetInstance().GetResourceManager().LoadTexture(*sprite);

			break;
		}
		case P3D::ChunkType::TextureFont:
		{
			auto p3dFont = P3D::TextureFont::Load(*chunk);
			auto font = std::make_unique<Font>(*p3dFont);
			Game::GetInstance().GetResourceManager().AddFont(p3dFont->GetName(), std::move(font));
			break;
		}
		}
	}
}

void FrontendProject::Draw(const Matrix4x4& proj)
{
	for (const auto& sprite : _sprites)
	{
		if (!sprite.visible)
			continue;
		if (sprite.fill < 1.0f)
		{
			if (sprite.fill > 0.0f)
				_spriteBatch.Draw(sprite.texture, Vector2(sprite.positionX, sprite.positionY), Vector2(0.0f, 0.0f),
				                  Vector2(sprite.fill, 1.0f), Vector2(sprite.width * sprite.fill, sprite.height), sprite.color);
			continue;
		}
		_spriteBatch.Draw(sprite.texture, Vector2(sprite.positionX, sprite.positionY), Vector2(sprite.width, sprite.height),
		                  sprite.color);
	}

	for (const auto& text : _texts)
	{
		const std::string& value =
		    !text.shown.empty() ? text.shown
		                        : (text.current < text.strings.size() ? text.strings[text.current] : std::string());
		if (value.empty())
			continue;
		if (text.wrapped && text.width > 0 && text.height > 0)
		{
			// Measured, not guessed: the largest size, from one line filling the
			// box's height down, at which the text broken at the box's width
			// fits its height too. Each line used to be drawn the box's height,
			// one under the other: three lines stood out of the objective's
			// frame by two.
			const float fontHeight = text.font->GetHeight();
			const float tallest = text.lineHeight > 0.0f ? std::min(text.lineHeight, static_cast<float>(text.height))
			                                             : static_cast<float>(text.height);
			float scale = std::min(1.0f, tallest / fontHeight);
			std::vector<std::string> lines = wrapLines(*text.font, value, text.width / scale);
			constexpr float kSmallest = 0.35f;
			while (lines.size() * fontHeight * scale > text.height && scale > kSmallest)
			{
				scale *= 0.92f;
				lines = wrapLines(*text.font, value, text.width / scale);
			}
			// The block in its box as the text's alignment says, each line too.
			const float lineHeight = fontHeight * scale;
			float y = static_cast<float>(text.y);
			const float block = lineHeight * static_cast<float>(lines.size());
			if (static_cast<Alignment>(text.alignY) == Alignment::Center)
				y += (text.height - block) * 0.5f;
			else if (static_cast<Alignment>(text.alignY) == Alignment::Bottom)
				y += text.height - block;
			for (const std::string& line : lines)
			{
				const float width = std::min(text.font->MeasureWidth(line) * scale, static_cast<float>(text.width));
				const float lineScale = std::min(scale, text.font->MeasureWidth(line) > 0.0f
				                                            ? text.width / text.font->MeasureWidth(line)
				                                            : scale);
				float x = static_cast<float>(text.x);
				if (static_cast<Alignment>(text.alignX) == Alignment::Center)
					x += (text.width - width) * 0.5f;
				else if (static_cast<Alignment>(text.alignX) == Alignment::Right)
					x += text.width - width;
				_spriteBatch.DrawText(text.font, line, Vector2(x, y), text.color, lineScale);
				y += lineHeight;
			}
			continue;
		}
		// Shrunk to fit its box, as the game's are: "RESUME GAME" is wider in
		// its font than the main menu's box between the arrows.
		const float natural = text.font->MeasureWidth(value);
		float scale = natural > text.width && text.width > 0 ? text.width / natural : 1.0f;
		if ((_fitHeight || text.wrapped) && text.height > 0 && text.font->GetHeight() * scale > text.height)
			scale = text.height / text.font->GetHeight();
		const float width = natural * scale;
		const float height = text.font->GetHeight() * scale;
		float x = static_cast<float>(text.x);
		float y = static_cast<float>(text.y);
		if (static_cast<Alignment>(text.alignX) == Alignment::Center)
			x += (text.width - width) * 0.5f;
		else if (static_cast<Alignment>(text.alignX) == Alignment::Right)
			x += text.width - width;
		if (static_cast<Alignment>(text.alignY) == Alignment::Center)
			y += (text.height - height) * 0.5f;
		else if (static_cast<Alignment>(text.alignY) == Alignment::Bottom)
			y += text.height - height;
		_spriteBatch.DrawText(text.font, value, Vector2(x, y), text.color, scale);
	}

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	if (viewport[2] > 0 && viewport[3] > 0)
	{
		_scale = std::min(viewport[2] / 640.0f, viewport[3] / 480.0f);
		_offset = Vector2((viewport[2] - 640.0f * _scale) * 0.5f, (viewport[3] - 480.0f * _scale) * 0.5f);
	}
	glBindSampler(0, _sampler);
	_spriteBatch.Flush(proj, _scale, _offset);
}
} // namespace Donut
