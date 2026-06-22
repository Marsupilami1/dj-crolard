#include "crow_all.h"
#include "message.h"
#include "room.h"
#include "ytapi.h"

#include <asio/steady_timer.hpp>
#include <cpr/cpr.h>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

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
    dj::Room room(ioc);

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

            std::lock_guard _(room.Lock());
            room.AddClient(&conn);
            room.Greet(conn);
        })
        .onclose([&](crow::websocket::connection &conn,
                     const std::string &reason, uint16_t with_status_code) {
            CROW_LOG_INFO << "Websocket close (" << conn.get_remote_ip() << ")";

            std::lock_guard _(room.Lock());
            room.RemoveClient(&conn);
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

                // Fetch video info from the youtube API
                auto info = yt.Video(video_id);
                if (!info) {
                    CROW_LOG_INFO << "Failed to get video info for "
                                  << video_id;
                    return;
                }

                dj::Video video = info.value();
                std::lock_guard _(room.Lock());
                room.Add(video);
                return;
            }
            if (req["message"] == "search") {
                std::string query = payload.s();
                CROW_LOG_INFO << "Search: " << query;

                // TODO: Let the client perform the yt search, so that ``weird''
                // searches won't bring the GIGN to my home...

                // Currently, we do not relate this logic to the room session,
                // that's why we send the message here and via the room class
                auto results = yt.Search(query);
                if (!results) {
                    CROW_LOG_INFO << "Failed to perform yt search";
                    auto msg = dj::msg::SearchResponse(); // empty response
                    conn.send_text(msg.Serialize());
                    return;
                }
                auto msg = dj::msg::SearchResponse(results.value());
                conn.send_text(msg.Serialize());
                return;
            }
            if (req["message"] == "delete") {
                const std::size_t index = payload.i();
                CROW_LOG_INFO << "Deleting video: " << index;

                std::lock_guard _(room.Lock());
                room.Delete(index);
                return;
            }
            if (req["message"] == "next") {
                CROW_LOG_INFO << "Next video";
                std::lock_guard _(room.Lock());
                room.Next();
                return;
            }
            if (req["message"] == "pause") {
                // "pause" event can also resume the video
                CROW_LOG_INFO << "Pause";

                std::lock_guard _(room.Lock());
                room.Pause();
                return;
            }
            if (req["message"] == "clear") {
                CROW_LOG_INFO << "Clearing queue";

                std::lock_guard _(room.Lock());
                room.Clear();
                return;
            }
            if (req["message"] == "reorder_queue") {
                auto lo = payload.lo();
                std::vector<int64_t> q_permut;
                q_permut.reserve(lo.size());
                std::transform(
                    lo.begin(), lo.end(), std::back_inserter(q_permut),
                    [](const auto &item) -> int64_t { return item.i(); });
                CROW_LOG_INFO << "Reordering queue";

                std::lock_guard _(room.Lock());
                room.Reorder(q_permut);
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
