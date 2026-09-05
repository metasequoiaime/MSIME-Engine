# WebView messages

`messages.json` owns the message envelope, payload shape, and allowed sender windows.
Change it here, run `python3 generate.py`, and update the consuming Engine pins together.
The generated TypeScript union and embedded C++ schema must pass `generate.py --check`.
`runtime.js` and `validator.h` implement the documented schema subset and run the same
`fixtures.json` compatibility cases. The standalone CMake test requires Boost.JSON;
the input engine itself does not acquire that dependency.

New messages carry `protocolVersion: 1`. A missing version is accepted as legacy v1
for existing skins; an explicit unknown version, unknown message, invalid payload,
or message sent from the wrong window is rejected before dispatch. This is envelope
validation, not a replacement for domain checks such as dictionary key validation,
allowed configuration values, or the host's HTTPS URL policy.

UiHtml materializes these bindings from its pinned Engine submodule into
`webview2/shared` and checks byte equality before builds. Settings imports the typed
serializer and checks incoming messages. Classic candidate, toolbar and menu pages
load the same runtime from the host's `https://msime-contracts/` virtual mapping;
packaging must include `webview2/shared`. Windows product CI pins the Server and
UiHtml Engine revisions to the same product contract.

Server-to-page candidate rendering currently uses the existing view functions;
this contract covers WebMessage actions and settings responses. Candidate view
model ownership is documented separately from transport envelopes.
