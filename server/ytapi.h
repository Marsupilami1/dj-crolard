#pragma once

#include "room.h"
#include <string>

namespace dj {

class YtApi {
  public:
    YtApi(const std::string &api_key);
    virtual ~YtApi() = default;

    std::optional<Video> Video(const std::string &id);
    std::optional<std::vector<SearchResult>> Search(const std::string &query);

  private:
    std::string api_key_;
};

} // namespace dj
