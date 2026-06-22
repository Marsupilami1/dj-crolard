#pragma once

#include <chrono>
#include <string>

namespace dj {
struct Video {
    std::string id;
    std::string title;
    std::chrono::seconds duration;
};

struct SearchResult {
    std::string id;
    std::string title;
    std::string thumbnail;
};

using Ms = std::chrono::milliseconds;
} // namespace dj
