#include "crow_all.h"
#include <asio/steady_timer.hpp>
#include <chrono>
#include <cpr/cpr.h>
#include <deque>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

struct Video {
    std::string id;
    std::string title;
    std::chrono::seconds duration;
};

struct State {
    State(asio::io_context &timer_ioc);

    std::mutex mutex;
    Video current_video;
    std::chrono::milliseconds start_time;
    std::deque<Video> queue;
    std::unordered_set<crow::websocket::connection *> clients;
    asio::steady_timer timer;
};

State::State(asio::io_context &timer_ioc) : timer(timer_ioc) {}

void Broadcast(State &state, const std::string &message) {
    for (auto &client : state.clients) {
        client->send_text(message);
    }
}

void UpdateQueue(State &state) {
    std::vector<crow::json::wvalue> queue;
    for (const auto &video : state.queue) {
        queue.push_back({{"title", video.title}, {"id", video.id}});
    }
    crow::json::wvalue res{{"message", "queue"}, {"payload", queue}};
    Broadcast(state, res.dump());
}

void SynchronizeVideo(State &state, crow::websocket::connection &conn) {
    crow::json::wvalue res{{"message", "sync"},
                           {"payload",
                            {{"videoId", state.current_video.id},
                             {"title", state.current_video.title},
                             {"startTime", state.start_time.count()}}}};
    conn.send_text(res.dump());
}

void NextVideo(State &state) {
    if (state.clients.size() == 0)
        return;
    CROW_LOG_INFO << "Next video";
    std::lock_guard _(state.mutex);

    state.start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());

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
    crow::json::wvalue res{{"message", "sync"},
                           {"payload",
                            {{"videoId", state.current_video.id},
                             {"title", state.current_video.title},
                             {"startTime", state.start_time.count()}}}};
    Broadcast(state, res.dump());
    UpdateQueue(state);
}

std::string LoadApiKey() {
    std::ifstream file("/home/martin/api-keys/yt-dj-crolard");
    std::string key;
    std::getline(file, key);
    return key;
}

std::chrono::seconds ParseISO8601(const std::string &s) {
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

std::optional<Video> GetVideoInfo(const std::string &yt_api_key,
                                  const std::string &video_id) {
    cpr::Response r =
        cpr::Get(cpr::Url{"https://www.googleapis.com/youtube/v3/videos"},
                 cpr::Parameters{{"part", "snippet,contentDetails"},
                                 {"id", video_id},
                                 {"key", yt_api_key}});

    if (r.status_code != 200) {
        return {};
    }

    crow::json::rvalue yt_data = crow::json::load(r.text);
    std::string title = yt_data["items"][0]["snippet"]["title"].s();
    auto duration =
        ParseISO8601(yt_data["items"][0]["contentDetails"]["duration"].s());

    Video video{video_id, title, duration};
    return video;
}

int main(int argc, char *argv[]) {
    asio::io_context ioc;
    auto work = asio::make_work_guard(ioc);
    State state(ioc);

    std::string yt_api_key = LoadApiKey();

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

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([&](crow::websocket::connection &conn) {
            CROW_LOG_INFO << "Websocket open (" << conn.get_remote_ip() << ")";

            // Synchronize clients when then join the room
            SynchronizeVideo(state, conn);

            std::lock_guard _(state.mutex);
            state.clients.insert(&conn);
        })
        .onclose([&](crow::websocket::connection &conn,
                     const std::string &reason, uint16_t with_status_code) {
            CROW_LOG_INFO << "Websocket close (" << conn.get_remote_ip() << ")";
            std::lock_guard _(state.mutex);
            state.clients.erase(&conn);
        })
        .onmessage([&](crow::websocket::connection &conn,
                       const std::string &message, bool is_binary) {
            CROW_LOG_DEBUG << "Websocket message (" << conn.get_remote_ip()
                           << ")";

            crow::json::rvalue req = crow::json::load(message);
            crow::json::rvalue payload = req["payload"];

            // Message dispatch
            if (req["message"] == "add_video") {
                const std::string video_id = payload.s();
                CROW_LOG_INFO << "Adding video: " << video_id;

                auto info = GetVideoInfo(yt_api_key, video_id);
                if (!info.has_value()) {
                    return;
                }

                Video video = info.value();

                // if (!state.currentVideoId) {
                //     playVideo(video_item);
                // } else {
                std::lock_guard _(state.mutex);
                state.queue.push_back(video);
                UpdateQueue(state);
                // }
                return;
            }
            if (req["message"] == "delete") {
                const std::size_t index = payload.i();
                CROW_LOG_INFO << "Deleting video: " << index;
                std::lock_guard _(state.mutex);
                if (index < state.queue.size()) {
                    state.queue.erase(state.queue.begin() + index);
                    UpdateQueue(state);
                }
                return;
            }
            if (req["message"] == "clear") {
                CROW_LOG_INFO << "Clearing queue";
                std::lock_guard _(state.mutex);
                state.queue.clear();
                UpdateQueue(state);
                return;
            }
            if (req["message"] == "next") {
                NextVideo(state);
            }
        });

    std::thread timer_thread([&]() { ioc.run(); });
    app.port(8000).multithreaded().run();

    ioc.stop();
    timer_thread.join();

    return 0;
}
