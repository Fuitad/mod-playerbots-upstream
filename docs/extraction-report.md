# Playerbots extraction report

## Compared repositories

The extraction source is `Fuitad/mod-playerbots` at `9b17934e875e65e408a7122f4ea9d0fb59a65da0`.
The comparison base is `mod-playerbots/mod-playerbots:master` as fetched on 2026 08 10.

The source inventory is reproducible with this command in the preserved source repository.

```bash
git diff --name-status upstream/master...9b17934e875e65e408a7122f4ea9d0fb59a65da0
```

That command returns 228 paths. Every returned path is classified by the ordered rules below. The first
matching rule is authoritative.

## Classification rules

1. A path in the retained seam list is a retained seam.
2. A path in an extracted feature root, or a path named by an extracted feature pattern, is an extracted
   implementation.
3. Every remaining path is a documented compatibility change.

The third rule is deliberately exhaustive. It covers verification services, movement recovery, bot cleanup,
vanilla realm defaults, development files, and other custom work that is not owned by Personality, Economy,
Social, or LLM. Those changes remain recoverable from the source bundle. They are not part of this public seam
fork.

## Retained seam list

The public fork differs from upstream on exactly these generic integration or repository contract paths.

1. `.codegraph/.gitignore`
2. `.gitignore`
3. `CMakeLists.txt`
4. `conf/playerbots.conf.dist`
5. `docs/extraction-report.md`
6. `src/Ai/World/Rpg/Action/RpgSubActions.cpp`
7. `src/Bot/Engine/AiObjectContext.cpp`
8. `src/Bot/Engine/ExternalEventHelper.cpp`
9. `src/Bot/Engine/ExternalEventHelper.h`
10. `src/Bot/Extension/PlayerbotExtension.cpp`
11. `src/Bot/Extension/PlayerbotExtension.h`
12. `src/Bot/Factory/AiFactory.cpp`
13. `src/Bot/Factory/PlayerbotFactory.cpp`
14. `src/Bot/PlayerbotAI.cpp`
15. `src/Bot/PlayerbotAI.h`
16. `src/PlayerbotAIConfig.cpp`
17. `src/PlayerbotAIConfig.h`
18. `src/Script/Playerbots.cpp`
19. `src/Util/BroadcastHelper.cpp`
20. `tests/cpp/PlayerbotExtensionRegistryTests.cpp`

The two ignore paths, the report, and the three configuration paths are repository compatibility changes.
`CMakeLists.txt` supplies the standalone seam test contract. The remaining paths implement or exercise neutral extension registration, default strategy
registration, first login trade skill initialization, database module discovery, command recognition without
execution, channel membership inspection, authoritative bot event observation, and correct RPG target selection.

## Extracted feature roots and patterns

The following rules classify source inventory paths as extracted implementations.

1. `src/Bot/Economy/**`, `src/Bot/Personality/**`, and `src/Bot/Social/**`.
2. `src/Ai/Base/Actions/EconomyAction.cpp` and `src/Ai/Base/Actions/EconomyAction.h`.
3. `src/Bot/Telemetry/PlayerbotEconomyTelemetry.cpp`.
4. SQL paths whose names contain `social`, `personality`, `llm`, `economy`, or `budget_lane`.
5. Documentation paths whose names contain `economy`, `personality`, `social`, `claude_playerbot_chat`, or
   `trainer_profession`.
6. Test paths whose names contain `Career`, `Economy`, `FictionalIdentity`, `Gathering`, `Personality`,
   `ProfessionCapability`, or `Social`.
7. Integration paths whose names contain `economy` or `social`.
8. The feature owned portions of `conf/playerbots.conf.dist`, `src/PlayerbotAIConfig.cpp`, and
   `src/PlayerbotAIConfig.h`.
9. The feature owned portions of mixed call sites that invoked Personality, Economy, Social, or LLM symbols.

