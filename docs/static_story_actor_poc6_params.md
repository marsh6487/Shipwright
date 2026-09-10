# Static Story Actor Catalogue — POC6

Place these as **Cutscene Actors (`En_Viewer`)** in Prelude and enter the full
four-digit value in **Params**. These placements use self-contained static
adapters: they do not call the source NPC's story actions, grant rewards, start
cutscenes, or set progression flags.

## POC6 expansion page

| Param | Actor | Pose / behavior | Collision | Blink | Tracking | Water requirement |
|---|---|---|---|---|---|---|
| `0x7E01` | Darunia | Standing idle | Soft pushable | Yes | While conversational | None |
| `0x7E11` | Darunia | Complete four-part native dance cycle | Soft pushable | Yes | No | None |
| `0x7E02` | Nabooru | Standing, hands on hips | Soft pushable | Yes | While conversational | None |
| `0x7E03` | Adult Ruto | Grounded idle | Soft pushable | Yes | While conversational | None |
| `0x7E13` | Adult Ruto | Hold at native water-surface height | Soft pushable while surfaced | Yes | While conversational | Valid waterbox |
| `0x7E23` | Adult Ruto | Repeating dive, submerge, and rise cycle | Surfaced only | Yes while visible | No during performance | Valid waterbox |

If a water-mode Ruto cannot find a valid waterbox, she safely falls back to her
Prelude Y placement instead of running story logic or searching indefinitely.

## Existing POC4 page (preserved)

| Param | Actor | Pose | Collision | Blink | Tracking |
|---|---|---|---|---|---|
| `0x7F01` | Impa | Idle | Soft pushable | Yes | Yes |
| `0x7F02` | Child Malon | Idle | Soft pushable | Yes | Yes |
| `0x7F12` | Child Malon | Singing animation | Soft pushable | Yes | No |
| `0x7F22` | Child Malon | Singing animation with vanilla vocal pattern | Soft pushable | Yes | No |
| `0x7F03` | Saria | Arms at sides | Soft pushable | Yes | Yes |
| `0x7F13` | Saria | Hands behind back | Soft pushable | Yes | Yes |
| `0x7F23` | Saria | Playing ocarina | Soft pushable | Fixed performance face | No |
| `0x7F33` | Saria | Seated | Soft pushable | Yes | No |
| `0x7F04` | Adult Zelda | Native standing idle (repaired in POC6) | Soft pushable | Yes | No |
| `0x7F14` | Adult Zelda | Native standing idle alias | Soft pushable | Yes | No |
| `0x7F05` | Sheik | Idle | Soft pushable | Yes | Yes |
| `0x7F15` | Sheik | Arms crossed | Soft pushable | Yes | Yes |
| `0x7F25` | Sheik | Playing harp | Soft pushable | Yes | No |
| `0x7F06` | Adult Ruto | Idle | Soft pushable | Yes | Yes |
| `0x7F16` | Adult Ruto | Hands on hips | Soft pushable | Yes | Yes |
| `0x7F26` | Adult Ruto | Looking down-left | Soft pushable | Yes | Yes |
| `0x7F07` | Child Ruto | Hands behind back | Soft pushable | Yes | Yes |
| `0x7F17` | Child Ruto | Hands on hips | Soft pushable | Yes | Yes |
| `0x7F27` | Child Ruto | Sitting | Soft pushable | Yes | No |
| `0x7F08` | Kokiri Girl | Idle | Soft pushable | Yes | Yes |
| `0x7F18` | Kokiri Girl | Arms behind back | Soft pushable | Yes | Yes |
| `0x7F28` | Kokiri Girl | Hands on hips | Soft pushable | Yes | Yes |
| `0x7F38` | Kokiri Girl | Sitting, head on hand | Soft pushable | Yes | No |
| `0x7F48` | Kokiri Girl | Sitting cross-legged | Soft pushable | Yes | No |
| `0x7F58` | Kokiri Girl | Sitting, arms and legs crossed | Soft pushable | Yes | No |
| `0x7F09` | Fado | Idle | Soft pushable | Yes | Yes |
| `0x7F19` | Fado | Arms behind back | Soft pushable | Yes | Yes |
| `0x7F29` | Fado | Hands on hips | Soft pushable | Yes | Yes |
| `0x7F39` | Fado | Sitting, head on hand | Soft pushable | Yes | No |
| `0x7F49` | Fado | Sitting cross-legged | Soft pushable | Yes | No |
| `0x7F59` | Fado | Sitting, arms and legs crossed | Soft pushable | Yes | No |
| `0x7F0A` | Adult Malon | Idle | Soft pushable | Yes | Yes |
| `0x7F1A` | Adult Malon | Holding basket | Soft pushable | Yes | Yes |
| `0x7F2A` | Adult Malon | Singing animation | Soft pushable | Yes | No |
| `0x7F3A` | Adult Malon | Singing animation with vanilla vocal pattern | Soft pushable | Yes | No |

## Runtime test priorities

1. Verify native Child Malon, Child Saria, and Child Ruto story placements with
   ordinary entrances; debug-menu warps can suppress vanilla NPC setup state.
2. Test `0x7E13` and `0x7E23` in both shallow and deep valid waterboxes, then
   leave and re-enter the room.
3. Let `0x7E11` run long enough to see all four Darunia dance clips repeat.
4. Test Adult Zelda under vanilla assets and alternate assets.
5. Unsupported `0x7Exx` values are intentionally rejected rather than aliased
   to an existing actor.
