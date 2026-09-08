---
name: skytycoon-workflow
description: Develop the SkyTycoon Unreal Engine property board game, including deterministic rules, board definitions, local prototype and future multiplayer or platform adapters.
---

# SkyTycoon development

This is an independent UE 5.7.4 C++ project under the UEproject collection. Read the collection AGENTS.md before changes.

- `Source/SkyTycoon/TycoonRules.*` contains serializable state and rule transitions. Keep platform APIs and random generation outside this layer. Only an authoritative host may supply die/event outcomes in multiplayer.
- `TycoonGame.*` is currently a local hotseat HUD and controller, not a replicated implementation. Do not describe it as online multiplayer.
- Tile array order determines movement. Tile zero is the sole start; cell positions must form a closed orthogonally adjacent path. Invalid maps must not partially replace an existing match.
- No project map asset is supplied yet. In the editor create an empty level with the configured default game mode to play. A cookable map is required before packaging.
- Tests use automation prefix `SkyTycoon.Rules`. Build the SkyTycoonEditor Win64 Development target before running them.
- Complete engine location on this host: `D:\Epic Games\UE_5.7`. The old 5.6 registry path contains leftovers only. Use BuildSettingsVersion.V6 with UE 5.7. For UBA low-memory retry loops use `-NoUBA -MaxParallelActions=1` for this build, without editing global settings.
- Total seats include bots and must never exceed four. Bot purchase decisions use public state only; the local game mode advances them on a timer. Planned online behavior is documented in the project-root `Docs/GameDesign.md`.
- WeChat export, authentication and cloud storage are unverified and unimplemented. Record platform proof separately from desktop compilation and editor tests in the README.
- Item catalog and facility transitions live in TycoonItems.cpp. Trap checks must occur per traversed tile (also for cars); facility transfers grant no salary. Arrival never decrements detention; only Recover(wait) does. Failed item/shop actions must not consume inventory or money.
- Ordinary movement accepts 1–2 dice, cars 1–3 for three moves; zero is the unused second/third parameter sentinel in Roll. UI DiceMask uses individual on/off bits and may never leave all usable dice off. Remote dice overrides total distance, not the legal count.
- TycoonHUD.cpp draws and hit-tests a shared 1000×760 virtual layout. AI speed affects only local bot action intervals (.8/.4 seconds), never game rules.
