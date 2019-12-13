#include <stdio.h>
#include <stdlib.h>     // Provides standard functions like exit() & atoi()
#include <string.h>     // Provides string functions like strcmp()
#include <sys/socket.h> // Provides socket functions
#include <netinet/in.h> // Provides socket structs like sockaddr_in
#include <unistd.h>     // Provides read(), used when reading clients messages
#include <sys/ioctl.h>
#include <dirent.h>     // Provides directory reading capabilities
#include <sys/stat.h>

#define BUFFSIZE 1024


// === Debugging methods ===
// Wrapper for printf that appends a newline
void debug(char const *msg) {
    printf("%s\n", msg);
}

// === Basic Methods ===
// Quits if invalid number of parameters are specified - For server (this file), converts returns port num as int
int checkForParameters(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <directory name> <port #>\n", argv[0]);
        exit(1);
    }
    return atoi(argv[2]);
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

// === Network Methods ===
void setupServerSocket(int port, struct sockaddr_in* server_address, int* server_fd) {
    *server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if ((*server_fd) < 0) {
        debug("Error opening socket!");
        exit(1);
    }
    int optval;
    setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int));
    server_address->sin_family = AF_INET;
    server_address->sin_addr.s_addr = INADDR_ANY;
    server_address->sin_port = htons(port);
    if (bind(*server_fd, (struct sockaddr *) server_address, sizeof(*server_address)) < 0) {
        debug("Error on bind!");
        exit(1);
    }
    // Listen to socket and create socket to reply on
    if (listen(*server_fd, 100) < 0) {
        debug("Error on listen!");
        exit(1);
    }
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
// Creates proper message when the command is "list"
// Writes a file given a file component
void writeFile(char* filename, char* file_data, int file_size) {
    FILE* f;
    f = fopen(filename, "wb" );
    if (f == NULL) {
        debug("Error opening file to write to!");
        fclose(f);
        exit(1);
    }
    fwrite(file_data, 1, file_size, f);
    fclose(f);
}
void handleListAndGet(int client_fd, char* dirname) {
    DIR *directory;
    struct dirent *dirStruct;
    directory = opendir(dirname);
    if (directory != NULL) {
        while ((dirStruct = readdir(directory)) != NULL) {
            if (!areEqual(dirStruct->d_name, ".") && !areEqual(dirStruct->d_name, "..")) {
                // For every file, send its core components and the file itself
                char *temp_filename = calloc(1, BUFFSIZE);
                char *temp_dirname = calloc(1, BUFFSIZE);
                char *filename = calloc(1, BUFFSIZE);
                char *filepart = calloc(1, BUFFSIZE);
                char *partsize = calloc(1, BUFFSIZE);

                // Find required components
                strcpy(temp_filename, dirStruct->d_name);
                temp_filename = getToken(temp_filename, '.');       // Remove dot in front of filename
                char *temp_filepart = getToken(temp_filename, ','); // Separate filename from the file part
                strcat(filename, temp_filename);
                strcat(filepart, temp_filepart);
                long file_content_size;
                char *file_content;
                strcat(temp_dirname, dirname);
                strcat(temp_dirname, dirStruct->d_name);
                getFile(temp_dirname, &file_content_size, &file_content);
                sprintf(partsize, "%lu", file_content_size);

                // Send components
                write(client_fd, "not done", BUFFSIZE);
                write(client_fd, filename, BUFFSIZE);
                write(client_fd, filepart, BUFFSIZE);
                write(client_fd, partsize, BUFFSIZE);
                write(client_fd, file_content, file_content_size);

                free(temp_filename-1);
                free(temp_dirname);
                free(filename);
                free(filepart);
                free(partsize);
            }
        }
        // Send message indicating that we are done listing
        char* send_buffer = calloc(1, BUFFSIZE);
        strcpy(send_buffer, "done");
        write(client_fd, send_buffer, BUFFSIZE);
        free(send_buffer);
        closedir(directory);
    }
}
// Receives file and writes it if the command is put
void handlePut(int client_fd, char* dirname, char* filename) {
    int i;
    for(i = 0; i < 2; i++) {
        // Generate buffers to hold components
        char *filepart = calloc(1, BUFFSIZE);
        char *partsize = calloc(1, BUFFSIZE);

        // Read in components
        read(client_fd, filepart, BUFFSIZE);
        read(client_fd, partsize, BUFFSIZE);
        // Use partsize to create buffer large enough to store file data and read it
        int partsize_int = atoi(partsize);
        char *partdata = calloc(1, partsize_int);
        read(client_fd, partdata, partsize_int);

        printf("%s: Part    #: %s\n", dirname, filepart);
        printf("%s: Part Size: %s\n", dirname, partsize);
        printf("%s: Part Data: %s\n", dirname, partdata);

        // Compile the correct filename
        char* true_filename = calloc(1, BUFFSIZE);
        strcpy(true_filename, dirname);
        strcat(true_filename, ".");
        strcat(true_filename, filename);
        strcat(true_filename, ",");
        strcat(true_filename, filepart);

        // Write data to file
        writeFile(true_filename, partdata, atoi(partsize));

        free(filepart);
        free(partsize);
        free(partdata);
    }
}
// Interprets client request and hands it off to each command's method
void handleRequest(int client_fd, char* server_name) {
    // Start by reading BUFFSIZE bytes; Assuming command, filename, and username all fit into BUFFSIZE
    char* reci_buffer = calloc(1, BUFFSIZE);
    char* command = calloc(1, BUFFSIZE);
    char* filename = calloc(1, BUFFSIZE);
    char* username = calloc(1, BUFFSIZE);
    char* dirname = calloc(1, BUFFSIZE);

    // Get components one at a time
    read(client_fd, reci_buffer, BUFFSIZE);
    strcpy(command, reci_buffer);
    read(client_fd, reci_buffer, BUFFSIZE);
    strcpy(filename, reci_buffer);
    read(client_fd, reci_buffer, BUFFSIZE);
    strcpy(username, reci_buffer);
    // Construct full directory name
    strcpy(dirname, server_name);
    strcat(dirname, "/");
    strcat(dirname, username);
    strcat(dirname, "/");
    // Make directory for user if doesn't exist
    mkdir(dirname, 0777);

    debug("Command & filename:");
    debug(command);
    debug(filename);
    debug(username);
    debug(dirname);

    if(areEqual(command, "list") || areEqual(command, "get")) {
        handleListAndGet(client_fd, dirname);
    }
    else {
        handlePut(client_fd, dirname, filename);
    }

    free(reci_buffer);
    free(command);
    free(filename);
    free(username);
    free(dirname);
}

// ===== MAIN METHOD =====
int main(int argc, char **argv) {
    // Handle params
    int port = checkForParameters(argc, argv);
    char* dirname = argv[1];

    // Setup socket to listen
    struct sockaddr_in server_address;
    int server_fd;
    setupServerSocket(port, &server_address, &server_fd);

    while(1) {
        // Setup client socket to reply on
        int client_fd;
        int server_address_size = sizeof(server_address);
        if ((client_fd = accept(server_fd, (struct sockaddr *)&server_address, (socklen_t*)&server_address_size)) < 0) {
            debug("Error on accept!");
            exit(1);
        }

        // Child will handle the new connection
        if(!fork()) {
        //if(1) {
            close(server_fd);           // Child need not deal with server connection socket
            handleRequest(client_fd, dirname);   // Interpret the data that came in
            close(client_fd);           // After we're done, we no longer need the response socket
            exit(0);             // Child finished all work
        }
        // Parent doesn't need response socket; Only handles further requests
        close(client_fd);
    }
}