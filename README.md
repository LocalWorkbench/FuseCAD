<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/brand/wordmark-dark.svg">
    <img src="docs/brand/wordmark-light.svg" alt="FuseCAD" width="360">
  </picture>
</p>

<p align="center"><b>Professional 3D CAD, CAM and CAE. Free and open source.</b></p>

![FuseCAD](docs/images/screenshot.png)

FuseCAD is one application for the whole product-development workflow, from
the first sketch to the part on the machine. No subscriptions, no cloud
lock-in, no feature tiers: every capability is free, and your designs stay in
open files on your own disk.

## One platform

- **Design:** parametric solid, surface and sheet-metal modelling on a full history timeline
- **Assemble:** components, joints and motion studies
- **Document:** production drawings generated from the 3D model
- **Manufacture:** CAM toolpaths for milling, turning and additive
- **Simulate:** structural, thermal and modal analysis
- **Electronics:** schematics and PCBs linked to the mechanical design

**Status:** pre-release, in active development. The [roadmap](docs/ROADMAP.md)
shows what works today and what comes next.

## Build

Requires CMake ≥ 3.25, Ninja, a C++20 compiler, [OpenCASCADE](https://occt3d.com) ≥ 7.8,
Qt ≥ 6.5, nlohmann-json and doctest.

macOS:

```sh
brew install cmake ninja opencascade qt nlohmann-json doctest
cmake --preset dev && cmake --build --preset dev
open build/dev/apps/fusecad/fusecad.app
```

Ubuntu 26.04:

```sh
sudo apt install cmake ninja-build g++ nlohmann-json3-dev doctest-dev \
  libocct-data-exchange-dev qt6-base-dev libgl-dev
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
```

## Learn more

[Architecture](docs/ARCHITECTURE.md) · [Roadmap](docs/ROADMAP.md) · [Contributing](CONTRIBUTING.md)

## License

[GPL-3.0-or-later](LICENSE). Uses OpenCASCADE (LGPL-2.1 with exception) and Qt (LGPL-3.0).
