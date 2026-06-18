#pragma once

#include <functional>
#include <string>
#include <vector>

namespace Donut
{

struct MenuButton
{
	std::string label;
	std::function<void()> action;
	float x, y, width, height;
	bool hovered = false;
};

class GameMenu
{
public:
	GameMenu() = default;

	void Clear() { _buttons.clear(); }
	void AddButton(const std::string& label, float x, float y, float w, float h, std::function<void()> action)
	{
		_buttons.push_back({label, std::move(action), x, y, w, h, false});
	}

	void Update(int mouseX, int mouseY)
	{
		for (auto& btn : _buttons)
		{
			btn.hovered = (mouseX >= btn.x && mouseX <= btn.x + btn.width &&
			               mouseY >= btn.y && mouseY <= btn.y + btn.height);
		}
	}

	bool CheckClick(int mouseX, int mouseY)
	{
		for (auto& btn : _buttons)
		{
			if (mouseX >= btn.x && mouseX <= btn.x + btn.width &&
			    mouseY >= btn.y && mouseY <= btn.y + btn.height)
			{
				if (btn.action) btn.action();
				return true;
			}
		}
		return false;
	}

	const std::vector<MenuButton>& GetButtons() const { return _buttons; }

private:
	std::vector<MenuButton> _buttons;
};

} // namespace Donut
