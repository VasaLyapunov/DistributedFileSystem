#include <stdio.h>
#include <stdlib.h>         // Provides standard functions like exit() & atoi()
#include <string.h>         // Provides string functions like strcmp()
#include <sys/socket.h>     // Provides socket functions
#include <netinet/in.h>     // Provides socket structs like sockaddr_in
#include <unistd.h>         // Provides read(), used when reading clients messages
#include <netdb.h>          // Provides socket structs like addr_info
#include <openssl/md5.h>    // Provides methods needed to create MD5 hash

#define BUFFSIZE 1024
#define SERVNUM 4

int global_server_err[] = {0,0,0,0};

// === Structs ===
struct dfs_file {
    char* part[4];
    char name[BUFFSIZE];
    int part_size[4];
};

// === Debugging methods ===
// Wrapper for printf that appends a newline
void debug(char const *msg) {
    printf("%s\n", msg);
}

// === Basic Methods ===
// Quits if invalid number of parameters are specified
void checkForParameters(int argc, char** argv) {
    if (argc != 1) {
        fprintf(stderr, "usage: %s\n", argv[0]);
        exit(1);
    }
}

// === String Manipulation Methods ===
// Compares two strings and returns *1* if they are the same
int areEqual(char* str1, char* str2) {
    return strcmp(str1, str2) == 0;
}
// Modify string to be itself until a delimiter; return the remaining string
char* getToken(char* str, char delim) {
    char* ptr = strchr(str, delim);
    if(ptr == NULL) {
        return NULL;
    }
    *ptr = '\0';
    return ptr+1;
}
// Converts a string to its MD5 hash
// Thanks Todd! - https://stackoverflow.com/questions/7627723/how-to-create-a-md5-hash-of-a-string-in-c
char* str2md5(const char* str, long length) {
    int n;
    MD5_CTX c;
    unsigned char digest[16];
    char *out = (char*)malloc(33);
    MD5_Init(&c);
    while (length > 0) {
        if (length > 512) {
            MD5_Update(&c, str, 512);
        } else {
            MD5_Update(&c, str, length);
        }
        length -= 512;
        str += 512;
    }
    MD5_Final(digest, &c);
    for (n = 0; n < 16; ++n) {
        snprintf(&(out[n*2]), 16*2, "%02x", (unsigned int)digest[n]);
    }
    return out;
}
// Converts a single hex character to an int
// Thanks Paul! - https://stackoverflow.com/questions/26839558/hex-char-to-int-conversion
int hex2int(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}

// === Network Methods ===
int setupClientSocket(char* server_port) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    char* server_ip = "127.0.0.1";
    getaddrinfo(server_ip, server_port, &hints, &res);
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(sockfd, res->ai_addr, res->ai_addrlen)) {
        //debug("Error on connection!");
        //perror("err");
        //exit(1);
        int server_id = atoi(server_port)-10001;
        global_server_err[server_id] = 1;
    }
    return sockfd;
}

