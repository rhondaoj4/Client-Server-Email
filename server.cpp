#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <vector>
#include <algorithm> 

using namespace std;

// --- Helper Functions ---

// Send a response string to the client
void send_response(int socket, const string &msg) {
    string full_msg = msg + "\r\n";
    send(socket, full_msg.c_str(), full_msg.length(), 0);
    cout << "Server Sent: " << msg << endl;
}

// Function to safely extract a field (like recipient email)
string extract_field(const string& command, const string& prefix) {
    size_t start = command.find(prefix);
    if (start != string::npos) {
        // Find the start of the address (after the prefix)
        start += prefix.length();
        // Trim leading spaces
        start = command.find_first_not_of(" <", start);
        // Find the end of the address (before >)
        size_t end = command.find('>', start);
        if (end != string::npos) {
            return command.substr(start, end - start);
        }
    }
    return "";
}

string clean_email_for_filename(const string& email) {
    string cleaned = email;
    // Remove leading '<'
    if (!cleaned.empty() && cleaned.front() == '<') {
        cleaned.erase(0, 1);
    }
    // Remove trailing '>'
    if (!cleaned.empty() && cleaned.back() == '>') {
        cleaned.pop_back();
    }
    return cleaned;
}

// --- POP3 Mail Retrieval Logic ---

// Structure to hold one email's contents
struct Mail {
    string content;
};

// Function to load all emails for a user from their file
vector<Mail> load_mailbox(const string& username) {
    vector<Mail> mailbox;
    string filename = username + ".txt";
    ifstream infile(filename);
    string line;
    string current_email_content;

    // A unique delimiter to split emails within the file
    const string DELIMITER = "--- END OF MESSAGE ---";

    // Read the file line by line
    while (getline(infile, line)) {
        if (line == DELIMITER) { 
            if (!current_email_content.empty()) {
                // Remove trailing \n added from the final line of the body
                if (current_email_content.back() == '\n') {
                    current_email_content.pop_back();
                }
                mailbox.push_back({current_email_content});
            }
            current_email_content.clear();
        } else {
            // Append the line and add the newline back
            current_email_content += line + "\n";
        }
    }
    return mailbox;
}

void handle_pop3_client(int client_socket) {
    send_response(client_socket, "+OK POP3 Server ready");

    string username;
    bool logged_in = false;
    vector<Mail> mailbox;

    // Main POP3 Session Loop
    while (true) {
        char buffer[1024] = {0};
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) break;

        string command(buffer);
        // Clean up command: remove trailing whitespace and \r\n
        command.erase(command.find_last_not_of(" \r\n") + 1);
        cout << "POP3 Client Recv: " << command << endl;

        string verb = command.substr(0, command.find(' '));
        // Convert command verb to uppercase
        std::transform(verb.begin(), verb.end(), verb.begin(), ::toupper);

        if (verb == "QUIT") {
            send_response(client_socket, "+OK Bye");
            break;
        }

        if (!logged_in) {
            if (verb == "USER") {
                // For simplicity, we assume the USER command contains the full email address
                username = command.substr(command.find(' ') + 1);
                send_response(client_socket, "+OK User name accepted, password please");
            } else if (verb == "PASS") {
                if (!username.empty()) {
                    // Load the mailbox based on the username provided
                    mailbox = load_mailbox(username);
                    logged_in = true;
                    // Just send a simple +OK, don't include STAT info here
                    send_response(client_socket, "+OK Logged in");
                } else {
                    send_response(client_socket, "-ERR USER command required first");
                }
            } else {
                send_response(client_socket, "-ERR Authentication required");
            }
        } else {
            // Commands requiring authentication
            if (verb == "STAT") {
                // Returns number of messages and total size (size estimate)
                int total_size = 0;
                for (const auto& mail : mailbox) {
                    total_size += mail.content.length();
                }
                send_response(client_socket, "+OK " + to_string(mailbox.size()) + " " + to_string(total_size));
            } else if (verb == "LIST") {
                // Lists message numbers and sizes
                string list_response = "+OK Mailbox scan listing follows";
                for (size_t i = 0; i < mailbox.size(); ++i) {
                    list_response += "\r\n" + to_string(i + 1) + " " + to_string(mailbox[i].content.length());
                }
                list_response += "\r\n."; // POP3 termination dot
                // Send LIST response as a single block
                string full_msg = list_response + "\r\n";
                send(client_socket, full_msg.c_str(), full_msg.length(), 0);
                cout << "Server Sent: [LIST Response]" << endl;
            } else if (verb == "RETR") {
                // Retrieve email by index
                int msg_num = 0;
                try {
                    msg_num = stoi(command.substr(command.find(' ') + 1));
                } catch (...) {
                    send_response(client_socket, "-ERR Invalid message number");
                    continue;
                }

                if (msg_num > 0 && msg_num <= (int)mailbox.size()) {
                    string email_content = mailbox[msg_num - 1].content;
                    
                    string success_msg = "+OK " + to_string(email_content.length()) + " octets";
                    
                    // 2. Build final response: Header + CRLF + Content + CRLF + Dot + CRLF
                    string full_response = success_msg + "\r\n";
                    full_response += email_content; // Content (ends in \n from load_mailbox)
                    
                    // Append the dot terminator: POP3 requires a line with just a dot, followed by CRLF
                    full_response += ".\r\n"; 

                    send(client_socket, full_response.c_str(), full_response.length(), 0);
                    cout << "Server Sent: [Full Email Content]" << endl;
                } else {
                    send_response(client_socket, "-ERR No such message");
                }
            } else {
                send_response(client_socket, "-ERR Unknown command");
            }
        }
    }

    close(client_socket);
}

