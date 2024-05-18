#include "GitBot.hpp"
#include <csignal>

int main(int argc, const char *argv[]) {
  static std::unique_ptr<GitBot> BOT = std::make_unique<GitBot>();
  for (const int sig: {SIGINT, SIGABRT, SIGKILL, SIGTERM, SIGSEGV, SIGHUP}) {
    std::signal(sig, [](int s) { // Graceful Bot exit on CTRL+C, Segmentation Fault, Abort, Console close (hangup)...
      if (BOT) BOT->stop();
      std::exit(s);
    });
  }
  BOT->start();
  return EXIT_SUCCESS;
}