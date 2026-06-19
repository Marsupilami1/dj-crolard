#include "message.h"

using namespace dj;

std::string OutMessage::Serialize() {
    crow::json::wvalue res;
    res["message"] = Message();
    res["payload"] = Payload();
    return res.dump();
};

// Synchronize video
Sync::Sync(const Video &video, const std::chrono::milliseconds &elapsed_time)
    : payload_(video), elapsed_time_(elapsed_time) {}

Sync::Sync(const Video &video) : payload_(video), elapsed_time_(0) {}

std::string Sync::Message() { return "sync"; }

crow::json::wvalue Sync::Payload() {
    crow::json::wvalue payload;
    payload["videoId"] = payload_.id;
    payload["title"] = payload_.title;
    payload["elapsedTime"] = elapsed_time_.count();
    return payload;
}

// Update queue
std::string Queue::Message() { return "queue"; }

crow::json::wvalue Queue::Payload() {
    std::vector<crow::json::wvalue> items;
    items.reserve(payload_.size());
    std::transform(payload_.begin(), payload_.end(), std::back_inserter(items),
                   [](const auto &video) -> crow::json::wvalue {
                       return {{"title", video.title}, {"id", video.id}};
                   });
    return items;
}
