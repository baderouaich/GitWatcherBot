#!/bin/bash
echo "Building..."
mkdir build
pushd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc
make -j$(nproc)
popd


echo "Stopping any existing instance of GitWatcherBot..."
pkill -SIGINT GitWatcherBot


echo "Running GitWatcherBot detached from console... (so you can logout)"
./build/GitWatcherBot &


echo "Installing cron jobs to automatically start GitWatcherBot after system reboot..."
bash ./cron_jobs.sh

echo "Listing process list..."
ps
