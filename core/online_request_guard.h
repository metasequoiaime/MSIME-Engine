#pragma once
#include "input_session_types.h"
#include <atomic>
namespace metasequoia
{
// A response belongs to both one session and one composition generation.
class OnlineRequestGuard
{
  public:
    OnlineRequestGuard() : id_(next_id_.fetch_add(1, std::memory_order_relaxed))
    {
    }
    void invalidate()
    {
        ++generation_;
    }
    void stamp(OnlineQuery &query) const
    {
        query.session_id = id_;
        query.generation = generation_;
    }
    bool matches(const OnlineQuery &current, const OnlineQuery &incoming) const
    {
        return incoming.session_id == id_ && incoming.generation == generation_ && current.scheme == incoming.scheme &&
               current.identity == incoming.identity && current.query_text == incoming.query_text &&
               current.cache_key == incoming.cache_key && current.pinyin_segments == incoming.pinyin_segments &&
               current.cloud_eligible == incoming.cloud_eligible && current.ai_eligible == incoming.ai_eligible;
    }

  private:
    inline static std::atomic<std::uint64_t> next_id_{1};
    const std::uint64_t id_;
    std::uint64_t generation_ = 0;
};
} // namespace metasequoia
