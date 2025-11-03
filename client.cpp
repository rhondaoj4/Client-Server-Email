#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <limits> 

using namespace std;

// Define Ports (Must match server.cpp)
const char* SERVER_IP = "127.0.0.1";
const int SMTP_PORT = 2525;
const int POP3_PORT = 8110; 

// --- Protocol Helper Functions (Shared) ---

// Send a command to the server (adds \r\n)
void send_command(int sock, const string& command) {
    string full_command = command + "\r\n";
    send(sock, full_command.c_str(), full_command.length(), 0);
    // cout << "Client Sent: " << command << endl; // Commented out for cleaner UI
}

// Function to receive and print server response for general commands (SMTP/POP3)
string receive_response(int sock, bool check_dot = false) {
    char buffer[4096] = {0};
    string response = "";
    
    // Read data until disconnection or termination sequence is found
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received <= 0) break;
        
        string received_chunk(buffer, bytes_received);
        response += received_chunk;

        if (check_dot) {
            // POP3 message termination: look for "\r\n.\r\n" OR just ".\r\n" at the end
            if (response.find("\r\n.\r\n") != string::npos || 
                (response.length() >= 3 && response.substr(response.length() - 3) == ".\r\n")) {
                break;
            }
        } else {
            // For standard single-line responses, break after finding a newline
            if (response.find("\r\n") != string::npos) break;
        }
    }
    return response;
}

// --- SMTP Sending Logic ---

void send_mail(const string& sender_email) {
    int client_socket;
    struct sockaddr_in server_addr;
    
    // 1. Setup and Connect to SMTP
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) { cerr << "SMTP Socket creation failed" << endl; return; }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SMTP_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) { close(client_socket); return; }

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        cout << "\n--- ERROR: Could not connect to SMTP server on port " << SMTP_PORT << " ---" << endl;
        close(client_socket);
        return;
    }

    receive_response(client_socket); // 220 Greeting

    // 2. Get Input
    string recipient, subject, line;
    cout << "\n----------------------------------------\n";
    cout << "          COMPOSE NEW EMAIL\n";
    cout << "----------------------------------------\n";
    cout << "To: ";
    getline(cin, recipient);
    
    cout << "Subject: ";
    getline(cin, subject);

    cout << "Body (End with a single line containing only a dot '.'):\n";
    string body_content;
    
    while (getline(cin, line)) {
        if (line == ".") {
            break;
        }
        body_content += line + "\r\n"; 
    }

    // 3. SMTP Conversation
    send_command(client_socket, "HELO localhost"); receive_response(client_socket);
    send_command(client_socket, "MAIL FROM: <" + sender_email + ">"); receive_response(client_socket);
    send_command(client_socket, "RCPT TO: <" + recipient + ">"); receive_response(client_socket);
    send_command(client_socket, "DATA"); receive_response(client_socket);
    
    // Construct and send message block
    string full_email_message;
    full_email_message += "Subject: " + subject + "\r\n";
    full_email_message += "From: <" + sender_email + ">\r\n";
    full_email_message += "To: <" + recipient + ">\r\n";
    full_email_message += "\r\n"; 
    full_email_message += body_content; 
    
    
    // 4. Send Message Block
    send(client_socket, full_email_message.c_str(), full_email_message.length(), 0); 
    
    // 5. Send Termination Dot
    send_command(client_socket, ".");
    
    // 6. Wait for Final Server Response
    string final_response = receive_response(client_socket); 

    
    // Final check and cleanup
    if (final_response.find("250") != string::npos) {
        cout << "\n[SUCCESS] Message accepted for delivery to " << recipient << ".\n";
    } else {
        cout << "\n[ERROR] Message sending failed: " << final_response << endl;
    }

    send_command(client_socket, "QUIT"); receive_response(client_socket);
    close(client_socket);
}

// --- POP3 Receiving Logic ---

