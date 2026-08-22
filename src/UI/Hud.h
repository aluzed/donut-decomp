// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector4.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Donut
{

class Font;
class SpriteBatch;
class Texture;

// Everything the HUD needs for one frame, gathered by Game so that Hud never
// reaches back into the game object.
struct HudState
{
	float viewportWidth = 0.0f;
	float viewportHeight = 0.0f;

	int fps = 0;

	// player / vehicle condition
	float health = 100.0f;   // 0..100
	bool inVehicle = false;
	float speedKmh = 0.0f;
	float topSpeedKmh = 160.0f; // full-scale deflection of the speedometer
	float boost = 1.0f;         // 0..1, see the note on the nitro gauge in Hud.cpp

	// police heat, 0..10 (matches ChaseManager::GetHeat)
	bool chaseActive = false;
	float heat = 0.0f;
	bool busted = false;

	// collectibles
	int coins = 0;
	int coinsTotal = 0;
	int cards = 0;
	int cardsTotal = 0;

	// mission
	bool missionActive = false;
	bool showGo = false;
	std::string objective;
	float timeRemaining = -1.0f; // < 0 when the stage is untimed
	int lap = 0;
	int totalLaps = 0;
	int checkpoint = 0;
	int totalCheckpoints = 0;

	// radar, in world XZ; the HUD rotates them into the player's frame
	Vector2 playerXZ;
	float playerHeading = 0.0f; // radians, 0 = +Z
	std::vector<Vector2> missionBlips;
	std::vector<Vector2> policeBlips;
	std::vector<Vector2> checkpointBlips; // upcoming checkpoints, in order
	bool hasNextCheckpoint = false;
	Vector2 nextCheckpointXZ;
	float nextCheckpointDistance = 0.0f;
	float radarRange = 120.0f;

	// free-form lines drawn under the HUD (control hints, help overlay)
	std::vector<std::string> hints;
	Vector4 hintColour = Vector4(0.5f, 0.5f, 0.5f, 1.0f);
	std::string banner; // centred call-out such as "BUSTED!"
	Vector4 bannerColour = Vector4(1.0f, 0.0f, 0.0f, 1.0f);
};

// Draws the in-game overlay. Screens that replace the world (menus, mission
// results) stay in Game; this only handles what sits on top of gameplay.
class Hud
{
public:
	Hud();
	~Hud();

	void Draw(SpriteBatch& sprites, const Font* font, const HudState& state);

private:
	void drawShadowedText(SpriteBatch&, const Font*, const std::string&, const Vector2&, const Vector4&);
	void drawRect(SpriteBatch&, const Vector2& position, const Vector2& size, const Vector4& colour);
	void drawBar(SpriteBatch&, const Font*, const std::string& label, const Vector2& position, const Vector2& size,
	             float fill, const Vector4& colour);
	void drawSpeedometer(SpriteBatch&, const Font*, const HudState&);
	void drawRadar(SpriteBatch&, const Font*, const HudState&);
	// draws the coin/card row at `y` and returns the next free y
	float drawCollectibles(SpriteBatch&, const Font*, const HudState&, float y);

	// 1x1 opaque white, tinted per draw: the sprite shader multiplies the
	// sampled texel by the vertex colour, so this gives flat coloured quads.
	std::unique_ptr<Texture> _white;
};

} // namespace Donut
