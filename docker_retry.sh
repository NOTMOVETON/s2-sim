#!/bin/bash

SERVICE=${1:-sim_ros2}  # build / tests / sim / sim_ros2 / etc.
INNER=${2:-10}          # retries per outer attempt
OUTER=${3:-2}           # outer attempts

DOCKER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/docker"

for o in $(seq 1 "$OUTER"); do
    echo "=== Outer attempt $o/$OUTER ==="
    for i in $(seq 1 "$INNER"); do
        echo "  --- Build attempt $i/$INNER (service: $SERVICE) ---"
        if (cd "$DOCKER_DIR" && docker compose up "$SERVICE" --build); then
            echo "Success."
            exit 0
        fi
        echo "  Attempt $i failed."
    done
    echo "=== All $INNER inner attempts failed. Outer $o/$OUTER done. ==="
done

echo "All $((INNER * OUTER)) attempts exhausted. Build failed."
exit 1
