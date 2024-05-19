#include "GitBot.hpp"
#include <algorithm>
#include <chrono>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <source_location>
#include "db/Database.hpp"
#include "log/Logger.hpp"

using namespace tgbotxx;

/// Constructor loads Bot token from res/BOT_TOKEN.txt and admin user id from res/ADMIN_USER_ID.txt
GitBot::GitBot() : Bot(tgbotxx::FileUtils::read(fs::path(RES_DIR) / "BOT_TOKEN.txt")) {
  m_adminUserId = StringUtils::to<UserId>(FileUtils::read(fs::path(RES_DIR) / "ADMIN_USER_ID.txt"));
}

void GitBot::onStart() {
  LOGI("Starting bot");

  // Drop pending updates
  api()->deleteWebhook(true);
  // Set long polling timeout to 5 minutes, so Telegram server can hold the request for up to 5 minutes when
  // there are no updates
  api()->setLongPollTimeout(cpr::Timeout(std::chrono::seconds(300)));

  // Init thread pool so we can handle multiple requests simultaneously
  m_threadPool = std::make_unique<cpr::ThreadPool>();
  m_threadPool->SetMinThreadNum(0); // Join all threads when there is no work to do
  m_threadPool->SetMaxThreadNum(std::thread::hardware_concurrency()); // spawn up to CPU cors workers
  m_threadPool->SetMaxIdleTime(std::chrono::milliseconds(86400000)); // Idle up to a day waiting for work to do
  m_threadPool->Start(0); // No threads should be started by default until there is work to do

  // Register my commands
  Ptr<BotCommand> start(new BotCommand());
  start->command = "/start";
  start->description = "Start interacting with the Bot";
  Ptr<BotCommand> watch_repo(new BotCommand());
  watch_repo->command = "/watch_repo";
  watch_repo->description = "Add a new repository to your watch list";
  Ptr<BotCommand> unwatch_repo(new BotCommand());
  unwatch_repo->command = "/unwatch_repo";
  unwatch_repo->description = "Remove a repository from your watch list";
  Ptr<BotCommand> my_repos(new BotCommand());
  my_repos->command = "/my_repos";
  my_repos->description = "Display repositories you are watching";
  api()->setMyCommands({start, watch_repo, unwatch_repo, my_repos});

  // Create GitHub Api
  m_gitApi = std::make_unique<GitApi>();

  // Create Watchdog thread, which will check for repository changes by the hour
  m_watchdogThread = std::make_unique<std::thread>(&GitBot::watchDog, this);

  // Alert admin that the Bot has started successfully.
  notifyAdmin("Bot Started");
}

void GitBot::onStop() {
  LOGI("Stopping bot");
  notifyAdmin("Stopping Bot...");

  // Stop watchdog thread
  m_watchdogRunning = false;
  m_watchdogCv.notify_one(); // waky waky
  if (m_watchdogThread && m_watchdogThread->joinable()) {
    m_watchdogThread->join();
  }

  // Notify admin that the bot has stopped, but before threadPool is stopped since
  // sendSafeMessage uses the m_threadPool.
  notifyAdmin("Bot Stopped.");

  // Stop the thread pool
  m_threadPool->Stop();
}

// Returns true if str is a repository full name e.g "torvalds/linux"
bool GitBot::isRepositoryFullName(const std::string &str) {
  static std::regex rgxFullname(R"(^[a-zA-Z0-9_\-\.]+\/[a-zA-Z0-9_\-\.]+$)");
  return std::regex_match(str, rgxFullname);
}

// Returns true if str is a repository full url e.g "https://github.com/torvalds/linux"
bool GitBot::isRepositoryFullURL(const std::string &str, std::string &fullname) {
  static std::regex rgxFullUrl(R"(.*github.com\/(.*\/.*))");
  std::smatch m;
  if (std::regex_match(str, m, rgxFullUrl)) {
    fullname = m.str(1);
    return true;
  }
  return false;
}

