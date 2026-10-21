#include "UI/GameText.h"

#include "Audio/AudioManager.h"

#include <map>
#include <utility>

namespace Donut
{

namespace
{
struct Line
{
	const char* id;
	// English, French, German, Italian, Spanish, in UTF-8.
	const char* text[5];
};

// clang-format off
const Line kLines[] = {
	// An objective's plain line, when its stage names no phrase of the game's.
	{"objective.goto",      {"GO TO THE MARKER", "ALLEZ AU REPERE", "GEH ZUR MARKIERUNG", "VAI AL SEGNALINO", "VE A LA MARCA"}},
	{"objective.getin",     {"GET IN THE CAR", "MONTEZ DANS LA VOITURE", "STEIG INS AUTO", "SALI IN MACCHINA", "SUBE AL COCHE"}},
	{"objective.talkto",    {"TALK TO THEM", "ALLEZ LEUR PARLER", "SPRICH MIT IHNEN", "PARLA CON LORO", "HABLA CON ELLOS"}},
	{"objective.interior",  {"GO INSIDE", "ENTREZ", "GEH HINEIN", "ENTRA", "ENTRA"}},
	{"objective.gooutside", {"GO OUTSIDE", "SORTEZ", "GEH HINAUS", "ESCI", "SAL FUERA"}},
	{"objective.race",      {"WIN THE RACE", "GAGNEZ LA COURSE", "GEWINNE DAS RENNEN", "VINCI LA GARA", "GANA LA CARRERA"}},
	{"objective.follow",    {"FOLLOW THAT CAR", "SUIVEZ CETTE VOITURE", "FOLGE DEM AUTO", "SEGUI QUELLA MACCHINA", "SIGUE A ESE COCHE"}},
	{"objective.destroy",   {"DESTROY THAT CAR", "DETRUISEZ CETTE VOITURE", "ZERSTÖRE DAS AUTO", "DISTRUGGI QUELLA MACCHINA", "DESTRUYE ESE COCHE"}},
	{"objective.dump",      {"RAM THE TRUCK, TAKE WHAT FALLS", "PERCUTEZ LE CAMION, RAMASSEZ CE QUI TOMBE", "RAMME DEN LASTER, SAMMLE AUF, WAS HERUNTERFÄLLT", "SPERONA IL CAMION, RACCOGLI CIO' CHE CADE", "EMBISTE EL CAMION, RECOGE LO QUE CAIGA"}},
	{"objective.delivery",  {"COLLECT THEM ALL", "RAMASSEZ-LES TOUS", "SAMMLE SIE ALLE", "RACCOGLILI TUTTI", "RECOGELOS TODOS"}},
	{"objective.losetail",  {"LOSE YOUR TAIL", "SEMEZ VOS POURSUIVANTS", "HÄNG DEINE VERFOLGER AB", "SEMINA GLI INSEGUITORI", "DESPISTA A TUS PERSEGUIDORES"}},
	{"objective.coins",     {"PAY THE FEE", "PAYEZ LA SOMME", "BEZAHLE DIE GEBÜHR", "PAGA LA TARIFFA", "PAGA LA TARIFA"}},
	{"objective.buycar",    {"BUY THE CAR", "ACHETEZ LA VOITURE", "KAUFE DAS AUTO", "COMPRA LA MACCHINA", "COMPRA EL COCHE"}},
	{"objective.buyskin",   {"BUY THE OUTFIT", "ACHETEZ LA TENUE", "KAUFE DAS OUTFIT", "COMPRA IL VESTITO", "COMPRA EL TRAJE"}},
	// What the arrow on the HUD points at.
	{"target.checkpoint",   {"Next checkpoint", "Prochain point de passage", "Nächster Kontrollpunkt", "Prossimo checkpoint", "Siguiente control"}},
	{"target.target",       {"Target", "Cible", "Ziel", "Bersaglio", "Objetivo"}},
	{"target.item",         {"Nearest item", "Objet le plus proche", "Nächster Gegenstand", "Oggetto più vicino", "Objeto más cercano"}},
	{"target.talkto",       {"Talk to {}", "Parler à {}", "Sprich mit {}", "Parla con {}", "Habla con {}"}},
	{"target.destination",  {"Destination", "Destination", "Ziel", "Destinazione", "Destino"}},
	// The hints on the screen.
	{"hint.getin",          {"Press {} to get in", "Appuyez sur {} pour monter", "Drücke {} zum Einsteigen", "Premi {} per salire", "Pulsa {} para subir"}},
	{"hint.vehicle",        {"Car: {:.0f}m", "Voiture : {:.0f} m", "Auto: {:.0f} m", "Auto: {:.0f} m", "Coche: {:.0f} m"}},
	// The results and the pause.
	{"result.record",       {"NEW RECORD!", "NOUVEAU RECORD !", "NEUER REKORD!", "NUOVO RECORD!", "¡NUEVO RÉCORD!"}},
	{"result.time",         {"Time: {:.1f}s", "Temps : {:.1f} s", "Zeit: {:.1f} s", "Tempo: {:.1f} s", "Tiempo: {:.1f} s"}},
	{"result.best",         {"Best: {:.1f}s", "Record : {:.1f} s", "Bestzeit: {:.1f} s", "Record: {:.1f} s", "Récord: {:.1f} s"}},
	{"result.next",         {"Next in {}...", "Suite dans {}...", "Weiter in {}...", "Prossima tra {}...", "Siguiente en {}..."}},
	{"result.retry",        {"Retrying in {}...", "Nouvel essai dans {}...", "Neuer Versuch in {}...", "Nuovo tentativo tra {}...", "Reintento en {}..."}},
	{"result.complete",     {"MISSION COMPLETE!", "MISSION ACCOMPLIE !", "MISSION ERFÜLLT!", "MISSIONE COMPIUTA!", "¡MISIÓN CUMPLIDA!"}},
	{"result.failed",       {"MISSION FAILED!", "MISSION ÉCHOUÉE !", "MISSION GESCHEITERT!", "MISSIONE FALLITA!", "¡MISIÓN FALLIDA!"}},
	{"result.level",        {"Level Complete", "Niveau terminé", "Level geschafft", "Livello completato", "Nivel completado"}},
	{"pause",               {"PAUSED", "PAUSE", "PAUSE", "PAUSA", "PAUSA"}},
	// The options' game settings.
	{"options.subtitles",   {"Subtitles", "Sous-titres", "Untertitel", "Sottotitoli", "Subtítulos"}},
	{"options.language",    {"Language", "Langue", "Sprache", "Lingua", "Idioma"}},
};
// clang-format on

const char* const kLanguageNames[] = {"English", "Français", "Deutsch", "Italiano", "Español"};

int column(char language)
{
	for (int i = 0; i < 5; ++i)
		if (kGameLanguages[i] == language)
			return i;
	return 0;
}
} // namespace

const std::string& GameText(const std::string& id, char language)
{
	// Converted to Latin-1 once each.
	static std::map<std::pair<std::string, char>, std::string> converted;
	const auto key = std::make_pair(id, language);
	if (const auto found = converted.find(key); found != converted.end())
		return found->second;
	std::string text = id;
	for (const Line& line : kLines)
		if (id == line.id)
			text = AudioManager::ToLatin1(line.text[column(language)]);
	return converted.emplace(key, std::move(text)).first->second;
}

const std::string& GameLanguageName(char language)
{
	static std::map<char, std::string> names;
	if (const auto found = names.find(language); found != names.end())
		return found->second;
	return names.emplace(language, AudioManager::ToLatin1(kLanguageNames[column(language)])).first->second;
}

} // namespace Donut
