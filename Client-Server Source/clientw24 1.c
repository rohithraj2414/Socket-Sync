#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>   // Helps us talk to other computers.
#include <netinet/in.h>   // Gives us addresses for the internet.
#include <sys/types.h>    // Defines basic system data types.
#include <sys/wait.h>     // Helps manage child processes.
#include <sys/stat.h>     // Provides information about files.
#include <ftw.h>          // Helps us explore directories and files.
#include <limits.h>       // Tells us the maximum length for file paths.
#include <time.h>         // Keeps track of time and dates.


#define SERVER_PORT 8084  // client port number matching main server port

// Function to receive a file from the server
void receive_file_from_server(int sock) {
    FILE *file; // File pointer
    char buffer[3024]; // Buffer for receiving data
    ssize_t bytes_received; // Number of bytes received

    // Create directory for storing the received file
    char w24projectDir[PATH_MAX];
    snprintf(w24projectDir, sizeof(w24projectDir), "%s/w24project", getenv("HOME"));
    mkdir(w24projectDir, 0775); // Create directory with permissions

    // Construct the file path for saving the received file
    char filePath[PATH_MAX];
    snprintf(filePath, sizeof(filePath), "%s/temp.tar.gz", w24projectDir);
    file = fopen(filePath, "wb"); // Open file for writing in binary mode
    if (!file) {
        perror("Failed to open file for writing"); // Print error if file opening fails
        return; // Return if file opening fails
    }

    int endOfFileMarkerFound = 0; // Flag to indicate if end-of-file marker is found

    // Loop until end-of-file marker is found
    while (!endOfFileMarkerFound) {
        // Receive data from the server
        memset(buffer, 0, sizeof(buffer)); // Clear buffer
        bytes_received = recv(sock, buffer, sizeof(buffer), 0); // Receive data
        if (bytes_received <= 0) { // Check for errors or closed connection
            printf("Server closed connection or recv error during file transfer.\n"); // Print error message
            break; // Exit loop if error occurs or connection is closed
        }

        // Check if end-of-file marker is found in the received data
        if (strstr(buffer, "END_OF_FILE_TRANSFER") != NULL) {
            endOfFileMarkerFound = 1; // Set flag to indicate end-of-file marker found
            fwrite(buffer, 1, strstr(buffer, "END_OF_FILE_TRANSFER") - buffer, file); // Write data to file until end-of-file marker
            printf("File transfer complete. File saved to '%s'\n", filePath); // Print message indicating file transfer completion
       
        } else {
        
            fwrite(buffer, 1, bytes_received, file); // Write received data to file
        
        }
    }

    fclose(file); // Close the file
    fflush(NULL); // Flush any buffered data
}


// Function to reconnect to a server with a new port
int reconnect(int *sock, int new_port, const char *last_command) {
    close(*sock); // Close the previous socket connection
    *sock = socket(AF_INET, SOCK_STREAM, 0); // Create a new socket
    if (*sock < 0) { // Check if socket creation failed
        perror("Socket creation error"); // Print error message
        return -1; // Return -1 to indicate failure
    }

    // Define server address structure
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr)); // Clear server address structure
    serv_addr.sin_family = AF_INET; // Set address family to IPv4
    serv_addr.sin_port = htons(new_port); // Set port number and convert to network byte order
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) { // Convert IP address to binary form
        printf("Invalid address/ Address not supported\n"); // Print error message
        return -1; // Return -1 to indicate failure
    }

    // Connect to the server
    if (connect(*sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
        perror("Connection Failed"); // Print error message
        return -1; // Return -1 to indicate failure
    }

    printf("Reconnected to port %d\n", new_port); // Print success message
    return 0; // Return 0 to indicate success
}


// Function to validate a date string
int validate_date(const char* date) {
    int year, month, day; // Variables to store year, month, and day

    // Attempt to parse the date string
    if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3) {
        printf("Error: Date must be in YYYY-MM-DD format.\n"); // Print error message if parsing fails
        return -1; // Return -1 to indicate failure
    }

    // Check if the parsed date components are valid
    if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31) {
        printf("Error: Invalid date provided.\n"); // Print error message if date components are invalid
        return -1; // Return -1 to indicate failure
    }

    // Get current local time
    time_t t_now; // Variable to store current time
    struct tm* tm_now; // Pointer to structure to store broken-down time
    time(&t_now); // Get current time
    tm_now = localtime(&t_now); // Convert to broken-down time

    // Create a time structure for the input date
    struct tm tm_input = {0}; // Initialize structure to all zeros
    tm_input.tm_year = year - 1900; // Set year (years since 1900)
    tm_input.tm_mon = month - 1; // Set month (0-11)
    tm_input.tm_mday = day; // Set day

    // Convert input date to time_t
    time_t t_input = mktime(&tm_input); // Convert structure to time_t
    // Check if input date is in the future
    if (difftime(t_input, t_now) > 0) {
        printf("Error: Greater than current date.\n"); // Print error message if input date is in the future
        return -1; // Return -1 to indicate failure
    }

    return 0; // Return 0 to indicate success
}


