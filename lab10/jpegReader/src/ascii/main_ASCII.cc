#include "AsciiArtDisplay.hpp"
#include "JpegDecoder.hpp"
#include <iostream>
int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <jpeg_file> <type=0,1>" << std::endl;
        return 1;
    }
    int opt = 0;
    if (argc < 3)
    {
        std::cout << "Not type Specified, using default \n";
    }
    else
    {
        opt = atoi(argv[2]);
    }

    auto decoder = createJpegDecoder();
    auto image = decoder->decode(argv[1]);
    if (!image)
    {
        std::cerr << "Failed to decode image" << std::endl;
        return 1;
    }

    auto display = createAsciiArtDisplay(opt);
    if (!display->display(image))
    {
        std::cerr << "Failed to display image" << std::endl;
        return 1;
    }

    return 0;
}
