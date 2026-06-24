// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Core/Log.h"

namespace Donut
{
namespace Log
{

namespace
{
Level g_level = Level::Debug;
}

void SetLevel(Level level) { g_level = level; }
Level GetLevel() { return g_level; }

}
} // namespace Donut
