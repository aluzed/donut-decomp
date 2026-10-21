// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Font.h"

#include <P3D/P3D.generated.h>
#include <P3D/P3DChunk.h>
#include <Render/Texture.h>

#include <cstdint>

namespace Donut
{
Font::Font(P3D::TextureFont& fontP3D)
{
	_height = fontP3D.GetHeight();

	for (const auto& texture : fontP3D.GetTextures()) { _textures.push_back(std::make_shared<Texture>(*texture)); }

	for (const auto& glyph : fontP3D.GetGlyphs())
	{
		_glyphs.insert({glyph.id, Glyph {glyph.textureId, glyph.bottomLeftX, glyph.bottomLeftY, glyph.topRightX,
		                                 glyph.topRightY, glyph.leftBearing, glyph.rightBearing, glyph.width, glyph.advance}});
	}
}

bool Font::TryGetGlyph(int32_t id, Font::Glyph& glyph) const
{
	if (_glyphs.find(id) != _glyphs.end())
	{
		glyph = _glyphs.at(id);
		return true;
	}
	// A Latin-1 accented letter the font lacks (font0_16 has no ù): its
	// letter without the accent.
	static const char kBase[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
	if (id >= 0xC0 && id <= 0xFF && _glyphs.find(kBase[id - 0xC0]) != _glyphs.end())
	{
		glyph = _glyphs.at(kBase[id - 0xC0]);
		return true;
	}
	// The French quotes, as plain ones.
	if ((id == 0xAB || id == 0xBB) && _glyphs.find('"') != _glyphs.end())
	{
		glyph = _glyphs.at('"');
		return true;
	}
	return false;
}

float Font::MeasureWidth(const std::string& text) const
{
	float width = 0.0f;
	float lineWidth = 0.0f;
	Glyph glyph;

	for (const char& c : text)
	{
		if (c == '\n')
		{
			if (lineWidth > width) width = lineWidth;
			lineWidth = 0.0f;
			continue;
		}

		if (!TryGetGlyph(static_cast<unsigned char>(c), glyph))
			continue;

		lineWidth += glyph.advance;
	}

	return lineWidth > width ? lineWidth : width;
}

} // namespace Donut
