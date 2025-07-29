#include "ClientConnection.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <filename>\n";
        return 1;
    }

    std::string host = argv[1];
    int port = std::stoi(argv[2]);
    std::string filename = argv[3];

    try {
        tcp::ClientConnection client(host, port);
        std::string output = "downloaded_" + filename;
        if (!client.requestFile(filename, output)) {
            std::cerr << "File download failed.\n";
            return 1;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
