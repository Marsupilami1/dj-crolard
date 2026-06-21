#include "crow_all.h"
#include "message.h"
#include "state.h"
#include "ytapi.h"

#include <asio/steady_timer.hpp>
#include <chrono>
#include <cpr/cpr.h>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

struct State {
    State(asio::io_context &timer_ioc);

    std::mutex mutex;
    dj::Video current_video;
    std::chrono::milliseconds start_time;
    std::chrono::milliseconds elapsed_time;
    bool playing = true;
    std::deque<dj::Video> queue;
    std::unordered_set<crow::websocket::connection *> clients;
    asio::steady_timer timer;
};

State::State(asio::io_context &timer_ioc) : timer(timer_ioc) {}

void Broadcast(State &state, const std::string &message) {
    for (auto &client : state.clients) {
        client->send_text(message);
    }
}

std::chrono::milliseconds GetCurrentTime() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
}

void UpdateViewers(State &state) {
    auto msg = dj::Viewers(state.clients.size());
    Broadcast(state, msg.Serialize());
}

void SendQueue(State &state) {
    auto msg = dj::Queue(state.queue);
    Broadcast(state, msg.Serialize());
}

void SendQueue(State &state, crow::websocket::connection &conn) {
    auto msg = dj::Queue(state.queue);
    conn.send_text(msg.Serialize());
}

void SynchronizeVideo(State &state, crow::websocket::connection &conn) {
    auto elapsed_time = GetCurrentTime() - state.start_time;
    auto msg = dj::Sync(state.current_video, elapsed_time);
    conn.send_text(msg.Serialize());
}

void NextVideo(State &state) {
    if (state.clients.size() == 0)
        return;
    CROW_LOG_INFO << "Next video";
    std::lock_guard _(state.mutex);

    state.start_time = GetCurrentTime();
    if (!state.queue.empty()) {
        state.current_video = state.queue[0];
        state.queue.pop_front();
    }

    state.timer.expires_after(state.current_video.duration);
    state.timer.async_wait([&](const asio::error_code &ec) {
        if (ec)
            return;
        NextVideo(state);
    });

    // Synchronize all clients
    auto msg = dj::Sync(state.current_video);
    Broadcast(state, msg.Serialize());
    SendQueue(state);
}

std::string LoadApiKey() {
    const char *env_key = std::getenv("YT_API_KEY");
    if (env_key != nullptr && std::strlen(env_key) > 0) {
        return std::string(env_key);
    }

    const char *env_path = std::getenv("YT_API_KEY_FILE");
    std::string key_file =
        env_path != nullptr ? std::string(env_path) : "api-key.txt";

    std::ifstream file(key_file);
    if (!file.is_open()) {
        CROW_LOG_ERROR << "Failed to open API key file: " << key_file;
        CROW_LOG_ERROR << "Set YT_API_KEY environment variable or "
                          "YT_API_KEY_FILE to specify a file path";
        return "";
    }

    std::string key;
    std::getline(file, key);
    return key;
}

