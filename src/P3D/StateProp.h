#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Donut::P3D
{

class P3DChunk;

// A state prop (StaticPropData, 0x8020000): an animated object's states and
// what moves it from one to the next -- a crate, a vending machine, a phone
// stop, a wasp camera. In an AnimDynamicPhysics' object wrapper, or on its own
// in an effects file.
//
// Each state (StaticPropStateData, 0x8020001) shows some of the object's
// drawables (StaticPropVisibilityData, 0x8020002), plays a stretch of each of
// its frame controllers (StaticPropFrameControllerData, 0x8020003: PTRN_ its
// pose, EFX_ a particle effect, BQG_ a billboard group, TEX_ a texture's
// frames, PVIS its visibility), goes to another state on an event
// (StaticPropEventData, 0x8020004: HIT, STOMP, VEHICLE_HIT, DESTROYED...) or
// by itself after a while, and calls the game back at moments of it
// (StaticPropCallbackData, 0x8020005: coins to spawn, its collision to
// remove, itself to go).
//
// Times are seconds in the state: a wasp's coming in, 75 frames at twice
// their speed, goes on after 1.25 s; its frames run at 30 a second.
struct StateProp
{
	static constexpr float kFramesPerSecond = 30.0f;

	struct Visibility
	{
		std::string drawable;
		bool visible = true;
	};
	struct FrameController
	{
		std::string name;
		bool cyclic = false;
		uint32_t cycles = 0;
		bool holdFrame = false;
		float firstFrame = 0.0f, lastFrame = 0.0f;
		float speed = 1.0f;
		// Its frame `seconds` into the state.
		float FrameAt(float seconds) const;
	};
	struct Event
	{
		std::string name;
		uint32_t toState = 0;
		uint32_t event = 0;
	};
	// The game's callbacks, by their number (their names vary: 11 is called
	// SPAWN_5_COINS_Z and SPAWN_25_COINS_Z).
	enum Callback : uint32_t
	{
		RemoveFromWorld = 0,
		Spawn5Coins = 1,
		RemoveCollisionVolume = 2,
		FireEnergyBolt = 3,
		KillSpeed = 4,
		Spawn10Coins = 5,
		Spawn20Coins = 7,
		RadiateForce = 8,
		ObjectDestroyed = 10,
		Spawn5CoinsZ = 11,
		Spawn1Coin = 12,
		ColaDestroyed = 13,
		CameraShake = 14,
		RemoveSecondVolume = 16,
	};
	struct CallbackAt
	{
		std::string name;
		uint32_t callback = 0;
		float onSeconds = 0.0f;
	};
	struct State
	{
		std::string name;
		bool autoTransition = false;
		uint32_t outState = 0;
		float outSeconds = 0.0f;
		std::vector<Visibility> visibility;
		std::vector<FrameController> controllers;
		std::vector<Event> events;
		std::vector<CallbackAt> callbacks;
		// Its controller of that name ("PTRN_l1_crate"), or null.
		const FrameController* Controller(const std::string& name) const;
		// The state an event of that name takes it to, or -1.
		int OnEvent(const std::string& event) const;
	};

	std::string name;
	std::string objectFactory;
	std::vector<State> states;

	static StateProp Load(const P3DChunk& chunk);
	// The coins a callback spawns, 0 for none.
	static int Coins(uint32_t callback);
};

} // namespace Donut::P3D
