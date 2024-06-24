#define _XOPEN_SOURCE 500 // Define _XOPEN_SOURCE macro with value 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h> // Header file for socket programming
#include <netinet/in.h> // Header file for internet address family
#include <sys/types.h> // Header file for various system data types
#include <sys/wait.h> // Header file for process-related functions
#include <sys/stat.h> // Header file for file status information
#include <ftw.h> // Header file for file tree traversal functions
#include <limits.h> // Header file for system limits
#include <time.h> // Header file for time-related functions


#define PORT 8089  //Port number for mirror2

static int connection_count = 0; // counter for keeping track of client

static long g_size1, g_size2;
static time_t g_date;

// This function filters and tars files based on their size.
static int filterAndTarBySize(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
// Checking if the current item is a regular file.
    if (typeflag == FTW_F) {
        long filesize = sb->st_size;
        // Check if the file size falls within the specified range.
        if (filesize >= g_size1 && filesize <= g_size2) {
            printf("File within size range: %s\n", path);
        }
    }
    return 0;
}

// This function filters and tars files based on their modification date.
static int filterAndTarByDate(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {
        if (difftime(sb->st_mtime, g_date) > 0) {
            printf("File modified after given date: %s\n", path);
        }
    }
    return 0;
}


// Sends a file to the client connected to the provided socket.
void send_file_to_client(int new_socket, const char *file_path) {
// Open the file in binary mode for reading.
    FILE *file = fopen(file_path, "rb");
    char buffer[1024];
    ssize_t bytes_read;

    if (file == NULL) {
        perror("File open error");
        return;
    }

    // Notify the client that file transfer is starting
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "\n");
    send(new_socket, buffer, strlen(buffer), 0);

    // Transfer the file content
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    // Send the content of the buffer to the client.
        ssize_t sent = send(new_socket, buffer, bytes_read, 0);
        // Check for any errors that occurred during the transfer.
        if (sent == -1) {
            perror("send error while transferring file");
            break;
        }
    }
    fclose(file); // Close the file after sending its content

    // After completing the file transfer, send the end-of-file marker
    const char *endOfFileMarker = "END_OF_FILE_TRANSFER";
    send(new_socket, endOfFileMarker, strlen(endOfFileMarker), 0);

    // Optionally, log the completion of the file transfer
    printf("File '%s' sent successfully. End-of-file marker sent.\n", file_path);
}

