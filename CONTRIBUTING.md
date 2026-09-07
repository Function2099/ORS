# Contributing to ORS

Thank you for your interest in Open Recording Software. By participating you agree to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Getting started

1. Read the [README](README.md) (or [繁體中文](README.zh-TW.md)) so you know what already works.
2. For architecture and the original phase list, see [`docs/architecture.md`](docs/architecture.md) and [`screen-recorder-plan.md`](screen-recorder-plan.md).
3. Set up the build ([`docs/building.md`](docs/building.md)).
4. Open an issue before large changes, especially new capture backends or container formats.

## What to work on

Good first contributions: bug fixes in the Windows screen path, tests, docs, and translations (`resources/i18n/`).

Please do **not** send a PR that only wires a stub muxer or the Game/Audio tabs unless an issue agrees on the approach. Those features are listed as unimplemented in the README on purpose.

## Development

- Module boundaries are one-way: `ui` → `core` → `capture` / `audio` / `encode` / `mux`. Muxers must not depend on UI.
- New user-visible strings go through `tr()` and both `ors_zh_TW.ts` and `ors_en.ts`.
- Put build output and personal scratch files under [`ignore/`](ignore/README.md). Do not commit `ORS.exe`, Qt DLLs, or CMake cache.
- After a behavior change, run:

```powershell
cmake --build ignore/build
ctest --test-dir ignore/build --output-on-failure
```

## Pull requests

- Keep each PR focused on one feature or fix.
- Follow the style and naming already used in the files you touch.
- Update `CHANGELOG.md` under `[Unreleased]` when user-visible behavior changes.
- Fill in the pull request template (summary, test plan).
- CI on `windows-latest` must stay green.

## Issues

Use the Bug report or Feature request templates. Search existing issues first. Security problems go to [SECURITY.md](SECURITY.md), not a public issue.

## Branches

- `main` — default branch; keep it buildable
- `feature/<short-name>` — new work
- `fix/<short-name>` — bug fixes
