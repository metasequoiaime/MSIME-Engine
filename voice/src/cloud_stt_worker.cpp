#include "cloud_stt_worker.h"
#include "wav_writer.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>

// Helper for CURL write callback
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

CloudSttWorker::CloudSttWorker(const std::string &api_token) : api_token_(api_token)
{
    curl_global_init(CURL_GLOBAL_ALL);
}

CloudSttWorker::~CloudSttWorker()
{
    curl_global_cleanup();
}

std::string CloudSttWorker::recognize(const std::vector<float> &pcm)
{
    // 1. Convert PCM to WAV in memory
    std::vector<uint8_t> wav_data = WavWriter::create_wav(pcm);

    // 2. Prepare CURL request
    CURL *curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl)
    {
        // Headers
        struct curl_slist *headers = NULL;
        std::string auth_header = "Authorization: Bearer " + api_token_;
        headers = curl_slist_append(headers, auth_header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Multipart form data
        curl_mime *mime;
        curl_mimepart *part;

        mime = curl_mime_init(curl);

        // "file" part: WAV data
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "file");
        curl_mime_filename(part, "audio.wav"); // Filename is required by some servers
        curl_mime_data(part, reinterpret_cast<const char *>(wav_data.data()), wav_data.size());
        curl_mime_type(part, "audio/wav");

        // "model" part
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "model");
        curl_mime_data(part, model_.c_str(), CURL_ZERO_TERMINATED);

        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(curl, CURLOPT_URL, api_url_.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // Timeout
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        // Perform request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
        {
            std::cerr << "[CloudSTT] curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        else
        {
            // Parse JSON response
            try
            {
                // Response format: {"text": "Transcribed text"}
                auto json = nlohmann::json::parse(readBuffer);
                if (json.contains("text"))
                {
                    std::string text = json["text"].get<std::string>();
                    // std::cout << "[CloudSTT] Recognized: " << text << std::endl;
                    return text;
                }
                else if (json.contains("error"))
                {
                    std::cerr << "[CloudSTT] API Error: " << json["error"] << std::endl;
                }
                else
                {
                    std::cerr << "[CloudSTT] Unexpected response: " << readBuffer << std::endl;
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "[CloudSTT] JSON Parse Error: " << e.what() << " | Raw: " << readBuffer << std::endl;
            }
        }

        // Cleanup
        curl_mime_free(mime);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return "";
}
