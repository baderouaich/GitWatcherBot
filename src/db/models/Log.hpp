#pragma once

#include <cstdint>
#include <string>
#include <cstring>
#include <ctime>
#include <sqlite_orm/sqlite_orm.h>
#include <tgbotxx/utils/DateTimeUtils.hpp>
#include <tgbotxx/utils/FileUtils.hpp>
#include <source_location>

namespace models {

  struct Log {
    std::int64_t id{};
    std::string severity; // TRACE, INFO, WARN, ERROR..
    std::string shortMessage; // Summary
    std::string longMessage; // Larger text, dump of json of update for example
    std::time_t timestamp{};
    // source location
    std::string filename;
    std::uint_least32_t line{};
    std::uint_least32_t column{};
    std::string functionName;

    static auto table() {
      using namespace sqlite_orm;
      return make_table("Logs",
                        make_column("id", &Log::id, primary_key().autoincrement()),
                        make_column("severity", &Log::severity),
                        make_column("shortMessage", &Log::shortMessage),
                        make_column("longMessage", &Log::longMessage, default_value("")),
                        make_column("timestamp", &Log::timestamp),
                        make_column("filename", &Log::filename),
                        make_column("line", &Log::line),
                        make_column("column", &Log::column),
                        make_column("functionName", &Log::functionName)
      );
    }

    Log() = default;

    Log(const std::string &sev, const std::string &shortMsg, const std::string &longMsg = "", const std::source_location& here = std::source_location::current()) {
      severity = sev;
      shortMessage = shortMsg;
      longMessage = longMsg;
      timestamp = std::time(nullptr);
      filename = here.file_name();
      line = here.line();
      column = here.column();
      functionName = here.function_name();
    }


    std::string toString() const noexcept {
      std::ostringstream oss{};
      oss << fs::path(filename).filename().string() << ':' << line << ':' << column << " [" << functionName << "] [" << tgbotxx::DateTimeUtils::toString(timestamp) << "] [" << severity << "]: " << shortMessage;
      return oss.str();
    }
  };
}