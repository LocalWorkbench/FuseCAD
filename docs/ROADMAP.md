# Roadmap

What we plan to work on next, in rough order. **Everything here is subject to
change**: items may move, merge or be dropped as we learn more. Each milestone
should leave the project usable.

**Goal for 1.0:** single-part modelling that a person can do entirely in the
interface and an agent can do entirely through the API, with the same
commands. Sketch on a plane or face, dimension it, extrude with a live
preview, pick edges to fillet, edit any step from the timeline, change a
parameter and watch the design regenerate, export STEP.

## Where we are (0.1)
The engine exists: expression language and parameters, sketch constraint
solver, OpenCASCADE kernel, feature timeline with recompute and undo, the
`.fcad` format and the command-line tool. The first interface was removed so
the new one can be designed around commands from the start.

- [ ] Rename the GitHub repository to match

## 0.2: Commands
The single entry point that the interface and agents will share.
- [x] Every document edit is a typed command with a machine-readable schema
      (name, arguments, units, result, errors)
- [x] One executor: validate, apply, record an undo step, report what changed
- [x] Commands carry their origin (person, script, agent) onto the timeline
- [x] Read-only queries as commands too: parameters, features, bodies, volume,
      area, bounding box, sketch state
- [x] The CLI rebuilt on top of the command layer
- [x] Transactions for live previews, committed as one undo step
- [ ] Centre of mass, inertia and point-to-point measurement queries
- [ ] Deleting sketch entities and constraints (with stable ids)

## 0.3: Interface shell (Qt Quick)
A viewport-first window with as little chrome as possible, polished from the
first build. Every control is our own QML component styled from one set of
design tokens (colour, type, spacing, radius, motion); no platform-native
widgets or default control styles.
- [ ] Design system: tokens, Inter typography, icon set, and the base controls
      (button, field, slider, menu, popover, tooltip, list, toast)
- [ ] Full-window viewport drawn by our own renderer on Qt's GPU layer
      (Metal, Vulkan, Direct3D) inside the Qt Quick scene, with panels over
      it; orbit, pan, zoom, view snapping
- [ ] All native: no embedded web content; performance-critical items are
      custom scene-graph nodes in C++, with QML only for layout and binding
- [ ] Dark, flat visual language: one accent colour, crisp edges, short
      purposeful animations, no ornament
- [ ] Keyboard-first: command palette, single-key shortcuts, every command
      reachable without menus
- [ ] Compact floating panels for the timeline, parameters and the active
      command's options, shown only when relevant
- [ ] Open, save, undo and redo wired to the command layer

## 0.4: Agent access
Exposes exactly the commands the interface already offers.
- [ ] API and MCP server over the command layer, running inside the app
- [ ] Agent edits appear live in the viewport and on the timeline, and undo
      like any other edit
- [ ] Viewport snapshot and geometric queries so agents can check their work
- [ ] Parity test: the build fails if a command is exposed to agents without
      an interface binding

## 0.5: Selection and persistent naming
- [ ] Tessellation that records which face, edge and vertex each piece came from
- [ ] Picking with hover highlight and selection filters; selection is shared
      state that agents can read and set
- [ ] Element map from the kernel's modification history, so references to
      faces and edges survive upstream edits
- [ ] Fillet and chamfer reference named edges, replacing `EdgeQuery`
- [x] Kernel choice: stay on OpenCASCADE. An open-source NURBS kernel was
      trialled (2026-10): strong fillets, but booleans that can return wrong
      solids which pass validation, no STEP, and no thread safety. Worth
      keeping in mind as a second backend for cross-checking geometry

## 0.6: Sketcher
- [ ] Sketch on an origin plane or a planar face; camera aligns to it
- [ ] Line, rectangle, circle, arc and point tools; construction toggle
- [ ] Snapping and inferred constraints; drag to solve
- [ ] Dimensions bound to parameters; fully constrained geometry highlighted
- [ ] Trim, extend, offset, fillet in sketch

## 0.7: Features
- [ ] Extrude and revolve from selected profiles, with live preview and handles
- [ ] Fillet, chamfer, shell, hole, mirror, linear and circular pattern
- [ ] Edit any feature from the timeline; errors shown where they happen
- [ ] Construction planes and axes; measure tool; materials and mass properties

## Continuous: production readiness
Worked on alongside every milestone, and required for 1.0.
- [ ] Recompute in the background, with per-feature result caching
- [ ] Autosave and crash recovery
- [x] Every kernel call guarded; results validated before use
- [ ] Fuzz testing of the document loader and of command arguments
- [ ] Performance budget for large designs (recompute, tessellation, frame rate)
- [ ] Signed macOS builds; Windows and Linux packages; release automation
- [ ] User documentation, and a guide for driving FuseCAD from an agent

## After 1.0
Considered only once single-part modelling is solid.
- [ ] Assemblies and joints
- [ ] Sweep, loft and surface modelling
- [ ] Production drawings
