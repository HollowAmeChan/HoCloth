# 2026-04-24 Progress Notes

## This round

- Rechecked `MagicaCloth2` BoneSpring and confirmed that branching support comes from a tree-based prebuild topology, not from flattening everything into one fake chain.
- Started the first HoCloth migration step toward that model.
- Reverted the temporary "truncate at branch" strategy and replaced it with subtree compilation again.

## What MC2 is actually doing

MC2 BoneSpring does **not** compile a branching hierarchy as a single linear chain.

Its important pieces are:

- collect the whole bone subtree into transform data
- build parent/child relationships for every transform
- create `Line` connections from each node to its parent
- create one or more `BaseLine` paths over the tree
- run simulation/writeback over topology data, not over a single ordered chain

That means the real unit is closer to:

- a tree of simulated points
- explicit parent-child edges
- baseline groups used by constraints and pose reconstruction

## HoCloth first-stage migration completed here

### 1. Spring compilation now restores subtree topology

`compile/compiler.py` now resolves the full subtree again instead of cutting off at the first branch.

### 2. Spring joints now carry graph-oriented metadata

`CompiledSpringJoint` now stores:

- `parent_index`
- `depth`

This lets runtime logic reason about tree order instead of assuming `bone_index == chain order`.

### 3. Spring chains now emit explicit lines and baselines

`CompiledSpringBone` now stores:

- `lines`
- `baselines`

Current meaning:

- `lines`: every parent-child bone edge
- `baselines`: every root-to-leaf path

This is the first direct HoCloth equivalent of MC2's topology-oriented BoneSpring prebuild data.

### 4. Tail tips are now generated per leaf

The old implementation appended only one tail-tip joint, which was only correct for a single chain.

Now tail tips are appended for every leaf branch.

### 5. Viewport drawing no longer fakes a single chain

`authoring/viewport.py` now draws:

- real parent-child segments for the subtree
- per-leaf tail-tip segments

So branching previews are no longer rendered as a wrongly zipped line list.

### 6. Stub runtime now steps by baseline paths

The current Python stub is still not MC2-level, but it has been moved off the worst single-chain assumption:

- root joints are treated as anchors
- step order now follows root-to-leaf baseline paths
- branching children inherit from their actual parent
- collision response is followed by reverse-path relaxation

This is still a transitional runtime, but it now matches the compiled branching topology much better.

### 7. Native XPBD step now uses baseline path order too

`_native/src/hocloth_runtime_api.cpp` has also been moved off the old full-array forward/reverse assumption.

Current first-stage behavior:

- root joints are pinned to their rest-driven base positions each step
- solver passes iterate each baseline path forward and backward
- collision projection is applied per baseline path
- the second post-collision solve also follows baseline paths

This is still not a full MC2 reimplementation, but it means the native step logic is no longer fundamentally single-chain.

### 8. Constraint responsibilities now move closer to MC2

One important MC2 observation is that not every constraint should be driven by `baseline`.

The current HoCloth migration now starts reflecting that:

- `lines` are becoming the structural distance-constraint carrier
- `baselines` are kept mainly for pose/alignment-style passes

This is closer to MC2, where baseline-oriented jobs and graph-edge-oriented constraints are not the same thing.

### 9. Native angle restoration now follows basic-pose direction more directly

The native C++ runtime used to feed the alignment pass with an absolute tail target.

That was still too close to a "single-chain relaxation" mindset, because once a branch parent drifted away, the child target direction was being inferred from a mixed current-head / absolute-tail relationship.

This round changes that toward the MC2-style reading:

- distance solve continues to use `lines`
- angle/alignment solve now uses the bone's basic-pose segment direction
- the current simulated parent position is used only as the segment head anchor

This is still a simplified HoCloth adaptation, not a verbatim port of MC2's `AngleConstraintJob`, but it is much closer to the idea of:

- compute a step basic pose
- compare simulated segment direction against that pose
- restore/limit along the baseline grouping

### 10. Native writeback now matches the authoring/runtime contract better

The native transform export now skips:

- root bones
- generated tail-tip helper joints

That matches the Python stub and avoids writing solver-only helper nodes back into Blender pose channels.

