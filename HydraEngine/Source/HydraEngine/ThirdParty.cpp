module;

#include "HydraEngine/Base.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

module HE;

namespace HE {

    //////////////////////////////////////////////////////////////////////////
    // Image
    //////////////////////////////////////////////////////////////////////////

    Image::Image(const std::filesystem::path& filename, int desiredChannels, bool flipVertically)
    {
        bool isHDR = filename.extension() == ".hdr";

        stbi_set_flip_vertically_on_load(flipVertically);
        
        if(isHDR)
        {
            if (width != height * 2)
            {
                HE_CORE_ERROR("{} is not an equirectangular image!", filename.string());
                return;
            }

            data = (uint8_t*)stbi_loadf(filename.string().c_str(), &width, &height, &channels, 0);
        }
        else
        {
            data = stbi_load(filename.string().c_str(), &width, &height, &channels, desiredChannels);
        }

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

    Image::Image(int pWidth, int pHeight, int pChannels, uint8_t* pData)
        : width(pWidth)
        , height(pHeight)
        , channels(pChannels)
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

    bool Image::GetImageInfo(const std::filesystem::path& filePath, int& outWidth, int& outHeight, int& outChannels)
    {
        return stbi_info(filePath.string().c_str(), &outWidth, &outHeight, &outChannels);
    }

    bool Image::SaveAsPNG(const std::filesystem::path& filePath, int width, int height, int channels, const void* data, int strideInBytes)
    {
        if (!data) return false;
        return stbi_write_png(filePath.string().c_str(), width, height, channels, data, strideInBytes);
    }

    bool Image::SaveAsJPG(const std::filesystem::path& filePath, int width, int height, int channels, const void* data, int quality)
    {
        if (!data) return false;
        return stbi_write_jpg(filePath.string().c_str(), width, height, channels, data, quality);
    }

    bool Image::SaveAsBMP(const std::filesystem::path& filePath, int width, int height, int channels, const void* data)
    {
        if (!data) return false;
        return stbi_write_bmp(filePath.string().c_str(), width, height, channels, data);
    }

    void Image::SetData(uint8_t* pData)
    {
        if (data)
        {
            stbi_image_free(data);
        }
        data = pData;
    }
    
    uint8_t* Image::ExtractData()
    {
        uint8_t* extracted = data;
        data = nullptr;
        return extracted;
    }
}
