#pragma once
#include <string>
#include <vector>

class SttService
{
  public:
    virtual ~SttService() = default;
    virtual std::string recognize(const std::vector<float> &pcm) = 0;
};