All code selected by these rules now lives in `mod-playerbot-personality`, `mod-playerbots-economy`,
`mod-playerbots-social`, or `mod-playerbot-llm`. None of those feature roots or feature named paths exists in
this fork outside the retained seam list.

## Configuration inventory

The following settings moved from `playerbots.conf.dist` into `mod_playerbots_economy.conf.dist`.

1. `AiPlayerbot.ClassMatchingProfessionChance` became `PlayerbotsEconomy.ClassMatchingProfessionChance`.
2. `AiPlayerbot.EconomyLifecycleEnabled` became `PlayerbotsEconomy.LifecycleEnabled`.
3. `AiPlayerbot.EconomyMarketMakingEnabled` became `PlayerbotsEconomy.MarketMakingEnabled`.
4. `AiPlayerbot.EconomyMarketMakingPerGroupExposurePercent` became
   `PlayerbotsEconomy.MarketMakingPerGroupExposurePercent`.
5. `AiPlayerbot.EconomyMarketMakingTotalExposurePercent` became
   `PlayerbotsEconomy.MarketMakingTotalExposurePercent`.
6. `AiPlayerbot.EconomyMarketMakingMinimumEvidence` became `PlayerbotsEconomy.MarketMakingMinimumEvidence`.
7. `AiPlayerbot.EconomyMarketMakingHoldingHorizonSeconds` became
   `PlayerbotsEconomy.MarketMakingHoldingHorizonSeconds`.
8. `AiPlayerbot.EconomyMarketMakingMaximumRelistAttempts` became
   `PlayerbotsEconomy.MarketMakingMaximumRelistAttempts`.
9. `AiPlayerbot.EconomyMarketMakingCooldownSeconds` became `PlayerbotsEconomy.MarketMakingCooldownSeconds`.

The following settings moved from `playerbots.conf.dist` into `mod_playerbots_social.conf.dist`.

1. `AiPlayerbot.SocialChat.Enable` became `PlayerbotsSocial.Enable`.
2. `AiPlayerbot.SocialChat.Stage` became `PlayerbotsSocial.Stage`.
3. `AiPlayerbot.SocialChat.Density` became `PlayerbotsSocial.Density`.
4. `AiPlayerbot.SocialChat.DensityMultiplier.Quiet` became `PlayerbotsSocial.DensityMultiplier.Quiet`.
5. `AiPlayerbot.SocialChat.DensityMultiplier.Normal` became `PlayerbotsSocial.DensityMultiplier.Normal`.
6. `AiPlayerbot.SocialChat.DensityMultiplier.Lively` became `PlayerbotsSocial.DensityMultiplier.Lively`.
7. `AiPlayerbot.SocialChat.GeneralStarterPressureMultiplier` became
   `PlayerbotsSocial.GeneralStarterPressureMultiplier`.
8. `AiPlayerbot.SocialChat.TelemetryRetentionHours` became `PlayerbotsSocial.TelemetryRetentionHours`.
9. `AiPlayerbot.SocialChat.ControlToken` became `PlayerbotsSocial.ControlToken`.

Personality did not own a setting in `playerbots.conf.dist`. LLM settings were already in
`mod_playerbot_llm.conf.dist` and remain there. The legacy `AiPlayerbot.RpgStatusProbWeight.DoProfession`
setting is removed because Economy now schedules trainer travel through its own strategy and career engagement
interval.

## Verification commands

The retained fork inventory is checked with these commands.

```bash
git diff --name-only upstream/master
git diff --name-only upstream/master | wc -l
git grep -nE 'Playerbot(Personality|Career|Economy|Social|LLM)' -- ':!docs/extraction-report.md'
if rg -n 'AiPlayerbot\.(ClassMatchingProfessionChance|Economy|SocialChat)|RpgStatusProbWeight\.DoProfession' \
  conf/playerbots.conf.dist; then exit 1; fi
```

The expected path count is 20. The feature symbol scan must return no extracted implementation from this fork.
