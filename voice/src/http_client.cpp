#include "http_client.h"
#include <limits>
#include <memory>
#include <mutex>
namespace metasequoia::voice::detail {
namespace {
struct Response { std::string body; };
std::size_t write(void* data, std::size_t size, std::size_t count, void* context) noexcept {
    constexpr std::size_t limit = 1024 * 1024;
    if (size && count > (std::numeric_limits<std::size_t>::max)() / size) return 0;
    const auto length = size * count;
    auto& body = static_cast<Response*>(context)->body;
    if (length > limit - body.size()) return 0;
    try { body.append(static_cast<const char*>(data), length); } catch (...) { return 0; }
    return length;
}
int progress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) noexcept {
    const auto* cancelled = static_cast<const std::atomic_bool*>(context);
    return cancelled && cancelled->load() ? 1 : 0;
}
struct CurlRuntime {
    CurlRuntime() { if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) throw VoiceError("Cannot initialize HTTP runtime"); }
    ~CurlRuntime() { curl_global_cleanup(); }
};
}
std::string request(const RequestOptions& options, const std::function<std::shared_ptr<void>(CURL*)>& configure) {
    if (options.cancelled && options.cancelled->load()) throw VoiceError("Voice request cancelled");
    if (options.endpoint.empty() || options.token.empty() || options.model.empty() || options.timeout_ms <= 0)
        throw VoiceError("Voice endpoint, token, model and positive timeout are required");
    if (options.token.find_first_of("\r\n") != std::string::npos) throw VoiceError("Invalid voice token");
    static CurlRuntime runtime;
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(curl_easy_init(), curl_easy_cleanup);
    if (!handle) throw VoiceError("Cannot create HTTP request");
    const auto auth = "Authorization: Bearer " + options.token;
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(curl_slist_append(nullptr, auth.c_str()), curl_slist_free_all);
    if (!headers) throw VoiceError("Cannot create HTTP headers");
    auto* next = curl_slist_append(headers.get(), "Content-Type: application/json");
    // configure() can override Content-Type for multipart requests.
    if (!next) throw VoiceError("Cannot create HTTP headers");
    headers.release(); headers.reset(next);
    CURL* curl = handle.get();
    Response response;
    curl_easy_setopt(curl, CURLOPT_URL, options.endpoint.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, options.timeout_ms);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, options.timeout_ms);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, options.cancelled.get());
    const auto request_resources = configure(curl);
    const auto result = curl_easy_perform(curl);
    if (result != CURLE_OK) throw VoiceError(std::string("Voice HTTP request failed: ") + curl_easy_strerror(result));
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (status < 200 || status >= 300) throw VoiceError("Voice HTTP status " + std::to_string(status));
    if (options.cancelled && options.cancelled->load()) throw VoiceError("Voice request cancelled");
    return response.body;
}
}
