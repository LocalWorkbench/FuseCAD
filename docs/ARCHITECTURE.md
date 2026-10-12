# Architecture

AgeCAD is a history-based parametric modeller. A design is a list of user
parameters plus an ordered **timeline of features**; solid geometry is derived
state, regenerated from that definition on every change.

## Interface first, agents equal

People and agents drive the same engine through the same commands. A command
is added together with its interface; only then is it exposed to the API and
MCP server, with the same arguments, validation, errors and undo step. There
is no agent-only feature and no second code path, so anything an agent does is
something a person could have done, and it shows up on the timeline like any
other edit. The interface is being rewritten (see the roadmap); this document
describes the engine underneath it.

## Layers

| Library | Responsibility | Depends on |
|---|---|---|
| `age::core` | Vectors and planes, units, the expression language, the parameter table | standard library only |
| `age::sketch` | 2D sketch entities, constraints, the constraint solver, profile detection | `core` |
| `age::kernel` | Abstract `Kernel` interface plus kernel-neutral data (`Mesh`, `EdgeQuery`) | `sketch` (header-only) |
| `age::model` | Features, recompute, `Document`, undo, `.agecad` serialisation, modelling commands | `kernel`; nlohmann-json (private) |
| `age::occt` | `Kernel` implemented with OpenCASCADE | `kernel`; OCCT (private) |
| `age::command` | Typed commands (one per user action), their JSON Schemas, and `Session`: the single path for every edit | `model`; nlohmann-json (public: JSON is its interface) |
| `apps/agecad-cli` | Command-line front end over `Session` | `command`, `occt` |

Rules that keep this healthy:

- Dependencies only point downwards. Third-party libraries are `PRIVATE`
  dependencies, so their headers never leak into our public headers.
- Only `age::occt` includes OpenCASCADE headers. Everything above it sees opaque
  `ShapePtr` handles, so the kernel can be replaced or supplemented later.
- The model layer is tested against `FakeKernel` (no geometry), and the OCCT
  backend against closed-form volumes. A geometry regression shows up as a
  wrong number, not as a screenshot diff.

## Key design decisions

### 1. OpenCASCADE behind an interface, not a home-grown kernel (for now)
A production B-rep kernel (robust booleans, fillets, offsets, STEP) is a
decade-scale effort. OCCT is mature and LGPL. Wrapping it behind `age::Kernel`
keeps the option to replace individual operations later without touching the
model or anything built on it.

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

### 6. Every edit is a command run through one session
A command is a struct of arguments with a `fields` list, an `info` block (id,
title, description) and a `run` member; the type of `run`'s parameter makes it
an edit, a query or a session command (files, undo). From the field list the
`Codec` derives JSON parsing, argument errors that name the field, and the JSON
Schema that agents are given, so the description can never drift from the code.
Ids and argument names are `snake_case`, valid as tool names in every agent API.

`cmd::Session` owns the document, undo history and a journal. Callers name a
command and pass JSON; the interface's typed `Session::run<C>()` serialises
through the same path. An edit runs on a copy of the document, is diffed
against the original *before* recompute (recompute rewrites sketch solver
state, which is not a user change), then recomputed. It is refused, changing
nothing, if it throws, creates a failing feature, or breaks a feature that was
working; a parameter change that breaks dependents is allowed and reported as
issues. Created and modified features are stamped with the acting person,
script or agent, which the `.agecad` file keeps.

Transactions preview one edit: `begin`, any number of `update`s re-applying
it to the committed state (dragging a handle, an agent trying values), then
`commit` as a single undo step or `cancel`. Queries see the preview; other
edits wait.

### 7. Kernel results are checked at the boundary
Every `OcctKernel` call converts OpenCASCADE exceptions into `age::Error`, and
every modelling operation (extrude, revolve, booleans, fillet, chamfer, STEP
import) rejects results that hold no solid or fail the B-rep checker, with a
message a CAD user can act on. Queries never modify their input: meshing works
on a copy, so a shape can be shared and cached safely.

## Recompute

```
Document::recompute(kernel)
  for each feature before the marker, not suppressed:
      feature.execute(ctx)      // may throw age::Error
  ctx.bodies -> document bodies
```

A failing feature records its error and recompute continues; dependents then
fail with a clear message ("referenced sketch 'Sketch1' has errors"), just as
in other parametric CAD tools. Body ids are the ids of the features that created
them, so they are stable across recomputes.

## File format

`.agecad` files are pretty-printed JSON with a `format` tag and an integer
`version`. Readers reject files from newer versions rather than misreading
them. Until 1.0 the version is simply bumped when the format changes; older
files are not migrated. Sketch entities and constraints are stored by index; the raw solver state
is stored too, so a sketch re-solves from exactly where it was saved.