bool GitBot::middleware(const tgbotxx::Ptr<tgbotxx::Message> &message) {
  // Admin bypasses middleware
  if (message->from->id == m_adminUserId) return true;

  // Must be a Private chat.
  if (message->chat->type != Chat::Type::Private) {
    safeSendMessage(message->chat->id, "Sorry, Bot can only be interacted with in private chats.");
    return false;
  }

  // Must be a User to Bot interaction, not Bot to Bot
  if (message->from->isBot) {
    safeSendMessage(message->from->id, "Sorry, Bot cannot be interacted with by other Bots.");
    return false;
  }

  // User must exist in the database
  if (not Database::userExists(message->from->id)) {
    safeSendMessage(message->from->id, "Send a /start command first to start interacting with the Bot.");
    return false;
  }

  // User status is Banned.
  const models::UserStatus userStatus = Database::getUserStatus(message->from->id);
  switch (userStatus) {
    case UserStatus::ACTIVE:
      [[likely]]
      break;
    case UserStatus::BANNED:
      safeSendMessage(message->from->id, "Sorry, You are currently banned from using this Bot.");
      return false;
    case UserStatus::BLOCKED_BOT:
      [[unlikely]]
      LOGW2("User " << message->from->id << " has unblocked the Bot. Now the /start handler will reset his status back to ACTIVE", message->toJson().dump())
      break;
    default:
      break;
  }


  // OK
  return true;
}

void GitBot::onCommand(const Ptr<Message> &message) {
  if (!middleware(message)) return;

  /// Handle command simultaneously
  m_threadPool->Submit([this, message]() -> void {
    LOGT2("onCommand", message->toJson().dump());

    if (message->text == "/start") {
      this->onStartCommand(message);
    } else if (message->text == "/my_repos") {
      this->onMyReposCommand(message);
    } else if (message->text == "/watch_repo") {
      this->onWatchRepoCommand(message);
    } else if (message->text == "/unwatch_repo") {
      this->onUnwatchRepoCommand(message);
    }

  });
}

void GitBot::onCallbackQuery(const tgbotxx::Ptr<tgbotxx::CallbackQuery> &callbackQuery) {
  /// Handle callbackQuery simultaneously
  m_threadPool->Submit([callbackQuery, this]() -> void {
    const auto parts = StringUtils::split(callbackQuery->data, '|');
    if(parts.size() != 3) {
      safeSendMessage(callbackQuery->from->id, "Invalid Action. Please try again later.");
      LOGE2("Invalid CallbackQuery payload", callbackQuery->toJson().dump());
      notifyAdmin("Invalid CallbackQuery payload: " + callbackQuery->data);
      return;
    }

    const std::string action = parts[0];
    const UserId watcherId = StringUtils::to<UserId>(parts[1]); // or callbackQuery->message->from->id;
    const std::int64_t repoId = StringUtils::to<std::int64_t>(parts[2]);
    const std::int32_t messageId = callbackQuery->message->messageId;

    if (action == "unwatch_repo") {
      try {
        Database::removeUserRepo(watcherId, repoId);
        safeSendMessage(watcherId, "Repo successfully removed from your watch list");
        LOGI2("Repo " << repoId << " successfully removed from user " << watcherId << " watch list", callbackQuery->toJson().dump());
      } catch (const std::exception &err) {
        LOGE2(err.what(), callbackQuery->toJson().dump());
        safeSendMessage(watcherId, "Failed to remove repo from watch list, Please try again");
        notifyAdmin("Failed to remove repo id " + std::to_string(repoId) + " from watch list for user id " + std::to_string(watcherId) + "\nReason: " + err.what());
      }

      // Delete the buttons message so user doesn't mistakenly remove other repos
      try {
        api()->deleteMessage(watcherId, messageId);
      } catch (...) {
        // ignore If button clicked many times, what():  Bad Request: message to delete not found
      }
    } else if (action == "unwatch_repo_cancel") {
      try {
        api()->deleteMessage(watcherId, messageId);
      } catch (...) {
        // ignore If button clicked many times, what():  Bad Request: message to delete not found
      }
    }
  });
}

