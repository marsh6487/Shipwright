#!/usr/bin/env bash
# Required source-level checks before distributing a cumulative working build.
set -euo pipefail
cd "$(dirname "$0")/../.."
python3 -B scripts/diagnostics/check_feature_baselines.py
python3 -B -m unittest discover -s scripts/diagnostics -p test_feature_baselines.py
python3 -B scripts/diagnostics/test_young_epona_assets.py
python3 -B scripts/diagnostics/run_young_epona_tests.py
python3 -B scripts/diagnostics/run_young_epona_actor_tests.py
python3 -B scripts/diagnostics/run_young_epona_player_tests.py
python3 -B scripts/diagnostics/run_young_epona_save_tests.py
python3 -B scripts/diagnostics/run_midna_navi_draw_test.py
python3 -B scripts/diagnostics/run_midna_audio_test.py
python3 -B scripts/diagnostics/run_epona_cosmetics_tests.py
python3 -B scripts/diagnostics/run_alt_segment_binding_tests.py
python3 -B scripts/diagnostics/run_house_rocs_feather_tests.py
python3 -B scripts/diagnostics/run_zora_barrier_cosmetics_tests.py
python3 -B scripts/diagnostics/run_oot_custom_cosmetics_tests.py
python3 -B scripts/diagnostics/run_stat_upgrade_tests.py
python3 -B scripts/diagnostics/run_chest_size_tests.py
python3 -B scripts/diagnostics/run_time_pedestal_tests.py
python3 -B scripts/diagnostics/run_pedestal_sword_selection_tests.py
python3 -B scripts/diagnostics/check_time_pedestal_syntax.py
bash scripts/diagnostics/run_stabilization_tests.sh
