#!/bin/bash
#
# build_queue_status.sh
# Shows whether a build is currently running and what the queue looks like.
# Safe to run anytime (read-only).
#

QUEUEDIR=~/projects/build_queue
LOCKFILE=~/projects/build.lock

echo -e "=== LCR Build Queue Status ==="
echo "Time: $(date '+%Y-%m-%d %H:%M:%S')"
echo

# ------------------ Check if a build is running ------------------
echo -e "Current Status:"

(
    # Try to acquire lock non-blocking
    if flock -n 200; then
        echo -e "  No build is currently running."
        # Release immediately
        flock -u 200
        exit 0
    else
        echo -e "  A build is currently RUNNING."
        exit 1
    fi
) 200<"$LOCKFILE" 2>/dev/null

# Try to show which process holds the lock (best effort)
if command -v fuser >/dev/null 2>&1; then
    HOLDER_PID=$(fuser "$LOCKFILE" 2>/dev/null | awk '{print $1}' | head -1)
    if [ -n "$HOLDER_PID" ]; then
        echo -e "  Lock held by PID: $HOLDER_PID"
        ps -p "$HOLDER_PID" -o pid,ppid,user,cmd --no-headers 2>/dev/null | sed 's/^/    /'
    fi
fi

echo

# ------------------ Show the queue ------------------
echo -e "Build Queue:"

if [ ! -d "$QUEUEDIR" ] || [ -z "$(ls -A "$QUEUEDIR" 2>/dev/null)" ]; then
    echo "  Queue is empty."
    exit 0
fi

position=0
ls -1 "$QUEUEDIR" | sort | while read -r qfile; do
    position=$((position + 1))

    # Parse filename: TIMESTAMP_PID  (e.g. 20250617123456123456789_12345)
    if [[ "$qfile" =~ ^([0-9]{14,})_([0-9]+)$ ]]; then
        timestamp="${BASH_REMATCH[1]}"
        pid="${BASH_REMATCH[2]}"

        # Try to make timestamp human-readable
        if [ ${#timestamp} -ge 14 ]; then
            year=${timestamp:0:4}
            mon=${timestamp:4:2}
            day=${timestamp:6:2}
            hour=${timestamp:8:2}
            min=${timestamp:10:2}
            sec=${timestamp:12:2}
            humantime="${year}-${mon}-${day} ${hour}:${min}:${sec}"
        else
            humantime="$timestamp"
        fi

        # Check if the queue file contains a branch name (future-proofing)
        qpath="$QUEUEDIR/$qfile"
        if [ -s "$qpath" ]; then
            branch=$(head -n1 "$qpath" | tr -d '\n\r')
            if [ -n "$branch" ] && [ "$branch" != "unknown" ]; then
                branch_info="  branch: $branch"
            else
                branch_info=""
            fi
        else
            branch_info=""
        fi

        printf "  %2d. %s  (PID: %s)%s\n" "$position" "$humantime" "$pid" "$branch_info"

    else
        printf "  %2d. %s  (unrecognized queue file)\n" "$position" "$qfile"
    fi
done

echo
total=$(ls -1 "$QUEUEDIR" 2>/dev/null | wc -l)
echo -e "Total jobs in queue: $total"
echo
echo "Tip: Run this script anytime to check status without affecting the queue."
