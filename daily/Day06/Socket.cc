#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>

class Socket {
private:
    int32_t _fd;
    int32_t _port;
    std::atomic<bool> _running { true };

    void bind()
    {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(_port);
        addr.sin_addr.s_addr = INADDR_ANY;

        // Set SO_REUSEADDR option
        int opt = 1;
        if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            close(_fd);
            throw std::runtime_error("Failed to set socket options");
        }

        if (::bind(_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(_fd);
            throw std::runtime_error("Failed to bind socket");
        }
    }

    void listen()
    {
        if (::listen(_fd, SOMAXCONN) < 0) {
            close(_fd);
            throw std::runtime_error("Failed to listen on socket");
        }
    }

public:
    Socket(int32_t port)
        : _port(port)
        , _fd(-1)
    {
        _fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_fd < 0) {
            throw std::runtime_error("Failed to create socket");
        }
        bind();
        listen();
    }

    ~Socket()
    {
        stop();
        if (_fd >= 0) {
            close(_fd);
        }
    }

    void stop()
    {
        _running = false;
        if (_fd >= 0) {
            shutdown(_fd, SHUT_RDWR);
        }
    }

    void accept(std::function<void(int32_t)> callback)
    {
        while (_running) {
            sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int32_t client_fd = ::accept(_fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

            if (client_fd < 0) {
                if (!_running) {
                    break;
                }
                std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
                continue;
            }

            try {
                callback(client_fd);
            } catch (const std::exception& e) {
                std::cerr << "Error in callback: " << e.what() << std::endl;
                close(client_fd);
            }
        }
    }
};

// Client implementation
class Client {
public:
    static void run()
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            throw std::runtime_error("Failed to create client socket");
        }

        sockaddr_in serv_addr;
        memset(reinterpret_cast<void*>(&serv_addr), 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(10028);

        if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
            close(sock);
            throw std::runtime_error("Invalid address/ Address not supported");
        }

        if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr))) {
            close(sock);
            throw std::runtime_error("Connection failed");
        }

        char buffer[1024] = { 0 };
        ssize_t bytes_read = read(sock, buffer, sizeof(buffer));
        if (bytes_read < 0) {
            close(sock);
            throw std::runtime_error("Read failed");
        }

        std::cout << "Server says: " << buffer;
        close(sock);
    }
};

int main()
{
    try {
        auto server = std::thread([&]() {
            auto session = Socket(10028);
            session.accept([](int32_t client_fd) {
                std::cout << "Accepted connection on fd: " << client_fd << std::endl;
                const char* msg = "Hello, client!\n";
                ssize_t bytes_written = ::write(client_fd, msg, strlen(msg));
                if (bytes_written < 0) {
                    std::cerr << "Failed to write to client: " << strerror(errno) << std::endl;
                }
                close(client_fd);
            });
            session.stop();
        });

        // Start client in a separate thread after a short delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto client = std::thread([]() {
            try {
                Client::run();
            } catch (const std::exception& e) {
                std::cerr << "Client error: " << e.what() << std::endl;
            }
        });

        // Wait for user input to exit
        std::cout << "Server running. Press enter to stop..." << std::endl;
        std::cin.get();

        server.join();
        client.join();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}