int main(int argc, char *argv[]) {
    asio::io_context ioc;
    auto work = asio::make_work_guard(ioc);
    State state(ioc);

    std::string yt_api_key = LoadApiKey();
    if (yt_api_key.empty()) {
        return 1;
    }
    dj::YtApi yt(yt_api_key);

    crow::SimpleApp app;

    CROW_LOG_INFO << "Starting Server";

    crow::mustache::set_global_base("templates");

    CROW_ROUTE(app, "/")([](const crow::request &, crow::response &res) {
        res.set_static_file_info("public/index.html");
        res.end();
    });

    CROW_ROUTE(app,
               "/style.css")([](const crow::request &, crow::response &res) {
        res.set_static_file_info("public/style.css");
        res.end();
    });

    CROW_ROUTE(app,
               "/client.js")([](const crow::request &, crow::response &res) {
        res.set_static_file_info("public/client.js");
        res.end();
    });

    CROW_ROUTE(app,
               "/favicon.ico")([](const crow::request &, crow::response &res) {
        res.set_static_file_info("public/favicon.ico");
        res.end();
    });

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([&](crow::websocket::connection &conn) {
            CROW_LOG_INFO << "Websocket open (" << conn.get_remote_ip() << ")";

            std::lock_guard _(state.mutex);
            state.clients.insert(&conn);

            // When a client join the room:
            SendQueue(state, conn);        // Send it the queue
            SynchronizeVideo(state, conn); // Synchronize it
            UpdateViewers(state);          // Tell everybody than someone joined
        })
        .onclose([&](crow::websocket::connection &conn,
                     const std::string &reason, uint16_t with_status_code) {
            CROW_LOG_INFO << "Websocket close (" << conn.get_remote_ip() << ")";
            std::lock_guard _(state.mutex);
            state.clients.erase(&conn);
            UpdateViewers(state);
        })
        .onmessage([&](crow::websocket::connection &conn,
                       const std::string &message, bool is_binary) {
            CROW_LOG_DEBUG << "Websocket message (" << conn.get_remote_ip()
                           << ")";

            crow::json::rvalue req = crow::json::load(message);
            crow::json::rvalue payload = req["payload"];

            // Message dispatch
            if (req["message"] == "add") {
                const std::string video_id = payload.s();
                CROW_LOG_INFO << "Adding video: " << video_id;

                auto info = yt.Video(video_id);
                if (!info.has_value()) {
                    CROW_LOG_INFO << "Failed to get video info for "
                                  << video_id;
                    return;
                }

                dj::Video video = info.value();

                // if (!state.currentVideoId) {
                //     playVideo(video_item);
                // } else {
                std::lock_guard _(state.mutex);
                state.queue.push_back(video);
                SendQueue(state);
                // }
                return;
            }
            if (req["message"] == "search") {
                CROW_LOG_INFO << "Search: " << payload.s();
                auto results = yt.Search(payload.s());
                if (!results.has_value()) {
                    CROW_LOG_INFO << "Failed to perform yt search";
                    auto msg = dj::SearchResponse(); // empty response
                    conn.send_text(msg.Serialize());
                }
                auto msg = dj::SearchResponse(results.value());
                conn.send_text(msg.Serialize());
            }
            if (req["message"] == "delete") {
                const std::size_t index = payload.i();
                CROW_LOG_INFO << "Deleting video: " << index;
                std::lock_guard _(state.mutex);
                if (index < state.queue.size()) {
                    state.queue.erase(state.queue.begin() + index);
                    SendQueue(state);
                }
                return;
            }
            if (req["message"] == "next") {
                NextVideo(state);
                return;
            }
            if (req["message"] == "pause") {
                CROW_LOG_INFO << "Pause";
                // "pause" event can also resume the video
                std::lock_guard _(state.mutex);
                state.playing = !state.playing;
                if (state.playing) {
                    state.start_time = GetCurrentTime() - state.elapsed_time;
                    state.timer.expires_after(state.current_video.duration -
                                              state.elapsed_time);
                    state.timer.async_wait([&](const asio::error_code &ec) {
                        if (ec)
                            return;
                        NextVideo(state);
                    });
                    // Resume clients with a "sync" event
                    auto msg =
                        dj::Sync(state.current_video, state.elapsed_time);
                    Broadcast(state, msg.Serialize());
                } else {
                    state.elapsed_time = GetCurrentTime() - state.start_time;
                    // Stop the automatic queue pulling
                    state.timer.cancel();
                    // Pause clients with a "pause" evetn
                    auto msg = dj::Pause();
                    Broadcast(state, msg.Serialize());
                }
                return;
            }
            if (req["message"] == "clear") {
                CROW_LOG_INFO << "Clearing queue";
                std::lock_guard _(state.mutex);
                state.queue.clear();
                SendQueue(state);
                return;
            }
            if (req["message"] == "reorder_queue") {
                CROW_LOG_INFO << "Reordering queue";
                std::lock_guard _(state.mutex);
                std::deque<dj::Video> new_queue;
                for (const auto &idx : payload) {
                    new_queue.emplace_back(state.queue[idx.i()]);
                }
                state.queue = std::move(new_queue);
                SendQueue(state);
                return;
            }
            CROW_LOG_WARNING << "Received unknown message: " << req["message"];
        });

    std::thread timer_thread([&]() { ioc.run(); });
    app.port(8000).multithreaded().run();

    ioc.stop();
    timer_thread.join();

    return 0;
}