void GitBot::onNonCommandMessage(const Ptr<tgbotxx::Message> &message) {
  if (!middleware(message)) return;

  LOGT2("onNonCommandMessage", message->toJson().dump());

  // Check if sent message is a repo name, example: "torvalds/linux" or "https://github.com/torvalds/linux"
  std::string repoFullName{};
  if (isRepositoryFullName(message->text))
    repoFullName = message->text;
  else if (!isRepositoryFullURL(message->text, repoFullName))
    return;

  if (not repoFullName.empty()) { // It's a repo full name.
    try {
      // First, check if user has not exceeded the maximum repos in watch list (we can't afford GitHub Api rate limit on too many repos...)
      const int userReposCount = Database::userReposCount(message->from->id);
      if (userReposCount >= kMaxWatchListRepositories) {
        safeSendMessage(message->from->id, "You have reached the maximum watch list repositories " + std::to_string(userReposCount) + "/" +
                                           std::to_string(kMaxWatchListRepositories) + ". This limit is set due to avoid Github Api Rate Limit for the Bot :(");
        return;
      }

      // Check if the new repository was already added to user's watch list
      if (Database::repoExistsByFullName(repoFullName)) {
        safeSendMessage(message->from->id, "Repository " + repoFullName + " was already added to your watch list.");
        return;
      }

      // All good until here! Let's add new repository to user's watch list
      Repository newRepo = m_gitApi->getRepository(repoFullName);
      newRepo.watcher_id = std::make_unique<UserId>(message->from->id);
      Database::addRepo(newRepo);

      safeSendMessage(message->from->id, "Repository " + newRepo.full_name + " added to watch list.");
      notifyAdmin("Repository " + newRepo.full_name + " added to watch list for user " + message->from->username);

    } catch (const GitApiRateLimitExceededException &err) {
      LOGW(err.what());
      safeSendMessage(message->from->id, "Github API Rate Limit Exceeded :( Please try again later.");
    }
    catch (const GitApiRepositoryNotFoundException &err) {
      LOGW(err.what());
      safeSendMessage(message->from->id, "Repository '" + repoFullName + "' not found.");
    }
    catch (const std::exception &e) {
      LOGE(e.what());
      safeSendMessage(message->from->id, "Could not add repository to your watch list. Please try again later");
      notifyAdmin("Error adding new repo for user id: " + std::to_string(message->from->id) + "\nRepo: " + repoFullName + "\nReason: " + e.what());
    }
  }

}

void GitBot::onLongPollError(const std::string &errorMessage, ErrorCode errorCode) {
  LOGE("Long poll error: " << errorMessage << ". error_code: " << errorCode);
  switch (errorCode) {
    case ErrorCode::OTHER:
    case ErrorCode::SEE_OTHER:
    case ErrorCode::FLOOD:
    case ErrorCode::TOO_MANY_REQUESTS:
    case ErrorCode::BAD_GATEWAY:
      LOGE("Sleeping Bot for 10s due error code: " << errorCode);
      /// It's usually network issue or Telegram servers are not responding.
      /// So we wait 10s and try again.
      std::this_thread::sleep_for(std::chrono::seconds(10));
      break;
    default:
      break;
  }

  /// Let Admin know about this, so if it occurs too many times it has to be fixed.
  notifyAdmin("Long poll error: " + errorMessage);
}

void GitBot::watchDog() {
  m_watchdogRunning = true;
  while (m_watchdogRunning) {
    try {
      Database::iterateRepos([this](const models::Repository &localRepo) {
        if (Database::getUserStatus(*localRepo.watcher_id) != UserStatus::ACTIVE)
          // Skip repositories that belong to users who blocked the bot and banned users.
          continue;

        models::Repository remoteRepo = m_gitApi->getRepository(localRepo.full_name);

        /// Stars
        if (remoteRepo.stargazers_count != localRepo.stargazers_count) {
          alertUserRepositoryStarsChange(*localRepo.watcher_id, remoteRepo.full_name, localRepo.stargazers_count,
                                         remoteRepo.stargazers_count);
        }
        /// Watchers
        if (remoteRepo.watchers_count != localRepo.watchers_count) {
          alertUserRepositoryWatchersChange(*localRepo.watcher_id, remoteRepo.full_name, localRepo.watchers_count,
                                            remoteRepo.watchers_count);
        }
        /// Issues
        if (remoteRepo.open_issues_count != localRepo.open_issues_count) {
          alertUserRepositoryIssuesChange(*localRepo.watcher_id, remoteRepo.full_name, localRepo.open_issues_count,
                                          remoteRepo.open_issues_count);
        }
        /// Pull requests
        if (remoteRepo.pulls_count != localRepo.pulls_count) {
          alertUserRepositoryPullRequestsChange(*localRepo.watcher_id, remoteRepo.full_name, localRepo.pulls_count,
                                                remoteRepo.pulls_count);
        }
        /// Forks
        if (remoteRepo.forks_count != localRepo.forks_count) {
          alertUserRepositoryForksChange(*localRepo.watcher_id, remoteRepo.full_name, localRepo.forks_count,
                                         remoteRepo.forks_count);
        }

        // Update local db repo
        remoteRepo.watcher_id = std::make_unique<UserId>(*localRepo.watcher_id);
        Database::updateRepo(remoteRepo);

        // Little nap before next repo check to not get banned by GitHub Api
        std::this_thread::sleep_for(std::chrono::seconds(1));
      });

    } catch (const GitApiRateLimitExceededException &err) {
      LOGW(err.what());
      notifyAdmin("Github API Rate Limit Exceeded :( Going to sleep and try again next hour");
    }
    catch (const std::exception &e) {
      LOGE(e.what());
      notifyAdmin(e.what());
    }

    {
      // Save db backup before going to sleep every hour
      Database::backup();

      // To check for repo updates every clock hour, which means code will check
      // at 7:00am 8:00am 9:00am...
      // Get the current time
      const auto now = std::chrono::system_clock::now();
      const auto now_c = std::chrono::system_clock::to_time_t(now);
      const std::tm now_tm = *std::localtime(&now_c);
      // Calculate the time until the start of the next clock hour
      const auto nextHour = now + std::chrono::hours(1) - std::chrono::minutes(now_tm.tm_min) - std::chrono::seconds(now_tm.tm_sec);
      // Calculate the remaining time until the next hour
      const auto remainingTime = nextHour - now;
      // Sleep until the next hour
      LOGI("Watchdog Sleeping for " << std::chrono::duration_cast<std::chrono::minutes>(remainingTime).count() << " minutes until next hour");
      // Go to sleep, but wake up if m_watchdogCv got notified and check if m_watchdogRunning.
      std::unique_lock<std::mutex> lock(m_sleepMutex);
      m_watchdogCv.wait_for(lock, remainingTime, [this] { return not m_watchdogRunning.load(); });
    }

  }
}

