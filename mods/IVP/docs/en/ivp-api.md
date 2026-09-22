# Original IVP API

The independent `IVP.bmodp` base Mod exposes the IVP engine embedded in
Ballance's original `physics_RT.dll`. It publishes `bml.ivp` through BML's
provider-interface registry; it exports no IVP API symbol and consumer Mods
never link against it. It does not link or wholesale import a different IVP
release. A consumer declares `AddDependency(IVP_MOD_ID)` in its constructor and
includes the reconstructed C++ surface with:

```cpp
#include <BML/IVP/IVP.h>

IVP_Environment *environment = BML::IVP::Environment();
IVP_Real_Object *object = BML::IVP::RealObject(entity);
IVP_Core *core = object ? object->get_core() : nullptr;
```

These are borrowed game-thread pointers. Do not delete them or retain them
across object deletion, unphysicalize, level reset, or environment reset.

Solver scratch state can use the retail transaction arena. Every returned
pointer expires when the transaction ends; allocation and teardown remain in
the DLL heap:

```cpp
IVP_U_Memory memory;
memory.init_mem_transaction_usage();
memory.start_memory_transaction();
auto *scratch = static_cast<float *>(
    memory.get_memc(64 * sizeof(float)));
// Use scratch only in this solver step.
memory.end_memory_transaction();
```

Use `IVP_Hash(bucketCount, keySize, notFound)` for fixed-width binary keys. NUL
bytes do not terminate a key. Construction, destruction, `add`, and `find` use
retained DLL bodies; `remove` and the source-inline CRC index are reconstructed
over the same verified chain layout. It is not interchangeable with the
string-only `IVP_U_String_Hash`.

Continuous collision queries use `IVP_U_Matrix_Cache` to retain up to 21
object transforms within one PSI interval. Construction and time-code refresh
enter retail `p_init`; indexed lazy evaluation and the current-time accessor
are reconstructed from their inlined retail callsites. The cache borrows its
`IVP_Cache_Object`, `IVP_Real_Object`, and `IVP_Core`; callers that acquired a
locking Cache Object must still pair it with `remove_reference()`.

`IVP_3D_Solver` exposes the three retained retail bodies for maximum-deviation
search, collision-aware search, and root refinement. A Mod-derived solver
implements only vtable slot zero, `get_value(matrixA, matrixB)`; the DLL calls
back through that slot during raster stepping and refinement. The neighboring
`find_first_t_for_value_max_dev2` is only a one-line forward to the retained
max-deviation body and is reconstructed as such. `find_first_t_for_value_no_zero_dev`
is compiled out in the neighboring source and needs cache fields absent from
Ballance, while `print` has no implementation evidence, so both are deleted.

Two retail boundary rules are explicit. VC6 returns the eight-byte `IVP_Time`
from `calc_nullstelle` through a hidden first stack argument, whereas current
MSVC returns the otherwise-trivial type in registers; the wrapper therefore
uses a dedicated sret thunk, not generic `InvokeThis<IVP_Time>`. The collision
path can also sample one `0.005f` raster step beyond `tMax` and lacks the
max-deviation path's index-20 stop. Callers must keep that extra sample inside
the 21-entry cache; normal sub-0.1-second PSI intervals do so.

## Native ABI and ownership boundary

The native C++ surface is an MSVC x86 ABI, not a portable C++ ABI. The public
`BML_IvpInterface` gateway spells every function pointer `BML_CDECL`; compile
tests build it under both `/Gz` and `/Gr` so a consuming translation unit's
default convention cannot change the DLL boundary. Native IVP headers also
reject a Windows MSVC target whose pointer size is not 32 bits.

Ballance's `physics_RT.dll` imports `operator new`, `operator delete`,
`malloc`, and `free` from the legacy `MSVCRT.dll`. BML+ and current Mods use
VCRUNTIME/UCRT. Consequently, matching class size and vtable slots is not
enough: an allocation must be released by the same CRT family that created it.

Public IVP classes that can be deleted inside the retail engine, or whose
retained complete constructor leaves a retail vptr, provide class-specific
scalar `new`/`delete` backed by the verified DLL's allocator. Array allocation
is intentionally rejected because no compatible retail array-cookie contract
has been established. Factory-only `IVP_Environment` and
`IVP_Controller_Phantom` expose only retail deallocation. This changes no
object layout. The rule also covers Mod-derived controllers, materials,
collision filters, forcefields, and raycast cars that the engine may destroy.

Do not use `malloc`, placement storage, a custom arena, `BML_MALLOC`, or a
different module's global `new` for an object that can reach an IVP ownership
callback. Do not throw a C++ exception from a Mod virtual called by
`physics_RT.dll`, and do not let an exception unwind across that DLL boundary.
Keep Mod-owned callbacks and policy objects alive until they have been removed
from every environment, core, active set, or listener list.

