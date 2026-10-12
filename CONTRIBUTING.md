# Contributing to AgeCAD

Thanks for helping! This page covers how to get a working build and what we
expect from a change.

## Getting started

1. Install the dependencies listed in the [README](README.md#build).
2. `cmake --preset dev && cmake --build --preset dev && ctest --preset dev`

| Preset | Purpose |
|---|---|
| `dev` | Debug build of everything, warnings as errors |
| `asan` | `dev` plus AddressSanitizer and UndefinedBehaviorSanitizer |
| `core-only` | `core`, `sketch` and `model` and their tests, without OpenCASCADE |

Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) before larger changes, and
check the [roadmap](docs/ROADMAP.md) for what is planned.

## What a good change looks like

- **Tests.** New behaviour in `core`, `sketch` or `model` comes with doctest
  cases. Geometry changes come with an OCCT integration test that checks a
  number (volume, area, bounding box) against a value you can derive by hand.
- **No warnings.** Every preset builds with `-Werror`.
- **Sanitizer clean.** `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`
- **Formatted.** Run `clang-format -i` on the files you touch (config in `.clang-format`).
  CI uses clang-format **23.1.3**; other major versions format differently. Get it with
  `brew install clang-format` or `pipx install clang-format==23.1.3`.
- **Static analysis.** `run-clang-tidy -p build/dev -quiet "$PWD/(src|apps|tests)/"` reports
  nothing (config in `.clang-tidy`; CI runs this too). On macOS with Homebrew LLVM, add
  `-extra-arg=-isysroot$(xcrun --show-sdk-path)`.
- **Layering.** Dependencies point downwards only (see the architecture doc).
  Never include OpenCASCADE headers from `core`, `sketch` or `model`.
- **Errors.** User-facing failures throw `age::Error` with a message a CAD user
  can act on ("fillet radius is too large for the selected edges").

## Conventions

- C++20. `PascalCase` types, `camelCase` functions and variables, trailing `_` for private data members, `kPascalCase` constants.
- Every source file starts with `// SPDX-License-Identifier: GPL-3.0-or-later`.
- Comments explain *why*; the code should already say *what*.
- Base units are millimetres and radians throughout the model.
- Commit messages: a short imperative subject line, then a body explaining the motivation.

## Licensing

By contributing you agree that your contribution is licensed under the
GNU GPL v3.0 or later, the project licence.
