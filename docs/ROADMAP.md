# Roadmap

Rough order of work. Each milestone should leave the application usable.

## 0.2: Interactive modelling
- [ ] Selection in the viewport: faces, edges, vertices, bodies (GPU picking), with highlight
- [ ] Interactive sketcher: sketch on a plane or planar face; line, rectangle, circle and arc tools; snapping; drag to solve
- [ ] Constraint and dimension glyphs in the sketch; colour fully constrained geometry
- [ ] Edit Feature dialogs for every feature type (double-click on the timeline)
- [ ] Extrude and Revolve commands driven by viewport selection, with a live preview
- [ ] Split crossing sketch curves at intersections for profile detection

## 0.3: Robust references
- [ ] Persistent naming for faces and edges (topological naming), replacing geometric `EdgeQuery` re-resolution
- [ ] Construction geometry: offset planes, planes at an angle, midplanes, axes, points
- [ ] Sketches on faces that follow model changes
- [ ] Per-feature result caching so recompute only re-runs what changed

## 0.4: More modelling
- [ ] Hole (simple, counterbore, countersink, threaded), shell, draft
- [ ] Rectangular and circular patterns, mirror
- [ ] Sweep, loft, press-pull
- [ ] Appearances and physical materials (mass, centre of gravity)
- [ ] Measure tool with interactive picking

## 0.5: Assemblies
- [ ] Components and occurrences
- [ ] Joints and degrees of freedom
- [ ] Interference checking

## Later
- [ ] Drawings: 2D views and dimensions from the 3D model
- [ ] CAM: toolpath generation
- [ ] Simulation: linear static FEA
- [ ] Electronics: schematic and PCB with links to the mechanical model
- [ ] Python scripting API
- [ ] Dark theme; Windows and Linux desktop packages
