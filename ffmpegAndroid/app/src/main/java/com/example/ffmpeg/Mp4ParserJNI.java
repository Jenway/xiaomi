package com.example.ffmpeg;

import androidx.annotation.NonNull;

public class Mp4ParserJNI {
    private long nativeHandle;
    static {
        System.loadLibrary("avutil");   // libavutil.so
        System.loadLibrary("swscale");  // libswscale.so (if used for frame conversion)
        System.loadLibrary("avcodec");  // libavcodec.so (depends on avutil)
        System.loadLibrary("avformat"); // libavformat.so (depends on avcodec, avutil)

        System.loadLibrary("mp4parser"); // libmp4parser.so
    }
    public native boolean open(String filePath);

    public native PacketInfo readNextVideoPacket();

    public native java.util.List<PacketInfo> readAllVideoPackets();

    public native FrameInfo decodeNextVideoFrame();

    public native boolean dumpAllFramesToYUV(String yuvOutputPath);

    public native void close();

    public static class PacketInfo {
        public long pts;
        public long dts;
        public int streamIndex;

        // Constructor for JNI to populate
        public PacketInfo() {}

        @NonNull
        @Override
        public String toString() {
            return "PacketInfo{" +
                    "pts=" + pts +
                    ", dts=" + dts +
                    ", streamIndex=" + streamIndex +
                    '}';
        }
    }

    // Inner class for FrameInfo
    public static class FrameInfo {
        public int width;
        public int height;
        public int format; // AVPixelFormat enum value (int)
        public long pts;

        // Constructor for JNI to populate
        public FrameInfo() {}

        @NonNull
        @Override
        public String toString() {
            return "FrameInfo{" +
                    "width=" + width +
                    ", height=" + height +
                    ", format=" + format +
                    ", pts=" + pts +
                    '}';
        }
    }
}
