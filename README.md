[![MIT License](https://img.shields.io/badge/license-MIT-yellow)](https://github.com/baderouaich/GitWatcherBot/blob/main/LICENSE)
[![Docs](https://codedocs.xyz/doxygen/doxygen.svg)](https://baderouaich.github.io/GitWatcherBot)
[![Language](https://img.shields.io/badge/C++-23-blue.svg?style=flat&logo=c%2B%2B)](https://img.shields.io/badge/C++-23-blue.svg?style=flat&logo=c%2B%2B)

## Goal
This small project demonstrates how to create a real world Telegram Bot using C++ and [tgbotxx](https://github.com/baderouaich/tgbotxx) Telegram Bot library with SQLite3 [orm](https://github.com/fnc12/sqlite_orm) database.

It is meant to run for the long term if nothing really bad went wrong, the Bot will restart after system reboot with a [cron jobs script](./cron_jobs.sh) and carry on running.

The Bot will handle long polling errors that occur sometimes which can be caused usually by network issues, Telegram server not responding and more. It will also notify an admin when some issue occurs so it can be fixed in a sooner time.

The project also demonstrates how you can implement a middleware like function to handle users requests securely. As well as a thread pool to handle multiple user requests simultaneously. 

Finally, it also shows how to use a SQLite3 database to store data safely with multiple threads readers and writers, as well as database backup periodically (every hour). 


## GitWatcherBot
A Telegram Bot that notifies you when a new change is made in your repositories (issues, pull requests, stars, forks, and watches).

You can add specific repositories to your watch list and get changes notifications.

<img src="https://i.ibb.co/XDXV2PZ/NEW.jpg" alt="Demo Image" width="300">

## Run your own version of the Bot
1. Clone the repository
3. Acquire a new Bot token from @BotFather and store it in `res/BOT_TOKEN.txt`.
4. Put your Telegram User ID in `res/ADMIN_USER_ID.txt` to get notifications about issues. 
      > if you don't know what is your telegram user id, there are 2 ways to get it:
      > 1. Run the bot and send a message to it, then print your id in one of the callbacks such as onAnyMessage(message) { std::cout << message->from->id << std::endl; }
      > 2. Open Telegram app. Then, search for “userinfobot”, click Start button and it will prompt the bot to display your user ID
6. Build & Run your Bot detached from the console with the [build_and_run.sh](./build_and_run.sh) script
8. Congratulations! your Bot is now running in the background. To stop your Bot, run `pkill GitWatcherBot`

### Requirements
- Linux OS (Ubuntu recommended)
- cmake 3.20+
- g++-11/clang++-12 and up
- tgbotxx (will be fetched by cmake)
- sqlite3_orm (will be fetched by cmake)

### CI Status

| Operating system | Build status                                                                                                                                                                                      |
|------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Ubuntu Debug (x64) | [![Ubuntu](https://img.shields.io/github/actions/workflow/status/baderouaich/GitWatcherBot/build-ubuntu-debug.yml?branch=main)](https://github.com/baderouaich/GitWatcherBot/actions/workflows/build-ubuntu-debug.yml)    |
| Ubuntu Release (x64) | [![Ubuntu](https://img.shields.io/github/actions/workflow/status/baderouaich/GitWatcherBot/build-ubuntu-release.yml?branch=main)](https://github.com/baderouaich/GitWatcherBot/actions/workflows/build-ubuntu-release.yml)    |
