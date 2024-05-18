#include "GitApi.hpp"

models::Repository GitApi::getRepository(const std::string &repositoryFullName) {
  cpr::Session session{};
  session.SetUrl("https://api.github.com/repos/" + repositoryFullName);
  session.SetConnectTimeout(cpr::ConnectTimeout{std::chrono::milliseconds(20'000)});
  session.SetTimeout(cpr::Timeout{std::chrono::seconds(60)});

  cpr::Response res = session.Get();
  nl::json json{};
  try {
    json = nl::json::parse(res.text);
  } catch (const std::exception &e) {
    LOGE2("Github Api json parsing error: " << e.what(), res.text);
    throw std::runtime_error("Failed to get Repository '" + repositoryFullName + "'. Please try again later.");
  }
  if (json.contains("message")) {
    std::string msg = json["message"];
    if (tgbotxx::StringUtils::toLower(msg).contains("rate limit exceeded")) {
      throw GitApiRateLimitExceededException(msg);
    } else if (tgbotxx::StringUtils::toLower(msg).contains("not found")) {
      throw GitApiRepositoryNotFoundException(msg);
    } else {
      throw std::runtime_error("Failed to get Repository '" + repositoryFullName + "': " + json["message"].get<std::string>());
    }
  }
  return models::Repository(json);
}