The recommended `<BML/IVP/IVP.h>` umbrella temporarily selects MSVC's
eight-byte packing used by the retail types and restores the consumer's prior
packing afterwards. `IvpPackedConsumerCompile` verifies this from inside a
hostile `#pragma pack(1)` scope; do not copy declarations into a differently
packed local header.

`IvpCrossDllOwnershipTest` locks the complete audited allocation surface to the
retail pair. `IvpRetailCrossDllOwnershipTest` additionally loads the exact game
DLL and observes retail vptrs plus real deleting-destructor/environment/active-
set teardown paths. These tests specifically guard against the cross-CRT bug;
ordinary stack-based ABI tests cannot detect it.

## Installed surface and retail contract

`cmake/IVPPublicSurface.cmake` is the reviewed inventory shared by source
validation and installation. Configure-time checks keep the on-disk headers and
`<BML/IVP/IVP.h>` in step, while the isolated SDK consumer compiles the umbrella
using only installed files. This closes the previous omissions of `Attacher.h`,
`Car.h`, `ConstraintCar.h`, `Forcefield.h`, `ObjectAttach.h`, `RaycastCar.h`, and
`Reaction.h`.

`tools/ivp/retail-contract.json` is the curated IVP Retail Contract. It owns the
exact DLL identity, five independent instruction anchors, runtime object
offsets, 685 code RVAs, five data RVAs, the 681-entry direct dependency closure,
14 indirect callsites, and 1071 graded public-candidate evidence records. Run
`python tools/ivp/sync_retail_contract.py --check` to reject drift in the C++ and
TSV views; use `--write` only after reviewing a catalog change. The 1600-name
corrected-IDB manifest and 879-entry guarded correction script remain separate evidence:
their uncertain names, types, and vtables are not promoted to ground truth.

Three bounded read-only IDB audits keep names and prototypes separate. The current
header/contract reverse closure maps 681 Address ids to 680 unique public RVAs.
The canonical IDB has transactionally committed types for all 681 direct targets,
including the protected Phantom transitions, Core worst-case mass body, stripped
OV Element bodies, Spring, Mindist Manager, `IVP_Hash`, `IVP_U_Memory`, and the
Matrix Cache initializer and the three 3D Solver entries. A closed-and-reopened audit reports 681/681 non-empty
prototypes. The internal
OV Tree Manager now routes construction, destruction and insertion/removal through
its four retained retail bodies; its separately allocated `IVP_ov_tree_hash`, not
the non-polymorphic manager, owns vtable `0x10063A24`. `IVP_OV_Node` likewise
uses its retained constructor/private destructor so the DLL exclusively owns both
embedded vector lifetimes. All 409
public methods that resolve to exact decorated retail bodies are typed with zero
unresolved names. These results constrain the callable ABI; they do not make
every imported class layout authoritative.

The Spring hierarchy now also routes its retained complete constructor, broken-
spring listener dispatch, Active Float callback, and Active Spring complete
constructor through RVAs `0x14250`, `0x14220`, `0x14440`, and `0x144D0`.
Storage-only base construction and inactive listener-vector storage prevent a
Mod compiler from constructing those retail-owned subobjects twice. A host
mechanism scenario covers all four live parameters, broken-listener delivery,
destruction-time detachment, and untouched constructor-entry storage.

`Audit-IvpIdbNamedFunctionTypes.py` extends the missing-prototype check to all
1044 named `IVP_`/`ivp_` functions in the database. Thirteen passes type
Object/Cluster lifecycle, Friction System, Simulation Unit, core memory/math,
Buoyancy, Mindist/Minimizer/Event and hash/pair topology, Impact System, the
OV/delegator/surface collision chain, and Compact Ledge/3D Solver geometry
entries and the Linear Constraint Solver state machine, then close the remaining
utility, Buoyancy, Surface Builder, recursive compact geometry, mass-center and
polygon/tetra entries. Empty prototypes fell from 253 to zero, so all 1044/1044
named entries currently have a parseable prototype. The reverse public-address
audit additionally found and corrected the previously untracked
`IVP_Mindist_Manager::insert_exact_mindist` and `p_strdup` prototypes;
that count does not assert that every imported semantic type is final.

Update the canonical IDB only through the transactional wrapper:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/ivp/Invoke-IvpIdbTransaction.ps1
```

Run ordinary inspection scripts against an automatically discarded copy too:

```powershell
& tools/ivp/Invoke-IvpIdbReadOnly.ps1 `
  -Script tools/ivp/Inspect-IvpTypes.py `
  -ScriptArgument IVP_Core
