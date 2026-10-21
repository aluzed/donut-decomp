#include "UI/ScreenText.h"

#include "Input/Keymap.h"
#include "Render/Font.h"
#include "Render/SpriteBatch.h"
#include "UI/GameMenu.h"
#include "UI/GameText.h"

#include <fmt/format.h>

#include <utility>
#include <vector>

// windows.h maps DrawText to DrawTextA, which collides with SpriteBatch::DrawText
#undef DrawText

namespace Donut
{

float CentredTextX(const Font* font, const std::string& text, float viewportWidth)
{
	const float width = font != nullptr ? font->MeasureWidth(text) : 0.0f;
	return (viewportWidth - width) * 0.5f;
}

std::string ControlLabel(GameAction action, bool preferGamepad)
{
	const Keymap& km = Input::GetKeymap();
	const auto& binds = BindingsFor(km, action);
	const InputSource* chosen = nullptr;
	for (const auto& src : binds)
	{
		const bool isPad = src.kind == InputSourceKind::GamepadButton || src.kind == InputSourceKind::GamepadAxis;
		if (isPad == preferGamepad)
		{
			chosen = &src;
			break;
		}
	}
	if (!chosen && !binds.empty())
		chosen = &binds.front();
	return chosen ? KeymapConfig::HumanLabel(*chosen) : "—";
}

void DrawTextMenu(SpriteBatch& sprites, const Font* font, GameMenu& menu, const std::string& title,
                  const Vector4& titleColour, const Vector4& idle, const Vector4& hover, int viewportWidth,
                  int viewportHeight, int mouseX, int mouseY)
{
	const float lineHeight = font != nullptr ? font->GetHeight() : 16.0f;
	const float spacing = lineHeight + 14.0f;
	const float top = viewportHeight / 2.0f - 40.0f;

	sprites.DrawText(font, title, Vector2(CentredTextX(font, title, viewportWidth), top - spacing * 1.5f), titleColour);

	// Laid out first, then hit-tested: hover and click must use the rects the
	// labels are actually drawn at, not the placeholder passed to AddButton.
	for (size_t i = 0; i < menu.GetButtons().size(); ++i)
	{
		const auto& btn = menu.GetButtons()[i];
		const float width = font != nullptr ? font->MeasureWidth(btn.label) : btn.width;
		menu.SetButtonRect(i, CentredTextX(font, btn.label, viewportWidth), top + i * spacing, width, lineHeight);
	}
	menu.Update(mouseX, mouseY);
	for (const auto& btn : menu.GetButtons())
		sprites.DrawText(font, btn.label, Vector2(btn.x, btn.y), btn.hovered ? hover : idle);
}

void DrawMissionResult(SpriteBatch& sprites, const Font* font, const MissionResult& result, int viewportWidth,
                       int viewportHeight)
{
	const float w = static_cast<float>(viewportWidth), h = static_cast<float>(viewportHeight);
	const Vector4 gold(1.0f, 0.84f, 0.0f, 1.0f), grey(0.6f, 0.6f, 0.6f, 1.0f), white(1.0f, 1.0f, 1.0f, 1.0f);
	// The lines one under the other, as high as the font is, each centred and
	// the whole in the middle of the screen. They stood at fixed offsets 20
	// pixels apart, in a font taller than that: the time and the record
	// overlapped.
	std::vector<std::pair<std::string, Vector4>> lines;
	if (result.complete)
	{
		// The level's last story mission: the level is won too.
		if (!result.levelComplete.empty())
			lines.emplace_back(result.levelComplete, Vector4(1.0f, 0.5f, 0.1f, 1.0f));
		if (result.newRecord)
			lines.emplace_back(GameText("result.record", result.language), gold);
		lines.emplace_back(result.title, gold);
		lines.emplace_back(fmt::format(fmt::runtime(GameText("result.time", result.language)), result.elapsed), white);
		if (result.best < 999.0f)
			lines.emplace_back(fmt::format(fmt::runtime(GameText("result.best", result.language)), result.best),
			                   result.elapsed <= result.best ? gold : Vector4(0.5f, 0.5f, 0.5f, 1.0f));
		lines.emplace_back(fmt::format(fmt::runtime(GameText("result.next", result.language)), result.nextIn), grey);
	}
	else
	{
		lines.emplace_back(result.title, Vector4(1.0f, 0.2f, 0.2f, 1.0f));
		// Why, and the game's advice for it.
		if (!result.reason.empty())
			lines.emplace_back(result.reason, Vector4(1.0f, 0.6f, 0.6f, 1.0f));
		if (!result.hint.empty())
			lines.emplace_back(result.hint, white);
		lines.emplace_back(fmt::format(fmt::runtime(GameText("result.retry", result.language)), result.nextIn), grey);
	}
	const float step = (font != nullptr ? static_cast<float>(font->GetHeight()) : 16.0f) + 6.0f;
	float y = h / 2.0f - step * static_cast<float>(lines.size()) / 2.0f;
	for (const auto& [text, colour] : lines)
	{
		sprites.DrawText(font, text, Vector2(CentredTextX(font, text, w), y), colour);
		y += step;
	}
}

void DrawHints(SpriteBatch& sprites, const Font* font, const std::vector<std::string>& hints, const Vector4& colour,
               int fps, int viewportWidth, int viewportHeight)
{
	float y = viewportHeight * 0.32f;
	for (const auto& hint : hints)
	{
		sprites.DrawText(font, hint, Vector2(12.0f, y), colour);
		y += font != nullptr ? font->GetHeight() + 4.0f : 20.0f;
	}
	if (fps > 0)
		sprites.DrawText(font, fmt::format("{} fps", fps), Vector2(viewportWidth * 0.5f - 40.0f, 4.0f),
		                 Vector4(1.0f, 0.84f, 0.0f, 1.0f));
}

} // namespace Donut
