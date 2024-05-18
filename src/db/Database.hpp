#pragma once

#include "models/User.hpp"
#include "models/Repository.hpp"
#include "models/Log.hpp"
#include "tgbotxx/utils/DateTimeUtils.hpp"
#include "sqlite_orm/sqlite_orm.h"
#include <filesystem>

namespace fs = std::filesystem;
using namespace sqlite_orm;
using namespace models;

class Database {
private:
  static std::mutex m_mutex;

public:
  [[nodiscard]] static std::mutex &getDbMutex() noexcept;

public:
  static auto& getStorage() {
    static auto storage = make_storage(RES_DIR "/Database.db",
                                       Repository::table(),
                                       User::table(),
                                       Log::table()
    );
    static bool schemaSynced = false;
    if (!storage.on_open) {
      storage.on_open = []([[maybe_unused]] sqlite3 *handle) {
        if (not schemaSynced) {
          int rc = sqlite3_exec(handle, "PRAGMA synchronous=NORMAL;" // wait for data to be written to disk io
                                        "PRAGMA locking_mode=EXCLUSIVE;" // Only 1 connection can write to db at a time, others will be blocked
                                        "PRAGMA journal_mode=WAL;" // record changes before they are applied to the main database file
                                        "PRAGMA cache_size=50000;" // 50000=50mb 800000=800MB (default -2000 which is 2kb)
                                        "PRAGMA temp_store=MEMORY;" // Storing temporary tables and indices in memory can improve performance, but it can also increase memory usage and the risk of running out of memory, especially for large temporary datasets.
                                        "PRAGMA auto_vacuum=0;", // DO NOT Vacuum
                                nullptr, nullptr, nullptr);
          if (rc != SQLITE_OK) {
            sqlite_orm::throw_translated_sqlite_error(handle);
          }
          storage.sync_schema(/*preserve*/true); // PRESERVE=TRUE Don't delete my table data when I add a new column in a table. (https://github.com/fnc12/sqlite_orm/issues/1261)
          schemaSynced = true;
        }
      };
    }
    return storage;
  }

  /// Call this periodically to backup the database in res/DbBackups/ periodically
  static void backup();

public: // Users
  static bool userExists(const models::UserId userId);
  static models::User getUser(const models::UserId userId);
  static void addUser(const models::User& newUser);
  static models::UserStatus getUserStatus(const models::UserId userId);
  static void updateUserStatus(const models::UserId userId, const models::UserStatus newStatus);
  static void updateUser(const models::User& updatedUser);
  static int userReposCount(const models::UserId userId);

public: // Repositories
  static bool repoExists(const models::RepositoryId repoId);
  static bool repoExistsByFullName(const std::string& full_name);
  static void addRepo(const models::Repository& newRepo);
  static void updateRepo(const models::Repository& updatedRepo);
  static void removeUserRepo(const models::UserId watcherId, const models::RepositoryId repoId);
  static void iterateRepos(const std::function<void(const models::Repository&)>& callback); // Lock secure iterate over repositories to not hold the db mutex for a long time
  static std::vector<std::string> getUserReposFullnames(const models::UserId watcherId);
  static std::vector<models::Repository> getUserRepos(const models::UserId watcherId);

public: // Logs
  static std::int64_t addLog(const models::Log& newLog);
};

