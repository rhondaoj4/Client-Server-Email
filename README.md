C++ Email Client & Server

This project is a complete, multi-threaded email system built in C++, featuring concurrent SMTP and POP3 servers and a command-line client for sending and receiving messages.

Core Features

    Multi-Threaded Server: The server uses std::thread to run the SMTP and POP3 listeners concurrently. It also detaches a new thread for each connected client, allowing it to handle simultaneous connections.

    SMTP Server: A functional SMTP server listens on port 2525, handling HELO, MAIL FROM, RCPT TO, and DATA commands to receive and store mail.

    POP3 Server: A functional POP3 server listens on port 8110, handling USER, PASS, STAT, LIST, and RETR commands to allow users to retrieve their mail.

    Persistent Storage: The SMTP server saves all incoming emails to a local flat-file system. Each recipient email address (e.g., user@example.com) gets its own mailbox file (user@example.com.txt).

    Interactive Client: A command-line client provides a unified mailbox experience.

    Send Mail: Users can compose and send new emails (To, Subject, and multi-line Body) through the client's SMTP functionality.

    Check Mail: The client automatically logs into the POP3 server to fetch and display all messages for the user. It provides a simple menu to Send New Mail, Refresh Mailbox, or Quit.

Planned Features

    Secure Authentication: Implement actual password validation for the POP3 PASS command instead of the current implicit login.

    Message Deletion: Add support for the POP3 DELE command to allow users to mark messages for deletion from the server.

    Robust Storage: Upgrade from a flat-file system to a more robust database (like SQLite) for managing mailboxes and user accounts.

    Configuration File: Move hard-coded ports (2525, 8110) and settings to an external configuration file.

    Error Handling: Improve error handling for failed connections and invalid user inputs on both the client and server.

Technologies

    C++: All core server and client logic is written in C++.

    POSIX Sockets (sys/socket): Used for all low-level TCP/IP network communication on both the server and client.

    C++ Multi-threading (<thread>): The server uses std::thread to manage its concurrent listeners and client connections.

    Makefile: A Makefile is provided for easy compilation of the server_app and client_app executables. 

How to Run

1. Compile the Applications

In your terminal, run the make command to build both the server and client executables.
Bash

make

This will create two files: server_app and client_app.

2. Start the Server

Open a terminal window and run the server. This must be running in the background to handle email.
Bash

./server_app

You should see output confirming the SMTP and POP3 servers are listening on their respective ports.

3. Run the Client(s)

Open one or more new terminal windows to run the client application.
Bash

./client_app

To test the system:

    When the client starts, "log in" with a fictional email address, for example: rhonda@mail.com.

    The client will check your (empty) mailbox. Choose option 1 to Send New Mail.

    Send the email to a different user, for example: gemini@mail.com.

    Once the email is sent, open a second terminal window and run ./client_app again.

    This time, "log in" as gemini@mail.com.

    You will see the email from rhonda@mail.com appear in your inbox.
