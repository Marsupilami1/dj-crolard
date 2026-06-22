#include "message.h"

using namespace dj::msg;

std::string OutMessage::Serialize() const {
    crow::json::wvalue res;
    res["message"] = Message();
    res["payload"] = Payload();
    return res.dump();
};

// Synchronize video
Sync::Sync(const Video &video, const std::chrono::milliseconds &elapsed_time)
    : payload_(video), elapsed_time_(elapsed_time) {}

Sync::Sync(const Video &video) : payload_(video), elapsed_time_(0) {}

std::string Sync::Message() const { return "sync"; }

crow::json::wvalue Sync::Payload() const {
    crow::json::wvalue payload;
    payload["videoId"] = payload_.id;
    payload["title"] = payload_.title;
    payload["elapsedTime"] = elapsed_time_.count();
    return payload;
}

// Pause
std::string Pause::Message() const { return "pause"; }

crow::json::wvalue Pause::Payload() const { return {}; }

// Viewers count
Viewers::Viewers(const int &viewers) : payload_(viewers) {}

std::string Viewers::Message() const { return "viewers"; }

crow::json::wvalue Viewers::Payload() const { return payload_; }

// Update queue
std::string Queue::Message() const { return "queue"; }

crow::json::wvalue Queue::Payload() const {
    std::vector<crow::json::wvalue> items;
    items.reserve(payload_.size());
    std::transform(payload_.begin(), payload_.end(), std::back_inserter(items),
                   [](const auto &video) -> crow::json::wvalue {
                       return {{"id", video.id}, {"title", video.title}};
                   });
    return items;
}

// Response to a Search query
SearchResponse::SearchResponse() : payload_() {}

std::string SearchResponse::Message() const { return "search-response"; }

crow::json::wvalue SearchResponse::Payload() const {
    std::vector<crow::json::wvalue> items;
    items.reserve(payload_.size());
    std::transform(payload_.begin(), payload_.end(), std::back_inserter(items),
                   [](const auto &item) -> crow::json::wvalue {
                       return {{"id", item.id},
                               {"title", item.title},
                               {"thumbnail", item.thumbnail}};
                   });
    return items;
}
