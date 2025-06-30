module;

#include "HydraEngine/Base.h"
#include <nfd.hpp>
#include <simdjson.h>

#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_ZLIB_COMPATIBLE_NAME
#include <miniz.c>

module HE;
import std;

namespace HE {

    namespace MouseKey
    {
        constexpr CodeStrPair c_CodeToStringMap[] = {
            { Left,    "Left"    }, { Right,   "Right"   }, { Middle,  "Middle"  },
            { Button3, "Button3" }, { Button4, "Button4" }, { Button5, "Button5" },
            { Button6, "Button6" }, { Button7, "Button7" },
        };

        constexpr std::string_view ToString(MouseCode code) { return c_CodeToStringMap[code].codeStr; }

        constexpr MouseCode FromString(std::string_view code)
        {
            for (auto& pair : c_CodeToStringMap)
                if (pair.codeStr == code)
                    return pair.code;

            HE_CORE_VERIFY(false);
            return -1;
        }
    }

    namespace Joystick
    {
        constexpr CodeStrPair c_CodeToStringMap[] = {
            { Joystick0,  "Joystick1"  }, { Joystick1,  "Joystick2"  }, { Joystick2,  "Joystick3"  },
            { Joystick3,  "Joystick4"  }, { Joystick4,  "Joystick5"  }, { Joystick5,  "Joystick6"  },
            { Joystick6,  "Joystick7"  }, { Joystick7,  "Joystick8"  }, { Joystick8,  "Joystick9"  },
            { Joystick9,  "Joystick10" }, { Joystick10, "Joystick11" }, { Joystick11, "Joystick12" },
            { Joystick12, "Joystick13" }, { Joystick13, "Joystick14" },
            { Joystick14, "Joystick15" }, { Joystick15, "Joystick16" }
        };

        constexpr std::string_view ToString(JoystickCode code) { return c_CodeToStringMap[code].codeStr; }

        constexpr JoystickCode FromString(std::string_view codeStr)
        {
            for (auto& pair : c_CodeToStringMap)
                if (pair.codeStr == codeStr)
                    return pair.code;

            HE_CORE_VERIFY(false);
            return -1;
        }
    }

    namespace GamepadButton
    {
        constexpr CodeStrPair c_CodeToStringMap[] = {
            { A,          "A"           }, { B,           "B"            }, { X,           "X"           }, { Y,     "Y"     },
            { LeftBumper, "Left Bumper" }, { RightBumper, "Right Bumper" }, { Back,        "Back"        }, { Start, "Start" },
            { Guide,      "Guide"       }, { LeftThumb,   "Left Thumb"   }, { RightThumb,  "Right Thumb" }, { Up,    "Up"    },
            { Right,      "Right"       }, { Down,        "Down"         }, { Left,        "Left"        }
        };

        constexpr std::string_view ToString(GamepadCode code) { return c_CodeToStringMap[code].codeStr; }

        constexpr GamepadCode FromString(std::string_view codeStr)
        {
            for (auto& pair : c_CodeToStringMap)
                if (pair.codeStr == codeStr)
                    return pair.code;

            HE_CORE_VERIFY(false);
            return -1;
        }
    }

    namespace GamepadAxis
    {
        constexpr CodeStrPair c_CodeToStringMap[] = {
            { Left, "Left"  }, { Right, "Right" }
        };

        constexpr std::string_view ToString(GamepadAxisCode code) { return c_CodeToStringMap[code].codeStr; }

        constexpr GamepadAxisCode FromString(std::string_view codeStr)
        {
            for (auto& pair : c_CodeToStringMap)
                if (pair.codeStr == codeStr)
                    return pair.code;

            HE_CORE_VERIFY(false);
            return -1;
        }
    }

