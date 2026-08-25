/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

// PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
// Prefer adding here over editing an upstream file. See docs/local-changes.md.

#ifndef PLAYERBOTS_PLAYERBOTTRAINERLEARNINGPOLICY_H
#define PLAYERBOTS_PLAYERBOTTRAINERLEARNINGPOLICY_H

#include "Trainer.h"

namespace playerbots
{
constexpr bool IsTrainerAutoLearned(Trainer::Type type) { return type == Trainer::Type::Class; }
}  // namespace playerbots

#endif
