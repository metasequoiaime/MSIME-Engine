# Voice controller v2

`voice_controller.h` defines a separate local message-mode pipe. It does not
change the TSF v1 hello, voice-composition framing or Main/reverse/Aux protocol.
Wire integers are little endian; header sizes and offsets are asserted. Each
pipe message is exactly one header followed by its declared UTF-8 payload.
Receivers validate UTF-8, reject NUL/control characters in language, reject
unknown fields/versions, and never log payloads or credentials.

## Identity and ownership

The first request is Hello. The server verifies the OS-reported client PID
against the high 32 bits of controller_id, retains a live process handle, and
requires the same account and OS logon session. Local-only pipe access and a
restricted DACL are required; a hello number alone is not authentication. The
client independently authenticates the server process/account/session. Neither
side may accept an unverified endpoint or follow endpoint replacement.

After Hello, the controller identity cannot change and request IDs strictly
increase without wraparound. Each connection allows one outstanding request
and at most one recording. A Start payload is a nonempty language identifier.
The server captures the current TSF focus lease internally and revalidates it
on the owning control thread before recording starts. No client-supplied TSF
identity, registration generation, activation epoch or focus token is accepted.
The returned session_id is nonzero, server-issued, and scoped to this live
connection. Stop, Cancel and Poll must match that session; stale commands must
not mutate a later recording. Repeated Hello is invalid. A connection cannot
resume an earlier disconnected session.

## Lifecycle and result delivery

Start returns Recording after capture actually starts, not when a task is
merely queued. Stop returns Recognizing, leaving the session alive for its
final result. Poll returns its current phase, optional bounded transcript and
recording level (0..1000). Complete may legitimately contain empty text for
silence. Cancelled/Failed and non-Ok responses carry no transcript or level.
Every reply echoes its request ID; clients reject mismatches. Transport errors
after a submitted lifecycle write are uncertain, never automatic replay.

For these review-panel sessions, the controller is the only final text-submit
owner. The native recognizer must suppress inline TSF composition and automatic
TSF/SendInput/clipboard commit, including fallbacks. The existing hotkey-owned
native voice path keeps its separate behavior. Reviewing/receiving a result is
not authorization to type it into a later focused window.

The server serializes microphone ownership across hotkey and controller
sessions. It cancels the controller-owned session on disconnect, peer death or
idle timeout, invalidating callbacks before releasing ownership. Cancellation
must not affect a newer hotkey session. Completed snapshots remain available
until cancellation, the next Start, or disconnect; Start may replace only a
terminal snapshot, never an active recording/recognition.

Both sides bound queued work, accepted connections, handshakes, reads, writes
and teardown. A queued command whose caller has timed out/disconnected must
not execute later. OS cancellation is drained before destroying I/O buffers.
Clients poll at least once per second while active; the server may close a
connection idle for five seconds. Slow readers never hold the UI/focus lock.

This contract is not a listener, recognizer, Tauri client or platform-integration
completion claim. Those consumers must implement the ownership checks above.
