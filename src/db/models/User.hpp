#pragma once

#include <cstdint>
#include <string>
#include <cstring>
#include <ctime>
#include <sqlite_orm/sqlite_orm.h>
#include <tgbotxx/tgbotxx.hpp>
#include "enums/UserStatus.inl"

namespace models {
  using UserId = decltype(tgbotxx::User::id);
  using ChatId = decltype(tgbotxx::Chat::id);

  struct User {
    UserId id{};
    ChatId chatId{};
    UserStatus status{UserStatus::ACTIVE};
    std::string firstName;
    std::string lastName;
    std::string username;
    std::string languageCode;
    bool isBot{};
    bool isPremium{};
    bool addedToAttachmentMenu{};
    std::time_t createdAt{};
    std::time_t updatedAt{};

    static auto table() {
      using namespace sqlite_orm;
      return make_table("Users",
                        make_column("id", &User::id, primary_key()),
                        make_column("chatId", &User::chatId),
                        make_column("status", &User::status),
                        make_column("firstName", &User::firstName),
                        make_column("lastName", &User::lastName),
                        make_column("username", &User::username),
                        make_column("languageCode", &User::languageCode),
                        make_column("isBot", &User::isBot),
                        make_column("isPremium", &User::isPremium),
                        make_column("addedToAttachmentMenu", &User::addedToAttachmentMenu),
                        make_column("createdAt", &User::createdAt),
                        make_column("updatedAt", &User::updatedAt)
      );
    }

    User() = default;

    User(tgbotxx::Ptr<tgbotxx::User> tgUser, ChatId chatId) {
      id = tgUser->id;
      this->chatId = chatId;
      status = UserStatus::ACTIVE;
      firstName = tgUser->firstName;
      lastName = tgUser->lastName;
      username = tgUser->username;
      languageCode = tgUser->languageCode;
      isBot = tgUser->isBot;
      isPremium = tgUser->isPremium;
      addedToAttachmentMenu = tgUser->addedToAttachmentMenu;
      createdAt = std::time(nullptr);
      updatedAt = createdAt;
    }
  };
}

