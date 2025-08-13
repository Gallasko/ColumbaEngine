#include "stdafx.h"

#include "savemanager.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <SDL2/SDL.h>
#include <SDL_opengles2.h>
// #include <SDL_opengl_glext.h>
// #include <GLES2/gl2.h>
// #include <GLFW/glfw3.h>
#else
#ifdef __linux__
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#elif _WIN32
#include <SDL.h>
#include <SDL_opengl.h>
#endif
#include <GL/gl.h>
#endif

#include "logger.h"
#include "serialization.h"
#include "pgconstant.h"

#include "Files/filemanager.h"
#include "Files/fileparser.h"

#include <iostream>
namespace pg
{
    namespace
    {
        /** Name of the current domain (for logging purposes)*/
        static constexpr char const * DOM = "SaveSystem";
    }

    template <>
    void serialize(Archive& archive, const SaveData& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization(SaveData::getType());

        serialize(archive, "nbValue", value.elements.size());

        size_t i = 0;

        for (const auto& element : value.elements)
        {
            auto str = std::to_string(i);

            serialize(archive, "id" + str, element.first);
            serialize(archive, "value" + str, element.second);

            ++i;
        }

        archive.endSerialization();
    }

    template <>
    SaveData deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing SaveData");

            SaveData data;

            auto nbValues = deserialize<size_t>(serializedString["nbValue"]);

            for (size_t i = 0; i < nbValues; ++i)
            {
                auto str = std::to_string(i);

                auto id = deserialize<std::string>(serializedString["id" + str]);
                auto value = deserialize<ElementType>(serializedString["value" + str]);

                data.elements.emplace(id, value);
            }

            return data;
        }

        return SaveData{};
    }

    SaveManager::SaveManager(const std::string& savePath)
    {
        LOG_THIS_MEMBER(DOM);

        loadSave(savePath);
    }

    void SaveManager::execute()
    {
        if (needSave)
        {
            save();
            needSave = false;
        }
    }

    void SaveManager::onProcessEvent(const SaveElementEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        currentSave.data.elements[event.name] = event.element;

        needSave = true;
    }

    ElementType SaveManager::getValue(const std::string& id) const
    {
        LOG_THIS_MEMBER(DOM);

        const auto& it = currentSave.data.elements.find(id);

        if (it != currentSave.data.elements.end())
        {
            return it->second;
        }

        return ElementType{};
    }

    void SaveManager::loadSave(const std::string& savePath)
    {
        LOG_THIS_MEMBER(DOM);

        auto saveFile = UniversalFileAccessor::openTextFile(savePath);

        SaveFile newSave;

        newSave.file = saveFile;

        serializer.setFile(saveFile);

        newSave.data = serializer.deserializeObject<SaveData>("savedata");

        currentSave = newSave;
    }

    void SaveManager::save()
    {
        LOG_INFO(DOM, "Saving data to disk");

        std::cout << "Saving data to disk..." << std::endl;

        serializer.serializeObject("savedata", currentSave.data);

        std::cout << "Data saved syncing..." << std::endl;

// Todo this don't work !
// #ifdef __EMSCRIPTEN__
//         EM_ASM(
//             FS.syncfs(function (err) {
//                 assert(!err);
//             });
//         );
// #endif
    }
}