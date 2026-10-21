#pragma once

#include <string>

namespace Donut
{

// The engine's own words, where the game's phrase table (TextBible) has none: an
// objective's plain line when its stage names no phrase, the HUD's labels, the
// results' figures, the options' rows. In the game's five languages (E, F, G,
// I, S), in Latin-1 as the game's fonts take it; the objectives in capitals, as
// the game's own are ("PARLEZ A MARGE").
//
// `id` is one of the table's (see GameText.cpp); an unknown one comes back as
// itself. A "{}" in a line is for fmt::format.
const std::string& GameText(const std::string& id, char language);

// The languages to choose from, in the options' order, and each one's name in
// itself ("Français").
constexpr char kGameLanguages[] = {'E', 'F', 'G', 'I', 'S'};
const std::string& GameLanguageName(char language);

} // namespace Donut