void GitBot::alertUserRepositoryStarsChange(UserId userId, const std::string &repositoryName, std::int64_t oldStarsCount,
                                            std::int64_t newStarsCount) {
  std::ostringstream oss{};
  oss << "New change in " << repositoryName << "!\n";
  std::int64_t newStars = newStarsCount - oldStarsCount;
  if (newStars > 0) {
    oss << newStars << " New Star(s) ⭐ 😃\n";
  } else {
    oss << newStars << " Star(s) ⭐ 😢\n";
  }
  oss << "Current stars " << newStarsCount << " ⭐";

  safeSendMessage(userId, oss.str());
}

void GitBot::alertUserRepositoryWatchersChange(UserId userId, const std::string &repositoryName,
                                               std::int64_t oldWatchersCount, std::int64_t newWatchersCount) {
  std::ostringstream oss{};
  oss << "New change in " << repositoryName << "!\n";
  std::int64_t newWatchers = newWatchersCount - oldWatchersCount;
  if (newWatchers > 0) {
    oss << newWatchers << " New Watcher(s) 👀\n";
  } else {
    oss << newWatchers << " Watcher(s) 😢\n";
  }
  oss << "Current watchers " << newWatchersCount << " 👀";

  safeSendMessage(userId, oss.str());
}

void GitBot::alertUserRepositoryIssuesChange(UserId userId, const std::string &repositoryName, std::int64_t oldIssuesCount,
                                             std::int64_t newIssuesCount) {
  std::ostringstream oss{};
  oss << "New change in " << repositoryName << "!\n";
  std::int64_t newIssues = newIssuesCount - oldIssuesCount;
  if (newIssues > 0) {
    oss << newIssues << " New Issue(s) 🐛\n";
  } else {
    oss << std::abs(newIssues) << " Issue(s) Closed 😃 🎉\n";
  }
  oss << "Current issues " << newIssuesCount << " 🐛";

  safeSendMessage(userId, oss.str());
}

void GitBot::alertUserRepositoryForksChange(UserId userId, const std::string &repositoryName, std::int64_t oldForksCount,
                                            std::int64_t newForksCount) {
  std::ostringstream oss{};
  oss << "New change in " << repositoryName << "!\n";
  std::int64_t newForks = newForksCount - oldForksCount;
  if (newForks > 0) {
    oss << newForks << " New Fork(s) 🍴\n";
  } else {
    oss << std::abs(newForks) << " Deleted Fork(s) 🍴\n";
  }
  oss << "Current forks " << newForksCount << " 🍴";

  safeSendMessage(userId, oss.str());
}


