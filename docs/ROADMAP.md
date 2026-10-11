# Roadmap

What we plan to work on next, in rough order. **Everything here is subject to
change**: items may move, merge or be dropped as we learn more. Each milestone
should leave the application usable.

**Goal for 1.0:** the complete part-modelling workflow by mouse alone. Sketch on
a plane or face, dimension it, extrude with a live preview, pick edges to
fillet, edit any step from the timeline, change a parameter and watch the design
regenerate.

## Where we are (0.1)
The parametric core, solver, OpenCASCADE kernel, file format, CLI and desktop
shell all exist. Designs can be created from code and the CLI, but not yet
interactively in the viewport.

## 0.1.x: Name and identity
- [x] Rename the project to FuseCAD: app, bundle id, CLI, namespaces, docs, CI
- [x] Native file extension `.fcad`
- [x] App icon and README logo from `docs/brand/`
- [ ] Rename the GitHub repository to match

## 0.2: Selection
The foundation every interactive command depends on.
- [ ] Tessellation that records which face, edge and vertex each piece came from
- [ ] GPU picking of faces, edges, vertices and bodies
- [ ] Hover pre-highlight, selection set, selection filters
- [ ] Kernel queries for what was picked (plane, cylinder, normal, axis, length)
- [ ] Selection synced between the viewport and the browser
- [ ] Design document for persistent naming (see 0.3)

## 0.3: Persistent naming
References to faces and edges must survive upstream edits; this is the most
important open design problem.
- [ ] Element map built from the kernel's modification history (which feature
      created or changed each face and edge)
- [ ] Fillet and chamfer reference named edges, replacing `EdgeQuery`
- [ ] Clear errors when a reference really disappears, and a way to re-pick it
- [ ] Tests: change upstream parameters, check references still resolve

## 0.4: Interactive sketcher
- [ ] Create Sketch on an origin plane or a planar face; camera aligns to it
- [ ] Line, rectangle, circle, arc and point tools; construction toggle
- [ ] Snapping and automatically inferred constraints
- [ ] Drag geometry to solve; dimension input boxes bound to parameters
- [ ] Constraint and dimension glyphs; colour fully constrained geometry
- [ ] Split crossing curves at intersections for profile detection
- [ ] Trim, extend, offset, fillet in sketch

## 0.5: Selection-driven features (first demo release)
- [ ] Extrude and Revolve from selected profiles, with live preview and drag handles
- [ ] Fillet and Chamfer from selected edges, with live preview
- [ ] Edit Feature: double-click a timeline entry to reopen its command
- [ ] Feature errors and warnings shown on the timeline and in the viewport
- [ ] Sketches on faces follow model changes

## 0.6: Modelling breadth
- [ ] Construction geometry: offset plane, plane at angle, midplane, axis, point
- [ ] Hole (simple, counterbore, countersink, threaded)
- [ ] Shell, draft, press-pull
- [ ] Mirror, rectangular and circular patterns
- [ ] Sweep and loft
- [ ] Combine, split body, move/copy
- [ ] Measure tool; appearances and physical materials (mass, centre of gravity)

## 0.7: Assemblies
- [ ] Components and occurrences
- [ ] Joints and degrees of freedom
- [ ] Interference checking

## Continuous: production readiness
Worked on alongside every milestone, and required for 1.0.
- [ ] Recompute in the background, with per-feature result caching
- [ ] Autosave and crash recovery
- [x] Every kernel call guarded; results validated before use
- [ ] Fuzz testing of the document loader
- [ ] Performance budget for large designs (recompute, tessellation, frame rate)
- [ ] Signed macOS builds; Windows and Linux packages; release automation
- [ ] User documentation and a getting-started tutorial

## Later
- [ ] Drawings: 2D views and dimensions from the 3D model
- [ ] CAM: toolpath generation
- [ ] Simulation: linear static FEA
- [ ] Electronics: schematic and PCB linked to the mechanical model
- [ ] Python scripting API
- [ ] Dark theme