// === Core Component Methods ===
// Opens file and gets contents and size
void getFile(char* filename, long* file_content_size, char** file_content) {
    FILE* f = fopen(filename, "rb");
    if (f) {
        fseek (f, 0, SEEK_END);
        (*file_content_size) = ftell(f);
        fseek (f, 0, SEEK_SET);
        (*file_content) = calloc(1, *file_content_size);
        if (*file_content) {
            fread(*file_content, 1, *file_content_size, f);
        }
        fclose (f);
    }
    else {
        debug("Error opening file!\n");
        exit(1);
    }
}
// Writes file given the mighty DFS file array
void writeFile(struct dfs_file* dfs_files, int file_count, char* filename) {
    FILE* f;
    f = fopen(filename, "wb" );
    if (f == NULL) {
        debug("Error opening file to write to!");
        exit(1);
    }
    int i;
    for(i = 0; i < file_count; i++) {
        if(areEqual(dfs_files[i].name, filename)) {
            // First check that all parts are here
            int j;
            for (j = 0; j < 4; j++) {
                if (dfs_files[i].part[j] == NULL) {
                    debug("Parts of file are missing!");
                    fclose(f);
                    return;
                }
            }
            // Write parts to file!
            for (j = 0; j < 4; j++) {
                fwrite(dfs_files[i].part[j], 1, dfs_files[i].part_size[j], f);
            }
            debug("File written!");
            fclose(f);
            return;
        }
    }
    debug("File not found!");
    fclose(f);
}
// Creates proper message when the command is "list"
void handleListAndGet(int* server_fd, char* file_needed) {
    // Create array of files
    int file_count = 0;
    struct dfs_file* dfs_files = malloc(file_count * sizeof(struct dfs_file));

    // Begin acquiring files
    int i;
    for(i = 0; i < SERVNUM; i++) {
        // Don't wait on a server that never started!
        if(global_server_err[i]) {
            continue;
        }

        char *recv_buffer = calloc(1, BUFFSIZE);
        // Get first directory item
        read(server_fd[i], recv_buffer, BUFFSIZE);
        // Continue getting more directory items until done message received
        while (!areEqual(recv_buffer, "done")) {
            char *filename = calloc(1, BUFFSIZE);
            char *filepart = calloc(1, BUFFSIZE);
            char *partsize = calloc(1, BUFFSIZE);

            // Handle file name
            read(server_fd[i], filename, BUFFSIZE);
            read(server_fd[i], filepart, BUFFSIZE);
            read(server_fd[i], partsize, BUFFSIZE);
            // Get numeric versions of numeric components
            int filepart_int = atoi(filepart)-1;
            int partsize_int = atoi(partsize);
            char *file_content = calloc(1, partsize_int);
            read(server_fd[i], file_content, partsize_int);

            //debug(filename);
            //debug(filepart);
            //debug(partsize);
            //debug(file_content);

            // Check if in DFS file array
            int j;
            int file_index = -1;
            for(j = 0; j < file_count; j++) {
                if(areEqual(dfs_files[j].name, filename)) { // Does this file exist in our DFS array?
                    file_index = j;
                    break;
                }
            }
            if(file_index != -1) {
                dfs_files[file_index].part[filepart_int] = file_content;
                dfs_files[file_index].part_size[filepart_int] = partsize_int;
            }
            else {
                file_count++;
                dfs_files = realloc(dfs_files, file_count * sizeof(struct dfs_file));
                strcpy(dfs_files[file_count-1].name, filename);
                dfs_files[file_count-1].part[0] = NULL;
                dfs_files[file_count-1].part[1] = NULL;
                dfs_files[file_count-1].part[2] = NULL;
                dfs_files[file_count-1].part[3] = NULL;
                dfs_files[file_count-1].part[filepart_int] = file_content;
                dfs_files[file_count-1].part_size[filepart_int] = partsize_int;
            }

            free(filename);
            free(filepart);
            free(partsize);
            //free(file_content); - Don't free this! dfs_files points to this!

            read(server_fd[i], recv_buffer, BUFFSIZE); // Read 'done' or 'not done'
        }
        free(recv_buffer);
        //debug(""); // Print a new line between servers' lists
    }

    // List files if no file requested (list command)
    if(file_needed == NULL) {
        debug("Directory Items:");
        for(i = 0; i < file_count; i++) {
            int all_parts = 1;
            int j;
            for(j = 0; j < 4; j++) {
                if(dfs_files[i].part[j] == NULL) {
                    all_parts = 0;
                }
            }
            printf("%s", dfs_files[i].name);
            if(!all_parts) {
                printf("\t[incomplete]");
            }
            debug(""); // Newline
        }
        //debug(""); // Extra space after listing files
    }
    // Write file if file WAS requested (get command)
    else {
        writeFile(dfs_files, file_count, file_needed);
    }

    // If needed can list status of DFS array
    /*debug("Files currently in DFS array:");
    for(i = 0; i < file_count; i++) {
        debug(dfs_files[i].name);
        int j;
        for(j = 0; j < 4; j++) {
            if(dfs_files[i].part[j] != NULL) {
                debug(dfs_files[i].part[j]);
            }
            else {
                debug("Null!");
            }
        }
    }*/
}
// Sends requested file to servers when command is "put"
void handlePut(int* server_fd, char* file_to_send) {
    // Decide how to cut up file
    long file_content_size;
    char* file_content;
    getFile(file_to_send, &file_content_size, &file_content);
    char* hash = str2md5(file_content, file_content_size);
    char last_hex = hash[strlen(hash) - 1];
    int v = hex2int(last_hex) % 4;
    //debug(hash);
    //printf("%c\n", last_hex);
    //printf("variant: %d\n", v);

    // Cut up, sticking the remainder into section 4
    long remainder = file_content_size%4;
    long part_size = file_content_size/4;
    char *part1 = calloc(1, part_size);
    char *part2 = calloc(1, part_size);
    char *part3 = calloc(1, part_size);
    char *part4 = calloc(1, part_size + remainder);
    char* parts[] = {part1, part2, part3, part4};

    //printf("%ld\n", file_content_size);
    //printf("remainder: %ld\n", remainder);
    //printf("%ld\n", part_size);

    // Fill part buffers
    // strncpy(part1, src + beginIndex, endIndex - beginIndex);
    strncpy(part1, file_content, part_size);
    strncpy(part2, file_content + part_size, part_size);
    strncpy(part3, file_content + 2*part_size, part_size);
    strncpy(part4, file_content + 3*part_size, part_size + remainder);

    int i;
    for(i = 0; i < SERVNUM; i++) {
        // Figure out which parts we're sending to this server
        int p1 = (4 + i - v) %4;
        int p2 = (5 + i - v) %4;

        // Fill & send buffers for PART 1!
        // Allocate the buffers to send components
        char *filepart = calloc(1, BUFFSIZE);
        char *partsize = calloc(1, BUFFSIZE);
        sprintf(filepart, "%d", p1+1);
        long true_part_size;
        if(p1 == 3) {
            true_part_size = part_size + remainder;
        }
        else {
            true_part_size = part_size;
        }
        sprintf(partsize, "%ld", true_part_size);
        // Send the components and the part!
        write(server_fd[i], filepart, BUFFSIZE);
        write(server_fd[i], partsize, BUFFSIZE);
        write(server_fd[i], parts[p1], true_part_size);
        // Free our buffers!
        free(filepart);
        free(partsize);

        // Fill & send buffers for PART 2!
        // Allocate the buffers to send components
        filepart = calloc(1, BUFFSIZE);
        partsize = calloc(1, BUFFSIZE);
        sprintf(filepart, "%d", p2+1);
        if(p2 == 3) {
            true_part_size = part_size + remainder;
        }
        else {
            true_part_size = part_size;
        }
        sprintf(partsize, "%ld", true_part_size);
        // Send the components and the part!
        write(server_fd[i], filepart, BUFFSIZE);
        write(server_fd[i], partsize, BUFFSIZE);
        write(server_fd[i], parts[p2], true_part_size);
        // Free our buffers!
        free(filepart);
        free(partsize);
    }

    free(part1);
    free(part2);
    free(part3);
    free(part4);
}
// Allows user to log in
int validLogin(char** username, size_t* username_size) {
    // Get input
    debug("Input username & password: ");
    getline(username, username_size, stdin);
    getToken((*username), '\n');  // Strip newline!
    char* password = getToken((*username), ' ');
    // Open password file
    FILE* f = fopen("dfc_passwords.conf", "r");
    if (f) {
        char * line = NULL;
        size_t len = 0;
        while ((getline(&line, &len, f)) != -1) {
            getToken(line, '\n');  // Strip newline!
            char* f_username = calloc(1, strlen(line));
            char* f_password;
            strcpy(f_username, line);
            f_password = getToken(f_username, ' ');
            if(areEqual(f_username, *username) && areEqual(f_password, password)) {
                fclose (f);
                debug("Login successful!");
                return 1;
            }
        }
        fclose (f);
        debug("Username & password combination not found!");
        return 0;
    }
    else {
        debug("Error opening password file!\n");
        exit(1);
    }
}


