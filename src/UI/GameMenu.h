#pragma once

#include <cstddef>
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

	// The draw code decides where a button actually lands (it needs the viewport
	// size, unknown at AddButton time), so it hands the layout back here before
	// hover/click testing — otherwise both test against a stale rect.
	void SetButtonRect(size_t index, float x, float y, float w, float h)
	{
		if (index >= _buttons.size()) return;
		auto& btn = _buttons[index];
		btn.x = x;
		btn.y = y;
		btn.width = w;
		btn.height = h;
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
