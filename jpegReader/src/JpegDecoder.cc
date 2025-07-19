#include "../include/JpegDecoder.hpp"
#include <cstdio>
#include <iostream>
#include <setjmp.h>

extern "C"
{
#include <jpeglib.h>
}

struct JPEGCustomErrorMgr
{
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

METHODDEF(void)
jpeg_error_exit(j_common_ptr cinfo)
{
    JPEGCustomErrorMgr *myerr = (JPEGCustomErrorMgr *)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}

class LibJpegDecoder : public JpegDecoder
{
  public:
    ImageData decode(const std::string &filepath) override
    {
        ImageData img_data;
        struct jpeg_decompress_struct cinfo;
        struct JPEGCustomErrorMgr jerr;

        JSAMPARRAY buffer;
        int row_stride;

        cinfo.err = jpeg_std_error(&jerr.pub);
        jerr.pub.error_exit = jpeg_error_exit;

        if (setjmp(jerr.setjmp_buffer))
        {
            jpeg_destroy_decompress(&cinfo);
            std::cerr << "JPEG decoding failed for: " << filepath << std::endl;
            return img_data;
        }

        jpeg_create_decompress(&cinfo);

        FILE *infile = fopen(filepath.c_str(), "rb");
        if (!infile)
        {
            std::cerr << "Error opening JPEG file: " << filepath << std::endl;
            jpeg_destroy_decompress(&cinfo);
            return img_data;
        }
        jpeg_stdio_src(&cinfo, infile);

        (void)jpeg_read_header(&cinfo, TRUE);

        cinfo.out_color_space = JCS_RGB; // Request RGB output

        (void)jpeg_start_decompress(&cinfo);

        img_data.width = cinfo.output_width;
        img_data.height = cinfo.output_height;
        img_data.channels = cinfo.output_components;

        img_data.pixel_data.resize(static_cast<size_t>(img_data.width) * img_data.height * img_data.channels);

        row_stride = img_data.width * img_data.channels;

        buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

        while (cinfo.output_scanline < cinfo.output_height)
        {
            (void)jpeg_read_scanlines(&cinfo, buffer, 1);
            size_t dest_offset = (static_cast<size_t>(cinfo.output_scanline - 1) * row_stride);
            std::copy(buffer[0], buffer[0] + row_stride, img_data.pixel_data.begin() + dest_offset);
        }

        (void)jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);

        return img_data;
    }
};

std::unique_ptr<JpegDecoder> createJpegDecoder()
{
    return std::make_unique<LibJpegDecoder>();
}