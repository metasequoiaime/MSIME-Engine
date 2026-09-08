#include <metasequoia/handwriting.h>
#include "third_party/zinnia/zinnia.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace metasequoia::handwriting {
struct Recognizer::Impl { std::unique_ptr<zinnia::Recognizer> engine{zinnia::Recognizer::create()}; };
Recognizer::Recognizer(const std::string& path) : impl_(std::make_unique<Impl>()) {
  if (!impl_->engine->open(path.c_str())) throw std::runtime_error("Cannot open handwriting model");
}
Recognizer::~Recognizer() = default;
std::vector<std::string> Recognizer::recognize(const std::vector<Stroke>& strokes, float width, float height) {
  if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0 || width > 10000 || height > 10000 || strokes.size() > 64)
    throw std::invalid_argument("Invalid handwriting canvas");
  if (strokes.empty()) return {};
  std::unique_ptr<zinnia::Character> ink(zinnia::Character::create());
  ink->set_width(1000); ink->set_height(1000);
  float min_x = width, min_y = height, max_x = 0, max_y = 0;
  for (size_t i = 0; i < strokes.size(); ++i) {
    if (strokes[i].empty() || strokes[i].size() > 512) throw std::invalid_argument("Invalid handwriting stroke");
    for (auto point : strokes[i]) {
      if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x < 0 || point.y < 0 || point.x > width || point.y > height)
        throw std::invalid_argument("Invalid handwriting point");
      min_x = std::min(min_x, point.x); min_y = std::min(min_y, point.y);
      max_x = std::max(max_x, point.x); max_y = std::max(max_y, point.y);
    }
  }
  // Normalize the character as a whole without distorting its aspect ratio on wide keyboards.
  const float scale = 800 / std::max({max_x - min_x, max_y - min_y, 1.0f});
  const float center_x = (min_x + max_x) / 2, center_y = (min_y + max_y) / 2;
  for (size_t i = 0; i < strokes.size(); ++i)
    for (auto point : strokes[i]) ink->add(i, 500 + (point.x - center_x) * scale, 500 + (point.y - center_y) * scale);
  std::unique_ptr<zinnia::Result> result(impl_->engine->classify(*ink, 8));
  if (!result) throw std::runtime_error("Handwriting recognition failed");
  std::vector<std::string> candidates;
  for (size_t i = 0; i < result->size(); ++i) candidates.emplace_back(result->value(i));
  return candidates;
}
}
