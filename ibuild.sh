#!/bin/bash
LOCKFILE=~/projects/build.lock  # Adjust this path as needed
QUEUEDIR=~/projects/build_queue  # Directory for queue files; adjust as needed
SCRIPT_NAME=$(basename "$0")    # Name of this script for any additional checks

BOLD='\e[1m'
BOLD_GREEN='\e[1;32m'
BOLD_RED='\e[1;31m'
BOLD_BLUE='\e[1;34m'
RESET='\e[0m'

# Ensure queue directory exists
mkdir -p "$QUEUEDIR"

# Generate a unique queue file name: timestamp_nanoseconds_PID
TIMESTAMP=$(date +%Y%m%d%H%M%S%N)
QUEUEFILE="$QUEUEDIR/${TIMESTAMP}_$$"

# Add to queue
touch "$QUEUEFILE" || { echo "${BOLD_RED}Error: Failed to create queue file.${RESET}"; exit 1; }

# Set up trap to delete queue file on Ctrl+C (SIGINT)
trap 'rm -f "$QUEUEFILE"; printf "${BOLD_RED} Build interrupted. Queue file cleaned up.${RESET}\n"; exit 130' INT

# Function to count builds ahead (files with earlier timestamps)
builds_ahead() {
    # List and sort files by name (timestamp order)
    ls -1 "$QUEUEDIR" | sort | awk -v myfile="${TIMESTAMP}_$$" '
    BEGIN { count = 0; found = 0 }
    {
        if (found) next;  # Skip after finding ours
        if ($0 == myfile) { found = 1; next }  # Ours: stop counting
        if ($0 ~ /^[0-9]+_[0-9]+$/) count++  # Valid queue file
    }
    END { print count }
    '
}

# Display initial queue position
AHEAD=$(builds_ahead)

printf "${BOLD_GREEN}Build queued. Currently %s build(s) ahead of this one.${RESET}\n" "$AHEAD"

# Acquire exclusive lock (-x) on file descriptor 200, waiting until available
printf "${BOLD_BLUE}Acquiring lock. Please wait until other builds are finished...\n${RESET}"
(
    # While waiting for the lock, periodically update the queue position
    while ! flock -n 200; do
        AHEAD=$(builds_ahead)
        if [ "$AHEAD" -eq 1 ]; then
            S=""; 
        else 
            S="s"; 
        fi
        printf "${BOLD_BLUE}\rStill waiting... %d build%s ahead.${RESET}" "$AHEAD" "$S"
        sleep 3
    done || { printf "${BOLD_RED}Error: Failed to acquire lock.${RESET}\n"; rm "$QUEUEFILE"; exit 1; }

    # Once lock is acquired, we're building
    printf "\n${BOLD_GREEN}Lock acquired. Starting build (your position: 0 ahead).${RESET}\n\n"

    cleaned=0
    for qfile in $(ls -1 "$QUEUEDIR"/[0-9]*_[0-9]* 2>/dev/null | sort); do
        filename=$(basename "$qfile")
        if [[ "$filename" < "${TIMESTAMP}_$$" ]]; then
            printf "${BOLD_BLUE}  Removing stale: $filename ${RESET}\n"
            rm -f "$qfile"
            ((cleaned++))
        else
            break  # everything after this is ours or newer - stop
        fi
    done

    if [ $cleaned -gt 0 ]; then
        printf "${BOLD_BLUE}  → Cleaned up $cleaned stale queue file(s).${RESET}\n"
    else
        printf "${BOLD_BLUE}  → No stale queue files found.${RESET}\n"
    fi

    SIM_MODE=0
    BRANCH=""

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --sim)
                SIM_MODE=1
                shift
                ;;
            *)
                BRANCH="$1"
                shift
                ;;
        esac
    done

    if [ -n "$BRANCH" ]; then
        printf "${BOLD_BLUE}Switching to branch: $BRANCH ${RESET}\n"
        git fetch
        git checkout "$BRANCH" || { echo "${BOLD_RED}Error: Failed to checkout branch $BRANCH ${RESET}"; exit 1; }
    else
        printf "${BOLD_BLUE}No branch specified; using current branch: $(git branch --show-current)  ${RESET}\n"
        BRANCH=$(git branch --show-current)
    fi

    printf "exporting Variables for the build...\n"
    export SIMULATION=$SIM_MODE
    export BB_ENV_PASSTHROUGH_ADDITIONS="${BB_ENV_PASSTHROUGH_ADDITIONS} SIMULATION"

    if [ "$SIMULATION" = "1" ]; then
        printf "${BOLD_BLUE}Building for SIMULATION${RESET}\n"
    else
        printf "${BOLD_BLUE}Building for real hardware${RESET}\n"
    fi

    git fetch && git reset --hard origin/$BRANCH && ./patchmover.sh && ./buildImage.sh

) 200>"$LOCKFILE"
BUILD_EXIT=$?

# Cleanup queue file
rm -f "$QUEUEFILE"

if [ $BUILD_EXIT -ne 0 ]; then
    printf "${BOLD_RED}Build failed (exit code $BUILD_EXIT). Check $FAILURE_LOG or $BUILD_LOG on the build machine.${RESET}\n"
    exit 1
fi

printf "${BOLD_GREEN}Build finished successfully.${RESET}\n"