    namespace Key
    {
        constexpr CodeStrPair c_CodeToStringMap[] = {
            { Space,         "Space"           },
            { Apostrophe,    "'"               },
            { Comma,         ","               },
            { Minus,         "-"               },
            { Period,        "."               },
            { Slash,         "/"               },
            { D0,            "0"               },
            { D1,            "1"               },
            { D2,            "2"               },
            { D3,            "3"               },
            { D4,            "4"               },
            { D5,            "5"               },
            { D6,            "6"               },
            { D7,            "7"               },
            { D8,            "8"               },
            { D9,            "9"               },
            { Semicolon,     ";"               },
            { Equal,         "="               },
            { A,             "A"               },
            { B,             "B"               },
            { C,             "C"               },
            { D,             "D"               },
            { E,             "E"               },
            { F,             "F"               },
            { G,             "G"               },
            { H,             "H"               },
            { I,             "I"               },
            { J,             "J"               },
            { K,             "K"               },
            { L,             "L"               },
            { M,             "M"               },
            { N,             "N"               },
            { O,             "O"               },
            { P,             "P"               },
            { Q,             "Q"               },
            { R,             "R"               },
            { S,             "S"               },
            { T,             "T"               },
            { U,             "U"               },
            { V,             "V"               },
            { W,             "W"               },
            { X,             "X"               },
            { Y,             "Y"               },
            { Z,             "Z"               },
            { LeftBracket,   "["               },
            { Backslash,     "\\"              },
            { RightBracket,  "]"               },
            { GraveAccent,   "`"               },
            { World1,        "World1"          },
            { World2,        "World2"          },
            { Escape,        "Escape"          },
            { Enter,         "Enter"           },
            { Tab,           "Tab"             },
            { Backspace,     "Backspace"       },
            { Insert,        "Insert"          },
            { Delete,        "Delete"          },
            { Right,         "Right"           },
            { Left,          "Left"            },
            { Down,          "Down"            },
            { Up,            "Up"              },
            { PageUp,        "PageUp"          },
            { PageDown,      "PageDown"        },
            { Home,          "Home"            },
            { End,           "End"             },
            { CapsLock,      "CapsLock"        },
            { ScrollLock,    "Scroll Lock"     },
            { NumLock,       "Num Lock"        },
            { PrintScreen,   "Print Screen"    },
            { Pause,         "Pause"           },
            { F1,            "F1"              },
            { F2,            "F2"              },
            { F3,            "F3"              },
            { F4,            "F4"              },
            { F5,            "F5"              },
            { F6,            "F6"              },
            { F7,            "F7"              },
            { F8,            "F8"              },
            { F9,            "F9"              },
            { F10,           "F10"             },
            { F11,           "F11"             },
            { F12,           "F12"             },
            { F13,           "F13"             },
            { F14,           "F14"             },
            { F15,           "F15"             },
            { F16,           "F16"             },
            { F17,           "F17"             },
            { F18,           "F18"             },
            { F19,           "F19"             },
            { F20,           "F20"             },
            { F21,           "F21"             },
            { F22,           "F22"             },
            { F23,           "F23"             },
            { F24,           "F24"             },
            { F25,           "F25"             },
            { KP0,           "Keypad 0"        },
            { KP1,           "Keypad 1"        },
            { KP2,           "Keypad 2"        },
            { KP3,           "Keypad 3"        },
            { KP4,           "Keypad 4"        },
            { KP5,           "Keypad 5"        },
            { KP6,           "Keypad 6"        },
            { KP7,           "Keypad 7"        },
            { KP8,           "Keypad 8"        },
            { KP9,           "Keypad 9"        },
            { KPDecimal,     "Keypad ."        },
            { KPDivide,	     "Keypad /"        },
            { KPMultiply,    "Keypad *"        },
            { KPSubtract,    "Keypad -"        },
            { KPAdd,         "Keypad +"        },
            { KPEnter,       "Keypad Enter"    },
            { KPEqual,       "Keypad ="        },
            { LeftShift,     "Left Shift"      },
            { LeftControl,   "Left Control"    },
            { LeftAlt,       "Left Alt"        },
            { LeftSuper,	 "Left Super"      },
            { RightShift,    "Right Shift"     },
            { RightControl,  "Right Control"   },
            { RightAlt,      "Right Alt"       },
            { RightSuper,    "Right Super"     },
            { Menu,          "Menu"            },
        };

        constexpr std::string_view ToString(KeyCode code) { return c_CodeToStringMap[code].codeStr; }

        constexpr KeyCode FromString(std::string_view code)
        {
            for (auto& pair : c_CodeToStringMap)
                if (pair.codeStr == code)
                    return pair.code;

            HE_CORE_VERIFY(false);
            return -1;
        }
    }