### 11. Native runtime now has an explicit step-basic-pose stage

The native runtime now stores explicit per-chain buffers for:

- `step_basic_positions`
- `step_basic_rotations`

and refreshes them before the baseline restoration passes.

That is closer to the MC2 structure where:

- one stage builds step basic posture data
- the angle/restoration stage reads that data

HoCloth is still using a simplified implementation, but the architecture is no longer "recompute a temporary target inline inside the angle solve".

### 12. Spring topology prebuild has started to split out of the compiler

`compile/compiler.py` no longer owns all spring topology generation by itself.

A new `compile/spring_prebuild.py` module now holds the topology-oriented stage:

- subtree resolution
- rest joint extraction
- tail-tip expansion
- line generation
- baseline generation

This is still a lightweight HoCloth version of the idea, but it moves the project closer to the MC2 pattern where:

- authoring data is gathered first
- topology/prebuild data is organized separately
- runtime consumes that prepared topology

### 13. Baseline prebuild data now has MC2-like flattened chunks

Spring prebuild is no longer limited to a nested `baselines = [[...], [...]]` style representation.

It now also emits flattened baseline chunk data:

- `baseline_start_indices`
- `baseline_counts`
- `baseline_data`

The native runtime now prefers those arrays when reconstructing baseline paths.

That is materially closer to the MC2 shape where baseline data is stored as:

- start index array
- count array
- packed baseline element array

### 14. Step basic rotations are now used by native restoration

The native restoration solve is no longer driven only by:

- `step_basic_positions`

It now blends two references when building the restoration direction:

- the basic-pose segment direction reconstructed from positions
- the local segment direction rotated by the parent's `step_basic_rotation`

This is still simpler than MC2's full angle-limit/restoration buffer workflow, but it is an important step because `step_basic_rotations` is now part of the actual solve path instead of being only stored for future work.

### 15. Native baseline solve now has explicit per-baseline temp buffers

The native solver no longer walks baselines only as plain path lists during restoration.

It now builds explicit per-baseline step buffers for:

- baseline `start/count/data`
- per-entry rotation buffer
- per-entry restoration vector buffer

and the restoration pass reads those buffers by baseline entry index.

This is a meaningful structural step toward MC2's `AngleConstraint` style, where the solve uses prepared temporary arrays instead of recomputing all intermediate baseline state inline during every constraint call.

### 16. Angle limit has started to use the baseline temp buffers too

The native baseline temp buffers are no longer restoration-only.

They now also carry a separate limit-direction buffer, and the constraint solve applies:

- angle limit against the buffered limit direction
- then restoration toward the buffered restoration direction

This is still a simplified HoCloth version rather than a direct port of MC2's limit curve and iterative rotation-center logic, but it moves the structure in the right direction:

- baseline temp buffers serve both limit and restoration
- the solve order now resembles "limit first, restoration second"

### 17. Limit and restoration now use different iteration-phase rotation centers

The native baseline solve no longer applies all direction corrections as child-only pulls.

It now uses a pair-style correction with different rotation-center ratios for:

- angle limit
- restoration

and those ratios change with iteration phase.

This is still a simplified version of the MC2 idea, but it moves HoCloth closer to the same qualitative structure:

- earlier iterations keep stronger directional correction
- later iterations shift toward more stable center ratios
- parent and child can both participate in the correction instead of only pushing the tail

### 18. Native runtime phases are now explicit instead of duplicated inline

The native solver loop now has clearer stage functions for:

- pre/post collision solve phase
- collision phase
- baseline buffer solve phase

This does not change the architecture by itself, but it matters because the runtime is no longer carrying the same line/baseline solve logic twice in large inline blocks.

That makes the next MC2-style work much easier:

- adjust the order of phases
- split line solve and baseline solve responsibilities further
- refine collision grouping without rewriting the whole step loop each time

### 19. Line topology now has flattened chunk data too

Spring prebuild now emits flattened line chunk data alongside the explicit line list:

- `line_start_indices`
- `line_counts`
- `line_data`

The native runtime now prefers those arrays when reconstructing line pairs, and the collision phase also reads the flattened baseline buffers directly instead of going back through nested path lists.

