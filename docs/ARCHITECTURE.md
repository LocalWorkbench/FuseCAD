# Architecture

OpenCircuit is a history-based parametric modeller. A design is a list of user
parameters plus an ordered **timeline of features**; solid geometry is derived
state, regenerated from that definition on every change.

## Layers

| Library | Responsibility | Depends on |
|---|---|---|
| `oc::core` | Vectors and planes, units, the expression language, the parameter table | standard library only |
| `oc::sketch` | 2D sketch entities, constraints, the constraint solver, profile detection | `core` |
| `oc::kernel` | Abstract `Kernel` interface plus kernel-neutral data (`Mesh`, `EdgeQuery`) | `sketch` (header-only) |
| `oc::model` | Features, recompute, `Document`, undo, `.ocd` serialisation, modelling commands | `kernel`; nlohmann-json (private) |
| `oc::occt` | `Kernel` implemented with OpenCASCADE | `kernel`; OCCT (private) |
| `apps/oc-cli` | Headless command-line front end | `model`, `occt` |
| `apps/opencircuit` | Qt 6 desktop application | `model`, `occt`, Qt |

Rules that keep this healthy:

- Dependencies only point downwards. Third-party libraries are `PRIVATE`
  dependencies, so their headers never leak into our public headers.
- Only `oc::occt` includes OpenCASCADE headers. The model and the UI see opaque
  `ShapePtr` handles, so the kernel can be replaced or supplemented later.
- The model layer is tested against `FakeKernel` (no geometry), and the OCCT
  backend against closed-form volumes. A geometry regression shows up as a
  wrong number, not as a screenshot diff.

## Key design decisions

### 1. OpenCASCADE behind an interface, not a home-grown kernel (for now)
A production B-rep kernel (robust booleans, fillets, offsets, STEP) is a
decade-scale effort. OCCT is mature and LGPL. Wrapping it behind `oc::Kernel`
keeps the option to replace individual operations later without touching the
model or the UI.

### 2. Geometry is derived, never stored
Documents persist only the parametric definition. This makes files small,
diff-friendly and kernel-independent, and makes undo trivial (see 4). The cost
is a recompute on load, which is cheap at current model sizes; caching per
feature is on the roadmap.

### 3. The sketch solver is our own
The sketch is a vector of unknowns (point coordinates, radii) and every
constraint contributes residual equations. `solveLeastSquares` runs damped
Gauss-Newton (Levenberg) steps. Because steps stay in the row space of the
Jacobian, geometry that no constraint touches does not move, which is what makes
dragging an under-constrained sketch feel natural. After convergence, a
reduced-row-echelon analysis of the Jacobian gives the rank, the degrees of
freedom and, per variable, whether it is fully determined (used to colour fully
constrained geometry).

### 4. Undo is snapshot-based
Every committed edit stores a serialised snapshot of the document. The feature
tree is small, so this is cheap, and it is far more robust than hand-written
inverse operations for every command.

### 5. Feature references are re-resolved geometrically (interim)
Fillets and chamfers select edges through an `EdgeQuery` (all edges, edges
parallel to a direction, edges nearest to given points) that is re-evaluated on
every recompute. This sidesteps the *topological naming problem* for now. A
proper persistent-naming scheme is the most important open design problem;
see the roadmap.

### 6. One command registry for the whole UI
Every user action is a `QAction` registered in `CommandRegistry` under a stable
id. The ribbon, menus, marking menu and shortcuts all reference those ids, so a
command has one implementation, one label, one icon and one shortcut.
Roadmap commands are registered without a handler and announce themselves as
"coming soon".

### 7. Icons are code
The icon set is drawn procedurally with `QPainter` (`Icons.cpp`): no binary
assets to license or keep in sync, crisp at every DPI, and restyled together
with the theme.

## Recompute

```
Document::recompute(kernel)
  for each feature before the marker, not suppressed:
      feature.execute(ctx)      // may throw oc::Error
  ctx.bodies -> document bodies
```

A failing feature records its error and recompute continues; dependents then
fail with a clear message ("referenced sketch 'Sketch1' has errors"), just as
in other parametric CAD tools. Body ids are the ids of the features that created
them, so they are stable across recomputes.

## File format

`.ocd` files are pretty-printed JSON with a `format` tag and an integer
`version`. Readers reject files from newer versions rather than misreading
them. Sketch entities and constraints are stored by index; the raw solver state
is stored too, so a sketch re-solves from exactly where it was saved.
