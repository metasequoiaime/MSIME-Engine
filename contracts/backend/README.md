# Backend HTTP protocol v1

`protocol.json` is the authoritative HTTP contract for MSIME-Backend and the platform clients. It owns paths, authentication, limits, JSON examples and async-result invariants. This service is separate from the Windows named-pipe host and does not transfer ownership of local input behavior away from Engine.

The initial API covers cloud candidates, non-streaming Chat Completions for suggestions and polish, DeepLX-compatible translation and multipart WAV transcription. Feature availability is explicit through `/v1/capabilities`. Optional provider-only request hints are documented in the manifest. A disabled feature fails explicitly; it never triggers a fallback to another network destination.

Go imports an exact manifest copy and generates constants with:

```sh
python3 ../MSIME-Backend/scripts/sync_contract.py --source contracts/backend/protocol.json
python3 ../MSIME-Backend/scripts/sync_contract.py --source contracts/backend/protocol.json --check
```

The Server's normal CI validates its bundled manifest against generated constants and exercises every operation's example over HTTP/TLS. Cross-repository review additionally compares the bundled manifest to this source. The manifest's source revision must be merged before consumers pin a new Engine gitlink; an unmerged local copy is development evidence, not a released dependency.

Compatible responses may add fields that clients ignore. Unknown request fields currently fail, so adding a client request field requires a server compatibility change first. Breaking semantics require a new API version/path. Platform code must retain Engine query identity, composition generation and focus checks, even if the request text is unchanged.

RIFF/WAVE chunk framing follows the [Microsoft RIFF specification](https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-). Supported input is PCM (8/16/24/32 bit) or IEEE float (32/64 bit), 1–8 channels, 8–192 kHz, within the upload byte limit. Current Engine hosts produce mono 16-bit PCM WAV.
