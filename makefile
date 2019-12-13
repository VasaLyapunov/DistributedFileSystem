all: client server

client: dfs_client.c
	gcc -o dfs_client dfs_client.c -lssl -lcrypto

server: dfs_server.c
	gcc -o dfs_server dfs_server.c -lssl -lcrypto

clean: 
	$(RM) dfs_client dfs_server