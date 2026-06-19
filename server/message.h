#pragma once

#include "crow_all.h"
#include "state.h"

#include <chrono>
#include <ranges>
#include <string>

namespace dj {

class OutMessage {
  public:
    OutMessage() = default;
    virtual ~OutMessage() = default;

    virtual std::string Message() = 0; // type / event name
    virtual crow::json::wvalue Payload() = 0;

    virtual std::string Serialize();

  private:
};

class Sync : public OutMessage {
  public:
    Sync(const Video &video, const std::chrono::milliseconds &elapsed_time);
    Sync(const Video &video);
    virtual ~Sync() = default;

    std::string Message() override;
    crow::json::wvalue Payload() override;

  private:
    Video payload_;
    std::chrono::milliseconds elapsed_time_;
};

class Queue : public OutMessage {
  public:
    template <std::ranges::input_range R>
    explicit Queue(R &&range) : payload_(std::begin(range), std::end(range)) {}

    virtual ~Queue() = default;

    std::string Message() override;
    crow::json::wvalue Payload() override;

  private:
    std::vector<Video> payload_;
};

} // namespace dj
