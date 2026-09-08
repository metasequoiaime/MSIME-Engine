# Offline handwriting

`metasequoia::handwriting::Recognizer` accepts one character's ordered pen strokes and a canvas size, and returns up to eight candidate characters. Instantiate lazily on a worker and reuse it; instances are thread-confined. No ink, UI state, text insertion or network access belongs to this module. Empty ink returns no candidates; invalid dimensions, nonfinite/out-of-bounds points, more than 64 strokes or 512 points per stroke are rejected.

Zinnia performs online (stroke-based, not image OCR) recognition. The bundled Tegaki Simplified Chinese 0.3 model is memory-mapped, so platforms should pass its trusted packaged path. Do not open arbitrary user-supplied models. The model is 26,834,816 bytes; platforms can choose whether to bundle this optional component. Recognition is single-character, not multi-character sentence segmentation.

`provenance.json` records the exact upstream source revision, archive URL, SHA-256 and model SHA-256. Zinnia sources are BSD-licensed; `third_party/zinnia/Zinnia-LICENSE.txt` contains the notice. The Tegaki model is LGPL-2.1; `models/HandwritingModel-LICENSE.txt` contains its license. Its matching training XML, metadata and build instructions are available in the pinned source archive referenced in `provenance.json`. Distributors must include both notices and the model's source location.

Build the root CMake project and run `ctest`; `MetasequoiaHandwritingTests` exercises real 中 pen trajectories, empty/invalid input and missing-model errors. Apple consumes the same C++ implementation and packaged model through its Objective-C++ bridge.
