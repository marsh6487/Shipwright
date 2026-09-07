# Adult Ruto Water-Cycle POC Handoff

## Purpose

This is an isolated proof-of-concept for Adult Ruto static placement behavior. It intentionally does **not** modify native `En_Ru2`, native `En_Zo`, weather/audio code, or the static actor parameter encoding.

The POC adds **Adult Ruto pose 3** as a water-cycle pose and reuses:

- Adult Ruto's own `gAdultRutoSwimmingUpAnim`.
- The same waterbox depth/surface information used by ordinary actors.
- The behavioral shape of `En_Zo`: submerged -> surface -> tread -> dive -> submerged.

The implementation is supplied as `docs/pocs/adult-ruto-water-cycle-poc.patch` so it can be reviewed/applied independently of the current POC5 stabilization branch.

## Intended runtime behavior

Use Adult Ruto static type with pose 3 (`0x7F36` under the current legacy encoding: pose nibble 3, actor type 6).

- If placed deeper than the surface threshold inside a waterbox, Ruto remains at her Prelude `home.pos` underwater with interaction disabled.
- When Link approaches the water surface near her X/Z, she begins swimming upward using `gAdultRutoSwimmingUpAnim`.
- Near the actual waterbox surface she enters a small vertical tread/bob loop and becomes talkable through the existing static Adult Ruto dialogue path.
- When Link leaves for a short randomized delay, she dives by reversing the same animation and moving back toward her Prelude `home.pos`.
- Once back at the authored submerged point, the cycle resets and can repeat.

## Deliberate exclusions

- No horizontal patrol/pathfinding.
- No Water Temple one-point camera.
- No encounter switch flag.
- No self-kill at the end of the swim.
- No Water Medallion/cutscene actions.
- No native Ruto action-function calls.
- No Zora particle/effect array transplant in the first POC.
- No static catalogue parameter-extension work.

## Review notes for Work

1. The patch is intentionally small and should be applied on top of `codex/poc5-stabilization` or an equivalent descendant containing the current static Adult Ruto adapter.
2. First verify compile/API names (`Actor_MoveXZGravity`, `Actor_UpdateBgCheckInfo`, `yDistToWater`) against the branch; they were traced from the same source tree.
3. Run the focused `static_story_actor_test` after applying.
4. Runtime-test in a known vanilla waterbox first. Recommended acceptance test: place pose-3 Adult Ruto well below a pool surface, approach from the bank, verify surface/tread, move away, verify dive/reset, then repeat.
5. If the visual cycle is convincing, generalization into a reusable static water-behavior helper can happen later. Do not generalize before the runtime POC proves the animation looks natural.

## Verification status

The patch has been source-reviewed against the current `codex/poc5-stabilization` implementation and corrected for an early-call forward declaration. It has **not** been compiled in this chat environment because direct Git checkout is blocked by outbound DNS. Treat CI/Work's build as the authoritative compile verification after applying the patch.
