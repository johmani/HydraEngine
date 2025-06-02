module;

#include "HydraEngine/Base.h"
#include <nfd.hpp>

#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_ZLIB_COMPATIBLE_NAME
#include <miniz.c>

module HE;
import std;
import simdjson;

namespace HE {

    std::string_view ToString(EventType type)
    {
        switch (type)
        {
        case EventType::None:                  return "None";
        case EventType::WindowClose:           return "WindowClose";
        case EventType::WindowResize:          return "WindowResize";
        case EventType::WindowFocus:           return "WindowFocus";
        case EventType::WindowLostFocus:       return "WindowLostFocus";
        case EventType::WindowMoved:           return "WindowMoved";
        case EventType::WindowDrop:            return "WindowDrop";
        case EventType::WindowContentScale:    return "WindowContentScale";
        case EventType::WindowMaximize:        return "WindowMaximize";
        case EventType::WindowMinimized:       return "WindowMinimized";
        case EventType::KeyPressed:            return "KeyPressed";
        case EventType::KeyReleased:           return "KeyReleased";
        case EventType::KeyTyped:              return "KeyTyped";
        case EventType::MouseButtonPressed:    return "MouseButtonPressed";
        case EventType::MouseButtonReleased:   return "MouseButtonReleased";
        case EventType::MouseMoved:            return "MouseMoved";
        case EventType::MouseScrolled:         return "MouseScrolled";
        case EventType::MouseEnter:            return "MouseEnter";
        case EventType::GamepadButtonPressed:  return "GamepadButtonPressed";
        case EventType::GamepadButtonReleased: return "GamepadButtonReleased";
        case EventType::GamepadAxisMoved:      return "GamepadAxisMoved";
        case EventType::GamepadConnected:      return "GamepadConnected";
        }

        return "";
    }

    EventType FromStringToEventType(const std::string_view& str)
    {
        if (str == "None")                  return EventType::None;
        if (str == "WindowClose")           return EventType::WindowClose;
        if (str == "WindowResize")          return EventType::WindowResize;
        if (str == "WindowFocus")           return EventType::WindowFocus;
        if (str == "WindowLostFocus")       return EventType::WindowLostFocus;
        if (str == "WindowMoved")           return EventType::WindowMoved;
        if (str == "WindowDrop")            return EventType::WindowDrop;
        if (str == "WindowContentScale")    return EventType::WindowContentScale;
        if (str == "WindowMaximize")        return EventType::WindowMaximize;
        if (str == "WindowMinimized")       return EventType::WindowMinimized;
        if (str == "KeyPressed")            return EventType::KeyPressed;
        if (str == "KeyReleased")           return EventType::KeyReleased;
        if (str == "KeyTyped")              return EventType::KeyTyped;
        if (str == "MouseButtonPressed")    return EventType::MouseButtonPressed;
        if (str == "MouseButtonReleased")   return EventType::MouseButtonReleased;
        if (str == "MouseMoved")            return EventType::MouseMoved;
        if (str == "MouseScrolled")         return EventType::MouseScrolled;
        if (str == "MouseEnter")            return EventType::MouseEnter;
        if (str == "GamepadButtonPressed")  return EventType::GamepadButtonPressed;
        if (str == "GamepadButtonReleased") return EventType::GamepadButtonReleased;
        if (str == "GamepadAxisMoved")      return EventType::GamepadAxisMoved;
        if (str == "GamepadConnected")      return EventType::GamepadConnected;

        return EventType::None;
    }

    std::string_view ToString(EventCategory cat)
    {
        switch (cat) {
        case EventCategoryApplication:   return "Application";
        case EventCategoryInput:         return "Input";
        case EventCategoryKeyboard:      return "Keyboard";
        case EventCategoryMouse:         return "Mouse";
        case EventCategoryMouseButton:   return "MouseButton";
        case EventCategoryJoystick:      return "Joystick";
        case EventCategoryGamepadButton: return "GamepadButton";
        case EventCategoryGamepadAxis:   return "GamepadAxis";
        }

        return "";
    }

    EventCategory FromStringToEventCategory(const std::string_view& str)
    {
        if (str == "Application")   return EventCategoryApplication;
        if (str == "Input")         return EventCategoryInput;
        if (str == "Keyboard")      return EventCategoryKeyboard;
        if (str == "Mouse")         return EventCategoryMouse;
        if (str == "MouseButton")   return EventCategoryMouseButton;
        if (str == "Joystick")      return EventCategoryJoystick;
        if (str == "GamepadButton") return EventCategoryGamepadButton;
        if (str == "GamepadAxis")   return EventCategoryGamepadAxis;

        return EventCategory::EventCategoryNone;
    }

