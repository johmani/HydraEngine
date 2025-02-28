module;

#include "HydraEngine/Base.h"
#include <nfd.hpp>


#ifdef DeleteFile
#undef DeleteFile
#endif

module HydraEngine;
import std;

namespace HydraEngine::FileSystem {

    bool Delete(const std::filesystem::path& path)
    {
        namespace fs = std::filesystem;

        try
        {
            if (fs::exists(path))
            {
                if (fs::is_regular_file(path))
                {
                    fs::remove(path);
                    HE_CORE_INFO("File deleted successfully {}", path.string());
                    return true;
                }
                else if (fs::is_directory(path))
                {
                    fs::remove_all(path);
                    HE_CORE_INFO("Directory {} deleted successfully", path.string());
                    return true;
                }
                else
                {
                    HE_CORE_ERROR("Unknown file type");
                    return false;
                }
            }
            else
            {
                HE_CORE_ERROR("File or directory {} does not exist ", path.string());
                return false;
            }
        }
        catch (const std::exception& ex)
        {
            auto& e = ex;
            HE_CORE_ERROR("{}", e.what());
            return false;
        }
    }

    bool Rename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
    {
        try
        {
            std::filesystem::rename(oldPath, newPath);
            return true;
        }
        catch (const std::exception& ex) 
        {
            auto& e = ex;
            HE_CORE_ERROR("{}", e.what());
        }

        return false;
    }

    bool Copy(const std::filesystem::path& from, const std::filesystem::path& to)
    {
        try
        {
            std::filesystem::copy(from, to, std::filesystem::copy_options::recursive);
            return true;
        }
        catch (const std::exception& ex)
        {
            auto& e = ex;
            HE_CORE_ERROR("{}", e.what());
        }

        return false;
    }

    void Open(const std::filesystem::path& path)
    {
#ifdef HE_PLATFORM_WINDOWS
        system(std::format("explorer {}", path.string()).c_str());
#elif HE_PLATFORM_LINUX
        system(std::format("xdg-open {}", path.string()).c_str());
#else
        HE_CORE_VERIFY(false, "unsupported platform");
#endif
    }

    std::vector<uint8_t> FileSystem::ReadBinaryFile(const std::filesystem::path& filePath)
    {
        std::vector<uint8_t> buffer;
        std::ifstream inputFile(filePath, std::ios::binary);

        if (!inputFile)
        {
            HE_CORE_ERROR("Error: Unable to open input file {}", filePath.string());
            return buffer;
        }

        buffer.assign(std::istreambuf_iterator<char>(inputFile), {});
        inputFile.close();

        return buffer;
    }

    Buffer ReadBinaryFileToBuffer(const std::filesystem::path& filePath)
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        file.seekg(0, std::ios::beg);
        size_t size = file.tellg();
        uint8_t* data = new uint8_t[size];
        HydraEngine::Buffer buffer(data, size);
        file.read(reinterpret_cast<char*>(data), size);

        return buffer;
    }

    std::string FileSystem::ReadTextFile(const std::filesystem::path& filePath)
    {
        std::ifstream infile(filePath);
        if (!infile.is_open())
        {
            HE_CORE_ERROR("Could not open input file: {}", filePath.string());
            return "";
        }

        std::stringstream buffer;
        buffer << infile.rdbuf();
        std::string content = buffer.str();
        infile.close();

        return content;
    }

    bool ConvertBinaryToHeader(const std::filesystem::path& inputFileName, const std::filesystem::path& outputFileName, const std::string& arrayName)
    {
        std::ifstream inputFile(inputFileName, std::ios::binary);
        if (!inputFile)
        {
            HE_CORE_ERROR("Error: Unable to open input file ", inputFileName.string());
            return false;
        }

        std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inputFile), {});
        inputFile.close();

        std::ofstream outputFile(outputFileName);
        if (!outputFile)
        {
            HE_CORE_ERROR("Error: Unable to open input file ", inputFileName.string());
            return false;
        }

        outputFile << "#ifndef " << arrayName << "_H" << std::endl;
        outputFile << "#define " << arrayName << "_H" << std::endl;
        outputFile << std::endl;
        outputFile << "unsigned char " << arrayName << "[] = {" << std::endl;

        for (size_t i = 0; i < buffer.size(); ++i)
        {
            outputFile << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]);

            if (i != buffer.size() - 1)
            {
                outputFile << ", ";
            }

            if ((i + 1) % 12 == 0)  // 12 bytes per line
            {
                outputFile << std::endl;
            }
        }

        outputFile << std::endl << "};" << std::endl;
        outputFile << std::endl;
        outputFile << "#endif //" << arrayName << "_H" << std::endl;

        outputFile.close();
        return true;
    }
}

namespace HydraEngine::FileDialog {

	std::filesystem::path OpenFile(std::initializer_list<std::pair<std::string_view, std::string_view>> filters)
	{
		std::array<nfdfilteritem_t, 32> nfdFilters;
        HE_CORE_ASSERT(filters.size() >= nfdFilters.size());
		
        uint32_t filterCount = 0;
		for (const auto& filter : filters)
            nfdFilters[filterCount++] = { filter.first.data(), filter.second.data() };

		NFD::Guard nfdGuard;
		NFD::UniquePath outPath;

		nfdresult_t result = NFD::OpenDialog(outPath, nfdFilters.data(), filterCount);
		if (result == NFD_OKAY)        return outPath.get();
		else if (result == NFD_CANCEL) return {};
		else HE_CORE_ERROR("Error: {}", NFD::GetError());
		return {};
	}

	std::filesystem::path SaveFile(std::initializer_list<std::pair<std::string_view, std::string_view>> filters)
	{
		std::array<nfdfilteritem_t, 32> nfdFilters;
        HE_CORE_ASSERT(filters.size() >= nfdFilters.size());
		
        uint32_t filterCount = 0;
		for (const auto& filter : filters)
            nfdFilters[filterCount++] = { filter.first.data(), filter.second.data() };

		NFD::Guard nfdGuard;
		NFD::UniquePath outPath;

		nfdresult_t result = NFD::SaveDialog(outPath, nfdFilters.data(), filterCount);
		if (result == NFD_OKAY)        return outPath.get();
		else if (result == NFD_CANCEL) return {};
		
        HE_CORE_ERROR("Error: {}", NFD::GetError());

		return {};
	}

	std::filesystem::path SelectFolder()
	{
		NFD::Guard nfdGuard;
		NFD::UniquePath outPath;

		nfdresult_t result = NFD::PickFolder(outPath);
		if (result == NFD_OKAY)        return outPath.get();
		else if (result == NFD_CANCEL) return {};
		
        HE_CORE_ERROR("Error: {}", NFD::GetError());

		return {};
	}
}