```

The low-level runner rejects a database named `physics_RT-analysis.i64`, so an
omitted wrapper cannot expose the canonical database to an inspection script or
an automatic-analysis write.

It verifies the required Ballance installation DLL hash, refuses to run while
IDA is open, generates a fresh transaction-local public-method TSV from the
current headers and evidence, copies the canonical IDB to a same-directory temporary file,
applies all guarded corrections there and stages the symbol manifest, closes
and reopens the IDB for fourteen independent named, direct, reverse-public-address, constructor,
complete-destructor, deleting-destructor, ownership, constructor-lifetime,
direct-destructor, scalar-format, binary64-layout, bitfield, and vftable audits,
and commits with one same-volume overwrite move. A failed apply, save, reopen or
audit removes only the temporary IDB, manifest, and methods TSV; both canonical artifacts are
untouched. No persistent backup is created.

The public audit also matches each wrapper's concrete `ABI::Address` identifier
against the RVA of its own decorated body. Of those 409 methods, 397 call the
retail body directly. The remaining 12 are explicitly curated layered
destructor or thunk routes, where entering a DLL complete-object body
from an already active C++ body would repeat base/member lifetime work. No exact
body currently has an unexplained missing route.

The 17 deterministic physics scenarios now share the single
`BML_IvpInterface` and version negotiation in `tests/IvpTestAdapter.cpp`. Each
scenario still owns its Address-to-callable bindings, x86 `__fastcall` shims
that receive production `__thiscall` ECX, state, and physical invariants. The
adapter is test-only and never turns a decoded test model into production IVP
code. `IvpPublicHeadersCompile` also includes every public header independently;
intentional cross-owner inline definitions are inventoried in
`tools/ivp/header-completion-sites.tsv` instead of being moved across incomplete
types and changing consumer link semantics.

`IvpRetailContactLifecycleTest` is deliberately separate from that shimmed
suite: it loads the exact `BuildingBlocks/physics_RT.dll` from the configured
Ballance installation and executes the retail global contact-created,
post-collision, and contact-deleted dispatchers against Mod-owned listeners.
This locks the surviving collision-listener slots to post, object-deleted,
friction-created, friction-deleted, and destructor. The same exact-DLL case now
enters the four anonymous retained Environment object-created/deleted/frozen/
revived dispatchers and verifies reverse order plus removal during a revived
callback. Retail field accesses also lock the associated event/contact payload
sizes and offsets.

The compatibility surface covers the environment, its retail global collision
tolerance, objects and cores, the genuine six-slot inheritable material ABI,
collision filters, active-value lookup, terminals and composable oscillator/mixer/filter/switch nodes,
global and per-object listeners, controllers, object templates, compact geometry, surface managers,
cache transforms, spring and active-spring actuators, layout-checked Force,
Torque, and rotational-motor templates plus ordinary/active actuators and
environment factories, the separate Stiff Spring controller/active controller,
Check Dist anchors, hull updates, output, and factory, and Four Point/Stabilizer
four-anchor distance coupling, plus vehicle Suspension with separate compression/
rebound damping, reduced-virtual-mass scaling, and body-side force clipping,
Ballance's complete local-constraint public
interface, fixed/variable time stepping, a complete
motion controller, the complete retail `0x40` Phantom construction/destruction
lifecycle, phantom and collision-delegator callbacks, the exact five-slot
`IVP_Collision_Delegator_Root_Mindist` default factory, borrowed
mindist state and manager operations, time-event queue operations, protected
contact-point access, the retail `0xA0` `IVP_Mutual_Energizer` initialization,
relative-energy calculation, and proportional energy-reduction entries,
the real `0x08` static/energy friction helper-controller ABIs and borrowed
accessors to the instances embedded at friction-system offsets `+0x08/+0x10`,
plus source-visible friction-system topology vectors, state bits, and the
energy accumulator at their retail offsets,
the retail `0x840` temporary friction solver with eleven retained matrix,
coordinate, easing, and impulse-solve entries plus its verified short inline
impulse helpers, the distance-matrix column builder verified against both
expansions in `calc_solver_PSI`, two release no-op diagnostics, and
`IVP_Contact_Point::get_lt`,
selective contact unlinking, radar-neighborhood callbacks,
checked deletion after vicinity repair, compound-object-aware beam relocation
with collision-cache, mindist, broadphase, and hull maintenance, immediate shared-core simulation disable,
full environment/controller/core/object
time rebasing, the four-slot user-implementable universe-streaming policy,
the public per-core attachment template, the Mod-derivable `0x10` Forcefield
that tracks an active set of cores, the layout-checked Golem configuration
and runtime policy controller, and the `0x24`/`0xE0` Fixed-Keyframed configuration
and runtime constraint with reference-frame position and orientation targets,
synapse traversal, anomaly handling, per-object ray casts,
the Ballance 28-slot abstract car interface, `0x304` configuration, `0x308`
debug block, skid state, foundational raycast wheel/temporary/axis and
one-inline-core vector layouts, plus the `0x958` raycast-car controller whose
Ballance base/field ABI is preserved while Mods provide only `do_raycasts`,
the early real-wheel constraint-car surface (`0xB0` object, `0xA0` solver,
and `0x30` effective-mass builder) with a four-wheel 8x8 constraint solve,
and common IVP math and container
types, including all 33/33 sparse-matrix, 27/27 incremental-LU, and 7/7
historical `IVP_Complex_Simple` public declarations, the retail linear
constraint-solver entry point, and individually reconstructed Crout LU
factorization, solve, and inverse operations whose layout assumptions were
checked against the retail image. The Fixed-Keyframed implementation also uses the
layout-checked `0x148` core-reaction solver: its two retained methods call retail code,
while only the missing short methods are reconstructed. The `bml.ivp` C interface also exposes a
read-only catalog of 1600 corrected IDA function names/RVAs and version-locked executable RVA resolution.
The canonical IDB additionally passes a whole-database deleting-destructor
audit: all 47 named IVP deleting destructors have a typed `this`, unsigned
32-bit flags, pointer return, and machine-code-compatible MSVC x86 ABI. A
parallel audit verifies all 69 named complete destructors as void thiscalls
with a concrete owner and no explicit stack argument. The constructor-side
audit verifies 93/93 named complete constructors, including the MSVC x86
implementation rule that they return the concrete `this` pointer in EAX.
IDA's legacy type parser cannot express two verified template pointer spellings:
the constructor at RVA `0x10310` semantically takes
`IVP_Attacher_To_Cores<IVP_Controller_Buoyancy> *`, and the Buoyancy simulation
entry at RVA `0xFE50` takes `IVP_U_Vector<IVP_Core> *`. Their IDB prototypes use
ABI-equivalent `void *` parameters and retain the exact semantic types in comments.

`IVP_Real_Object::beam_object_to_new_position` is callable. Its short
object-to-core conversion and the link-stripped next-PSI transformation flow
are reconstructed selectively; retained Ballance helpers still perform time
cache invalidation, quaternion work, exact/invalid mindist refresh, broadphase
recheck, hull reset, and rotation-axis calculation. A deterministic test covers
both a sleeping compound-object teleport and the repeated moving-object fast
path. This is source-plus-adjacent-binary evidence, not a claim that a retained
retail body exists for the stripped method.

`IVP_Real_Object::change_unmovable_flag` is also callable now. The reconstructed
transaction unlinks every contact on a shared Core, releases the movable/static
friction representation, freezes an active Core, and—when making it static—uses
the retail allocator and the verified `0x24` `IVP_Simulation_Unit` constructor
to replace simulation ownership before retaining only the environment gravity
controller. The reverse transition destroys the static friction hash through
the retail hash destructor and allocator boundary. `SimulationUnit.h` exposes
the matching `0x24` unit and `0x1B0` manager layouts plus their retained
operations. Deterministic compound-object and static-obstacle cases verify the
transaction order; the link-stripped top-level object method itself is still
reported as a selective source reconstruction, not a retained body.

Simulation Unit split and fuse are callable as well. Retail `split_sim_unit`
(RVA `0x11470`) tail-loops and allocates another `0x24` MOVING unit when a
third union-find component remains, instead of the neighboring recursive call.
`fusion_simulation_unities` is reached twice from
`IVP_Mindist::try_to_generate_managed_friction`. `union_find_get_father` is
the 8-instruction walk of Core `tmp` at `+0x228`. Deterministic host cases
cover a two-core split, a three-component tail-loop split, fuse unregistering
the donor unit, and controller dedup plus `get_controller_priority` ordering.
Production methods now enter these retained retail bodies directly; the host
resolver models their decoded control flow only to verify state invariants,
instead of keeping duplicate implementations in the public header. This is
Ballance-reachable intern ABI: it is outside the 1575 public-candidate denominator
and does not raise the 522 public host-tested method count.

Ballance's imported types use a 28-slot `IVP_Car_System` and a `0x308` debug
block; the nearby source has three extra virtuals and four extra debug vectors.
Those later-version additions are excluded. The `0x958` raycast-car controller
is now exposed with its complete source-defined wheel-ray preparation, contact,
stabilizer, suspension, steering-force, booster, and controller-lifecycle
pipeline. Its concrete raycast batch remains the one protected pure virtual
extension point. This is an IDA-layout plus nearby-source reconstruction because
Ballance retains no class-specific body or concrete vtable. The neighboring
tree's `get_wheel_position(IVP_U_Point*, IVP_U_Quat*)` is explicitly unavailable in
raycast car, airboat, and fake jetski, with no Ballance body or callsite; the
signature is recorded as `= delete`; no behavior is invented and unsupported
use fails at compile time.

The real-wheel constraint solver is likewise a selective reconstruction, not a
copied IVP runtime. Its three layouts agree with the imported Ballance UDTs and
the nearby public header, but no class-specific retail body or concrete vtable
survives. Its builder explicitly initializes `aligned_row_len`; the nearby
implementation omits that write, while Ballance's verified great-matrix layout
requires it. A deterministic case gives four real wheel bodies independent x/z
velocities, runs one PSI, and checks per-wheel contact-speed convergence, total
linear-momentum conservation, and complete controller/core teardown.

Declaration coverage is deliberately separate from linkability. The nearby
tree and Ballance both lack implementations for the historical allocating
great-matrix constructor, three remaining complex/LP great-matrix solvers,
six index-LU entries and all seven `IVP_Complex_Simple` algorithms.
Their exact declarations are retained as explicitly deleted APIs so source can
be ported without receiving invented behavior from another IVP release. Small
operations fully determined by Ballance's retained storage and primitives are
restored separately; this includes full-row null-equation counting rather than
the incorrect shortcut of counting zero diagonal entries.

`IVP_FLOAT` is a 32-bit `float`; `IVP_DOUBLE` is a 64-bit `double`. The corrected
IDA copy now records the Torque and rotational-motor `rot_inertia` members as
`double` rather than the legacy import's size-compatible `long double` rendering.

## Real Player test

The commands below assume a standalone IVP build run from `mods/IVP`.
For the in-tree build, use the commands in the [IVP README](../../README.md).
Set `BML_BALLANCE_ROOT` to the Ballance installation before running a Player
script.

Static RVAs, type sizes, and non-null pointers do not prove runtime behavior.
Four independent scenarios follow Ballance's original menu into level one and
operate on the real active `Ball_Wood`. Each scenario is a test Mod of its own.
`PlayerFlowDriver` drives the shipped flow and starts a probe once gameplay
control is live, so one failing scenario never hides another.

The primary scenario is an ordinary gameplay Mod use case. Normal Ball
Navigation input accelerates the wooden ball, the Mod proportionally caps the
public IVP core's linear and angular velocity while control and movement remain
active, and the original game accelerates the ball again after the cap is
released.

```powershell
cmake --build ..\build-ivp --config RelWithDebInfo --target IVP `
  IvpBallSpeedGovernorTest IvpBallStateRoundTripTest `
  IvpBallSurfaceRaycastTest IvpBallConstraintTest
