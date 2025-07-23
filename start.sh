#!/bin/bash
./database/pocketbase serve --http 0.0.0.0:8090 &
source ./.venv/bin/activate
uvicorn main:server --reload --host 0.0.0.0

