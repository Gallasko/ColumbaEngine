#include "stdafx.h"

#include "soundbank.h"

#include <algorithm>
#include <cmath>
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

    void SoundBank::play(pg::EntitySystem* ecs, const std::string& name, float distance, float pan, float behind)
    {
        if (not ecs or name.empty() or distance > FAR)
            return;

        auto it = sounds.find(name);

        if (it == sounds.end())
            return;

        const auto& list = distance <= NEAR ? it->second.near : it->second.far;

        if (list.empty())
            return;

        // Each ear: the far one loses up to PAN_DEPTH, both a little behind the listener
        pan = std::clamp(pan, -1.0f, 1.0f);
        const float back = 1.0f - BEHIND_DROP * std::clamp(behind, 0.0f, 1.0f);
        const float left = (1.0f - PAN_DEPTH * std::max(0.0f, pan)) * back;
        const float right = (1.0f - PAN_DEPTH * std::max(0.0f, -pan)) * back;

        // The audio system reads its effects on its own queue: a path is enough
        pg::PlaySoundEffect play{list[rng() % list.size()]};
        play.left = static_cast<uint8_t>(std::lround(255.0f * left));
        play.right = static_cast<uint8_t>(std::lround(255.0f * right));
        ecs->sendEvent(play);
    }
}