void display_mailbox(const string& user_email) {
    int client_socket;
    struct sockaddr_in server_addr;
    
    // 1. Setup and Connect to POP3
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) { cerr << "POP3 Socket creation failed" << endl; return; }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(POP3_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) { close(client_socket); return; }

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        cout << "\n--- ERROR: Could not connect to POP3 server on port " << POP3_PORT << " ---" << endl;
        close(client_socket);
        return;
    }
    receive_response(client_socket); // +OK Greeting

    // 2. POP3 Authentication (USER/PASS)
    send_command(client_socket, "USER " + user_email); 
    receive_response(client_socket);
    send_command(client_socket, "PASS");
    receive_response(client_socket); // Just consume the PASS response
    
    // 3. Send STAT command to get message count
    send_command(client_socket, "STAT");
    string stat_response = receive_response(client_socket);

    // 4. Parse STAT response for message count
    size_t start = stat_response.find(' ') + 1;
    size_t end = stat_response.find(' ', start);
    int num_messages = 0;
    if (start != string::npos && end != string::npos && stat_response.find("+OK") != string::npos) {
        try { num_messages = stoi(stat_response.substr(start, end - start)); } catch(...) { /* Error parsing */ }
    }
    
    cout << "\n========================================\n";
    cout << "         YOUR MAILBOX (" << user_email << ")\n";
    cout << "========================================\n";

    if (num_messages > 0) {
        cout << "[STATUS] You have " << num_messages << " new message(s).\n\n";

        // 5. Retrieve and display all messages
        for (int i = 1; i <= num_messages; i++) {
            cout << "--- Message " << i << " of " << num_messages << " ---\n";
            send_command(client_socket, "RETR " + to_string(i));
            string email_content = receive_response(client_socket, true);
            
            // Display the cleaned content - skip the +OK line and the terminating dot
            size_t content_start = email_content.find("\r\n");
            if (content_start != string::npos) {
                content_start += 2; // Skip the \r\n
                
                // Find the terminating sequence
                size_t content_end = email_content.rfind("\r\n.\r\n");
                if (content_end == string::npos) {
                    content_end = email_content.rfind(".\r\n");
                    if (content_end != string::npos && content_end > content_start) {
                        //content_end -= 2; //
                    }
                }
                
                if (content_end != string::npos && content_end > content_start) {
                    string clean_content = email_content.substr(content_start, content_end - content_start);
                    cout << clean_content << endl;
                } else {
                    // Fallback: just print everything after the first line
                    cout << email_content.substr(content_start) << endl;
                }
            }
            cout << "----------------------------------------\n";
            if (i < num_messages) cout << endl; // Add spacing between messages
        }

    } else {
        cout << "[STATUS] Your mailbox is empty.\n";
        cout << "----------------------------------------\n";
    }

    send_command(client_socket, "QUIT"); receive_response(client_socket);
    close(client_socket);
}

// --- Main Program ---

int main() {
    string user_email;
    int choice = 0;
    
    // 1. LOGIN (Get Email Address)
    cout << "========================================\n";
    cout << "  Welcome to the Mailbox Client\n";
    cout << "========================================\n";
    cout << "Login: Enter your email address: ";
    getline(cin, user_email);
    
    // 2. Main Mailbox Loop
    while (true) {
        // Clear screen and display current mailbox status
        display_mailbox(user_email); 

        cout << "\n[ACTION MENU]\n";
        cout << "1. Send New Mail\n";
        cout << "2. Refresh Mailbox\n";
        cout << "3. Quit\n";
        cout << "Enter choice (1-3): ";
        
        // Handle input choice
        if (!(cin >> choice)) {
            // If input fails (e.g., user presses Ctrl+D), or if user just hits enter, refresh.
            if (cin.eof() || cin.fail()) {
                cin.clear();
                choice = 2; // Treat as Refresh
            }
        }
        
        // Consume the rest of the line (including the newline character)
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        
        if (choice == 1) {
            send_mail(user_email);
        } else if (choice == 3) {
            cout << "\nLogging out and closing client.\n";
            break;
        } else if (choice == 2) {
            cout << "\nRefreshing mailbox...\n";
            continue; // Loop back to display_mailbox
        } else {
            cout << "Invalid choice. Please enter 1, 2, or 3.\n";
        }
    }

    return 0;
}