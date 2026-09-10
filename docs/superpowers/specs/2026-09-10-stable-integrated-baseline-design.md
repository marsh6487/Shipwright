# Stable Integrated Baseline Design

Date: 2026-09-10
Status: Proposed for user review

## Goal

Create one durable, known-good Shipwright integration baseline for ongoing personal-fork work. The baseline should contain the features we actually intend to keep, while preventing known or newly introduced regressions from silently becoming part of the new master.

## Target feature set

The first stable integrated baseline is intended to contain:

- Static actor catalogue and the desired OoT/MM catalogue actors.
- Weather system and outdoor/intermittent rain behavior.
- Hyrule Field dusk/night music behavior and related audio fixes.
- Scene-edit compatibility required by the user's edited O2R scene stack.
- Custom player-model support.
- The NEI integration once it passes the same promotion gates.

A feature being present on an existing integration branch does not automatically qualify it for the stable baseline.

## Branch model

Use three levels of branches:

1. `stable` candidate/baseline: the known-good integration line. No exploratory fixes are made directly here.
2. Integration candidates: temporary branches that combine individually verified topic fixes for a release-candidate build.
3. Topic branches: isolated work for one regression or subsystem, such as actor rendering, weather timing, music transitions, or NEI integration.

The existing integration branches are source material, not automatically the stable baseline.

## First promotion cycle

The first candidate is green only after the already-known regressions are resolved or intentionally excluded:

- Lost Woods Room 8/static actor renderer crash, including Adult Malon basket and Child Malon paths.
- Skull Kid/MM catalogue render stability.
- Bomb Shop Lady/MM-backed actor stability.
- Zant placement/anchor regression.
- Ruto knee-axis correction and desired fade behavior.
- Lost Woods room 4/7 rain persistence when rain is disabled.
- Intermittent outdoor rain activation and phase timing.
- Hyrule Field dusk-to-night music persistence.
- NEI integration/build compatibility.

No unrelated feature expansion is part of this promotion cycle.

## Actor-renderer repair

The current Room 8 crash remains a topic-branch blocker. Evidence shows the actor reaches ready/draw and the renderer subsequently encounters malformed/invalid graphics commands. The repair should enforce explicit draw-time ownership of the model object's segmented resources rather than depending on graphics state left by another actor, player-model override, or mod.

The actor-renderer topic branch must add a focused regression contract/test before the implementation change. Runtime validation must include repeated Room 8 entry/re-entry with default Adult Link and a custom player model, and the affected catalogue actors. A single successful scene entry is not sufficient because the observed bug is state-dependent.

## Promotion gates

A topic fix may enter an integration candidate only when its focused automated tests pass and the relevant runtime reproduction no longer fails.

An integration candidate may become the stable baseline only when all of the following are fresh and green:

- Linux configure/build and relevant automated tests.
- Windows configure/build and relevant automated tests.
- Clean startup with the intended baseline mod stack.
- Repeated scene transition/re-entry smoke tests for known stateful failures.
- Catalogue actor smoke test covering the actors that previously exposed rendering/placement issues.
- Weather smoke test covering disabled rain, intermittent activation, dry/rain phase timing, and Lost Woods persistence.
- Audio smoke test covering Hyrule Field dusk-to-night transition.
- Custom player-model smoke test.
- NEI smoke/build test once NEI is included.

Logs from promotion testing should be retained so a future regression can be compared against a known-good run.

## Promotion rule

Do not stack an unverified fix onto an already-red integration candidate. A failing candidate is repaired on the responsible topic branch or the offending change is removed/reverted from the candidate. Only verified changes move upward.

The stable baseline advances by fast-forward or an explicit reviewed integration commit after all gates pass. The previous stable commit remains tagged/recoverable so there is always a known rollback point.

## Regression ownership

Each known regression gets one responsible topic branch and one success criterion. Cross-subsystem symptoms are diagnosed at the lowest shared ownership boundary rather than patched independently in every affected scene or actor. For example, a shared segmented-render-state corruption should be fixed in the catalogue renderer rather than by editing every Malon placement or scene O2R.

## Definition of done

The first stable integrated baseline is done when the intended feature set is present, the known regression list above is green, Linux and Windows builds pass, the runtime promotion checklist passes repeatedly, and the resulting commit is designated as the new known-good integration baseline for all subsequent work.
