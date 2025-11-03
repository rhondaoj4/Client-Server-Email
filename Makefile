email: server_app client_app

server_app: server.cpp
	g++ server.cpp -o server_app -pthread

client_app: client.cpp
	g++ client.cpp -o client_app


clean:
	rm -f server_app client_app