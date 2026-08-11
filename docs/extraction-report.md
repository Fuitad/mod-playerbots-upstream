# Playerbots extraction report

## Scope and immutable source

The preserved custom source is `Fuitad/mod-playerbots` at
`9b17934e875e65e408a7122f4ea9d0fb59a65da0`.

The exact upstream comparison commit is
`a7b885d27134466dbc1c91d39b8241ea725a1bbb` from `mod-playerbots/mod-playerbots`.

The comparison is intentionally commit based. It does not depend on a moving branch name.

```bash
git -C ../mod-playerbots diff --name-status \
  a7b885d27134466dbc1c91d39b8241ea725a1bbb...9b17934e875e65e408a7122f4ea9d0fb59a65da0
python3 tools/check_extraction_manifest.py
```

The diff contains exactly 228 paths. `docs/extraction-manifest.tsv` records the Git status, path,
classification, destination, and evidence for every one. The checker compares the complete status and path set
against Git, rejects duplicate or unsafe paths, and fails when either side has an unmatched entry.

## Source classification result

The exact source inventory contains these classes.

1. 183 paths are `extracted_implementation`.
2. 34 paths are `irreducible_compatibility`.
3. 10 paths are `retained_seam`.
4. 1 path is `retired_vanilla`.

No catchall rule is used. Every row carries an explicit destination and evidence value.

The 183 extracted paths are assigned as follows.

1. Personality owns 11 paths.
2. Economy owns 62 paths.
3. Social owns 54 paths.
4. LLM owns 3 paths.
5. Telemetry owns 11 paths.
6. MCP owns 22 paths.
7. Recovery owns 9 paths.
8. Lifecycle owns 4 paths.
9. Shared Economy and Recovery ownership covers 1 path.
10. Shared Economy and Social ownership covers 4 paths.
11. Shared Personality, Economy, and Lifecycle ownership covers 1 path.
12. Shared Personality, Social, and LLM ownership covers 1 path.

The extracted repositories own these implementations.

1. `mod-playerbot-personality` owns traits, fictional identity, and personality persistence.
2. `mod-playerbots-economy` owns careers, profession capability, economic policy, market execution, and economic
   telemetry.
3. `mod-playerbots-social` owns grounded conversation, relationships, biography, memory, privacy, routing, and
   social controls.
4. `mod-playerbot-llm` owns the C++ bridge and Python sidecar through one versioned provider neutral wire
   protocol.
5. `mod-playerbots-telemetry` owns read only inspection, action history, loop projection, and bounded payload
   rendering.
6. `mod-playerbots-mcp` owns the verification protocol, command server adapter, and Python MCP sidecar.
7. `mod-playerbots-recovery` owns loop detection, objective quarantine, movement recovery, combat recovery, and
   audited homebind recovery.
8. `mod-playerbots-lifecycle` owns guarded random bot cleanup, exact cohort confirmation, protected character
   enforcement, extension preparation, and durable completion checks.

## Generic seams retained in the fork

The retained seams contain no type owned by an extracted module. They provide these neutral capabilities.

1. Registration of action, trigger, strategy, and value contexts.
2. Registration of default combat, noncombat, and dead strategies.
3. First handler trade skill initialization.
4. Authoritative bot event delivery.
5. World update, bot update, action outcome, death, and removal observation.
6. Remote command dispatch without a feature specific command type.
7. Objective availability vetoes using a neutral objective value.
8. Guarded cleanup dispatch plus prepurge and postpurge notifications.
9. Playerbots database update discovery for installed AzerothCore modules.
10. Nonexecuting chat command recognition and channel membership inspection.

## Irreducible Wrath compatibility

Compatibility paths are individually listed in `docs/extraction-manifest.tsv` for the preserved source and in
`docs/final-fork-inventory.tsv` for the final fork. The final compatibility set is intentionally limited to these
behaviors.

1. DeepRun tram, boat, zeppelin, elevator, portal, taxi, and cross map travel execution.
2. Group invitation wakeup and pending invitation cancellation.
3. Quest target selection and acceptance using the authoritative object identity.
4. Correct combat reset action selection.
5. Correct RPG target identity.
6. Balanced faction and class creation with a global gender based name pool.

Recovery specific movement and combat stuck triggers are not retained in the fork. They are registered by
`mod-playerbots-recovery` through the generic trigger and strategy seams.

The legacy bulk random bot deletion implementation is also absent from the fork. The factory only asks the generic
cleanup handler. `mod-playerbots-lifecycle` owns the request switch and all deletion behavior. A missing module,
an empty protected character list, an unresolved protected character, a changed cohort, or a missing confirmation
therefore cannot fall through to the old deletion path.

## Configuration ownership

`conf/playerbots.conf.dist` contains no setting owned by an extracted module.

Economy owns these nine settings in `conf/mod_playerbots_economy.conf.dist`.

1. `AiPlayerbot.ClassMatchingProfessionChance` became
   `PlayerbotsEconomy.ClassMatchingProfessionChance`.
2. `AiPlayerbot.EconomyLifecycleEnabled` became `PlayerbotsEconomy.LifecycleEnabled`.
3. `AiPlayerbot.EconomyMarketMakingEnabled` became `PlayerbotsEconomy.MarketMakingEnabled`.
4. `AiPlayerbot.EconomyMarketMakingPerGroupExposurePercent` became
   `PlayerbotsEconomy.MarketMakingPerGroupExposurePercent`.
