#include "mp4_parser.hpp"
#include <iostream>
#include <vector>

void runAssignment3(const std::string& filePath)
{
    std::cout << "--- Running Assignment 3: AVPacket PTS/DTS ---\n";
    Mp4Parser parser;
    if (!parser.open(filePath)) {
        std::cerr << "Failed to open MP4 file for Assignment 3: " << filePath << std::endl;
        return;
    }

    std::vector<PacketInfo> packets = parser.readAllVideoPackets();
    if (packets.empty()) {
        std::cout << "No video packets found in " << filePath << std::endl;
    } else {
        std::cout << "Found " << packets.size() << " video packets.\n";
        for (size_t i = 0; i < packets.size(); ++i) {
            const PacketInfo& info = packets[i];
            std::cout << "Packet " << i + 1 << ": PTS = " << info.pts << ", DTS = " << info.dts
                      << ", Stream Index = " << info.streamIndex << std::endl;
            // Print only the first few packets to avoid excessive output
            if (i >= 9) {
                std::cout << "(Showing first 10 packets only...)\n";
                break;
            }
        }
    }
    parser.close();
    std::cout << "--- Assignment 3 Finished ---\n\n";
}

void runAssignment4(const std::string& filePath)
{
    std::cout << "--- Running Assignment 4: AVFrame Width/Height ---\n";
    Mp4Parser parser;
    if (!parser.open(filePath)) {
        std::cerr << "Failed to open MP4 file for Assignment 4: " << filePath << std::endl;
        return;
    }

    FrameInfo frameInfo;
    int frameCount = 0;
    while (parser.decodeNextVideoFrame(frameInfo)) {
        std::cout << "Decoded Frame " << ++frameCount << ": Width = " << frameInfo.width
                  << ", Height = " << frameInfo.height
                  << ", Format = " << av_get_pix_fmt_name(frameInfo.format)
                  << ", PTS = " << frameInfo.pts << std::endl;
        // Print only the first few frames to avoid excessive output
        if (frameCount >= 9) {
            std::cout << "(Showing first 10 frames only...)\n";
            break;
        }
    }
    if (frameCount == 0) {
        std::cout << "No video frames decoded from " << filePath << std::endl;
    }
    parser.close();
    std::cout << "--- Assignment 4 Finished ---\n\n";
}

void runAssignment5(const std::string& inputFilePath, const std::string& outputYUVPath)
{
    std::cout << "--- Running Assignment 5: MP4 to YUV Conversion ---\n";
    Mp4Parser parser;
    if (!parser.open(inputFilePath)) {
        std::cerr << "Failed to open MP4 file for Assignment 5: " << inputFilePath << std::endl;
        return;
    }

    std::cout << "Converting '" << inputFilePath << "' to YUV420P and saving to '" << outputYUVPath << "'...\n";
    if (parser.dumpAllFramesToYUV(outputYUVPath)) {
        std::cout << "Successfully converted and saved all video frames to YUV420P.\n";

        // Provide ffplay command for playback
        // You'll need to know the resolution of the video for this command to work correctly.
        // Assuming the parser's internal width/height are set after opening the file.
        // For a real scenario, you might want to retrieve these from a method like getWidth()/getHeight().
        int width = parser.getVideoStreamIndex() >= 0 ? parser.getFmtCtx()->streams[parser.getVideoStreamIndex()]->codecpar->width : 0;
        int height = parser.getVideoStreamIndex() >= 0 ? parser.getFmtCtx()->streams[parser.getVideoStreamIndex()]->codecpar->height : 0;

        if (width > 0 && height > 0) {
            std::cout << "\nTo play the generated YUV file with ffplay, use the following command (replace <WIDTH> and <HEIGHT> with actual values if needed, though they are attempted to be retrieved here):\n";
            std::cout << "ffplay -f rawvideo -pixel_format yuv420p -video_size " << width << "x" << height << " " << outputYUVPath << "\n";
        } else {
            std::cout << "Could not determine video resolution for ffplay command. Please manually provide width and height.\n";
            std::cout << "Example: ffplay -f rawvideo -pixel_format yuv420p -video_size 1920x1080 " << outputYUVPath << "\n";
        }

    } else {
        std::cerr << "Failed to convert MP4 to YUV.\n";
    }
    parser.close();
    std::cout << "--- Assignment 5 Finished ---\n\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_mp4_file> [output_yuv_file]\n";
        return 1;
    }

    std::string inputMp4Path = argv[1];
    std::string outputYuvPath = (argc > 2) ? argv[2] : "output.yuv"; // Default output YUV file

    runAssignment3(inputMp4Path);
    runAssignment4(inputMp4Path);
    runAssignment5(inputMp4Path, outputYuvPath);

    return 0;
}