// This function handles client requests received on the socket.
void crequest(int new_socket) {
    char buffer[1024] = {0}; // Initialize buffer to store received data.
    char command_output[8192]; // Buffer to store command output.
    FILE *fp; // File pointer for command output.

    while (1) {
        memset(buffer, 0, sizeof(buffer)); // Clear buffer before reading.
        int valread = read(new_socket, buffer, 1024); // Read data from socket.
        if (valread < 1) {
            printf("Child process %d: Client disconnected or read error\n", getpid());
            break;
        }

        printf("Child process %d: Received command: '%s'\n", getpid(), buffer);

   // If the received command is 'quitc', close connection and exit loop.
        if (strcmp(buffer, "quitc") == 0) {
            printf("Child process %d: 'quitc' command received, closing connection.\n", getpid());
            break;  
        } else if (strcmp(buffer, "dirlist -a") == 0) {
         // If the command is 'dirlist -a', list directories in alphabetical order.
            chdir(getenv("HOME"));
            // Execute command to list directories in alphabetical order
fp = popen("ls -1A | while read line; do if [ -d \"$line\" ]; then echo $line; fi; done", "r");
if (fp == NULL) {
    printf("Failed to run command\n"); // Print error message if command execution fails
    strcpy(command_output, "Failed to list directories\n"); // Set error message for command output
} else {
    strcpy(command_output, "Directory list in alphabetical order:\n"); // Set header for command output
    while (fgets(buffer, sizeof(buffer), fp) != NULL) { // Read output of command
        strcat(command_output, buffer); // Append output to command output string
    }
    pclose(fp); // Close pipe to command
}
write(new_socket, command_output, strlen(command_output)); // Send command output to client

// Execute command to list directories in time order
} else if (strcmp(buffer, "dirlist -t") == 0) {
    chdir(getenv("HOME")); // Change directory to user's home directory
    fp = popen("ls -ltr | grep ^d", "r"); // Execute command to list directories with time information
    if (fp == NULL) {
        printf("Failed to run command\n"); // Print error message if command execution fails
        strcpy(command_output, "Failed to list directories\n"); // Set error message for command output
    } else {
        strcat(command_output, "Directory list in time order:\n"); // Set header for command output
        while (fgets(buffer, sizeof(buffer), fp) != NULL) { // Read output of command
            strcat(command_output, buffer); // Append output to command output string
        }
        pclose(fp); // Close pipe to command
    }
    send(new_socket, command_output, strlen(command_output), 0); // Send command output to client
}
         else if (strncmp(buffer, "w24fn ", 6) == 0) {
            // If the command starts with 'w24fn', search for a file.
            char* filename = buffer + 6; // Extract filename from the command.
            char find_command[1024]; // Buffer to store the find command.
            chdir(getenv("HOME")); // Change directory to user's home directory.
            snprintf(find_command, sizeof(find_command), "find . -maxdepth 3 \\( -path './node_modules' -prune \\) -o -type f -name '%s' -print | head -1 | xargs -I {} stat --format='%%n %%s %%y %%A' {}", filename);

            fp = popen(find_command, "r");
            if (fp == NULL) {
                printf("Failed to run command\n");
                strcpy(command_output, "Failed to execute file search\n");
            } else {
                if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                    strncpy(command_output, buffer, sizeof(command_output) - 1);
                    command_output[sizeof(command_output) - 1] = '\0'; // Ensure null-termination
                } else {
                    strncpy(command_output, "File not found\n", sizeof(command_output) - 1);
                }
                pclose(fp);
            }
            write(new_socket, command_output, strlen(command_output));  // Send back the result
        }
        
else if (strncmp(buffer, "w24fz ", 6) == 0) {
    char tempTarPath[] = "/tmp/temp.tar.gz";  // Temporary file path for tar
    char destDir[PATH_MAX]; // Destination directory for the tar file
    char destPath[PATH_MAX]; // Full destination path including the file name

    // Extract the first and second size parameters from the command
    char *token = strtok(buffer + 6, " ");
    long size1 = strtol(token, NULL, 10); // First size parameter
    token = strtok(NULL, " ");
    long size2 = strtol(token, NULL, 10); // Second size parameter

    if (size1 <= size2 && size1 >= 0 && size2 >= 0) {
        // Forming the command to find and tar files within the specified size range
        char find_command[512];
        // Ensure the temp file does not already exist or can be overwritten
        unlink(tempTarPath);  // Attempt to delete the old file if it exists
        sprintf(find_command, "find ~ -type f -size +%ldc -size -%ldc -print0 | tar --null -czvf %s -T -", size1, size2, tempTarPath);

        // Execute the command and check for success
        if (system(find_command) != -1) {
            // Check if the tar file is created and has content
            struct stat st;
            if (stat(tempTarPath, &st) == 0 && st.st_size > 0) {
                printf("Tar file created at: %s\n", tempTarPath);

                // Prepare the destination directory and path
                snprintf(destDir, sizeof(destDir), "%s/w24project", getenv("HOME"));
                mkdir(destDir, 0775);  // Create the directory if it does not exist

                snprintf(destPath, sizeof(destPath), "%s/temp.tar.gz", destDir); // Correct the destination path construction

                // At this point, the directory is guaranteed to exist, so you can move the tar file safely
                char move_command[PATH_MAX + 50];
                sprintf(move_command, "mv %s %s", tempTarPath, destPath);
                if (system(move_command) == -1) {
                    perror("Error moving tar file to client directory");
                    send(new_socket, "Error moving tar file to client directory.\n", 42, 0);
                } else {
                    printf("Tar file successfully moved to: %s\n", destPath);
                    send_file_to_client(new_socket, destPath); // Send the file from its final location
                }
            } else {
                send(new_socket, "No file found\n", 14, 0);
            }
        } else {
            perror("Error executing find and tar command");
            send(new_socket, "Error creating tar.gz file.\n", 28, 0);
        }
    } else {
        send(new_socket, "Invalid size parameters.\n", 25, 0);
    }
}


else if (strncmp(buffer, "w24fda ", 7) == 0) {
    char tempTarPath[] = "/tmp/temp.tar.gz";  // Temporary file path for tar
    char destDir[PATH_MAX]; // Destination directory for the tar file
    char *destPath; // Full destination path including the file name
    char find_command[1024];  // Declaring find command string
    char *dateStr = buffer + 7;  // Extracting the date from the command
    // Validate the date format
    struct tm tm;
    if (strptime(dateStr, "%Y-%m-%d", &tm) != NULL) {
        char find_command[512];
        unlink(tempTarPath);  // Attempt to delete the old file if it exists

        // Correcting the date predicate to -newermt for find command
        sprintf(find_command, "find ~ -type f ! -name '.*' -newermt '%s' -print0 | tar --null -czvf %s -T -", dateStr, tempTarPath);

        // Execute the command and check for success
        if (system(find_command) != -1) {
            // Check if the tar file is created and has content
            struct stat st;
            if (stat(tempTarPath, &st) == 0 && st.st_size > 0) {
                printf("Tar file created at: %s\n", tempTarPath); // Log the creation

                // Prepare the destination directory and path
                snprintf(destDir, sizeof(destDir), "%s/w24project", getenv("HOME"));
                mkdir(destDir, 0775);  // Create the directory if it does not exist

                size_t destPathLen = strlen(destDir) + strlen("/temp.tar.gz") + 1;
                destPath = malloc(destPathLen);
                if (destPath == NULL) {
                    perror("Memory allocation failed");
                    exit(EXIT_FAILURE);
                }

                snprintf(destPath, destPathLen, "%s/temp.tar.gz", destDir); // Correct the destination path construction

                // Move the tar file to the client's w24project directory
                char move_command[PATH_MAX + 50];
                sprintf(move_command, "mv %s %s", tempTarPath, destPath);
                if (system(move_command) != -1) {
                    printf("Tar file moved to client's w24project directory.\n");
                    send_file_to_client(new_socket, destPath); // Now send from the final location
                } else {
                    perror("Error moving tar file to client directory");
                    send(new_socket, "Error moving tar file to client directory.\n", 42, 0);
                }

                free(destPath); // Free dynamically allocated memory
            } else {
                send(new_socket, "No file found\n", 14, 0);
            }
        } else {
            perror("Error executing find and tar command");
            send(new_socket, "Error creating tar.gz file.\n", 28, 0);
        }
    } else {
        // Send error message if the date format is incorrect
        send(new_socket, "Invalid date format. Use YYYY-MM-DD.\n", 36, 0);
    }
}



else if (strncmp(buffer, "w24fdb ", 7) == 0) {
    char tempTarPath[] = "/tmp/temp.tar.gz";  // Temporary file path for tar
    char destDir[PATH_MAX]; // Destination directory for the tar file
    char find_command[1024];  // Find command string
    char *dateStr = buffer + 7;  // Extracting the date from the command
    struct tm tm;

    if (strptime(dateStr, "%Y-%m-%d", &tm) != NULL) {
        tm.tm_sec = tm.tm_min = tm.tm_hour = 23;  // Set time to end of the day
        tm.tm_isdst = -1; // Not setting DST
        char formattedDate[100];
        strftime(formattedDate, sizeof(formattedDate), "%Y-%m-%d", &tm);

        unlink(tempTarPath);  // Remove old tar file if it exists

        // Construct the find command to capture files modified on or before the specified date
        sprintf(find_command, "find ~ -type f ! -name '.*' -not -newermt '%s 23:59' -print0 | tar --null -czvf %s -T -", formattedDate, tempTarPath);

        if (system(find_command) != -1) {
            struct stat st;
            if (stat(tempTarPath, &st) == 0 && st.st_size > 0) {
                snprintf(destDir, sizeof(destDir), "%s/w24project", getenv("HOME"));
                mkdir(destDir, 0775);

                char destPath[PATH_MAX];
                snprintf(destPath, sizeof(destPath), "%s/temp.tar.gz", destDir);

                char move_command[1024];
                snprintf(move_command, sizeof(move_command), "mv %s %s", tempTarPath, destPath);
                if (system(move_command) == -1) {
                    perror("Error moving tar file to client directory");
                    send(new_socket, "Error moving tar file to client directory.\n", 42, 0);
                } else {
                    printf("Tar file moved to client's w24project directory.\n");
                    send_file_to_client(new_socket, destPath);  // Ensure this function handles errors properly
                }
            } else {
                send(new_socket, "No file found or empty tar file.\n", 32, 0);
            }
        } else {
            perror("Error executing find and tar command");
            send(new_socket, "Error creating tar.gz file.\n", 28, 0);
        }
    } else {
        send(new_socket, "Invalid date format. Use YYYY-MM-DD.\n", 36, 0);
    }
}




else if (strncmp(buffer, "w24ft ", 6) == 0) {
    char tempTarPath[] = "/tmp/temp.tar.gz";  // Temporary file path for tar
    char find_command[1024];  // Buffer for the command

    // Prepare the find command with proper escaping
    snprintf(find_command, sizeof(find_command),
        "find ~/ -type f \\( -iname '*.pdf' -o -iname '*.c' -o -iname '*.txt' \\) -print0 | tar --null -czvf %s -T -",
        tempTarPath);

    printf("Executing command: %s\n", find_command);

    // Execute the find and tar command
    if (system(find_command) != -1) {
        struct stat st;
        if (stat(tempTarPath, &st) == 0 && st.st_size > 0) {
            printf("Tar file created at: %s, size: %ld bytes\n", tempTarPath, st.st_size);

            char destDir[PATH_MAX];
            snprintf(destDir, sizeof(destDir), "%s/w24project", getenv("HOME"));
            mkdir(destDir, 0775);

            char destPath[PATH_MAX];
            snprintf(destPath, sizeof(destPath), "%s/temp.tar.gz", destDir);

            char move_command[1024];
            snprintf(move_command, sizeof(move_command), "mv %s %s", tempTarPath, destPath);
            if (system(move_command) == -1) {
                perror("Error moving tar file to client directory");
                send(new_socket, "Error moving tar file to client directory.\n", 42, 0);
            } else {
                printf("Tar file successfully moved to: %s\n", destPath);
                send_file_to_client(new_socket, destPath); // Send the file from its final location
            }
        } else {
            send(new_socket, "No files found matching the criteria or empty tar file.\n", 55, 0);
        }
    } else {
        perror("Error executing find and tar command");
        send(new_socket, "Error creating tar.gz file.\n", 28, 0);
    }
}

  // Add other command handling as necessary
        else {
            strcpy(command_output, "Unknown command\n");
            write(new_socket, command_output, strlen(command_output));
        }
    }

    // Close the socket at the end of handling
    close(new_socket);
}



