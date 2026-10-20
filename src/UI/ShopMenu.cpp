#include "ShopMenu.h"

#include "Core/FileSystem.h"
#include "FrontendProject.h"
#include "Game.h"
#include "Game/GameInput.h"
#include "Render/OpenGL/VertexBinding.h"
#include "Render/OpenGL/VertexBuffer.h"
#include "Render/OpenGL/glad/glad.h"
#include "Render/SpriteBatch.h"
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
constexpr const char* kStats[] = {"Spe", "Acc", "Tou", "Sta"};
constexpr const char* kStatNames[] = {"Speed", "Acceleration", "Toughness", "Stability"};
// Where the game turned the 3D model, between the arrows: the picture's box.
constexpr float kPictureX = 180.0f, kPictureY = 120.0f, kPictureWidth = 280.0f, kPictureHeight = 200.0f;

bool inside(const FrontendProject& page, const std::string& name, int mouseX, int mouseY)
{
	float x, y, w, h;
	return page.GetRect(name, x, y, w, h) && mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h;
}
} // namespace

ShopMenu::ShopMenu(std::function<bool(const Item&)> take): _take(std::move(take))
{
	if (!FileSystem::exists(kIngame))
		return;
	_page = std::make_unique<FrontendProject>();
	_page->LoadP3D(kIngame, "Rewards.pag");
	_page->SetTextsFitHeight(true);
	_buy = std::make_unique<FrontendProject>();
	_buy->LoadP3D(kIngame, "Buy.pag");
	_buy->SetTextsFitHeight(true);
	_cancel = std::make_unique<FrontendProject>();
	_cancel->LoadP3D(kIngame, "Cancel.pag");
	_cancel->SetTextsFitHeight(true);
	// The 3D model's light cover, a quad behind all, holds the picture, in
	// its own colours; the pad's view toggle's button is up and down here.
	_page->SetSpriteColor("PreviewLightCover", Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	_page->SetSpriteVisible("ToggleView", false);
	const uint8_t black[] = {0, 0, 0, 255};
	_shade = std::make_unique<Texture>(black, 1, 1, "shop_shade");
	_page->SetText("PreviewNameDummy", " ");
}

ShopMenu::~ShopMenu() = default;

void ShopMenu::Open(std::vector<Item> items)
{
	if (!IsLoaded() || items.empty())
		return;
	_items = std::move(items);
	_shown = 0;
	_stats = false;
	_open = true;
	_ready = false;
	layOut();
}

void ShopMenu::layOut()
{
	const Item& item = _items[_shown];
	// Its picture (cars2D or skins2D), or its stats.
	const std::string folder = item.car ? "cars2D" : "skins2D";
	Texture* picture = ScrapBookMenu::image(fmt::format("./art/frontend/dynaload/images/{}/{}.png", folder, item.name));
	const bool stats = item.car && _stats;
	_page->SetSpriteVisible("PreviewLightCover", picture && !stats);
	if (picture)
	{
		_page->SetSpriteTexture("PreviewLightCover", picture);
		const float scale = std::min(kPictureWidth / picture->GetWidth(), kPictureHeight / picture->GetHeight());
		const float pw = picture->GetWidth() * scale, ph = picture->GetHeight() * scale;
		_page->SetSpriteBox("PreviewLightCover", kPictureX + (kPictureWidth - pw) * 0.5f,
		                    kPictureY + (kPictureHeight - ph) * 0.5f, pw, ph);
	}
	_page->SetSpriteVisible("StatsOverlay", stats);
	auto& resources = Game::GetInstance().GetResourceManager();
	for (int stat = 0; stat < 4; ++stat)
	{
		_page->SetText(kStatNames[stat], stats ? "" : " ");
		if (stats)
			_page->ResetTextColor(kStatNames[stat]);
		for (int star = 0; star < 5; ++star)
		{
			const std::string sprite = fmt::format("{}_Stars{}", kStats[stat], star);
			const float value = item.stars[stat];
			const char* image = value >= star + 1.0f ? "carstar.png" : value >= star + 0.5f ? "halfstar.png" : "greystar.png";
			if (Texture* texture = resources.GetTexture(image); texture && stats)
				_page->SetSpriteTexture(sprite, texture);
			_page->SetSpriteVisible(sprite, stats);
		}
	}
	const TextBible& bible = Game::GetInstance().GetTextBible();
	std::string key = item.name;
	std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::toupper(c); });
	const std::string& named = bible.Get(key);
	_page->SetText("PreviewName", named.empty() ? item.name : named);
	// Its price; had already, none, and the confirm key changes to it.
	if (item.owned)
	{
		_page->SetText("ToPurchase", " ");
		_page->SetText("Price", " ");
	}
	else
	{
		const std::string& toPurchase = bible.Get("TO_PURCHASE");
		const std::string& coins = bible.Get("COINS");
		_page->SetText("ToPurchase", toPurchase.empty() ? "To purchase:" : toPurchase);
		_page->SetText("Price", fmt::format("{} {}", item.price, coins.empty() ? "coins" : coins));
	}
	_buy->SetTextIndex("Accept", item.owned ? 1 : 0);
	// "View stats" on a car's picture, "View model" on its stats; a skin has
	// none.
	_page->SetTextIndex("ToggleView", stats ? 1 : 0);
	if (!item.car)
		_page->SetText("ToggleView", " ");
}

void ShopMenu::Draw(const GameIntent& intent, const Matrix4x4& proj)
{
	if (!_open)
		return;
	const bool left = intent.moveLeft > 0.5f, right = intent.moveRight > 0.5f;
	const bool up = intent.moveForward > 0.5f, down = intent.moveBackward > 0.5f;
	const std::size_t count = _items.size();
	std::size_t shown = _shown;
	bool stats = _stats;
	if (_ready && ((left && !_leftHeld) || (intent.mouseLeftClick && inside(*_page, "LeftArrow", intent.mouseX, intent.mouseY))))
		shown = (shown + count - 1) % count;
	if (_ready && ((right && !_rightHeld) || (intent.mouseLeftClick && inside(*_page, "RightArrow", intent.mouseX, intent.mouseY))))
		shown = (shown + 1) % count;
	if (_ready && ((up && !_upHeld) || (down && !_downHeld)))
		stats = !stats;
	_leftHeld = left;
	_rightHeld = right;
	_upHeld = up;
	_downHeld = down;
	if (shown != _shown || stats != _stats)
	{
		_shown = shown;
		_stats = stats;
		layOut();
	}
	// The game behind, darkened, over the whole viewport.
	{
		GLint viewport[4] = {};
		glGetIntegerv(GL_VIEWPORT, viewport);
		SpriteBatch batch;
		batch.Draw(_shade.get(), Vector2(0.0f, 0.0f), Vector2(static_cast<float>(viewport[2]), static_cast<float>(viewport[3])),
		           Vector4(1.0f, 1.0f, 1.0f, 0.75f));
		batch.Flush(proj);
	}
	_page->Draw(proj);
	_buy->Draw(proj);
	_cancel->Draw(proj);
	if (_ready && intent.pauseToggle)
		_open = false;
	else if (_ready && intent.uiConfirm && _take(_items[_shown]))
		_open = false;
	_ready = true;
}

} // namespace Donut
