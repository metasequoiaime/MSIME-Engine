#pragma once
#include <memory>
#include <string>
#include <vector>

namespace metasequoia::handwriting {
struct Point { float x; float y; };
using Stroke = std::vector<Point>;
// One character per recognition. Instances are thread-confined; strokes are never retained.
class Recognizer {
 public:
  // The model must be a trusted packaged Zinnia model, not a user-supplied file.
  explicit Recognizer(const std::string& model_path);
  ~Recognizer();
  Recognizer(const Recognizer&) = delete;
  Recognizer& operator=(const Recognizer&) = delete;
  std::vector<std::string> recognize(const std::vector<Stroke>& strokes, float width, float height);
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}
