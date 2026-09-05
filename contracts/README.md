# Product contracts

These headers are independently consumable C++17 contracts. Including them does not link the input engine, start threads, access dictionaries, or require its third-party submodules. Windows TSF consumes only these headers; Server consumes the same source through its existing Engine submodule.

`windows_ipc.h` owns the wire layouts, pipe names and opcodes. `voice_composition_pipe.h` owns voice framing. Platform repositories may wrap these headers but must not duplicate their definitions. The product lock requires the Windows and Server Engine pins to agree.

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
