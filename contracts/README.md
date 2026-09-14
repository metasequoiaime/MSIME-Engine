# Product contracts

The additive [voice controller v2 contract](voice_controller.md) separates
Tauri controller identity from the Server-owned TSF target lease. It uses a
dedicated endpoint and does not change the released v1 voice-control layout.

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

## Keyboard composition cancellation

`KeyboardCompositionCancel` is an optional capability, not part of `Capabilities`
or `RequiredCapabilities`. Both peers opt in after implementing it. Legacy
unversioned connections never authorize it, even if their negotiation result
contains server-local optional bits. Use `FanyImeKeyboardCompositionPipe::CanCancel`.

Worker opcode `CancelKeyboardComposition` (22) retains the 404-byte worker layout.
`keyboard_composition_pipe.h` encodes a canonical nonzero decimal activation/focus
token as UTF-16, followed by NUL and zero padding. Zero, the no-request sentinel,
overflow, non-ASCII digits and nonzero trailing data are invalid. The frame carries
no text and never requests a commit, a voice operation or a CN/EN compartment change.

The Server must serialize cancellation with key/selection transactions and validate
the current authenticated client, transport generation and focus lease before
writing. The TIP must require the negotiated capability, validate the complete
frame, match the token to its current focus fence and capture its local composition
epoch before queuing UI work. The UI handler and its TSF edit session must recheck
both identities before deleting keyboard preedit and ending composition without
committing. Idle cancellation is a no-op; stale work must not cancel a newer
composition or touch voice composition. A failed edit must not be reported as
applied. This version has no application acknowledgement: successful transport
delivery proves only delivery, not that the host executed the cancellation.

Older peers omit the capability and must not receive this opcode. Do not fall back
to an empty candidate commit (a reverse-pipe trigger), a voice cancellation, or a
pair of language switches. Unknown future worker opcodes retain the existing
ignore behavior. The contract test covers opt-in/new-old/legacy negotiation and
strict frame parsing; platform consumers still need native focus/edit-session tests.

`CharacterSetShortcut` is optional and is not included in the default capability set.
Windows implementations that support it pass `Capabilities | CharacterSetShortcut`
to `Hello` / `Negotiate`. Only after negotiating this bit may a Chinese-mode client
consume Ctrl+Shift+F (no Alt or Windows modifier) and send the existing `KeyEvent`
with keycode `F` and Shift/Control modifier bits. The server toggles the persisted
character set without clearing composition, and refreshes the current candidate
page and toolbar. It sends no key reply. Clients suppress auto-repeat and must not
replay an ambiguously delivered toggle; older peers simply omit the optional bit.

`tests/windows_ipc_contract.cpp` executes wire-layout/upgrade/framing cases on all Engine CI platforms and both Windows TSF architectures.

`backend/protocol.json` 定义 MSIME-Backend 的可选共通 HTTP 服务 API，详见[后端协议与兼容性](backend/README.md)。