    constexpr CodeStrPair c_EventTypeMap[] = {
        { (int)EventType::None,                  "None"                   },
        { (int)EventType::WindowClose,           "Window Close"           },
        { (int)EventType::WindowResize,          "Window Resize"          },
        { (int)EventType::WindowFocus,           "Window Focus"           },
        { (int)EventType::WindowLostFocus,       "Window LostFocus"       },
        { (int)EventType::WindowMoved,           "Window Moved"           },
        { (int)EventType::WindowDrop,            "Window Drop"            },
        { (int)EventType::WindowContentScale,    "Window ContentScale"    },
        { (int)EventType::WindowMaximize,        "Window Maximize"        },
        { (int)EventType::WindowMinimized,       "Window Minimized"       },
        { (int)EventType::KeyPressed,            "Key Pressed"            },
        { (int)EventType::KeyReleased,           "Key Released"           },
        { (int)EventType::KeyTyped,              "Key Typed"              },
        { (int)EventType::MouseButtonPressed,    "Mouse Button Pressed"   },
        { (int)EventType::MouseButtonReleased,   "Mouse Button Released"  },
        { (int)EventType::MouseMoved,            "Mouse Moved"            },
        { (int)EventType::MouseScrolled,         "Mouse Scrolled"         },
        { (int)EventType::MouseEnter,            "Mouse Enter"            },
        { (int)EventType::GamepadButtonPressed,  "Gamepad Button Pressed" },
        { (int)EventType::GamepadButtonReleased, "Gamepad ButtonReleased" },
        { (int)EventType::GamepadAxisMoved,      "Gamepad Axis Moved"     },
        { (int)EventType::GamepadConnected,      "Gamepad Connected"      },
    };

    constexpr std::string_view ToString(EventType code) { return c_EventTypeMap[int(code)].codeStr; }

    constexpr EventType FromStringToEventType(std::string_view code)
    {
        for (auto& pair : c_EventTypeMap)
            if (pair.codeStr == code)
                return (EventType)pair.code;

        HE_CORE_VERIFY(false);
        return EventType::None;
    }

    constexpr CodeStrPair c_EventCategoryMap[] = {
        { EventCategoryApplication,   "Application"    },
        { EventCategoryInput,         "Input"          },
        { EventCategoryKeyboard,      "Keyboard"       },
        { EventCategoryMouse,         "Mouse"          },
        { EventCategoryMouseButton,   "Mouse Button"   },
        { EventCategoryJoystick,      "Joystick"       },
        { EventCategoryGamepadButton, "Gamepad Button" },
        { EventCategoryGamepadAxis,   "Gamepad Axis"   },
    };

    constexpr std::string_view ToString(EventCategory code) { return c_EventCategoryMap[code].codeStr; }

    constexpr EventCategory FromStringToEventCategory(std::string_view code)
    {
        for (auto& pair : c_EventCategoryMap)
            if (pair.codeStr == code)
                return (EventCategory)pair.code;

        HE_CORE_VERIFY(false);
        return EventCategory::EventCategoryNone;
    }
}

namespace HE {

    void Input::SerializeKeyBindings(const std::filesystem::path& filePath)
    {
        std::ofstream file(filePath);
        if (!file.is_open())
        {
            HE_ERROR("Input::SerializeKeyBindings : Unable to open file for writing, {}", filePath.string());
        }

        std::ostringstream os;
        os << "{\n";
        os << "\t\"bindings\" : [\n";


        for (int bindingIndex = 0; auto & [key, desc] : Input::GetKeyBindings())
        {
            if (bindingIndex != 0) os << ",\n"; bindingIndex++;

            os << "\t\t{\n";
            os << "\t\t\t\"name\" : \"" << desc.name << "\",\n";
            os << "\t\t\t\"modifiers\" : [ ";
            for (int i = 0; i < desc.modifiers.size(); i++)
            {
                if (desc.modifiers[i] != 0)
                {
                    if (i > 0) os << ", ";
                    os << "\"" << Key::ToString(desc.modifiers[i]) << "\"";
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
                auto modifiers = desc["modifiers"].get_array();
                std::array<uint16_t, c_MaxModifierCount> arr = {};
                if (!modifiers.error())
                {
                    for (int i = 0; i < modifiers.size(); i++)
                        arr[i] = Key::FromString(modifiers.at(i).get_c_str().value());
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
                    .modifiers = arr,
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
        return { NFD_WINDOW_HANDLE_TYPE_WINDOWS, HE::Application::GetWindow().GetNativeHandle() };
#elif HE_PLATFORM_LINUX
        return { NFD_WINDOW_HANDLE_TYPE_X11, HE::Application::GetWindow().GetNativeHandle() };
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
