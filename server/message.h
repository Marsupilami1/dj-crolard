#pragma once

#include "crow_all.h"
#include "utils.h"

#include <chrono>
#include <ranges>
#include <string>

namespace dj::msg {

class OutMessage {
  public:
    OutMessage() = default;
    virtual ~OutMessage() = default;

    virtual std::string Message() const = 0; // type / event name
    virtual crow::json::wvalue Payload() const = 0;

    virtual std::string Serialize() const;

  private:
};

class Sync : public OutMessage {
  public:
    Sync(const Video &video, const std::chrono::milliseconds &elapsed_time);
    Sync(const Video &video);
    virtual ~Sync() = default;

    std::string Message() const override;
    crow::json::wvalue Payload() const override;

  private:
    Video payload_;
    std::chrono::milliseconds elapsed_time_;
};

class Pause : public OutMessage {
  public:
    Pause() = default;
    virtual ~Pause() = default;

    std::string Message() const override;
    crow::json::wvalue Payload() const override;
};

class Viewers : public OutMessage {
  public:
    Viewers(const int &viewers);
    virtual ~Viewers() = default;

    std::string Message() const override;
    crow::json::wvalue Payload() const override;

  private:
    int payload_;
};

class Queue : public OutMessage {
  public:
    template <std::ranges::input_range R>
    explicit Queue(R &&range) : payload_(std::begin(range), std::end(range)) {}

    virtual ~Queue() = default;

    std::string Message() const override;
    crow::json::wvalue Payload() const override;

  private:
    std::vector<Video> payload_;
};

class SearchResponse : public OutMessage {
  public:
    SearchResponse();
    template <std::ranges::input_range R>
    explicit SearchResponse(R &&range)
        : payload_(std::begin(range), std::end(range)) {}
    virtual ~SearchResponse() = default;

    std::string Message() const override;
    crow::json::wvalue Payload() const override;

  private:
    std::vector<SearchResult> payload_;
};

} // namespace dj::msg
