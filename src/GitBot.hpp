#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <tgbotxx/tgbotxx.hpp>
#include <type_traits>
#include "api/GitApi.hpp"
#include <cpr/threadpool.h>

/// @brief Bot class 
class GitBot : public tgbotxx::Bot {
  using ChatId = decltype(tgbotxx::Chat::id);
  using UserId = decltype(tgbotxx::User::id);

public:
  GitBot();
  virtual ~GitBot() = default;

private:
  /// @brief Returns true if the command/message is allowed to proceed,
  /// False for example the Bot was interacted with in a group,
  /// or another bot messaged our bot, or the message came from a BANNED user...
  /// So this function will be our middleware between User and the Bot, and should only allow the
  /// Operation to proceed under certain criteria.
  bool middleware(const tgbotxx::Ptr<tgbotxx::Message> &message);

  /// @brief Called on Bot Start (before receiving updates)
  void onStart() override;
  /// @brief Called on Bot Stop (before exiting)
  void onStop() override;
  /// @brief Called on new command message (example: /start)
  void onCommand(const tgbotxx::Ptr<tgbotxx::Message> &message) override;
  /// @brief Called on new non-command message (example: hello)
  void onNonCommandMessage(const tgbotxx::Ptr<tgbotxx::Message> &message) override;
  /// @brief Called back when a button was clicked by the user, providing us with the payload we assigned the button.
  void onCallbackQuery(const tgbotxx::Ptr<tgbotxx::CallbackQuery> &callbackQuery) override;
  /// @brief Called when an issue happened with the long polling (network issues for example)
  void onLongPollError(const std::string &errorMessage, tgbotxx::ErrorCode errorCode) override;

  /// @brief Command handlers
  void onStartCommand(const tgbotxx::Ptr<tgbotxx::Message> message);
  void onWatchRepoCommand(const tgbotxx::Ptr<tgbotxx::Message> message);
  void onUnwatchRepoCommand(const tgbotxx::Ptr<tgbotxx::Message> message);
  void onMyReposCommand(const tgbotxx::Ptr<tgbotxx::Message> message);

  /// @brief Triggers when we detect Bot was blocked by a User.
  /// It updates User's status in the database to BLOCKED_BOT.
  void onUserBlockedBot(const UserId userId);

private:
  /// @brief Watch dog that retrieves new repositories data by the hour
  void watchDog();

  /// @brief Alerts user that his repository's stars have changed
  void alertUserRepositoryStarsChange(UserId userId, const std::string& repositoryName, std::int64_t oldStarsCount, std::int64_t newStarsCount);
  /// @brief Alerts user that his repository's watchers have changed
  void alertUserRepositoryWatchersChange(UserId userId, const std::string& repositoryName, std::int64_t oldWatchersCount, std::int64_t newWatchersCount);
  /// @brief Alerts user that his repository's issues have changed
  void alertUserRepositoryIssuesChange(UserId userId, const std::string& repositoryName, std::int64_t oldIssuesCount, std::int64_t newIssuesCount);
  /// @brief Alerts user that his repository's forks have changed
  void alertUserRepositoryForksChange(UserId userId, const std::string& repositoryName, std::int64_t oldForksCount, std::int64_t newForksCount);
  /// @brief Alerts user that his repository's pull requests have changed
  void alertUserRepositoryPullRequestsChange(UserId userId, const std::string& repositoryName, std::int64_t oldPullsCount, std::int64_t newPullsCount);

private:
  /// @brief Notify admin with a message.
  /// Used on critical issues to alert admin of the Bot to fix the bug in a soon time instead of
  /// Checking the logs time to time.
  void notifyAdmin(const std::string& msg, const std::source_location& loc = std::source_location::current());

private:
  /// @brief Sends a message asynchronously and safely by retrying up to 5 times on failure
  /// Primarily inspired by @OMRKiruha from Issue: https://github.com/baderouaich/tgbotxx/issues/4#issuecomment-2016816708
  void safeSendMessage(UserId userId, std::string messageText,
                       std::int32_t messageThreadId = 0,
                       const std::string &parseMode = "",
                       const std::vector<tgbotxx::Ptr<tgbotxx::MessageEntity>> &entities = std::vector<tgbotxx::Ptr<tgbotxx::MessageEntity>>(),
                       bool disableWebPagePreview = false,
                       bool disableNotification = false,
                       bool protectContent = false,
                       std::int32_t replyToMessageId = 0,
                       bool allowSendingWithoutReply = false,
                       const tgbotxx::Ptr<tgbotxx::IReplyMarkup> &replyMarkup = nullptr);

  /// @brief Sends a large message > 4096 (Telegram message limit) partially in chunks [Used by safeSendMessage]
  void safeSendLargeMessage(UserId userId, const std::string &messageText);

public:
  /// @brief Returns true if str is a repository full name e.g "torvalds/linux"
  static bool isRepositoryFullName(const std::string &str);

  /// @brief Returns true if str is a repository full url e.g "https://github.com/torvalds/linux"
  static bool isRepositoryFullURL(const std::string &str, std::string &fullname);

private:
  UserId m_adminUserId{}; ///<! Telegram user id for Admin to be notified with critical issues
  std::unique_ptr<GitApi> m_gitApi; ///<! GitHub Api for getting repository information
  std::unique_ptr<std::thread> m_watchdogThread; ///<! Watch dog thread that retrieves repositories information and dispatches alerts
  std::mutex m_sleepMutex; ///<! Mutex for watch dog sleep
  std::atomic<bool> m_watchdogRunning; ///<! True if watch dog is currently running
  std::condition_variable m_watchdogCv; ///<! Watch dog conditional variable to be notified and awaken from sleep if Bot wants to exit immediately
  std::unique_ptr<cpr::ThreadPool> m_threadPool; ///<! Thread pool to handle multiple user requests simultaneously

  inline static constexpr std::size_t kTelegramMessageMax = 4096; ///<! Telegram limits each message to 4096 characters max
  inline static constexpr std::size_t kMaxWatchListRepositories = 25; ///<! For now 25 repos watch limit per user, to not exceed github api rate limits
};
