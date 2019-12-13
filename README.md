# Distributed File System in C

# Intro

The following C files implement a distributed file system in C from scratch. Using this program, a user can send take any file and distribute it to four different servers, which each contain several portions of the file. Even if one server is lost, the remaining servers will have enough pieces of the file to reconstruct it.

# Usage

Type 'make' to compile 'dfs_client.c' and 'dfs_server.c' into their respective executable forms.  Typing 'make clean' will remove these files. 

To run the program, first run four instances of the 'dfs_server' file. Each takes a directory name and port number as a parameter, and should be inputted like so:

./dfs_server ./DFS1 10001  
./dfs_server ./DFS2 10002  
./dfs_server ./DFS3 10003  
./dfs_server ./DFS4 10004  

For convenience, these very commands have been conglomerated into a bash script, called 'run_servers.sh'. Simply type .'/run_servers.sh' to run the servers. From here, the client program can be run without parameters by typing "./dfs_client".

Once running, the client first has to log in. Currently, all usernames and passwords are stored in plaintext in the file 'dfc_passwords.conf'. Once the user is logged in, they can use the following commands:

list  
get <filename>  
put <filename>  

The 'list' command simply shows all files in the DFS that belong to the logged in user. Files that can be reconstructed due to a missing server or missing parts are labeled as '[incomplete]'. 
The 'get' command reconstructs a file from the DFS, provided all the parts are in place.
The 'put' command sends a file to the DFS, provided it exists in the working directory.

# Limitations

This project is meant more as a proof as concept rather than a usable piece of software, as in its current state it has several key limitations. Most importantly, I have not provided any infrastructure to host the DFS servers on other machines, and everything is currently done locally. In addition, The error handling is relatively weak; Most failure cases result in the programs exiting. The login system serves entirely for organizational purposes at the moment. It is trivial to view and change any passwords.