void GitBot::alertUserRepositoryPullRequestsChange(UserId userId, const std::string &repositoryName, std::int64_t oldPullsCount,
                                                   std::int64_t newPullsCount) {
  std::ostringstream oss{};
  oss << "New change in " << repositoryName << "!\n";
  std::int64_t newPulls = newPullsCount - oldPullsCount;
  if (newPulls > 0) {
    oss << newPulls << " New Pull Request(s) ⛙\n";
  } else {
    oss << std::abs(newPulls) << " Closed Pull Request(s) ⛙\n";
  }
  oss << "Current pulls " << newPullsCount << " ⛙";

  safeSendMessage(userId, oss.str());
}

void GitBot::safeSendMessage(UserId userId, std::string messageText, std::int32_t messageThreadId, const std::string &parseMode, const std::vector<tgbotxx::Ptr<tgbotxx::MessageEntity>> &entities, bool disableWebPagePreview, bool disableNotification, bool protectContent, std::int32_t replyToMessageId, bool allowSendingWithoutReply, const Ptr<IReplyMarkup> &replyMarkup) {
  if (messageText.size() > kTelegramMessageMax) {
    m_threadPool->Submit([this, userId, msg = std::move(messageText)]() -> void {
      this->safeSendLargeMessage(userId, msg);
    });
  } else {
    m_threadPool->Submit([=, this]() -> void {
      using namespace std::chrono_literals;
      Ptr<tgbotxx::Message> sentMsg{};
      constexpr std::size_t MAX_ATTEMPTS = 5;
      auto attemptSleep = 2s;
      std::size_t attempt{};

      do {
        if (attempt != 0) {
          LOGW("Attempt №" << attempt << " to send message to user ID: " << userId);
          std::this_thread::sleep_for(attemptSleep);
          attemptSleep++;
        }
        try {
          sentMsg = api()->sendMessage(userId, messageText,
                                       messageThreadId, parseMode, entities,
                                       disableWebPagePreview, disableNotification,
                                       protectContent, replyToMessageId,
                                       allowSendingWithoutReply, replyMarkup);
        } catch (const tgbotxx::Exception &e) {
          if (std::string(e.what()) == "Forbidden: bot was blocked by the user") {
            LOGW("Bot is blocked by user id: " << userId << " (" << e.what() << ")");
            this->onUserBlockedBot(userId);
            return;
          }
          LOGE("Can't send message: '" << messageText << "' to user id " << userId << " on attempt №" << attempt << ": " << e.what());
        } catch (const std::exception &e) {
          LOGE("Can't send message: '" << messageText << "' to user id " << userId << " on attempt №" << attempt << ": " << e.what());
        }
        catch (...) {
          LOGE("Can't send message: '" << messageText << "' to user id " << userId << " on attempt №" << attempt);
        }
      } while ((sentMsg == nullptr) && (++attempt <= MAX_ATTEMPTS));
    });
  }
}

void GitBot::safeSendLargeMessage(UserId userId, const std::string &messageText) {
  if (messageText.size() > kTelegramMessageMax) {
    std::vector<std::string_view> msgChunks;
    // Split the message into chunks
    for (std::size_t i = 0; i < messageText.size(); i += kTelegramMessageMax) {
      msgChunks.emplace_back(messageText.data() + i, std::min(kTelegramMessageMax, messageText.size() - i));
    }
    // Send each chunk as a reply
    LOGT("Sending large message of " << messageText.size() << " bytes partially in " << msgChunks.size() << " chunks");
    for (const std::string_view &chunk: msgChunks) {
      Ptr<tgbotxx::Message> sent{nullptr};
      std::size_t attempt = 0;
      constexpr std::size_t MAX_ATTEMPTS = 5;
      do {
        if (attempt != 0)
          std::this_thread::sleep_for(std::chrono::seconds(1));
        try {
          sent = api()->sendMessage(userId, std::string{chunk});
        } catch (const tgbotxx::Exception &e) {
          if (std::string(e.what()) == "Forbidden: bot was blocked by the user") {
            LOGW("Bot is blocked by user id: " << userId << " (" << e.what() << ")");
            this->onUserBlockedBot(userId);
            return;
          }
          LOGE("Can't send message: '" << messageText << "' to user id " << userId << " on attempt №" << attempt << ": " << e.what());
        } catch (const std::exception &e) {
          LOGE("Can't send message: '" << messageText << "' to user id " << userId << " on attempt №" << attempt << ": " << e.what());
        }
        catch (...) {
          LOGE("Can't send message: '" << messageText << "' to user id " << userId << " on attempt №" << attempt);
        }
      } while (not sent && (++attempt <= MAX_ATTEMPTS));
    }
  } else {
    safeSendMessage(userId, messageText);
  }

}