// Function to validate a command string
int validate_command(const char* command) {
    char tmp[3024]; // Temporary buffer to store command
    strcpy(tmp, command); // Copy command to temporary buffer
    char *tok = strtok(tmp, " "); // Tokenize command by space delimiter

    // Check command type and validate accordingly
    if (strcmp(tok, "w24ft") == 0) { // If command is "w24ft"
        int count = 0; // Counter for extensions
        while ((tok = strtok(NULL, " ")) != NULL) { // Iterate through remaining tokens
            count++; // Increment extension count
        }
        if (count > 3) { // If more than three extensions provided
            printf("Error: More than three extensions provided.\n"); // Print error message
            return -1; // Return -1 to indicate failure
        }
    } else if (strcmp(tok, "w24fz") == 0) { // If command is "w24fz"
        int count = 0; // Counter for sizes
        while ((tok = strtok(NULL, " ")) != NULL) { // Iterate through remaining tokens
            count++; // Increment size count
        }
        if (count > 2) { // If more than two sizes provided
            printf("Error: More than two sizes provided.\n"); // Print error message
            return -1; // Return -1 to indicate failure
        }
    } else if (strcmp(tok, "w24fda") == 0 || strcmp(tok, "w24fdb") == 0) { // If command is "w24fda" or "w24fdb"
        // Assuming similar date validation needed for these as previously handled
        if (strtok(NULL, " ") == NULL || strtok(NULL, " ") != NULL) { // Check date format and additional arguments
            printf("Error: Incorrect date format or extra arguments provided.\n"); // Print error message
            return -1; // Return -1 to indicate failure
        }
    } else if (strcmp(tok, "w24fn") == 0) { // If command is "w24fn"
        // Check if exactly one argument is provided after the command
        char* filename = strtok(NULL, " "); // Get filename token
        if (filename == NULL || strtok(NULL, " ") != NULL) { // Check for extra arguments
            printf("Error: Extra command provided. Usage: w24fn <filename>\n"); // Print error message
            return -1; // Return -1 to indicate failure
        }
    }

    return 0; // Return 0 to indicate success
}



// Main function
int main() {
    int sock = 0; // Socket file descriptor
    struct sockaddr_in serv_addr; // Server address structure

    while (1) {
        // Create socket
        sock = socket(AF_INET, SOCK_STREAM, 0); // Create socket using IPv4 and TCP
        if (sock < 0) { // Check if socket creation failed
            perror("Socket creation error"); // Print error message
            continue; // Continue to next iteration to retry
        }

        // Initialize server address structure
        memset(&serv_addr, 0, sizeof(serv_addr)); // Clear server address structure
        serv_addr.sin_family = AF_INET; // Set address family to IPv4
        serv_addr.sin_port = htons(SERVER_PORT); // Set port number and convert to network byte order
        if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) { // Convert IP address to binary form
            printf("\nInvalid address/ Address not supported\n"); // Print error message
            close(sock); // Close the socket
            continue; // Continue to next iteration to retry
        }

        // Connect to the server
        if (connect(sock, (struct sockaddr *)&serv_addr,  sizeof(serv_addr)) < 0) { // Connect to server
            perror("Connection Failed"); // Print error message
            close(sock); // Close the socket
            continue; // Continue to next iteration to retry
        }

        while (1) {
            printf("Enter command: "); // Prompt user to enter command
            char buffer[3024] = {0}; // Buffer for user input and server response
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                printf("Error reading command. Try again.\n");
                break; // If fgets fails, break out of this loop to reconnect
            }
            buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline character

            if (strlen(buffer) == 0) { // If the user just pressed enter without a command
                continue; // Prompt the user again without sending anything to the server
            }

            // Validate the command before sending
            if (validate_command(buffer) != 0) { // Check if command is valid
                continue; // Skip sending the command if validation fails
            }

            // Send the command to the server
            if (send(sock, buffer, strlen(buffer), 0) < 0) { // Send command to server
                perror("Send failed"); // Print error message
                break; // If send fails, break out of this loop to reconnect
            }

            // Handle the command that involves file transfer
            if (strncmp(buffer, "w24fda", 6) == 0 || strncmp(buffer, "w24fdb", 6) == 0 || strncmp(buffer, "w24fz", 5) == 0 || strncmp(buffer, "w24ft", 5) == 0) {
                receive_file_from_server(sock); // Start receiving file from the server
            } else {
                // Receive normal response from the server
                memset(buffer, 0, sizeof(buffer)); // Clear buffer
                int bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0); // Receive response from server
                if (bytes_read <= 0) { // Check for errors or closed connection
                    printf("Server closed connection or recv error.\n"); // Print error message
                    break; // If recv fails, break out of this loop to reconnect
                }
                // Check if the server is redirecting to a new port
            if (strncmp(buffer, "REDIRECT", 8) == 0) {
                int new_port;
                sscanf(buffer, "REDIRECT %d", &new_port);
                printf("Received redirect to port %d\n", new_port);
                close(sock); // Close the current socket
                if (reconnect(&sock, new_port, buffer) < 0) {
                    break; // Reconnect failed, break out of the send/receive loop
                }
            } else {
                printf("Response: %s\n", buffer);
            }
            }
        }
        close(sock); // Close the socket before reconnecting
    }
    return 0; // Return 0 to indicate successful execution
}
