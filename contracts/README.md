# Product contracts

These headers are independently consumable C++17 contracts. Including them does not link the input engine, start threads, access dictionaries, or require its third-party submodules. The TSF text service consumes only these headers; the server consumes the same source through the Engine submodule. Both now live in MSIME-Windows — `windows/` and `server/` — so they share one pin rather than each carrying their own.

`windows_ipc.h` owns the wire layouts, pipe names and opcodes. `voice_composition_pipe.h` owns voice framing. Platform repositories may wrap these headers but must not duplicate their definitions. MSIME-Windows records the engine commit in `product-lock.json` as well as in the gitlink, and `product_lock.py verify-contracts` requires the two to agree — a bump that moves the submodule without the lock would otherwise attest to an engine revision the product was not built from.

`punctuation/policy.json` is the source of truth for the ASCII punctuation keys and their Chinese
translations. Its generated `policy.h` is consumed by the Engine and can be vendored by platform
hosts that route punctuation keys. Stateful quote alternation and nested book-title marks remain
part of the same contract, rather than being re-created in each host.

`product_lock.py` contains the shared product-lock primitives used by the three platform locks.
See [`product_lock.md`](product_lock.md) for the vendoring and byte-equality rule.

## Main handshake

Reverse endpoints retain their 16-byte hello and PipeReady acknowledgement. Once both endpoints are registered, the new client sends the existing 304-byte ClientHello with:

| Field | Meaning |
|---|---|
| keycode | MSIP magic |
| wch | Major wire protocol version |
| point[0] | Minor additive version |
| modifiers_down | Supported capability bits |
| point[1] | Required capability bits |
| request_id | Nonzero registration correlation ID |

The Server sends ProtocolReady or ProtocolMismatch on the registered reply endpoint before activating the main route. The reply echoes the correlation ID and contains major/minor, negotiated capabilities and magic in the first six UTF-16 units. These packets are consumed only during registration and are never text commits.

Existing DLLs with unversioned hello remain accepted by the new Server using the established v1 semantics; they receive no new opcode. New DLLs require an acknowledgement from the new Server within the existing bounded handshake budget. Against an old or incompatible Server they remain disconnected and use the existing raw-input fallback, rather than interpreting unnegotiated frames. A late acknowledgement from an old registration cannot authorize another one.

Append opcodes; never change released values or reuse them. An incompatible layout needs a new major protocol and an explicit migration, not another copy of a header. Minor additions must be optional capabilities.

`tests/windows_ipc_contract.cpp` executes wire-layout/upgrade/framing cases on all Engine CI platforms and both Windows TSF architectures.
