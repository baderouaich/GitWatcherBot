#pragma once

#include "sqlite_orm/sqlite_orm.h"

namespace models {
  /// @brief User's situation with the Bot
  enum class UserStatus : std::uint8_t {
    /// @brief User is active, not banned and can interact with the bot anytime and can receive repositories updates
    ACTIVE, 
    /// @brief User is banned and cannot interact with the Bot for some reason
    BANNED, 
    /// @brief User has blocked the Bot
    BLOCKED_BOT 
  };

  /// @brief Converts enum UserStatus to std::string
  static std::string userStatusToString(const UserStatus userStatus) noexcept {
    switch (userStatus) {
      case UserStatus::ACTIVE:
        return "ACTIVE";
      case UserStatus::BANNED:
        return "BANNED";
      case UserStatus::BLOCKED_BOT:
        return "BLOCKED_BOT";
      default:
        std::unreachable();
    }
  }

  /// @brief Converts std::string to enum UserStatus
  static std::optional<UserStatus> stringToUserStatus(const std::string &str) noexcept {
    if (str == "ACTIVE") return UserStatus::ACTIVE;
    else if (str == "BANNED") return UserStatus::BANNED;
    else if (str == "BLOCKED_BOT") return UserStatus::BLOCKED_BOT;
    else
      return std::nullopt;
  }

}

namespace sqlite_orm {
  using namespace models;
  /**
   *  First of all is a type_printer template class.
   *  It is responsible for sqlite type string representation.
   *  We want Gender to be `TEXT` so let's just derive from
   *  text_printer. Also there are other printers: real_printer and
   *  integer_printer. We must use them if we want to map our type to `REAL` (double/float)
   *  or `INTEGER` (int/long/short etc) respectively.
   */
  template<>
  struct type_printer<UserStatus> : public text_printer {
  };

  /**
   *  This is a binder class. It is used to bind c++ values to sqlite queries.
   *  Here we have to create gender string representation and bind it as string.
   *  Any statement_binder specialization must have `int bind(sqlite3_stmt*, int, const T&)` function
   *  which returns bind result. Also you can call any of `sqlite3_bind_*` functions directly.
   *  More here https://www.sqlite.org/c3ref/bind_blob.html
   */
  template<>
  struct statement_binder<UserStatus> {

    int bind(sqlite3_stmt *stmt, int index, const UserStatus &value) {
      return statement_binder<std::string>().bind(stmt, index, userStatusToString(value));
      //  or return sqlite3_bind_text(stmt, index++, GenderToString(value).c_str(), -1, SQLITE_TRANSIENT);
    }
  };

  /**
   *  field_printer is used in `dump` and `where` functions. Here we have to create
   *  a string from mapped object.
   */
  template<>
  struct field_printer<UserStatus> {
    std::string operator()(const UserStatus &t) const {
      return userStatusToString(t);
    }
  };

  /**
   *  This is a reverse operation: here we have to specify a way to transform string received from
   *  database to our Gender object. Here we call `GenderFromString` and throw `std::runtime_error` if it returns
   *  nullptr. Every `row_extractor` specialization must have `extract(const char*)` and `extract(sqlite3_stmt *stmt,
   * int columnIndex)` functions which return a mapped type value.
   */
  template<>
  struct row_extractor<UserStatus> {
    UserStatus extract(const char *row_value) {
      if (auto gender = stringToUserStatus(row_value); gender.has_value()) {
        return *gender;
      } else {
        throw std::runtime_error("Could not convert string " + std::string(row_value) + " to enum UserStatus");
      }
    }

    UserStatus extract(sqlite3_stmt *stmt, int columnIndex) {
      auto str = sqlite3_column_text(stmt, columnIndex);
      return this->extract((const char *) str);
    }
  };
}