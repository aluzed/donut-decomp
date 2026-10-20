#include "PhoneBoothMenu.h"

#include "Core/FileSystem.h"
#include "FrontendProject.h"
#include "Game.h"
#include "Game/GameInput.h"
#include "Render/Texture.h"
#include "ResourceManager.h"
#include "UI/ScrapBookMenu.h"
#include "UI/TextBible.h"

#include <algorithm>
#include <cctype>

#include <fmt/format.h>

namespace Donut
{

namespace
{
constexpr const char* kIngame = "./art/frontend/scrooby2/ingame.p3d";
// The stats' stars, in SetCarAttributes' order.
constexpr const char* kStats[] = {"Spe", "Acc", "Tou", "Sta"};

bool inside(const FrontendProject& page, const std::string& name, int mouseX, int mouseY)
{
	float x, y, w, h;
	return page.GetRect(name, x, y, w, h) && mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h;
}
} // namespace

PhoneBoothMenu::PhoneBoothMenu(std::function<void(const std::string&)> call): _call(std::move(call))
{
	if (!FileSystem::exists(kIngame))
		return;
	_page = std::make_unique<FrontendProject>();
	_page->LoadP3D(kIngame, "PhoneBooth.pag");
	// Its texts in their boxes' height, as the options' pages'.
	_page->SetTextsFitHeight(true);
	// Not shown: a car's damage and the cost of its repair (the booth
	// repairs it for nothing), and the message box's frame.
	for (const char* hidden : {"Coins", "CostToFix_Value", "CostToFix", "VehicleDamaged_Value", "VehicleDamaged",
	                           "PreviewNameDummy"})
		_page->SetText(hidden, " ");
	for (const char* hidden : {"LockedOverlay", "Frame_Bgd", "Frame_Bottom", "Frame_Right", "Frame_Left", "Frame_Top",
	                           "Frame_BottomRight", "Frame_BottomLeft", "Frame_TopRight", "Frame_TopLeft"})
		_page->SetSpriteVisible(hidden, false);
	_page->GetSpriteBox("PreviewImage", _previewX, _previewY, _previewWidth, _previewHeight);
	// The lock and its shade over the car's picture, which the page draws
	// after them.
	_page->BringSpriteToFront("Overlay");
	_page->BringSpriteToFront("Lock");
}

PhoneBoothMenu::~PhoneBoothMenu() = default;

void PhoneBoothMenu::Open(std::vector<Car> cars, const std::string& shown)
{
	if (!IsLoaded() || cars.empty())
		return;
	_cars = std::move(cars);
	_shown = 0;
	for (std::size_t i = 0; i < _cars.size(); ++i)
		if (_cars[i].name == shown)
			_shown = i;
	_open = true;
	_ready = false;
	layOut();
}

void PhoneBoothMenu::layOut()
{
	const Car& car = _cars[_shown];
	// Its picture as large as the page's box holds it, in its middle.
	if (Texture* picture = ScrapBookMenu::image(fmt::format("./art/frontend/dynaload/images/cars2D/{}.png", car.name)))
	{
		_page->SetSpriteTexture("PreviewImage", picture);
		const float scale = std::min(_previewWidth / picture->GetWidth(), _previewHeight / picture->GetHeight());
		const float pw = picture->GetWidth() * scale, ph = picture->GetHeight() * scale;
		_page->SetSpriteBox("PreviewImage", _previewX + (_previewWidth - pw) * 0.5f, _previewY + (_previewHeight - ph) * 0.5f,
		                    pw, ph);
	}
	// Its name, the text bible's ("PLOWK_V": Plow King).
	std::string key = car.name;
	std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::toupper(c); });
	const std::string& named = Game::GetInstance().GetTextBible().Get(key);
	_page->SetText("PreviewName", named.empty() ? car.name : named);
	// Five stars a stat: whole, half or grey.
	auto& resources = Game::GetInstance().GetResourceManager();
	for (int stat = 0; stat < 4; ++stat)
		for (int star = 0; star < 5; ++star)
		{
			const float value = car.stars[stat];
			const char* image = value >= star + 1.0f ? "carstar.png" : value >= star + 0.5f ? "halfstar.png" : "greystar.png";
			if (Texture* texture = resources.GetTexture(image))
				_page->SetSpriteTexture(fmt::format("{}_Stars{}", kStats[stat], star), texture);
		}
	// Not had yet: under a lock, with its level.
	_page->SetSpriteVisible("Lock", !car.unlocked);
	_page->SetSpriteVisible("Overlay", !car.unlocked);
	if (car.unlocked)
		_page->SetText("Level", " ");
	else
	{
		const std::string& level = Game::GetInstance().GetTextBible().Get(fmt::format("LEVEL_{}", car.level));
		_page->SetText("Level", level.empty() ? fmt::format("Level {}", car.level) : level);
	}
}

void PhoneBoothMenu::Draw(const GameIntent& intent, const Matrix4x4& proj)
{
	if (!_open)
		return;
	const bool left = intent.moveLeft > 0.5f, right = intent.moveRight > 0.5f;
	const std::size_t count = _cars.size();
	std::size_t shown = _shown;
	if (_ready && ((left && !_leftHeld) || (intent.mouseLeftClick && inside(*_page, "LeftArrow", intent.mouseX, intent.mouseY))))
		shown = (shown + count - 1) % count;
	if (_ready && ((right && !_rightHeld) || (intent.mouseLeftClick && inside(*_page, "RightArrow", intent.mouseX, intent.mouseY))))
		shown = (shown + 1) % count;
	_leftHeld = left;
	_rightHeld = right;
	if (shown != _shown)
	{
		_shown = shown;
		layOut();
	}
	_page->Draw(proj);
	const bool picked = intent.uiConfirm || (intent.mouseLeftClick && inside(*_page, "PreviewImage", intent.mouseX, intent.mouseY));
	if (_ready && intent.pauseToggle)
		_open = false;
	else if (_ready && picked && _cars[_shown].unlocked)
	{
		_open = false;
		_call(_cars[_shown].name);
	}
	_ready = true;
}

} // namespace Donut