// Redirect client to a new port
void redirect_client(int client_sock, int port) {
    char message[1024]; // Buffer to store redirect message
    snprintf(message, sizeof(message), "REDIRECT %d", port); // Format redirect message
    send(client_sock, message, strlen(message), 0); // Send redirect message to client
    close(client_sock); // Close client socket
}



int main() {
    int server_fd, new_socket; // Socket file descriptors
    struct sockaddr_in address; // Server address structure
    int addrlen = sizeof(address); // Size of address structure
    pid_t child_pid; // Process ID of child process

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed"); // Print error message if socket creation fails
        exit(EXIT_FAILURE); // Exit program with failure status
    }

    // Define the server address structure
    address.sin_family = AF_INET; // Address family is Internet
    address.sin_addr.s_addr = INADDR_ANY; // Accept connections from any IP address
    address.sin_port = htons(PORT); // Set port number

    // Binding the socket to the server address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed"); // Print error message if binding fails
        exit(EXIT_FAILURE); // Exit program with failure status
    }

    // Listen for incoming connections
    if (listen(server_fd, 10) < 0) {
        perror("listen"); // Print error message if listen fails
        exit(EXIT_FAILURE); // Exit program with failure status
    }

    printf("Server is waiting for connections on port %d...\n", PORT); // Print server status

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept"); // Print error message if accept fails
            continue;  // Continue to the next iteration of the loop
        }

        // Decide where to process the request based on the connection count
        child_pid = fork(); // Create a child process
        if (child_pid == 0) {  // Child process
            printf("Child process %d: Handling connection.\n", getpid()); // Print child process handling connection

            // Close the parent's socket descriptor
            close(server_fd);

            // Handle the client's request
            crequest(new_socket);

            printf("Child process %d: Connection handler terminating.\n", getpid()); // Print child process termination
            exit(0);  // Ensure child process exits after handling
        } else if (child_pid > 0) { // Parent process
            // Parent process doesn't need the new socket
            close(new_socket); // Close the new socket
        } else {
            // Fork failed
            perror("fork"); // Print error message if fork fails
            close(new_socket);  // Close the new socket since fork failed
        }
    }
    return 0; // Exit main function
}
