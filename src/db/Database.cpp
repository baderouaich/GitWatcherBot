#include "Database.hpp"
#include "log/Logger.hpp"

std::mutex Database::m_mutex{};

std::mutex &Database::getDbMutex() noexcept {
  return m_mutex;
}

void Database::backup() {
#ifdef _WIN32
#error "TODO: Must adapt this function to windows too, or have an tar.xz library"
#endif
  try {
    static const fs::path dbBackupsDir = fs::path(RES_DIR) / "DbBackups";
    fs::path yearMonthDayDir = dbBackupsDir / tgbotxx::DateTimeUtils::now("%Y/%m/%d"); // DbBackups/2024/03/25/
    if (!fs::exists(yearMonthDayDir)) fs::create_directories(yearMonthDayDir);
    fs::path dbBackupFilename = yearMonthDayDir / ("Database-" + tgbotxx::DateTimeUtils::now("%Y-%m-%d-%H-%M-%S") + ".db"); // DbBackups/2024/03/25/Database-2024-03-25-22-00-01.db
    getStorage().backup_to(dbBackupFilename.string());
    // Compress Database-2024-...db into Database-2024-...db.tar.xz
    // tar -cJf <archive.tar.xz> <files>
    std::string cmd = "cd \"" + dbBackupFilename.parent_path().string() + "\" && tar -cJf \"" + dbBackupFilename.filename().string() + ".tar.xz\" \"" + dbBackupFilename.filename().string() + "\" --remove-files";
    LOGI("Creating database backup tar.xz archive: " << cmd);
    [[maybe_unused]] int rc = std::system(cmd.c_str());
    LOGI("Backup " << rc << " " << (rc == EXIT_SUCCESS ? "success" : "failure"));
  }
  catch (const std::exception &err) {
    LOGE("Could not backup database: " << err.what());
  }
}


bool Database::userExists(const UserId userId) {
  std::lock_guard guard{m_mutex};
  return !!getStorage().count<models::User>(where(c(&models::User::id) == userId));
}

models::User Database::getUser(const models::UserId userId) {
  std::lock_guard guard{m_mutex};
  return getStorage().get<models::User>(userId);
}

void Database::addUser(const models::User &newUser) {
  std::lock_guard guard{m_mutex};
  getStorage().replace(newUser);
}

models::UserStatus Database::getUserStatus(const UserId userId) {
  std::lock_guard guard{m_mutex};
  // *  Select a single column into std::vector<T> or multiple columns into std::vector<std::tuple<...>>.
  // *  For a single column use `auto rows = storage.select(&User::id, where(...));
  // *  For multicolumns use `auto rows = storage.select(columns(&User::id, &User::name), where(...));
  // TODO: open issue in sqlite orm: single column select doesn't work with enums.
  //auto statuses = getStorage().select(&models::User::status, where(c(&models::User::id) == userId));
  //auto [status] = statuses.front();
  auto statuses = getStorage().select(columns(&models::User::status), where(c(&models::User::id) == userId));
  auto [status] = statuses.front();
  return status;
}

void Database::updateUserStatus(const models::UserId userId, const models::UserStatus newStatus) {
  std::lock_guard guard{m_mutex};
  getStorage().update_all(
    set(
      c(&models::User::status) = newStatus,
      c(&models::User::updatedAt) = std::time(nullptr)
    ),
    where(c(&models::User::id) == userId)
  );
}

void Database::updateUser(const models::User &updatedUser) {
  std::lock_guard guard{m_mutex};
  getStorage().update(updatedUser);
}

int Database::userReposCount(const UserId userId) {
  std::lock_guard guard{m_mutex};
  return Database::getStorage().count<models::Repository>(
    where(
      c(&Repository::watcher_id) == userId
    )
  );
}

bool Database::repoExists(const models::RepositoryId repoId) {
  std::lock_guard guard{m_mutex};
  return !!getStorage().count<models::Repository>(where(c(&models::Repository::id) == repoId));
}

bool Database::repoExistsByFullName(const std::string &full_name) {
  std::lock_guard guard{m_mutex};
  return !!getStorage().count<models::Repository>(
    where(lower(&models::Repository::full_name) == tgbotxx::StringUtils::toLowerCopy(full_name))
  );
}

void Database::addRepo(const models::Repository &newRepo) {
  std::lock_guard guard{m_mutex};
  Database::getStorage().replace(newRepo);
}

void Database::updateRepo(const models::Repository &newRepo) {
  std::lock_guard guard{m_mutex};
  /**
   *  Update routine. Sets all non primary key fields where primary key is equal.
   *  O is an object type. May be not specified explicitly cause it can be deduced by
   *      compiler from first parameter.
   *  @param o object to be updated.
   *
   *  But the bad thing is Repository has no primary key, even tried combining id & watcher id in one pk but doesn't work with sqlite_orm.
   */
  //  Database::getStorage().update(newRepo);
  Database::getStorage().update_all(
    set(
      c(&Repository::full_name) = newRepo.full_name,
      c(&Repository::stargazers_count) = newRepo.stargazers_count,
      c(&Repository::watchers_count) = newRepo.watchers_count,
      c(&Repository::open_issues_count) = newRepo.open_issues_count,
      c(&Repository::pulls_count) = newRepo.pulls_count,
      c(&Repository::forks_count) = newRepo.forks_count,
      c(&Repository::description) = newRepo.description,
      c(&Repository::size) = newRepo.size,
      c(&Repository::language) = newRepo.language,
      c(&Repository::updatedAt) = std::time(nullptr) // Don't forget
    ),
    where(c(&Repository::watcher_id) == *newRepo.watcher_id and c(&Repository::id) == newRepo.id)
  );
}

void Database::removeUserRepo(const models::UserId watcherId, const models::RepositoryId repoId) {
  std::lock_guard guard{m_mutex};
  getStorage().remove_all<models::Repository>(
    where(c(&models::Repository::watcher_id) == watcherId and c(&Repository::id) == repoId)
  );
}

void Database::iterateRepos(const std::function<void(const models::Repository &)> &callback) {
  m_mutex.lock();
  auto range = Database::getStorage().iterate<models::Repository>();
  m_mutex.unlock();

  for (auto begin = range.begin(); begin != range.end();) {
    callback(*begin);

    m_mutex.lock();
    begin++;
    m_mutex.unlock();
  }

}

std::vector<std::string> Database::getUserReposFullnames(const models::UserId watcherId) {
  std::lock_guard guard{m_mutex};
  return getStorage().select(&Repository::full_name,
                             where(
                               c(&Repository::watcher_id) == watcherId
                             ),
                             order_by(&Repository::full_name)
  );
}

std::vector<models::Repository> Database::getUserRepos(const models::UserId watcherId) {
  std::lock_guard guard{m_mutex};
  return Database::getStorage().get_all<models::Repository>(
    where(
      c(&Repository::watcher_id) == watcherId
    )
  );
}

std::int64_t Database::addLog(const models::Log& newLog) {
  std::lock_guard guard{m_mutex};
  return getStorage().insert(newLog);
}
