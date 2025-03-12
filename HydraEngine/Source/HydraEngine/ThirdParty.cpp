module;

#include "HydraEngine/Base.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

module HydraEngine;

namespace HydraEngine {

    //////////////////////////////////////////////////////////////////////////
	// Image
	//////////////////////////////////////////////////////////////////////////

    Image::Image(const std::filesystem::path& filename, int desiredChannels, bool flipVertically)
    {
        stbi_set_flip_vertically_on_load(flipVertically);
        data = stbi_load(filename.string().c_str(), &width, &height, &channels, desiredChannels);

        if (!data)
        {
            HE_CORE_ERROR("Failed to load image: {}", stbi_failure_reason());
        }
    }

    Image::Image(Buffer buffer, int desiredChannels, bool flipVertically)
    {
        stbi_set_flip_vertically_on_load(flipVertically);
        data = stbi_load_from_memory(buffer.data, (int)buffer.size, &width, &height, &channels, desiredChannels);

        if (!data)
        {
            HE_CORE_ERROR("Failed to load image: {}", stbi_failure_reason());
        }
    }

    Image::Image(int w, int h, int ch, unsigned char* pData)
        : width(w)
        , height(h)
        , channels(ch)
        , data(pData)
    {
    }

    Image::~Image()
    {
        if (data)
        {
            stbi_image_free(data);
        }
    }

    Image::Image(Image&& other) noexcept
        : data(other.data)
        , width(other.width)
        , height(other.height)
        , channels(other.channels)
    {
        other.data = nullptr;
    }

    Image& Image::operator=(Image&& other) noexcept
    {
        if (this != &other)
        {
            if (data)
            {
                stbi_image_free(data);
            }
            data = other.data;
            width = other.width;
            height = other.height;
            channels = other.channels;
            other.data = nullptr;
        }
        return *this;
    }

    bool Image::GetImageInfo(const std::filesystem::path& filename, int& out_width, int& out_height, int& out_channels)
    {
        return stbi_info(filename.string().c_str(), &out_width, &out_height, &out_channels);
    }

    bool Image::SaveAsPNG(const std::filesystem::path& filename, int width, int height, int channels, const void* data, int stride_in_bytes)
    {
        if (!data) return false;
        return stbi_write_png(filename.string().c_str(), width, height, channels, data, stride_in_bytes);
    }

    bool Image::SaveAsJPG(const std::filesystem::path& filename, int width, int height, int channels, const void* data, int quality)
    {
        if (!data) return false;
        return stbi_write_jpg(filename.string().c_str(), width, height, channels, data, quality);
    }

    void Image::SetData(unsigned char* pData)
    {
        if (data)
        {
            stbi_image_free(data);
        }
        data = pData;
    }
}
