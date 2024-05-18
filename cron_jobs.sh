#!/bin/sh

EXECUTABLE_NAME="GitWatcherBot"  # <--- change me accordingly
EXECUTABLE_DIR="$(pwd)/build"  # <--- change me accordingly


# Function to check if a cron job already exists
cron_job_exists() {
    crontab -l 2>/dev/null | grep -q "$1"
}

# Function to add a cron job
add_cron_job() {
    (crontab -l 2>/dev/null; echo "$1") | crontab -
}

# Add cron job to start the process on system reboot (wait 60s for system to fully boot)
START_PROCESS_AFTER_REBOOT_CRON_JOB="@reboot sleep 60 && cd $EXECUTABLE_DIR && ./$EXECUTABLE_NAME &"
if ! cron_job_exists "$START_PROCESS_AFTER_REBOOT_CRON_JOB"; then
    echo "Adding cron job $START_PROCESS_AFTER_REBOOT_CRON_JOB ..."
    add_cron_job "$START_PROCESS_AFTER_REBOOT_CRON_JOB"
else
    echo "Skipping cron job already exists: $START_PROCESS_AFTER_REBOOT_CRON_JOB"
fi


# add more cron jobs below...

# to delete a cron job from crontab: 
# run bash: $ (crontab -l | grep -v "your_cron_job e.g @reboot cd .." ) | crontab -
