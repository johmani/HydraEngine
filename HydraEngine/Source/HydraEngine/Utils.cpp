module;

#include "HydraEngine/Base.h"
#include <nfd.hpp>

#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_ZLIB_COMPATIBLE_NAME
#include <miniz.c>

module HE;
import std;

namespace HE::FileSystem {

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
                    return true;
                }
                else if (fs::is_directory(path))
                {
                    fs::remove_all(path);
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

    bool Copy(const std::filesystem::path& from, const std::filesystem::path& to, std::filesystem::copy_options options)
    {
        try
        {
            std::filesystem::copy(from, to, options);
            return true;
        }
        catch (const std::exception& ex)
        {
            auto& e = ex;
            HE_CORE_ERROR("{}", e.what());
        }

        return false;
    }

    std::vector<uint8_t> ReadBinaryFile(const std::filesystem::path& filePath)
    {
        std::ifstream inputFile(filePath, std::ios::binary | std::ios::ate);

        if (!inputFile)
        {
            HE_CORE_ERROR("Unable to open input file {}", filePath.string());
            return {};
        }

        std::streamsize fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer;
        buffer.resize(static_cast<size_t>(fileSize));
        inputFile.read(reinterpret_cast<char*>(buffer.data()), fileSize);

        return buffer;
    }

    bool ReadBinaryFile(const std::filesystem::path& filePath, Buffer buffer)
    {
        std::ifstream inputFile(filePath, std::ios::binary | std::ios::ate);
        if (!inputFile)
        {
            HE_CORE_ERROR("Unable to open input file {}", filePath.string());
            return false;
        }

        std::streamsize fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        if (buffer.size < static_cast<size_t>(fileSize))
        {
            HE_CORE_ERROR("Provided buffer is too small. Required size: {}", fileSize);
            return false;
        }

        inputFile.read(reinterpret_cast<char*>(buffer.data), fileSize);

        return true;
    }

    std::string FileSystem::ReadTextFile(const std::filesystem::path& filePath)
    {
        std::ifstream infile(filePath, std::ios::in | std::ios::ate);
        if (!infile)
        {
            HE_CORE_ERROR("Could not open input file: {}", filePath.string());
            return {};
        }

        std::streamsize size = infile.tellg();
        infile.seekg(0, std::ios::beg);

        std::string content(size, '\0');
        infile.read(content.data(), size);

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

    bool GenerateFileWithReplacements(const std::filesystem::path& input, const std::filesystem::path& output, const std::initializer_list<std::pair<std::string_view, std::string_view>>& replacements)
    {
        std::ifstream infile(input, std::ios::binary | std::ios::ate);
        if (!infile)
        {
            HE_CORE_ERROR("Could not open input file: {}", input.string());
            return false;
        }

        std::streamsize size = infile.tellg();
        infile.seekg(0, std::ios::beg);

        std::string content(size, '\0');
        infile.read(content.data(), size);

        for (const auto& [oldText, newText] : replacements)
        {
            size_t pos = 0;
            while ((pos = content.find(oldText, pos)) != std::string::npos)
            {
                content.replace(pos, oldText.length(), newText);
                pos += newText.length();
            }
        }

        std::ofstream outfile(output, std::ios::binary);
        if (!outfile)
        {
            HE_CORE_ERROR("Could not open output file: {}", output.string());
            return false;
        }

        outfile.write(content.data(), content.size());

        return true;
    }

    bool ExtractZip(const std::filesystem::path& zipPath, const std::filesystem::path& outputDir)
    {
        mz_zip_archive zip;
        memset(&zip, 0, sizeof(zip));

        auto pathStr = zipPath.string();
        if (!mz_zip_reader_init_file(&zip, pathStr.c_str(), 0))
            return false;

        int fileCount = (int)mz_zip_reader_get_num_files(&zip);
        for (int i = 0; i < fileCount; i++)
        {
            mz_zip_archive_file_stat file_stat;
            if (!mz_zip_reader_file_stat(&zip, i, &file_stat))
                continue;

            std::filesystem::path destPath = outputDir / file_stat.m_filename;

            if (file_stat.m_is_directory)
            {
                std::filesystem::create_directories(destPath);
            }
            else
            {
                std::filesystem::create_directories(destPath.parent_path());
                mz_zip_reader_extract_to_file(&zip, i, destPath.string().c_str(), 0);
            }
        }

        mz_zip_reader_end(&zip);
        return true;
    }
}

namespace HE::FileDialog {

	std::filesystem::path OpenFile(std::initializer_list<std::pair<std::string_view, std::string_view>> filters)
	{
		std::array<nfdfilteritem_t, 32> nfdFilters;
        HE_CORE_ASSERT(filters.size() < nfdFilters.size());
		
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
        HE_CORE_ASSERT(filters.size() < nfdFilters.size());
		
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