    void Input::SerializeKeyBindings(const std::filesystem::path& filePath)
    {
        std::ofstream file(filePath);
        if (!file.is_open())
        {
            HE_ERROR("Unable to open file for writing, {}", filePath.string());
        }

        std::ostringstream os;
        os << "{\n";
        os << "\t\"bindings\" : [\n";


        for (int bindingIndex = 0; auto & [key, desc] : Input::GetKeyBindings())
        {
            if (bindingIndex != 0) os << ",\n"; bindingIndex++;

            os << "\t\t{\n";
            os << "\t\t\t\"name\" : \"" << desc.name << "\",\n";
            os << "\t\t\t\"modifier\" : [ ";
            for (int i = 0; i < desc.modifier.size(); i++)
            {
                if (desc.modifier[i] != 0)
                {
                    if (i > 0) os << ", ";
                    os << "\"" << Key::ToString(desc.modifier[i]) << "\"";
                }
            }
            os << " ],\n";

            if(desc.eventCategory & EventCategoryKeyboard)
                os << "\t\t\t\"code\" : \"" << Key::ToString(desc.code) << "\",\n";
            if(desc.eventCategory & EventCategoryMouseButton)
                os << "\t\t\t\"code\" : \"" << MouseKey::ToString(desc.code) << "\",\n";

            os << "\t\t\t\"eventType\" : \"" << ToString(desc.eventType) << "\",\n";
            os << "\t\t\t\"eventCategory\" : \"" << ToString(desc.eventCategory) << "\"\n";
            os << "\t\t}";
        }


        os << "\n\t]\n";
        os << "}\n";

        file << os.str();
    }

    bool Input::DeserializeKeyBindings(const std::filesystem::path& filePath)
    {
        if (!std::filesystem::exists(filePath))
        {
            HE_ERROR("Unable to open file for reaading, {}", filePath.string());
            return false;
        }

        static simdjson::dom::parser parser;
        auto doc = parser.load(filePath.string());

        if (doc["bindings"].error())
            return false;

        auto bindings = doc["bindings"].get_array();
        if (!bindings.error())
        {
            for (auto desc : bindings)
            {
                auto modifier = desc["modifier"].get_array();
                std::array<uint16_t, c_MaxModifierCount> arr = {};
                if (!modifier.error())
                {
                    for (int i = 0; i < modifier.size(); i++)
                        arr[i] = Key::FromString(modifier.at(i).get_c_str().value());
                }

                uint16_t code;

                const char* name = !desc["name"].error() ? desc["name"].get_c_str().value() : "None";
                EventType eventType = !desc["eventType"].error() ? FromStringToEventType(desc["eventType"].get_c_str().value()) : EventType::None;
                EventCategory eventCategory = !desc["eventCategory"].error() ? FromStringToEventCategory(desc["eventCategory"].get_c_str().value()) : EventCategory::EventCategoryNone;

                if (eventCategory & EventCategoryKeyboard)
                    code = !desc["code"].error() ? Key::FromString(desc["code"].get_c_str().value()) : -1;
                if (eventCategory & EventCategoryMouseButton)
                    code = !desc["code"].error() ? MouseKey::FromString(desc["code"].get_c_str().value()) : -1;

                Input::RegisterKeyBinding({
                    .name = name,
                    .modifier = arr,
                    .code = code,
                    .eventType = eventType,
                    .eventCategory = eventCategory,
                });
            }
        }

        return true;
    }
}

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

    nfdwindowhandle_t GetNDFWindowHandle()
    {
#ifdef HE_PLATFORM_WINDOWS
        return { NFD_WINDOW_HANDLE_TYPE_WINDOWS, HE::Application::GetWindow().GetNativeWindow() };
#elif HE_PLATFORM_LINUX
        return { NFD_WINDOW_HANDLE_TYPE_X11, HE::Application::GetWindow().GetNativeWindow() };
#endif 
    }

    std::filesystem::path OpenFile(std::initializer_list<std::pair<std::string_view, std::string_view>> filters)
    {
        std::array<nfdfilteritem_t, 32> nfdFilters;
        HE_CORE_ASSERT(filters.size() < nfdFilters.size());
        
        uint32_t filterCount = 0;
        for (const auto& filter : filters)
            nfdFilters[filterCount++] = { filter.first.data(), filter.second.data() };

        NFD::Guard nfdGuard;
        NFD::UniquePath outPath;

        nfdresult_t result = NFD::OpenDialog(outPath, nfdFilters.data(), filterCount, nullptr, GetNDFWindowHandle());
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

        nfdresult_t result = NFD::SaveDialog(outPath, nfdFilters.data(), filterCount, nullptr, nullptr, GetNDFWindowHandle());
        if (result == NFD_OKAY)        return outPath.get();
        else if (result == NFD_CANCEL) return {};
        
        HE_CORE_ERROR("Error: {}", NFD::GetError());

        return {};
    }

    std::filesystem::path SelectFolder()
    {
        NFD::Guard nfdGuard;
        NFD::UniquePath outPath;

        nfdresult_t result = NFD::PickFolder(outPath, nullptr, GetNDFWindowHandle());
        if (result == NFD_OKAY)        return outPath.get();
        else if (result == NFD_CANCEL) return {};
        
        HE_CORE_ERROR("Error: {}", NFD::GetError());

        return {};
    }
}