This means HoCloth is no longer only "baseline-shaped" in a MC2-like way. Both:

- line-side structural connectivity
- baseline-side solve grouping

are now moving toward the same packed-topology style.

### 20. Bone transform writeback now uses step-basic-pose orientation as the reference

One likely cause of the strange twisting was that native bone writeback was still building bone rotations too directly from rest-space orientation plus current segment direction.

This round changes the writeback reference so that:

- the basic-pose world direction is used as the swing reference
- `step_basic_rotations` are used as the orientation baseline
- the current simulated segment direction only contributes the swing away from that baseline

This should reduce the "twist like a corkscrew" failure mode where the solve itself is roughly valid but the final bone rotation reconstruction is unstable.

### 21. Bone writeback now preserves a lateral reference to suppress free twist

The previous writeback fix still relied too much on a pure "rotate one direction vector onto another" style reconstruction.

That can still leave twist underconstrained even when the swing is correct.

The native writeback now builds the final bone orientation with:

- the simulated segment direction as the aimed Y axis
- the basic-pose orientation as the lateral reference

So the solver no longer has to guess the twist from a single segment direction alone.

This specifically targets the visual symptom where the chain seems structurally connected but bones spin and corkscrew in place.

## Important limitation after this step

The project is **not yet fully at MC2 BoneSpring architecture**.

What is still missing:

- fuller MC2-style step basic pose details and richer per-baseline temporary working buffers
- a stronger prebuild split beyond the current first extraction into `spring_prebuild.py`
- fuller native C++ runtime usage of `lines`, `baselines`, and per-baseline temp buffers for angle limit/restoration grouping details and iteration behavior
- collision and restoration grouping refined further around topology groups instead of the current simplified ordering

So this round should be understood as:

- topology model corrected
- authoring/preview corrected
- stub runtime partially adapted
- native solver architecture moved closer, but still simplified versus MC2

## Recommendation for the next step

The next large implementation step should be:

1. move spring topology generation into a clearer prebuild-style layer
2. add a more explicit MC2-like step-basic-pose buffer stage before restoration/limit
3. refine collision/restoration ordering around topology groups instead of the current simplified approximation

That is the point where HoCloth SpringBone will stop merely "supporting branching data" and start genuinely following the MC2 BoneSpring model.

### 22. The lateral-reference writeback experiment was rolled back, and the solver was pulled back toward MC2 parameter semantics

The first attempt to suppress free twist during native bone writeback by constructing a lateral-reference basis was **not stable enough** in HoCloth's current solver state.

That experiment was rolled back after it caused an immediate "bones flying away" failure during testing.

The current direction is now more conservative and closer to MC2's actual parameter semantics:

- BoneSpring effective gravity is forced back to `0`
- damping is consumed with the same `* 0.2` scale that MC2 applies during parameter conversion
- fixed-step solver iterations were increased from `1` to `3`, which is much closer to MC2's angle-side iterative behavior
- the start-step inertia mix was reduced so the solver is not hit by an oversized root/center shift before distance and angle constraints can stabilize the chain

This means the immediate priority is no longer "smarter writeback reconstruction."

It is now:

- remove non-MC2 external force behavior
- stabilize the first step / first few substeps
- then continue refining distance + angle behavior from that calmer baseline

### 23. The remaining gap was not just "one more parameter", but missing MC2 angle temporary buffers

After the "flying away" and then "collapsing into a bundle" failure modes, the next comparison against MC2 showed that HoCloth was still simplifying the angle phase too aggressively.

MC2's angle solve is not driven only by:

- one limit direction
- one restoration direction

It also depends on temporary per-baseline data such as:

- segment lengths captured before the limit pass
- parent-local segment directions
- parent-local baseline rotations
- a rotation buffer that is updated after angle-limit correction

HoCloth has now been moved closer to that structure:

- baseline buffers now keep local segment direction / local rotation / segment length
- angle-limit target direction is rebuilt from the parent rotation buffer instead of a fixed world-space blend
- the rotation buffer is updated after the limit pass so the next child in the baseline sees a propagated orientation

