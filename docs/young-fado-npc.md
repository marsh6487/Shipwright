# Young Fado NPC candidate

The optional `Young_Fado_SoH_NPC_POC1_Cloth_Reviewed.o2r` supplies a complete
Young Fado body with the native Kokiri hierarchy. Its private skeleton is
selected only by native Fado (`En_Ko` type 12) and static Fado. Without that
archive, both actors retain their original resource paths. The archive includes
the native fallback rig so Alternate Assets can switch an existing actor
between the two models without resetting its animation.

Native Fado keeps her game animations, dialogue, head tracking, blink and
distance fade. The NPC mouth is fixed. Static Fado retains her existing
placement and parameters, including crossed legs `0x7F49` and crossed arms
and legs `0x7F59`. Only these two poses select the archive's seated cloth
wrappers. The existing pose-4 leg sampler and head correction are unchanged.
The ordinary Kokiri girl and player model keep their own resources.

Install the archive in this build's active mods folder, enable Alternate Assets,
and remove conflicting Fado NPC replacement packs for the initial comparison.
The playable Young Fado pack has separate resources. The model archive is
distributed separately from this source change; its SHA-256 is
`cb79af0699e314207247304bf5ea6daeeb9c943e6cec6b9f1695ec283c4b87e8`.

The candidate preserves POC8's standing waist and hem and restores small
source-derived cloth folds. Its recovered archive passes serialized geometry,
material, resource-reference, blink, native fallback and fade checks. Standing
and both seated cloth variants pass 223 quantized animation samples. These
are offline checks; the candidate has not been accepted or promoted in game.

Run the adapter checks with
`python3 scripts/diagnostics/run_young_fado_npc_tests.py`. With a read-only
native archive, the existing motion regression is
`python3 scripts/diagnostics/run_kokiri_pose_tests.py /path/to/oot.o2r`.

In game, check Fado's idle/laughter, dialogue, tracking, blink and distance
fade; both seated poses; Alternate Assets toggling and scene re-entry; and
the ordinary Kokiri girl and playable Young Fado as controls. Boot height,
wrists and the previously reported right-elbow shading artifact still need
runtime review. Static pose 3 has not received this candidate's seated-cloth
validation.
