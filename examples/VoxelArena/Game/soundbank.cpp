#include "stdafx.h"

#include "soundbank.h"

#include <filesystem>

#include "Audio/audiosystem.h"
#include "ECS/entitysystem.h"

namespace fs = std::filesystem;

namespace arena
{
    void SoundBank::load(const std::map<std::string, SoundRecipe>& recipes, const std::string& cacheDir, ContentReport& report)
    {
        sounds.clear();

        std::error_code ec;
        fs::create_directories(cacheDir, ec);

        for (const auto& [name, r] : recipes)
        {
            Variants v;

            if (not r.file.empty())
            {
                if (not fs::exists(r.file, ec))
                {
                    report.error(r.file, 0, "sound \"" + name + "\": the file does not exist");
                    continue;
                }

                v.near.push_back(r.file);
                v.far.push_back(r.file);
            }
            else
            {
                for (int i = 0; i < r.variants; ++i)
                {
                    const std::string base = (fs::path(cacheDir) / (name + "_" + std::to_string(i))).string();

                    if (writeWav(base + ".wav", synthesise(r, static_cast<uint32_t>(i + 1))) and writeWav(base + "_far.wav", synthesise(r, static_cast<uint32_t>(i + 1), 0.4f)))
                    {
                        v.near.push_back(base + ".wav");
                        v.far.push_back(base + "_far.wav");
                    }
                    else
                    {
                        report.error(cacheDir, 0, "cannot write the sound \"" + name + "\"");
                    }
                }
            }

            sounds[name] = std::move(v);
        }
    }

    void SoundBank::play(pg::EntitySystem* ecs, const std::string& name, float distance)
    {
        if (not ecs or name.empty() or distance > FAR)
            return;

        auto it = sounds.find(name);

        if (it == sounds.end())
            return;

        const auto& list = distance <= NEAR ? it->second.near : it->second.far;

        if (list.empty())
            return;

        // The audio system reads its effects on its own queue: a path is enough
        ecs->sendEvent(pg::PlaySoundEffect{list[rng() % list.size()]});
    }
}