This is a much more important MC2 parity step than continuing to tune gravity or damping alone.

### 24. Startup stabilization and default-parameter propagation were still missing

After the solver began to behave more like MC2 during free swing, two practical gaps were still obvious:

- the chain would still kick too hard at simulation start
- changing the chain-level UI parameters often felt like it did nothing

Those turned out to be two separate issues.

For startup behavior:

- HoCloth still lacked an explicit reset-time stabilization ramp similar to MC2's `stablizationTimeAfterReset`
- a lightweight `velocity_weight / blend_weight` ramp has now been added on the native side so the chain eases into simulation instead of entering at full strength on frame one

For authoring behavior:

- chain-level defaults were not being pushed back into non-enabled joint overrides after editing the main component values
- property updates now resync disabled joint overrides for radius / stiffness / damping / drag so the default workflow behaves more predictably

Also note:

- MC2-style BoneSpring does not use gravity in the same way as cloth, so HoCloth now explicitly surfaces that limitation in the UI instead of silently implying that `gravity_strength` should currently respond like a cloth gravity slider

### 25. Startup flash and damping response needed another MC2-style pass

Two practical issues still remained after the solver became visually much closer to MC2:

- a small flash / kick could still happen when simulation first started
- the four spring presets still felt too close together, especially on the soft side

The startup issue was narrowed down further to a mismatch between the stabilized display position and the position used for velocity reconstruction.

HoCloth now applies the reset-time stabilization more consistently by:

- stabilizing both the predicted position and the velocity reference position
- avoiding a fake first-step velocity spike caused by mixing a blended display pose with an unstabilized velocity origin

The parameter-response issue came mostly from using the UI damping value too linearly.

To make the presets and manual tuning more useful:

- damping is now remapped nonlinearly before entering the runtime (`pow(damping, 2.2) * 0.2`)
- the stock presets were spread out again so `Soft Hair` sits much lower in effective damping / stiffness, while `Heavy` remains clearly more damped and resistant

This should make low-to-mid damping settings much more expressive instead of collapsing into the same "still pretty stiff" feel.

### 26. Branch imbalance came from shared-baseline over-influence and missing inverse-mass weighting

Another branch-specific issue remained visible in trees with many forks:

- the longest branch felt harder to move than shorter sibling branches

The likely reason was not a simple "wrong preset" problem.

It was structural:

- shared trunk joints were still being revisited by multiple baselines without enough normalization
- directional parent/child correction still lacked MC2-style mass asymmetry

To move closer to MC2:

- baseline buffers now count how many times each joint appears across all active baselines
- angle-limit and restoration weights are attenuated by that visit count so shared trunk joints are not over-constrained just because they belong to more branch paths
- parent/child directional correction now uses a simplified depth-based inverse-mass split inspired by MC2's depth-mass behavior, so roots stay heavier and distal joints remain easier to move

This should reduce the "longest branch feels unnaturally stiff" bias that can appear in heavily branched trees.

### 27. Added a real "restart from frame 1" tool for paused live simulation

The previous runtime reset tool only reset solver state.

It did not explicitly restore the current pose back to the captured baseline before restarting.

That made it inconvenient to restart a paused simulation cleanly after the pose had already been rotated by runtime output.

A new authoring-side operator now:

- stops live runtime if needed
- restores the captured baseline pose onto the armature
- resets runtime state
- refreshes runtime inputs and pose baseline

So users can now restart from a clean first-frame pose without manually zeroing rotations.

### 28. Runtime reset needed to reset the native scene too, not just the Python bookkeeping

One remaining usability bug was that "reset" and "restart from frame 1" could still leave visible pose residue or an unexplained kick on the first live frame.

The root cause was that the Python session layer was resetting counters and cached transforms, but it was not actually calling the native runtime scene reset.

That has now been corrected:

- `runtime.session.reset_runtime()` calls the native bridge reset directly
- live playback startup also forces a runtime reset before the first simulated frame
- the frame-1 restore path now pushes a harder pose clear + restore and forces a view-layer update

This should make both:

- the first live step
- the "return to frame 1" tool

behave much more like a true fresh start instead of a partial reset.