tests\player\Invoke-IvpBallSpeedGovernorTest.ps1 `
  -BallanceRoot $env:BML_BALLANCE_ROOT
```

The retained advanced scenario captures CK/IVP transform, linear velocity, and
angular velocity while the ball is rolling, lets retail physics create a
measurable divergence, restores the state, and observes simulation continue.
It updates the original PSI matrices through
`IVP_Core::transform_PSI_matrizes_core`; CK `SetPosition`/`SetQuaternion` alone
are overwritten by the old core transform on the next physics synchronization.

```powershell
tests\player\Invoke-IvpBallStateRoundTripTest.ps1 `
  -BallanceRoot $env:BML_BALLANCE_ROOT
```

The object-space compact-ledge helper is no longer a purely local reconstruction.
The retained ledge-tree walker at RVA `0x21EE0` calls an otherwise unnamed exact
Ballance body at RVA `0x21AB0`. That body implements both the two-triangle fast
path and the general convex-ledge path, but does not normalize EAX on every exit
despite the nearby source-level `IVP_BOOL` declaration. The public wrapper calls
the retail body as `void`, temporarily forwards its two-slot listener callback,
records whether a hit was emitted, restores the original listener with RAII, and
returns the observed stable boolean. Mandatory-retail host tests check a triangle
hit, a miss, repeated listener restoration, and the general tetrahedron branch;
the local algorithm remains only as the no-resolver test fallback.

The third scenario casts through the real wooden ball from its IVP geometry
center, exercises the public surface-manager and retail ray/compact-surface
paths, compares their hit distances, checks the radius and normal, and verifies
that the ball continues moving. It also casts down to a real static level
polygon, takes the returned compact ledge, invokes the recovered exact
object-space ledge body through its ABI-correct wrapper, and compares object,
ledge, triangle, and
distance.

```powershell
tests\player\Invoke-IvpBallSurfaceRaycastTest.ps1 `
  -BallanceRoot $env:BML_BALLANCE_ROOT
```

