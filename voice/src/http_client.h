#pragma once
#include "stt_service.h"
#include <curl/curl.h>
#include <functional>
namespace metasequoia::voice::detail
{
std::string request(const RequestOptions &options, const std::function<std::shared_ptr<void>(CURL *)> &configure);
}
