// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <Input/Binding.h>

#include <string>

namespace Donut
{
namespace KeymapConfig
{

/** The default keymap.conf that ships when no file exists. See docs/keymap.md. */
const char* DefaultFileContents();

/** Build the default keymap (parsed from DefaultFileContents — single source of truth). */
Keymap MakeDefault();

/**
 * Parse a single right-hand-side input token (e.g. "up", "pad_a",
 * "pad_leftx-:0.3") into an InputSource. Returns false on an unknown token.
 * Exposed so the in-game rebinder (#7) can validate captured inputs the same way.
 */
bool ParseInputToken(const std::string& token, InputSource& out);

/** Serialise one input back to its canonical keymap.conf token ("up", "pad_a", "pad_leftx-:0.3"). */
std::string ToToken(const InputSource& src);

/** Short human-friendly label for UI/HUD (e.g. "Up", "LShift", "Pad A", "LStick Up"). */
std::string HumanLabel(const InputSource& src);

/** Serialise a whole keymap to keymap.conf text (one line per action, enum order). */
std::string Serialize(const Keymap& keymap);

/** Atomically write `keymap` to `path` (temp file + rename). Returns false on I/O error. */
bool Save(const std::string& path, const Keymap& keymap);

/**
 * Load bindings from `path` into `out`. The file is authoritative for every
 * action it mentions; actions absent from the file keep their built-in default
 * (so partial / older files still work). Invalid tokens or unknown actions are
 * logged and skipped. Returns false if the file could not be opened.
 */
bool Load(const std::string& path, Keymap& out);

/**
 * Convenience for startup: if `path` exists, Load() it; otherwise write the
 * default file to `path` and return the default keymap. `created` (optional)
 * reports whether a new file was written. Always leaves `out` usable.
 */
bool LoadOrCreate(const std::string& path, Keymap& out, bool* created = nullptr);

} // namespace KeymapConfig
} // namespace Donut
