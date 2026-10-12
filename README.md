<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/brand/wordmark-dark.svg">
    <img src="docs/brand/wordmark-light.svg" alt="AgeCAD" width="360">
  </picture>
</p>

<p align="center"><b>Parametric CAD for people and their agents. Free and open source.</b></p>

AgeCAD is a parametric part modeller built on two equal footings. People get
a fast, quiet interface where the model fills the screen and every tool sits
one keystroke away. Agents get the very same tools through an API and an MCP
server, so they can sketch, dimension, model, measure and export alongside you.

## Principles

- **Polished from day one.** A modern, minimal interface with its own design
  system: no dated toolbars, no stock platform widgets.
- **Interface first, agents equal.** Every capability ships in the interface
  first. The command behind it is then exposed to scripts and agents unchanged:
  no agent-only features and no hidden headless paths.
- **One history.** Whether a person or an agent made an edit, it lands on the
  same timeline, can be inspected and undone, and is regenerated from parameters.
- **Few things, done well.** Version 1 is single-part modelling: constrained
  sketches, solid features and STEP/STL export, made robust before anything
  else is added.
- **Open all the way down.** OpenCASCADE kernel, open `.agecad` JSON files on
  your own disk, GPL-3.0.

**Status:** pre-release. The modelling engine and the command-line tool work;
the interface is being written from scratch. See the [roadmap](docs/ROADMAP.md).

## Build

Requires CMake ≥ 3.25, Ninja, a C++20 compiler, [OpenCASCADE](https://occt3d.com) ≥ 7.8,
nlohmann-json and doctest.

macOS:

```sh
brew install cmake ninja opencascade nlohmann-json doctest
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
build/dev/apps/agecad-cli/agecad-cli new part.agecad --sample pillow-block
```

Ubuntu 26.04:

```sh
sudo apt install cmake ninja-build g++ nlohmann-json3-dev doctest-dev libocct-data-exchange-dev
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
```

## Learn more

[Architecture](docs/ARCHITECTURE.md) · [Roadmap](docs/ROADMAP.md) · [Contributing](CONTRIBUTING.md)

## License

[GPL-3.0-or-later](LICENSE). Uses OpenCASCADE (LGPL-2.1 with exception).
