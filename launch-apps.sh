#!/bin/bash

AERON_MD="$HOME/Documents/aeron-master/cppbuild/Release/binaries/aeronmd"
PROJECT_DIR="$HOME/Documents/TradingExchangeDemo"
BIN_DIR="$PROJECT_DIR/build" 

gnome-terminal --tab --title="AeronMD" -- bash -c "$AERON_MD; exec bash"
sleep 1
gnome-terminal --tab --title="Gateway" -- bash -c "cd $BIN_DIR && ./src/gateway/gateway; exec bash"
gnome-terminal --tab --title="Matching Engine" -- bash -c "cd $BIN_DIR && ./src/matching/matching_engine; exec bash"
gnome-terminal --title="Market Viewer" -- bash -c "cd $BIN_DIR && ./src/viewer/market_viewer; exec bash"

sleep 0.5
#gnome-terminal --title="Client TUI" -- bash -c "cd $PROJECT_DIR/test && python3 load_test.py; exec bash"
gnome-terminal --title="Client TUI" -- bash -c "cd $PROJECT_DIR/test && python3 client_tui.py; exec bash"