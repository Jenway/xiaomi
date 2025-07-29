#include "Connection.hpp"
#include "Poller.hpp"
#include "Socket.hpp"
#include <iostream>

static void handle_new_connection(tcp::Socket& socket, Poller& poller, std::unordered_map<int, tcp::Connection>& conns);

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int32_t port = std::stoi(argv[1]);
    tcp::Socket socket(port);
    Poller poller;

    int server_fd = socket.fd();
    poller.add_fd(server_fd, EPOLLIN);

    std::unordered_map<int, tcp::Connection> connections;
    while (true) {
        for (const auto& ev : poller.wait()) {
            int fd = ev.data.fd;
            if (fd == server_fd) {
                handle_new_connection(socket, poller, connections);
            } else if ((ev.events & EPOLLIN) != 0U) {
                auto it = connections.find(fd);
                if (it != connections.end()) {
                    it->second.handle(poller);
                    connections.erase(it);
                }
            }
        }
    }

    return 0;
}

static void handle_new_connection(tcp::Socket& socket, Poller& poller,
    std::unordered_map<int, tcp::Connection>& conns)
{
    while (true) {
        int client_fd = socket.accept();
        if (client_fd < 0) {
            break;
        }
        poller.add_fd(client_fd, EPOLLIN | EPOLLET);
        conns.emplace(client_fd, tcp::Connection(client_fd));
    }
}
