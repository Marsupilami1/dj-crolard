#include "room.h"
#include "utils.h"

using namespace dj;

static Ms GetCurrentTime() {
    return std::chrono::duration_cast<Ms>(
        std::chrono::system_clock::now().time_since_epoch());
}

Room::Room(asio::io_context &timer_ioc) : playing_(true), timer_(timer_ioc) {}

Room::~Room() {
    timer_.cancel();
    for (auto conn : clients_) {
        conn->close("Room has been closed");
    }
}

std::lock_guard<std::mutex> Room::Lock() { return std::lock_guard(mutex_); }

void Room::AddClient(crow::websocket::connection *conn) {
    clients_.insert(conn);
}

void Room::RemoveClient(crow::websocket::connection *conn) {
    clients_.erase(conn);
    // Notify remaining clients that someone leaved
    SendViewers();
}

void Room::Greet(crow::websocket::connection &conn) {
    // When a client join the room:
    if (playing_)
        SendSync(conn); // Synchronize it
    SendQueue(conn);    // Send it the queue
    SendViewers();      // Tell everybody than someone joined

    // TODO: a dedicated greet message would be better
    // Currently if a client joins a paused room, it will see the video playing
}

void Room::Add(const Video &video) {
    queue_.push_back(video);
    SendQueue();
}

void Room::Delete(const std::size_t &index) {
    if (index >= queue_.size())
        return;
    queue_.erase(queue_.begin() + index);
    SendQueue();
}

void Room::Clear() {
    queue_.clear();
    SendQueue();
}

void Room::Next() {
    // TODO: Make a better idle state
    if (clients_.size() == 0)
        return;

    start_time_ = GetCurrentTime();
    if (!queue_.empty()) {
        current_video_ = queue_[0];
        queue_.pop_front();
    }

    timer_.expires_after(current_video_.duration);
    timer_.async_wait([this](const asio::error_code &ec) { Next(ec); });

    // Synchronize all clients
    SendSync();
    SendQueue();
}

void Room::Next(const asio::error_code &ec) {
    if (ec)
        return;
    CROW_LOG_INFO << "Automatic queue pulling";
    Next();
}

void Room::Pause() {
    playing_ = !playing_;
    if (playing_) {
        start_time_ = GetCurrentTime() - elapsed_time_;
        timer_.expires_after(current_video_.duration - elapsed_time_);
        timer_.async_wait([this](const asio::error_code &ec) { Next(ec); });
        // Resume clients with a "sync" event
        SendSync();
    } else {
        // Save the elapsed time for resuming later
        elapsed_time_ = GetCurrentTime() - start_time_;
        // Stop the automatic queue pulling
        timer_.cancel();
        // Pause clients with a "pause" event
        SendPause();
    }
}

void Room::Reorder(const std::vector<int64_t> &q_permut) {
    // TODO: use std:transform
    std::deque<dj::Video> new_queue;
    for (const auto &idx : q_permut) {
        new_queue.emplace_back(queue_[idx]);
    }
    queue_ = std::move(new_queue);
    SendQueue();
}

void Room::SendViewers() { Broadcast(msg::Viewers(clients_.size())); }

void Room::SendQueue() { Broadcast(msg::Queue(queue_)); }

void Room::SendQueue(crow::websocket::connection &conn) {
    auto msg = msg::Queue(queue_);
    conn.send_text(msg.Serialize());
}

void Room::SendSync() {
    auto elapsed_time = GetCurrentTime() - start_time_;
    auto msg = msg::Sync(current_video_, elapsed_time);
    Broadcast(msg);
}

void Room::SendSync(crow::websocket::connection &conn) {
    auto elapsed_time = GetCurrentTime() - start_time_;
    auto msg = msg::Sync(current_video_, elapsed_time);
    conn.send_text(msg.Serialize());
}

void Room::SendPause() { Broadcast(msg::Pause()); }

void Room::Broadcast(const msg::OutMessage &msg) {
    for (auto &client : clients_) {
        client->send_text(msg.Serialize());
    }
}
