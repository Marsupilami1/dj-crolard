#pragma once

#include "crow_all.h"
#include "message.h"
#include "utils.h"

#include <asio/steady_timer.hpp>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_set>

namespace dj {

class Room {
  public:
    Room(asio::io_context &timer_ioc);
    virtual ~Room();

    std::lock_guard<std::mutex> Lock();

    void AddClient(crow::websocket::connection *conn);
    void RemoveClient(crow::websocket::connection *conn);
    void Greet(crow::websocket::connection &conn);

    void Add(const Video &video);
    void Delete(const std::size_t &index);
    void Clear();
    void Next();
    void Next(const asio::error_code &ec);
    void Pause();
    void Reorder(const std::vector<int64_t> &q_permut);

    void SendViewers();
    void SendQueue();
    void SendQueue(crow::websocket::connection &conn);
    void SendSync();
    void SendSync(crow::websocket::connection &conn);
    void SendPause();

  private:
    void Broadcast(const msg::OutMessage &msg);

    std::mutex mutex_;
    Video current_video_;
    Ms start_time_;
    Ms elapsed_time_;
    bool playing_;
    std::deque<dj::Video> queue_;
    std::unordered_set<crow::websocket::connection *> clients_;
    asio::steady_timer timer_;
};

} // namespace dj
