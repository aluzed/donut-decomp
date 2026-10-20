// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "UI/Hud.h"

#include <sstream>
#include <vector>

#include "Render/Font.h"
#include "Render/SpriteBatch.h"
#include "Render/Texture.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Donut
{
namespace
{
constexpr float kMargin = 32.0f;
constexpr float kLinePad = 4.0f;
constexpr float kBarWidth = 180.0f;
constexpr float kBarHeight = 14.0f;

const Vector4 kPanel(0.0f, 0.0f, 0.0f, 0.55f);
const Vector4 kPanelEdge(1.0f, 1.0f, 1.0f, 0.25f);
const Vector4 kWhite(1.0f, 1.0f, 1.0f, 1.0f);
const Vector4 kGold(1.0f, 0.84f, 0.0f, 1.0f);

Vector4 healthColour(float health)
{
	if (health > 50.0f) return Vector4(0.2f, 0.9f, 0.2f, 1.0f);
	if (health > 25.0f) return Vector4(1.0f, 0.9f, 0.2f, 1.0f);
	return Vector4(1.0f, 0.2f, 0.2f, 1.0f);
}

Vector4 heatColour(float heat)
{
	if (heat > 7.0f) return Vector4(1.0f, 0.15f, 0.15f, 1.0f);
	if (heat > 3.0f) return Vector4(1.0f, 0.5f, 0.0f, 1.0f);
	return Vector4(1.0f, 1.0f, 0.2f, 1.0f);
}

float clamp01(float v)
{
	return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// boulder_16 is around 40px tall despite the name, so every vertical step is
// derived from the font rather than a hard-coded constant.
float lineHeight(const Font* font)
{
	return (font != nullptr ? font->GetHeight() : 16.0f) + kLinePad;
}
} // namespace

Hud::Hud()
{
	const uint8_t pixel[4] = {255, 255, 255, 255};
	_white = std::make_unique<Texture>(pixel, 1, 1, "hud_white");
}

Hud::~Hud() = default;

void Hud::drawShadowedText(SpriteBatch& sprites, const Font* font, const std::string& text, const Vector2& position,
                           const Vector4& colour)
{
	sprites.DrawText(font, text, position + Vector2(2.0f, 2.0f), Vector4(0.0f, 0.0f, 0.0f, colour.W * 0.8f));
	sprites.DrawText(font, text, position, colour);
}

void Hud::drawRect(SpriteBatch& sprites, const Vector2& position, const Vector2& size, const Vector4& colour)
{
	// SpriteBatch anchors sprites by their top-left corner (Flush adds half the
	// size before emitting the quad), so this maps straight through.
	sprites.Draw(_white.get(), position, size, colour);
}

void Hud::drawBar(SpriteBatch& sprites, const Font* font, const std::string& label, const Vector2& position,
                  const Vector2& size, float fill, const Vector4& colour)
{
	const float line = lineHeight(font);

	drawRect(sprites, position - Vector2(2.0f, 2.0f), size + Vector2(4.0f, 4.0f), kPanelEdge);
	drawRect(sprites, position, size, kPanel);

	const float filled = clamp01(fill) * size.X;
	if (filled > 0.0f)
		drawRect(sprites, position, Vector2(filled, size.Y), colour);

	if (!label.empty())
		drawShadowedText(sprites, font, label, position + Vector2(0.0f, -line), colour);
}

float Hud::drawCollectibles(SpriteBatch& sprites, const Font* font, const HudState& state, float y)
{
	const float x = kMargin;
	const float line = lineHeight(font);
	const float icon = line * 0.45f;
	const float iconY = (line - kLinePad - icon) * 0.5f;

	// The icons are flat coloured tiles standing in for the frontend sprites;
	// pulling the real artwork belongs to UI-TEXT / GAME-010.
	drawRect(sprites, Vector2(x, y + iconY), Vector2(icon, icon), kGold);
	drawShadowedText(sprites, font, state.coinsTotal > 0 ? fmt::format("{}/{}", state.coins, state.coinsTotal) : fmt::format("{}", state.coins),
	                 Vector2(x + icon + 10.0f, y), kGold);
	y += line;

	drawRect(sprites, Vector2(x, y + iconY), Vector2(icon * 1.4f, icon), Vector4(0.4f, 0.7f, 1.0f, 1.0f));
	drawShadowedText(sprites, font, fmt::format("{}/{}", state.cards, state.cardsTotal),
	                 Vector2(x + icon + 10.0f, y), Vector4(0.4f, 0.7f, 1.0f, 1.0f));
	return y + line;
}

void Hud::drawSpeedometer(SpriteBatch& sprites, const Font* font, const HudState& state)
{
	const float line = lineHeight(font);
	const float radius = 56.0f;
	// lift the dial by a line so the readout drawn inside it still clears the
	// bottom edge of the viewport
	const Vector2 centre(state.viewportWidth - kMargin - radius, state.viewportHeight - kMargin - radius - line * 0.5f);

	// 210 degrees of sweep, opening downward, drawn as short radial ticks
	const float startAngle = 2.6179939f; // 150 deg
	const float sweep = 4.1887902f;      // 240 deg
	const int ticks = 25;

	for (int i = 0; i <= ticks; ++i)
	{
		const float t = static_cast<float>(i) / ticks;
		const float angle = startAngle + sweep * t;
		const float inner = radius - (i % 5 == 0 ? 12.0f : 6.0f);
		const Vector2 dir(std::cos(angle), std::sin(angle));
		const Vector2 p = centre + dir * ((inner + radius) * 0.5f);
		const float ratio = state.topSpeedKmh > 0.0f ? clamp01(std::abs(state.speedKmh) / state.topSpeedKmh) : 0.0f;
		const Vector4 col = t <= ratio ? Vector4(1.0f, 0.55f, 0.1f, 1.0f) : Vector4(1.0f, 1.0f, 1.0f, 0.35f);
		drawRect(sprites, p - Vector2(1.5f, 1.5f), Vector2(3.0f, 3.0f), col);
	}

	// needle
	const float ratio = state.topSpeedKmh > 0.0f ? clamp01(std::abs(state.speedKmh) / state.topSpeedKmh) : 0.0f;
	const float needleAngle = startAngle + sweep * ratio;
	const Vector2 needleDir(std::cos(needleAngle), std::sin(needleAngle));
	for (float d = 6.0f; d < radius - 10.0f; d += 3.0f)
	{
		const Vector2 p = centre + needleDir * d;
		drawRect(sprites, p - Vector2(1.5f, 1.5f), Vector2(3.0f, 3.0f), Vector4(1.0f, 0.2f, 0.2f, 1.0f));
	}

	// readout centred inside the dial
	// Reversing reads as a speed too, and a car at rest as 0, not "-0".
	const std::string speed = fmt::format("{}", static_cast<int>(std::abs(state.speedKmh) + 0.5f));
	const float w = font != nullptr ? font->MeasureWidth(speed) : 0.0f;
	drawShadowedText(sprites, font, speed, centre + Vector2(-w * 0.5f, -line * 0.5f), kWhite);
}

void Hud::drawRadar(SpriteBatch& sprites, const Font* font, const HudState& state)
{
	const float size = 128.0f;
	const Vector2 origin(state.viewportWidth - kMargin - size, kMargin);
	const Vector2 centre = origin + Vector2(size * 0.5f, size * 0.5f);

	drawRect(sprites, origin - Vector2(2.0f, 2.0f), Vector2(size + 4.0f, size + 4.0f), kPanelEdge);
	drawRect(sprites, origin, Vector2(size, size), kPanel);

	const float scale = state.radarRange > 0.0f ? (size * 0.5f) / state.radarRange : 0.0f;
	// The player's right is (cos h, -sin h) and forward (sin h, cos h) for a
	// heading h, yaw from +Z. It was rotated by -h, which put what lay ahead
	// behind the arrow.
	const float cs = std::cos(state.playerHeading);
	const float sn = std::sin(state.playerHeading);

	auto plot = [&](const Vector2& worldXZ, const Vector2& blipSize, const Vector4& colour) {
		const Vector2 rel = worldXZ - state.playerXZ;
		// rotate into the player's frame, then flip Z so "forward" points up
		const float rx = rel.X * cs - rel.Y * sn;
		const float rz = rel.X * sn + rel.Y * cs;
		const Vector2 p = centre + Vector2(rx * scale, -rz * scale);
		if (p.X < origin.X || p.X > origin.X + size || p.Y < origin.Y || p.Y > origin.Y + size)
			return;
		drawRect(sprites, p - blipSize * 0.5f, blipSize, colour);
	};

	// The roads, as their nodes.
	for (const auto& road : state.roadPoints) plot(road, Vector2(2.0f, 2.0f), Vector4(0.6f, 0.6f, 0.65f, 0.8f));
	for (const auto& blip : state.checkpointBlips) plot(blip, Vector2(5.0f, 5.0f), Vector4(1.0f, 1.0f, 1.0f, 0.5f));
	for (const auto& blip : state.missionBlips) plot(blip, Vector2(6.0f, 6.0f), Vector4(0.2f, 1.0f, 1.0f, 1.0f));
	for (const auto& blip : state.policeBlips) plot(blip, Vector2(6.0f, 6.0f), Vector4(1.0f, 0.2f, 0.2f, 1.0f));

	// the checkpoint to head for, drawn last and larger so it reads at a glance
	if (state.hasNextCheckpoint)
		plot(state.nextCheckpointXZ, Vector2(10.0f, 10.0f), kGold);

	// player arrow, always centred and pointing up
	drawRect(sprites, centre - Vector2(2.0f, 6.0f), Vector2(4.0f, 12.0f), kGold);
	drawRect(sprites, centre - Vector2(5.0f, 0.0f), Vector2(10.0f, 4.0f), kGold);

}

void Hud::Draw(SpriteBatch& sprites, const Font* font, const HudState& state)
{
	const float line = lineHeight(font);

	if (state.fps > 0)
		drawShadowedText(sprites, font, fmt::format("{} fps", state.fps), Vector2(kMargin, kMargin), kGold);

	// --- condition bars, bottom left; each stacks above the previous one ---
	const float barX = kMargin;
	const float barStep = kBarHeight + line + 6.0f;
	float barY = state.viewportHeight - kMargin - kBarHeight;

	// Nitro is a placeholder: nothing simulates a boost reserve yet, so this
	// tracks whether boost is being held. See the note on UI-HUD.
	drawBar(sprites, font, "NITRO", Vector2(barX, barY), Vector2(kBarWidth, kBarHeight), state.boost,
	        Vector4(0.3f, 0.7f, 1.0f, 1.0f));

	barY -= barStep;
	drawBar(sprites, font, state.inVehicle ? "CAR" : "HEALTH", Vector2(barX, barY), Vector2(kBarWidth, kBarHeight),
	        state.health / 100.0f, healthColour(state.health));

	if (state.chaseActive)
	{
		barY -= barStep;
		drawBar(sprites, font, fmt::format("HIT & RUN {:.0f}/10", state.heat), Vector2(barX, barY),
		        Vector2(kBarWidth, kBarHeight), state.heat / 10.0f, heatColour(state.heat));
	}

	if (state.inVehicle)
		drawSpeedometer(sprites, font, state);

	drawRadar(sprites, font, state);

	// --- mission strip, top centre ---
	float topY = kMargin;
	if (state.missionActive)
	{
		if (!state.objective.empty())
		{
			const std::string& text = state.objective;
			const float w = font != nullptr ? font->MeasureWidth(text) : 0.0f;
			drawShadowedText(sprites, font, text, Vector2((state.viewportWidth - w) * 0.5f, topY), kGold);
			topY += line;
		}

		if (state.totalLaps > 1 || state.raceField > 1)
		{
			// A one-lap race (run start to finish) has no lap count to show.
			std::string text = state.totalLaps > 1 ? fmt::format("Lap {}/{}   ", state.lap + 1, state.totalLaps) : "";
			text += fmt::format("CP {}/{}", state.checkpoint, state.totalCheckpoints);
			if (state.raceField > 1)
			{
				static const char* const kSuffix[] = {"th", "st", "nd", "rd"};
				const int p = state.racePosition;
				const char* suffix = (p % 100 >= 11 && p % 100 <= 13) || p % 10 > 3 ? kSuffix[0] : kSuffix[p % 10];
				text += fmt::format("   {}{} / {}", p, suffix, state.raceField);
			}
			const float w = font != nullptr ? font->MeasureWidth(text) : 0.0f;
			drawShadowedText(sprites, font, text, Vector2((state.viewportWidth - w) * 0.5f, topY),
			                 Vector4(0.2f, 1.0f, 1.0f, 1.0f));
			topY += line;
		}

		if (state.hasNextCheckpoint)
		{
			const std::string text = fmt::format("{}: {:.0f}m", state.nextCheckpointLabel, state.nextCheckpointDistance);
			const float w = font != nullptr ? font->MeasureWidth(text) : 0.0f;
			drawShadowedText(sprites, font, text, Vector2((state.viewportWidth - w) * 0.5f, topY), kGold);
			topY += line;
		}

		if (state.timeRemaining >= 0.0f)
		{
			const std::string text = fmt::format("{:.0f}s", state.timeRemaining);
			const float w = font != nullptr ? font->MeasureWidth(text) : 0.0f;
			drawShadowedText(sprites, font, text, Vector2((state.viewportWidth - w) * 0.5f, topY),
			                 state.timeRemaining < 30.0f ? Vector4(1.0f, 0.3f, 0.3f, 1.0f) : kWhite);
		}
	}

	// --- top-left column: fps (already drawn), collectibles, then hints ---
	float hintY = drawCollectibles(sprites, font, state, kMargin + line);
	for (const auto& hint : state.hints)
	{
		drawShadowedText(sprites, font, hint, Vector2(kMargin, hintY), state.hintColour);
		hintY += line;
	}

	// --- centred call-outs ---
	if (!state.countdown.empty())
	{
		// Green for the go, yellow for everything before it.
		const bool go = state.countdown.rfind("GO", 0) == 0;
		const float w = font != nullptr ? font->MeasureWidth(state.countdown) : 0.0f;
		drawShadowedText(sprites, font, state.countdown,
		                 Vector2((state.viewportWidth - w) * 0.5f, state.viewportHeight * 0.5f),
		                 go ? Vector4(0.2f, 1.0f, 0.2f, 1.0f) : Vector4(1.0f, 0.85f, 0.1f, 1.0f));
	}

	DrawCallouts(sprites, font, state);
}

void Hud::DrawCallouts(SpriteBatch& sprites, const Font* font, const HudState& state)
{
	if (!state.banner.empty() && state.bannerPicture && state.bannerPicture->GetWidth() > 0)
	{
		// Over the banner, a third of the screen wide, its own shape kept.
		const float width = state.viewportWidth * 0.35f;
		const float height = width * static_cast<float>(state.bannerPicture->GetHeight()) /
		                     static_cast<float>(state.bannerPicture->GetWidth());
		sprites.Draw(state.bannerPicture,
		             Vector2((state.viewportWidth - width) * 0.5f, state.viewportHeight * 0.5f - 50.0f - height),
		             Vector2(width, height), Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	}

	if (!state.banner.empty())
	{
		const float w = font != nullptr ? font->MeasureWidth(state.banner) : 0.0f;
		drawShadowedText(sprites, font, state.banner,
		                 Vector2((state.viewportWidth - w) * 0.5f, state.viewportHeight * 0.5f - 40.0f),
		                 state.bannerColour);
	}

	if (!state.bannerDetail.empty())
	{
		// Word-wrapped to most of the screen's width: the game's mission
		// summaries run to a long sentence, which ran off both edges.
		std::vector<std::string> lines(1);
		std::stringstream words(state.bannerDetail);
		std::string word;
		const float maxWidth = state.viewportWidth * 0.85f;
		while (words >> word)
		{
			const std::string candidate = lines.back().empty() ? word : lines.back() + " " + word;
			if (font != nullptr && !lines.back().empty() && font->MeasureWidth(candidate) > maxWidth)
				lines.push_back(word);
			else
				lines.back() = candidate;
		}

		float y = state.viewportHeight * 0.5f;
		for (const auto& text : lines)
		{
			const float w = font != nullptr ? font->MeasureWidth(text) : 0.0f;
			drawShadowedText(sprites, font, text, Vector2((state.viewportWidth - w) * 0.5f, y), kWhite);
			y += font != nullptr ? font->GetHeight() + 6.0f : 22.0f;
		}
	}
}

} // namespace Donut
