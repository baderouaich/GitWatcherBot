## GitWatcherBot
Source code of https://t.me/GitWatcherBot

A Telegram Bot that notifies you when a new change is made in your repositories (issues, pull requests, stars, forks, and watches).

You can add specific repositories to your watch list and get changes notifications.

<img src="https://i.ibb.co/XDXV2PZ/NEW.jpg" alt="Demo Image" width="300">

## Run your own version of the Bot
1. Clone the repository
2. Put your Bot token in `res/BOT_TOKEN.txt`.
3. Put your Telegram user ID in `res/ADMIN_USER_ID.txt` to get notifications about issues.
4. Build & Run your Bot 

### Requirements
- Linux OS (Ubuntu recommended)
- cmake 3.20+
- g++-11/clang++-12 and up
- tgbotxx (will be fetched by cmake)
- sqlite3_orm (will be fetched by cmake)
