# Punctuation contract

`policy.json` is the authoritative list of ASCII punctuation keys handled by Chinese punctuation
mode. `generate.py` emits `policy.h`; consumers must use the generated header and must not copy the
mapping into a platform-specific table.

The contract contains stateless simple mappings plus the opening/closing pairs for alternating
quotes and nested book-title marks. The quote toggles and nesting depth are session state, so each
input session owns that state; the contract does not provide shared mutable state.

Run `python3 contracts/punctuation/generate.py --check` from the Engine root to verify the checked-in
header. The C++ contract test is registered by the root CMake project and runs on all supported
platforms.