The fourth scenario creates a retail world-to-ball ballsocket constraint,
drives the normal Ball Navigation input against it, validates both endpoints
and the associated core, then frees the X/Y/Z translation axes and verifies
that the same ball resumes moving.

```powershell
tests\player\Invoke-IvpBallConstraintTest.ps1 `
  -BallanceRoot $env:BML_BALLANCE_ROOT
```

On 2026-09-01, against the required retail installation, the latest run held the
maximum linear speed at `1.25`, allowed `4.161395` units of travel, and observed
speed rise to `2.719653` after release. The state round trip restored a ball after
`1.009137` units of divergence with `0.046073` position error, `0.000743` rotation
error, zero linear/angular velocity error, and `0.104141` units of subsequent
movement. The ray scenario also passed with radius `2.0`, matching `0.75` ball
hit distances, matching `2.020610` static-surface/direct-ledge distances, and
`0.504934` units of continued center travel on the latest rerun. In the local
constraint run, the ball moved only `0.004907` units under 1.2 seconds of
continuous input, then moved `0.501212` units after all translation axes were
freed. All runners
display the real Player, capture logs and a frame, and
restore the prior installation.

These scenarios prove active player-ball mapping, core velocity mutation, the
retail wake-up path, PSI transform behavior, and local ballsocket-constraint
creation, solving, mutation, and deletion. They do not stand in for collision,
spring, buoyancy, or surface-builder behavior tests.

Build-side domain tests separately exercise Force's equal/opposite two-anchor
impulses, Torque's axis impulse and speed output, the rotational motor's
power-to-torque conversion, low-speed rule, speed limit, torque clipping,
Stiff Spring's mass-adapted restoring/damping impulse and break listener, and
Check Dist's moving-object threshold crossings and hull-min-list lifecycle.
Stabilizer coverage checks its four paired impulses and pinned/sleeping gating.
Two Suspension cases check compression versus rebound damping, asymmetric body
force clipping, and reduced-virtual-mass scaling. A dynamic-core-set case checks
attachment of existing cores, live additions/removals, and shutdown teardown.
Two Forcefield cases check per-core controller registration/removal, listener
teardown, controller identity, and owner-set-triggered self-destruction.
Two Fixed-Keyframed cases check position following in the reference-object frame,
relative-orientation correction, balanced linear/angular impulses, and controller lifecycle.
Raycast-car cases configure a four-wheel/two-axle vehicle and verify track and
wheelbase geometry, suspension controls, front steering, rear drive-torque
wakeup, wheel locking, booster re-entry rejection, debug-ray data, and the
secondary controller base's `this+4` registration/removal identity. An airborne
PSI case executes the controller pipeline, receives all four wheel rays, checks
full extension and zero pressure on misses, and verifies extra-gravity and
booster speed/timer updates.
The real-wheel case independently constructs an 8x8 effective-mass system for
one body and four wheel cores, executes a solver step, and verifies each wheel's
x/z surface-speed constraint and conserved system momentum rather than mere
constructor reachability.
The tests also cover active-value
updates and dependency/controller teardown. These deterministic tests are not
presented as additional real-Player results.

## Evidence policy

Declarations are checked in this order: retail DLL instructions, sizes, field
offsets and vtables; retained MSVC decorated names; corrected IDA types; then a
nearby IVP source tree for inline behavior that independently matches the DLL.
Neither imported IDA types nor the nearby source version is ground truth alone.

The public audit therefore no longer treats one percentage as an accuracy
score. Of 1575 nearby public candidates, 1549 signatures match exactly, two use
retail-confirmed Ballance signature variants, and 24 later interfaces are
proven absent from Ballance or lack the required retail structure and must remain
omitted. Seventeen of those omissions belong to the MOPP manager/builder boundary,
whose owner layouts, vtables, and implementations do not survive in Ballance.
All signature differences are classified. Retail bodies, adapter
callsites, owner layout evidence, deterministic host tests, and real Player
scenarios are reported independently. The strict 2026-09-07 lower bounds are
403 retail function bodies attributed by exact IDB decorated names, 13
independently binary-confirmed body variants, 5 separately classified
retail-confirmed inline bodies, 398 methods that directly invoke
retail bodies, 6 methods reached through 14 documented indirect vtable callsites,
87 retail-confirmed owner
layouts, 46 IDA-import-only owner layouts, 284 methods explicitly based on nearby
source algorithms, 527 host-tested methods, and 15 Player-tested methods. These
dimensions overlap and must not be added together; see the
Chinese evidence ledger at `docs/zh-CN/ivp-coverage.md`.

Callability is now audited independently from signature presence. With MSVC
delayed template parsing disabled, the audit recognizes in-class templates,
out-of-class inline definitions, and explicitly defaulted methods instead of
misreporting them as declarations. The current split is 1,434 header-defined
methods, 90 pure-virtual contracts, 25 explicitly unavailable Ballance
interfaces, and zero ordinary declaration-only methods. All three
`IVP_Object_Attach` operations are selectively reconstructed and lifecycle-tested.
Explicitly unavailable methods use `= delete`, so
an unsupported historical call fails at compile time rather than at link time.

Public fields and free functions now have independent denominators. The strict
field audit enumerates 582 public fields: 567 match the nearby declaration's name,
normalized type, and relative order, while all 15 differences are recorded in
`tools/ivp/public-field-evidence.tsv`. Twelve are later fields absent from the
Ballance layout and three are ABI-equivalent storage representations; there are
zero unexplained, bit-width, order, or stale-ledger differences. Restored source
views include Core friction/flags/old-sync, nested Real Object flags, Compact
Surface's historical 8/24 names, and the IDB-supported
`IVP_Template_Extra::info` layout. The free-function audit accounts for all 34
candidates: 8 direct retail calls, 21 selective reconstructions, one composition
of an adjacent retained primitive, and 4 retail omissions.

The three large Clang AST audits run in CTest with a 64-bit Python interpreter and
bounded test/subprocess timeouts, preventing a 32-bit Python memory failure from
leaving an orphaned Clang process.

The reconstruction rule is deliberately asymmetric: a retained DLL body is
always called directly; a missing body is reimplemented from a nearby revision
only when Ballance's own object layout and vtable can support it; an interface
whose required storage or slot is absent is omitted or explicitly deleted.
Source-reconstructed methods never receive invented retail RVAs.

One important Environment mismatch is now enforced explicitly. The nearby
source places a `constraint_listeners` vector after `core_revive_list`, but the
retail constructor initializes only the vectors at `+0xF4/+0xFC/+0x104` and
stores the customer fields and `environment_manager` backlink at
`+0x10C/+0x110/+0x114/+0x118`. The retail destructor frees the pointer at
`+0x10C` and dereferences the manager at `+0x118`. Consequently the three
constraint-listener Environment methods are explicitly deleted: implementing
them would overwrite live retail ownership state. Retail Local-constraint
BREAK exits call the slot-6 scalar deleting destructor, not a hidden listener
dispatcher.

`IVP_Core` is audited separately because its mixed internal/public header is
outside that 1,575-method denominator. All 83 public-visible member signatures
now match the nearby Ballance-era declaration; 82 are callable. Newly identified
retail bodies cover physical stop, freeze-reference reset, simulation entry,
impact rotation synchronization, redundant-value calculation, movement-state
classification, exact-mindist refresh, and complete destruction. Mass/material
recompilation now combines retained retail contact-refresh, material,
virtual-mass, friction-fusion, and transaction-memory entries with the short
Ballance-era traversal. The protected single-object constructor is also
source-reconstructed: it calls retained `CoreInitialize` at RVA `0xD2F0`, then
registers the resulting simulation unit through the retained manager entry at
RVA `0x11EF0`, matching the neighboring constructor without inventing a retail
constructor RVA. Only the obsolete merged-core split helper remains
unsupported; it is explicitly deleted and therefore fails at compile time
rather than at link time.

The method audit spans 252 owners. Eighty-seven have whole-owner layout evidence from
retail instructions, allocation sizes, or vtables; 46 have only reviewed
IDA-import layout evidence, and 13 have no applicable cross-boundary instance
layout. All 87 retail-confirmed owners now have x86 size gates; 80 field-bearing
owners also have key `offsetof` or inherited-tail
gates. The other seven are vptr-only interfaces or template/derived wrappers
that add no storage. All 46 IDA-only owners have an x86 `sizeof` gate.
Forty-one field-bearing actuator, motion/Golem, fixed-keyframed, forcefield,
compact-grid, Q12, interpolator, raycast-car, and statistics-entity
owners were also read back field by field through a disposable IDB copy and
now have key `offsetof` or inherited-tail constraints. The remaining five are
pure interfaces or base-only wrappers with no tail fields. Six static-only helpers
were moved from IDB-only to layout-not-applicable because no instance or `this`
pointer crosses the ABI. `IVP_Compact_Surface` moved in the other direction:
retained RVAs `0x39600/0x39B30` write every field in its complete `0x30` public
header, and exact-DLL tetra/pointsoup construction validates the values. These
drift guards do not promote any other owner to retail-confirmed evidence. A
compile-time `sizeof` contract alone does not promote an owner into either
evidence tier. The imported IDB now documents
`IVP_Object_Attach` and all three constraint-car layouts field by field. The
previously absent `IVP_Complex_Simple` is added as an explicitly source-only
0x34 definition, not retail proof. Direct-base order, virtualness, and
new-vtable-slot order currently have zero unexplained mismatches. Three
car-method virtual differences supported only by imported vtable types are
reported separately as IDA variants rather than retail proof.

For example, the imported database understated `IVP_Statistic_Manager` as
`0x58` by applying four-byte alignment to its 64-bit members. The retail
constructor clears 24 dwords and the containing environment places the next
manager at `+0x98`, proving a `0x60` object at environment `+0x38`. The public
header now guards the key offsets through `global_fmd_counter +0x58`, and the
canonical IDB carries the corrected `0x60` UDT. The nearby source also added an
environment-manager field absent from the game; the DLL confirms a `0x178`
environment. Likewise, the nearby spring template has an extra field that the
game constructor does not read; Ballance uses `0x38`.

Value passing is now a closed ABI audit rather than a collection of spot
checks. The nearby public surface contains 20 methods with a record passed or
returned by value. Five cross into `physics_RT.dll`, and every one passes the
same eight-byte `IVP_Time`; all four record-return methods are local inline
code within that `IVP_EXPORT_PUBLIC` candidate set. The low-level
`IVP_3D_Solver::calc_nullstelle` comes from a neighboring private collision
header outside that denominator; its VC6 hidden structure-return pointer is
locked separately by the dedicated sret thunk and exact-DLL refinement case.
The 190 enum-value methods use 38 public enum types, all fixed to `std::int32_t`.
`IvpByValueAbiCompile` locks those declarations and layouts, while
`IvpRetailByValueAbiTest` runs the exact DLL's Core and Real Object transform
interpolation, stationary-Core slow/calm classification, performance-window
reset, and Controller `RET 8` stack cleanup.

The canonical IDB now also records both `calc_at_matrix` entries as `void`
methods taking `IVP_Time` rather than erasing the wrapper to `double`. All
legacy imported `long double` nodes under IVP/IVV UDTs have been converted to
binary64 `double` by recursively rebuilding only the scalar, array, pointer,
and function type nodes. The transformer snapshots and restores each owner's
IDA `sda`, preserves template references and calling conventions, and rejects
any total-size change. A second transaction gate checks all 57 direct-binary64
UDT owners, seven exact layouts, and the complete retail
`IVP_Environment 0x178` member map; both problem counts are zero.

Bitfield storage has a separate compiler gate instead of being inferred from
`sizeof`. Clang's i686-MSVC layout reports all 22 declared bitfields across 10
owners and compares every byte/bit position with the curated ABI manifest. It
locks the Simulation Unit's mixed-enum allocation, Impact's 8/2/22 group,
VHash's 24/8 group, and the two Force flags at bits 0/1 of `+0x74`. An
independent IDB readback checks 23 storage fields across 11 owners. Compact
Surface now exposes its historical `max_factor_surface_deviation`/`byte_size`
8/24 bitfields and the raw `factor_and_size` word through one anonymous union;
the exact-DLL pointsoup test checks that both views describe the same `+0x1C`
storage. Neither audit treats the imported database as ground truth, and both
currently report zero problems.

Vftables are audited independently from imported `*_vtbl` UDTs. Constructor
vptr stores reveal 75 concrete address points and 335 slots, including stripped
construction and secondary-base tables. Eighteen high-risk tables pin every
retail target address: the base Mindist table, Active Spring, active terminals,
Recursive Mindist, OO Watcher, the OV Element/OV hash tables, and the Collision
Delegator split. The OV hash table at `0x10063A24` belongs to the separately
allocated `IVP_ov_tree_hash`, not the non-polymorphic Tree Manager. The last
split is a two-slot
Ballance base table at `0x10063A34` followed by a distinct five-slot Root
Mindist table at `0x10063A3C`; the nearby source's two extra virtual bookkeeping
methods must therefore remain non-virtual in the public compatibility API.
The base Mindist table at `0x100637B4` is likewise eight slots, not the imported
nine: Ballance omitted the neighboring source's virtual `is_recursive`, so
`exact_mindist_went_invalid` and `do_impact` remain slots six and seven. The
compatibility query is non-virtual and recognizes the retained Recursive
address point through the version-locked module resolver.

Recursive Mindist is also a concrete Ballance version difference. Both factory
sites allocate `0x98`, not the imported/neighboring `0xA0`; its secondary
Collision Delegator lives at `+0x88`, status at `+0x8C`, and collision FVector
at `+0x90`. The later `spawned_mindist_count` tail is absent. The borrowed API
now exposes this exact layout and seven retained methods, but deletes ordinary
C++ construction because the retail complete constructor already constructs
both bases. Its primary and secondary tables are locked at 8 and 2 slots, and
the collision-deletion wrapper deliberately passes the `+0x88` adjusted ECX. A
separate i686-MSVC compiler audit locks the complete size, both base offsets and
both tail-field offsets, so the secondary-vptr contract cannot silently drift.

Verify the supported retail file with:

```powershell
tools\ivp\Test-RetailPhysicsImage.ps1 `
  -BallanceRoot $env:BML_BALLANCE_ROOT
```

The supported DLL SHA-256 is
`E72E4AFCFA5C33A7D3D27776137F8C997B3C52D89D8A8A4745F1CA21E45893EC`.
Raw `ResolveSymbol` results remain analysis aids, not automatically verified C++
signatures; prefer the reconstructed methods under `BML/IVP/`.
