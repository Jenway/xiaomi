#!/bin/bash

SERVER_PORT=18888
SERVER_BINARY="./build/fileServer"
CLIENT_BINARY="./build/fileClient"
SERVER_FILE="test.txt"
CLIENT_OUTPUT="downloaded_test.txt"
PCAP_FILE="capture.pcap"
INTERFACE="loopback0"
FILTER_PORT=$SERVER_PORT
SESSION="file-transfer"


echo "This is a test file" > "$SERVER_FILE"

rm -f "$CLIENT_OUTPUT" "$PCAP_FILE"

tmux new-session -d -s "$SESSION" -x 160 -y 48 "bash"

tmux send-keys -t "$SESSION:0.0" "bash -c '$SERVER_BINARY $SERVER_PORT'" C-m

tmux split-window -v -t "$SESSION:0" "bash"
tmux send-keys -t "$SESSION:0.1" "sudo tcpdump -i $INTERFACE port $FILTER_PORT -w $PCAP_FILE" C-m

sleep 1

tmux split-window -h -t "$SESSION:0.1" "bash"
tmux send-keys -t "$SESSION:0.2" "bash -c '$CLIENT_BINARY 127.0.0.1 $SERVER_PORT $SERVER_FILE'" C-m

sleep 3

tmux split-window -v -t "$SESSION:0.2" "bash"
tmux send-keys -t "$SESSION:0.3" "sudo pkill tcpdump" C-m

tmux select-pane -t "$SESSION:0.0"
tmux attach-session -t "$SESSION"