// ===== MAIN METHOD =====
int main(int argc, char **argv) {
    // Handle params
    checkForParameters(argc, argv);

    // User input variables
    char* username = NULL;
    size_t username_size;
    char* user_input = NULL;
    size_t user_input_size;

    // Have user login
    int valid = 0;
    while(!valid) {
        valid = validLogin(&username, &username_size);
    }

    while(1) {
        // Setup sockets to listen
        //int server_fd[] = {setupClientSocket("10001")};//, setupClientSocket("10002"), setupClientSocket("10003"), setupClientSocket("10004")};
        int server_fd[] = {setupClientSocket("10001"), setupClientSocket("10002"), setupClientSocket("10003"), setupClientSocket("10004")};

        // Allocate main buffers used for communication with servers
        char* send_buffer = malloc(BUFFSIZE);
        char* reci_buffer = malloc(BUFFSIZE);

        // Get input
        debug("\nInput command: ");
        getline(&user_input, &user_input_size, stdin);
        getToken(user_input, '\n');  // Strip newline!

        // Make sure input contains a valid command before sending!
        // Get copy of input and parse it
        char* user_input_copy = malloc(user_input_size);
        strcpy(user_input_copy, user_input);
        char* filename = getToken(user_input_copy, ' ');
        // In the event of list, there is no second filename; Append a dummy one to avoid a null pointer
        if(filename == NULL) {
            filename = "null_filename";
        }

        // Valid command?
        if(areEqual(user_input_copy,"list") || areEqual(user_input_copy,"get") || areEqual(user_input_copy,"put")) {
            // Send the command!
            int i;
            for(i = 0; i < SERVNUM; i++) {
                strcpy(send_buffer, user_input_copy); // Contains command
                write(server_fd[i], send_buffer, BUFFSIZE);
                strcpy(send_buffer, filename); // Contains filename
                write(server_fd[i], send_buffer, BUFFSIZE);
                strcpy(send_buffer, username); // Contains username
                write(server_fd[i], send_buffer, BUFFSIZE);
            }
            //debug(user_input);

            // Handle individual commands
            if(areEqual(user_input_copy,"list")) {
                handleListAndGet(server_fd, NULL);
            }
            else if(areEqual(user_input_copy,"get")) {
                handleListAndGet(server_fd, filename);
            }
            else {
                handlePut(server_fd, filename);
            }
        }
        else {
            debug("Invalid input. Try list, put, or get");
        }

        // Free main buffers used for communication with servers
        free(send_buffer);
        free(reci_buffer);
        free(user_input_copy);
    }
    free(user_input);
    free(username);
    return 250;
}