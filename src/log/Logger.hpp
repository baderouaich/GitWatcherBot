#pragma once

#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include "db/models/Log.hpp"

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

#ifdef NDEBUG
static constexpr bool kLogToConsole = false;
#else
static constexpr bool kLogToConsole = true;
#endif

/// Logs to console on Debug mode and saves log to db.
#define LOG(level, txt, longTxt, color)                         \
  try                                                           \
  {                                                             \
    std::ostringstream logoss{}, logosslng{};                   \
    logoss << txt;                                              \
    logosslng << longTxt;                                       \
    models::Log newLog(level, logoss.str(), logosslng.str());   \
    Database::addLog(newLog);                                   \
    if constexpr (kLogToConsole)                                \
    {                                                           \
      std::cout << color << newLog.toString() << std::endl;     \
    }                                                           \
  }                                                             \
  catch (const std::exception &e)                               \
  {                                                             \
    std::cerr << "Failed to log [" << txt << "]: " << e.what(); \
  }


#define LOGT(txt) LOG("trace", txt, "", KNRM)
#define LOGI(txt) LOG("info", txt, "", KGRN)
#define LOGW(txt) LOG("warn", txt, "", KYEL)
#define LOGE(txt) LOG("error", txt, "", KRED)

#define LOGT2(txt, longTxt) LOG("trace", txt, longTxt, KNRM)
#define LOGI2(txt, longTxt) LOG("info", txt, longTxt, KGRN)
#define LOGW2(txt, longTxt) LOG("warn", txt, longTxt, KYEL)
#define LOGE2(txt, longTxt) LOG("error", txt, longTxt, KRED)