void GitBot::onUserBlockedBot(const UserId userId) {
  m_threadPool->Submit([this, userId] -> void {
    // Update user status from anything to BLOCKED_BOT
    Database::updateUserStatus(userId, models::UserStatus::BLOCKED_BOT);
    // User will be ACTIVE again when he/she sends /start command.

    notifyAdmin("User " + std::to_string(userId) + " has blocked the Bot.");
  });
}

void GitBot::onStartCommand(const tgbotxx::Ptr<tgbotxx::Message> message) {
  const UserId userId = message->from->id;

  if (Database::userExists(userId)) // Already exists? update user info except createdAt
  {
    models::User existingUser(message->from, message->chat->id);
    existingUser.createdAt = Database::getUser(userId).createdAt; // TODO: better design...
    existingUser.updatedAt = std::time(nullptr);
    Database::updateUser(existingUser);
  } else // New user!
  {
    models::User newUser(message->from, message->chat->id);
    Database::addUser(newUser);
  }

  api()->sendMessage(userId, "Welcome! Please send me a repository full name to add to your watch list. Example: torvalds/linux or https://github.com/torvalds/linux", 0, "", {}, true);

  notifyAdmin("New user! -> id: " + std::to_string(userId));
}

void GitBot::onWatchRepoCommand(const tgbotxx::Ptr<tgbotxx::Message> message) {
  api()->sendMessage(message->from->id, "Please send me a repository full name to add to your watch list. Example: torvalds/linux or https://github.com/torvalds/linux", 0, "", {}, true);
}

void GitBot::onMyReposCommand(const tgbotxx::Ptr<tgbotxx::Message> message) {
  const UserId userId = message->from->id;
  const std::vector<std::string> repoFullNames = Database::getUserReposFullnames(userId);

  if (repoFullNames.empty()) {
    safeSendMessage(message->from->id, "Your watch list is empty.");
    return;
  }

  std::ostringstream oss{};
  oss << "You are watching <b>" << repoFullNames.size() << "</b> repositories for changes:\n";
  for (const std::string &repoFullName: repoFullNames)
    oss << "- <b>" << repoFullName << "</b>\n";

  safeSendMessage(userId, oss.str(), 0, "HTML");
}

void GitBot::onUnwatchRepoCommand(const tgbotxx::Ptr<tgbotxx::Message> message) {
  const UserId userId = message->from->id;

  // Display list of repos as buttons for the user
  const std::vector<models::Repository> repos = Database::getUserRepos(userId);

  if (repos.empty()) {
    safeSendMessage(message->from->id, "Your watch list is empty.");
    return;
  }

  // Make buttons layout
  Ptr<InlineKeyboardMarkup> keyboard(new InlineKeyboardMarkup());
  for (const models::Repository &repo: repos) {
    std::vector<Ptr<InlineKeyboardButton>> row;
    Ptr<InlineKeyboardButton> btn(new InlineKeyboardButton());
    btn->text = repo.full_name;
    std::string callbackData = "unwatch_repo|" + std::to_string(*repo.watcher_id) + "|" + std::to_string(repo.id); // bcz of the limit 1-64 byte, minimize it (todo callbackData sqlite table, put id here and fill any size in db)
    btn->callbackData = callbackData;
    row.push_back(btn);
    keyboard->inlineKeyboard.push_back(row);
  }
  std::vector<Ptr<InlineKeyboardButton>> row;
  Ptr<InlineKeyboardButton> cancelBtn(new InlineKeyboardButton());
  cancelBtn->text = "Cancel";
  cancelBtn->callbackData = "unwatch_repo_cancel|" + std::to_string(message->from->id) + "|0";
  row.push_back(cancelBtn);
  keyboard->inlineKeyboard.push_back(row);

  safeSendMessage(userId, "Click a repository to unwatch:", 0, "", {}, false, false, false, 0, false, keyboard);
}

void GitBot::notifyAdmin(const std::string &msg, const std::source_location &loc) {
  std::ostringstream oss{};
  oss << msg << "\n\n[" << loc.file_name() << ':' << loc.line() << ':' << loc.column() << "] " << loc.function_name();
  const std::string message = oss.str();
  safeSendMessage(m_adminUserId, message);
  LOGI2("Admin Notification", message);
}
