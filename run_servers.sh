trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT
./dfs_server ./DFS1 10001 &
./dfs_server ./DFS2 10002 &
./dfs_server ./DFS3 10003 &
./dfs_server ./DFS4 10004 &
read -r -d '' _ </dev/tty