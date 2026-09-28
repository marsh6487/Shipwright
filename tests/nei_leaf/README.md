# Deku Leaf ground activation probe

Run `python tests/nei_leaf/run_tests.py` from the repository root.

This fixture runs the production ground activation, upper-action, gust, and stop code, plus the native `LinkAnimation_Once` function extracted verbatim. The animation frame copy uses the checked-in source payload; the runner checks that it matches the packed resource byte-for-byte. ASan and UBSan remain enabled. Leak checking is disabled because the execution sandbox prevents LeakSanitizer from enumerating threads.

The checks cover first ground activation, normal completion and input-lock release, repeat activation, interruption, missing animation, insufficient magic, a single magic charge, and the six-tick gust collider window. They pass on the unchanged activation implementation. No production crash fix is claimed.

Input, resource-manager loading/ownership, animation initialization, collision registration, audio, and particle allocation are fixture boundaries. The fixture does not exercise native upper-action selection, display lists, GPU execution, active mod-pack overrides, or resource cache eviction. It cannot establish the cause of the reported first-use freeze/crash.

Source review also found that `ResourceMgr_LoadPlayerAnimAsHeader` retains a wrapper header but not the owning animation resource. Evicting that resource could invalidate its segment. There is no evidence that this occurred during the reported activation, so the helper was left unchanged.