// Thread function for the POP3 Listener
void pop3_listener() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    const int POP3_PORT = 8110; 
    int opt = 1;

    // Socket creation and setup
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        cerr << "POP3 Socket creation failed" << endl;
        return;
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("POP3 setsockopt");
        return;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(POP3_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        cerr << "POP3 Bind failed. Check if port " << POP3_PORT << " is already in use." << endl;
        return;
    }

    if (listen(server_fd, 10) < 0) {
        perror("POP3 listen failed");
        return;
    }

    cout << "POP3 Server listening on port " << POP3_PORT << endl;

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("POP3 accept failed");
            continue;
        }

        cout << "\nPOP3 client connected" << endl;
        // Handle POP3 client in a new thread
        thread client_thread(handle_pop3_client, new_socket);
        client_thread.detach();
    }
    close(server_fd);
}

// --- SMTP Mail Sending Logic ---

void handle_smtp_client(int client_socket) {
    send_response(client_socket, "220 localhost Simple SMTP Server");

    string mail_from;
    string rcpt_to; 
    string data_body;
    bool in_data_mode = false;
    char buffer[1024] = {0};

    // main session
    while(true){
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if(bytes_received <= 0) break;

        string command(buffer);
        command.erase(command.find_last_not_of(" \r\n") + 1);
        cout << "SMTP Client Recv: " << command << endl;

        if (in_data_mode) {
            if (command.length() == 1 && command[0] == '.') { 
                in_data_mode = false;
                
                string cleaned_rcpt_to = clean_email_for_filename(rcpt_to);
                string filename = cleaned_rcpt_to + ".txt";

                ofstream outfile(filename, ios::app);
                if (outfile.is_open()) {
                    // Save the received body content (which includes headers)
                    outfile << data_body; 
                    // Final newline before the delimiter
                    if (data_body.empty() || data_body.back() != '\n') {
                        outfile << "\n";
                    }
                    // Use a unique delimiter so POP3 can split messages
                    outfile << "--- END OF MESSAGE ---\n"; 
                    outfile.close();
                    send_response(client_socket, "250 OK Message accepted for delivery");
                } else {
                    send_response(client_socket, "451 Requested action aborted: local error in processing");
                }
                
                mail_from.clear();
                rcpt_to.clear();
                data_body.clear();
            } else {
                // Store in in-memory list. Newlines were added by client_smtp.
                data_body += command + "\n"; 
            }
        } else {
            string verb = command.substr(0, command.find(' '));
            std::transform(verb.begin(), verb.end(), verb.begin(), ::toupper);
            
            if(verb == "HELO") {
                send_response(client_socket, "250 Hello");
            } else if(verb == "MAIL") {
                mail_from = extract_field(command, "FROM:");
                send_response(client_socket, "250 Sender OK");
            } else if (verb == "RCPT") {
                rcpt_to = extract_field(command, "TO:");
                send_response(client_socket, "250 Recipient OK");
            } else if (verb == "DATA") {
                in_data_mode = true;
                send_response(client_socket, "354 Start mail input; end with <CRLF>.<CRLF>");
            } else if (verb == "QUIT") {
                send_response(client_socket, "221 Bye");
                break;
            } else {
                send_response(client_socket, "500 Syntax error, command unrecognized");
            }
        }
    }
    close(client_socket);
}

// Thread function for the SMTP Listener
void smtp_listener() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    const int SMTP_PORT = 2525;
    int opt = 1;

    // socket creation and setup
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
        cerr << "SMTP Socket creation failed" << endl;
        return;
    }
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        perror("SMTP setsockopt");
        return;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SMTP_PORT);

    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0){
        cerr << "SMTP Bind failed. Check if port " << SMTP_PORT << " is already in use." << endl;
        return;
    }

    if(listen(server_fd, 10) < 0){
        perror("SMTP listen failed");
        return;
    }

    cout << "SMTP Server listening on port " << SMTP_PORT << endl;

    while(true){
        if((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0){
            perror("SMTP accept failed");
            continue;
        }

        cout << "\nSMTP client connected" << endl;
        // Handle SMTP client in a new thread
        thread client_thread(handle_smtp_client, new_socket);
        client_thread.detach();
    }
    close(server_fd);
}

int main() {
    // Start both listeners concurrently
    thread smtp_thread(smtp_listener);
    thread pop3_thread(pop3_listener);

    // Keep the main thread alive until the user decides to exit
    smtp_thread.join();
    pop3_thread.join();
    
    return 0;
}