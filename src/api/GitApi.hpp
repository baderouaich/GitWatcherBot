#pragma once
#include <exception>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include "db/Database.hpp"
#include "log/Logger.hpp" ///<! must include Database with Logger for models::Log & Database::addLog
namespace nl = nlohmann;

/// @ref https://docs.github.com/en/rest/using-the-rest-api/troubleshooting-the-rest-api?apiVersion=2022-11-28#rate-limit-errors

/// @brief Thrown when we exceed GitHub's Api Rate Limit (60 requests an hour)
struct GitApiRateLimitExceededException : std::runtime_error {
  explicit GitApiRateLimitExceededException(const std::string& msg) : std::runtime_error(msg) {}
};

/// @brief Thrown when requested repository doesn't exist
struct GitApiRepositoryNotFoundException : std::runtime_error {
  explicit GitApiRepositoryNotFoundException(const std::string& msg) : std::runtime_error(msg) {}
};

class GitApi {
public:
  GitApi() = default;
  ~GitApi() = default;

  /// @brief Returns Repository information by fullname from GitHub Api
  models::Repository getRepository(const std::string& repositoryFullName = "torvalds/linux");

};