5. `AiPlayerbot.EconomyMarketMakingTotalExposurePercent` became
   `PlayerbotsEconomy.MarketMakingTotalExposurePercent`.
6. `AiPlayerbot.EconomyMarketMakingMinimumEvidence` became
   `PlayerbotsEconomy.MarketMakingMinimumEvidence`.
7. `AiPlayerbot.EconomyMarketMakingHoldingHorizonSeconds` became
   `PlayerbotsEconomy.MarketMakingHoldingHorizonSeconds`.
8. `AiPlayerbot.EconomyMarketMakingMaximumRelistAttempts` became
   `PlayerbotsEconomy.MarketMakingMaximumRelistAttempts`.
9. `AiPlayerbot.EconomyMarketMakingCooldownSeconds` became
   `PlayerbotsEconomy.MarketMakingCooldownSeconds`.

Social owns these nine settings in `conf/mod_playerbots_social.conf.dist`.

1. `AiPlayerbot.SocialChat.Enable` became `PlayerbotsSocial.Enable`.
2. `AiPlayerbot.SocialChat.Stage` became `PlayerbotsSocial.Stage`.
3. `AiPlayerbot.SocialChat.Density` became `PlayerbotsSocial.Density`.
4. `AiPlayerbot.SocialChat.DensityMultiplier.Quiet` became
   `PlayerbotsSocial.DensityMultiplier.Quiet`.
5. `AiPlayerbot.SocialChat.DensityMultiplier.Normal` became
   `PlayerbotsSocial.DensityMultiplier.Normal`.
6. `AiPlayerbot.SocialChat.DensityMultiplier.Lively` became
   `PlayerbotsSocial.DensityMultiplier.Lively`.
7. `AiPlayerbot.SocialChat.GeneralStarterPressureMultiplier` became
   `PlayerbotsSocial.GeneralStarterPressureMultiplier`.
8. `AiPlayerbot.SocialChat.TelemetryRetentionHours` became
   `PlayerbotsSocial.TelemetryRetentionHours`.
9. `AiPlayerbot.SocialChat.ControlToken` became `PlayerbotsSocial.ControlToken`.

Telemetry owns `PlayerbotsTelemetry.MaxPayloadBytes` in
`conf/mod_playerbots_telemetry.conf.dist`. It replaces `AiPlayerbot.TelemetryMaxPayloadBytes`.

MCP owns `PlayerbotsMCP.Port` in `conf/mod_playerbots_mcp.conf.dist`. It replaces
`AiPlayerbot.VerificationServerPort`. The upstream `AiPlayerbot.CommandServerPort` remains an upstream Playerbots
setting and is not owned by MCP.

Lifecycle owns these three settings in `conf/mod_playerbots_lifecycle.conf.dist`.

1. `AiPlayerbot.DeleteRandomBotAccounts` became `PlayerbotsLifecycle.CleanupRequested`.
2. `AiPlayerbot.DeleteRandomBotAccountsConfirmation` became
   `PlayerbotsLifecycle.CleanupConfirmation`.
3. `AiPlayerbot.DeleteRandomBotProtectedCharacters` became
   `PlayerbotsLifecycle.ProtectedCharacters`.

Personality and Recovery own no configuration setting. LLM settings were already owned by
`conf/mod_playerbot_llm.conf.dist` and remain there.

Every moved setting has a behavioral configuration test that loads a nondefault value through the module config
loader and asserts the resulting runtime value.

## Final fork inventory

`docs/final-fork-inventory.tsv` classifies every final path changed from the exact upstream commit. It contains 45
paths.

1. 18 paths are `retained_seam`.
2. 16 paths are `irreducible_compatibility`.
3. 11 paths are `repository_contract`.

The inventory can be checked without a path rule or a path count assumption.

```bash
comm -3 \
  <(git diff --name-status a7b885d27134466dbc1c91d39b8241ea725a1bbb | sort) \
  <(tail -n +2 docs/final-fork-inventory.tsv | cut -f1,2 | sort)
```

The command must print nothing.

## Vanilla only retirement

`docs/vanilla-only-server-rules.md` is the one source path classified as `retired_vanilla`. Vanilla policy hunks
inside mixed source paths were removed during extraction. They were not copied into another repository and are not
compatibility requirements.

The tested source and module set must contain no production reference to `VanillaOnlyRules`, no Vanilla only
default, and no Vanilla enforcement for maps, races, classes, professions, recipes, reputation, or progression.
Standard Wrath behavior is authoritative.

## Verification

The following checks are required in addition to repository quality, standalone build, and full integration tests.

```bash
python3 tools/check_extraction_manifest.py
python3 -m unittest tests/python/test_check_extraction_manifest.py

if rg -n \
  'AiPlayerbot\.(ClassMatchingProfessionChance|Economy|SocialChat|TelemetryMaxPayloadBytes|VerificationServerPort|DeleteRandomBotAccounts)' \
  conf/playerbots.conf.dist; then exit 1; fi

if rg -n \
  'Playerbot(Personality|Career|Economy|Social|LLM|Telemetry|MCP|Recovery|Lifecycle)|VanillaOnlyRules' \
  src --glob '!Bot/Extension/PlayerbotExtension.*'; then exit 1; fi

comm -3 \
  <(git diff --name-status a7b885d27134466dbc1c91d39b8241ea725a1bbb | sort) \
  <(tail -n +2 docs/final-fork-inventory.tsv | cut -f1,2 | sort)
```

The source manifest checker must report 228 exact paths. Both implementation scans and the final inventory
comparison must print nothing.
