#pragma once

#include <cstdint>
#include <string>
#include <cstring>
#include "sqlite_orm/sqlite_orm.h"
#include "User.hpp"


namespace models {
  using RepositoryId = std::int64_t;

  struct Repository {
    RepositoryId id{};
    std::string full_name;
    std::int64_t stargazers_count{};
    std::int64_t watchers_count{};
    std::int64_t open_issues_count{};
    std::int64_t pulls_count{};
    std::int64_t forks_count{};
    std::string description;
    std::size_t size{};
    std::string language;
    std::time_t createdAt{};
    std::time_t updatedAt{};
    std::unique_ptr<UserId> watcher_id; // user id who is watching changes on this repo

    static auto table() {
      using namespace sqlite_orm;
      return make_table("Repositories",
                        make_column("id", &Repository::id), // this is repository id from GitHub Api, don't make it primary_key() since multiple users can watch the same repository
                        make_column("full_name", &Repository::full_name),
                        make_column("stargazers_count", &Repository::stargazers_count),
                        make_column("watchers_count", &Repository::watchers_count),
                        make_column("open_issues_count", &Repository::open_issues_count),
                        make_column("pulls_count", &Repository::pulls_count),
                        make_column("forks_count", &Repository::forks_count),
                        make_column("description", &Repository::description),
                        make_column("size", &Repository::size),
                        make_column("language", &Repository::language),
                        make_column("createdAt", &Repository::createdAt),
                        make_column("updatedAt", &Repository::updatedAt),
                        make_column("watcher_id", &Repository::watcher_id),
                        foreign_key(&Repository::watcher_id).references(&User::id)
                        //primary_key(&Repository::id, &Repository::watcher_id) // TODO: don't try this, doesn't work with sqlite orm,
                        // try it later in separate project (Primary key ownership is repo id + watcher id since multiple
                        // users can watch the same repository, this way we can call storage.update(repo))
      );
    }

#define DESERIALIZE(field)  \
    try {\
      field = json[#field]; \
    }\
    catch(const std::exception& e)\
    {\
      throw std::runtime_error("Failed to deserialize json field '" + std::string(#field) + "': " + e.what());\
    }

    Repository() = default;

    explicit Repository(const nl::json &json) {

      DESERIALIZE(id);
      DESERIALIZE(full_name);
      DESERIALIZE(stargazers_count);
      DESERIALIZE(watchers_count);
      DESERIALIZE(open_issues_count);
      DESERIALIZE(forks_count);
      DESERIALIZE(description);
      DESERIALIZE(size);
      DESERIALIZE(language);



      // for pulls its different https://stackoverflow.com/questions/40534533/count-open-pull-requests-and-issues-on-github
      // https://api.github.com/search/issues?q=repo:baderouaich/tgbotxx%20is:pr%20is:open&per_page=1
      try {
        auto res = cpr::Get(cpr::Url("https://api.github.com/search/issues?q=repo:" + full_name + "%20is:pr%20is:open&per_page=1"));
        nl::json data = nl::json::parse(res.text);
        pulls_count = data["total_count"];
      } catch (const std::exception &e) {
        throw std::runtime_error("Failed to get pulls_count for " + full_name + ": " + e.what());
      }

      createdAt = std::time(nullptr);
      updatedAt = createdAt;
    }

  };

#undef DESERIALIZE
}