#include "ytapi.h"
#include "crow_all.h"
#include <cpr/cpr.h>
#include <sstream>

using namespace dj;

YtApi::YtApi(const std::string &api_key) : api_key_(api_key) {}

static std::chrono::seconds ParseISO8601(const std::string &s) {
    std::chrono::seconds result(0);
    std::istringstream is(s);
    uint64_t n = 0;

    is.get(); // P
    is.get(); // T
    while (is >> n) {
        switch (is.get()) {
        case 'H':
            result += std::chrono::hours(n);
            break;
        case 'M':
            result += std::chrono::minutes(n);
            break;
        case 'S':
            result += std::chrono::seconds(n);
            break;
        }
    }

    return result;
}

std::optional<Video> YtApi::Video(const std::string &id) {
    cpr::Response r = cpr::Get(
        cpr::Url{"https://www.googleapis.com/youtube/v3/videos"},
        cpr::Parameters{
            {"part", "snippet,contentDetails"}, {"id", id}, {"key", api_key_}});

    if (r.status_code != 200) {
        return std::nullopt;
    }

    crow::json::rvalue yt_data = crow::json::load(r.text);
    std::string title = yt_data["items"][0]["snippet"]["title"].s();
    auto duration =
        ParseISO8601(yt_data["items"][0]["contentDetails"]["duration"].s());

    dj::Video video{id, title, duration};
    return video;
}

std::optional<std::vector<SearchResult>>
YtApi::Search(const std::string &query) {
    cpr::Response r =
        cpr::Get(cpr::Url{"https://www.googleapis.com/youtube/v3/search"},
                 cpr::Parameters{
                     {"q", query},
                     {"maxResults", "50"},
                     {"type", "video"},
                     {"part", "snippet"},
                     {"safeSearch",
                      "moderate"}, // Restricted videos require age verification
                                   // anyway, and safe is kid-friendly which is
                                   // not the public of this project
                     {"topicId", "/m/04rlf"}, // Music (Freebase Id)
                     {"videoEmbeddable", "true"},
                     {"key", api_key_}});

    if (r.status_code != 200) {
        return std::nullopt;
    }

    crow::json::rvalue yt_data = crow::json::load(r.text);
    std::vector<SearchResult> results;

    //  An "item" has the follwing format {
    //    "id": {
    //      "videoId": "some-video-id"
    //    },
    //    "snippet": {
    //      "title": "SOME VIDEO TITLE",
    //      "thumbnails": {
    //        "default": {
    //          "url": "https://i.ytimg.com/vi/some-video-id/default.jpg",
    //        },
    //      },
    //    }
    //  }

    for (const auto &item : yt_data["items"].lo()) {
        // The youtube API can return channels when asked for videos
        if (!item["id"].has("videoId"))
            continue;
        results.emplace_back(
            item["id"]["videoId"].s(), item["snippet"]["title"].s(),
            item["snippet"]["thumbnails"]["default"]["url"].s());
    }

    return results;
}
