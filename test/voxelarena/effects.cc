#include "stdafx.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "Core/content.h"
#include "Core/effects.h"

using namespace arena;

TEST(arena_effects, the_shipped_sounds_and_effects_read_cleanly)
{
    const Content c = loadContent(VOXELARENA_MODS_DIR);
    ContentReport report;
    const auto sounds = readSounds(c.sounds, report);
    const auto effects = readEffects(c.effects, sounds, report);

    EXPECT_TRUE(report.errors.empty()) << (report.errors.empty() ? "" : report.errors[0].str());
    EXPECT_TRUE(report.warnings.empty()) << (report.warnings.empty() ? "" : report.warnings[0].str());
    EXPECT_TRUE(sounds.count("hit"));
    ASSERT_TRUE(effects.count("break"));
    EXPECT_TRUE(effects.at("break").particles.eventColor);
    EXPECT_EQ(effects.at("break").sound, "break");
}

TEST(arena_effects, a_recipe_becomes_a_wav_of_its_length_that_starts_and_ends_quietly)
{
    SoundRecipe r;
    r.wave = "square";
    r.duration = 0.25f;
    r.frequency = 400.0f;
    r.frequencyEnd = 100.0f;
    r.tone = 0.5f;
    r.decay = 0.08f;

    const auto a = synthesise(r, 1);
    const auto b = synthesise(r, 2);

    EXPECT_EQ(a.size(), static_cast<size_t>(0.25f * SAMPLE_RATE));
    EXPECT_NE(a, b);   // variants differ
    EXPECT_EQ(a.back(), 0);

    int16_t peak = 0;
    for (auto s : a)
        peak = std::max<int16_t>(peak, static_cast<int16_t>(std::abs(s)));
    EXPECT_GT(peak, 1000);

    const auto quiet = synthesise(r, 1, 0.25f);
    int16_t quietPeak = 0;
    for (auto s : quiet)
        quietPeak = std::max<int16_t>(quietPeak, static_cast<int16_t>(std::abs(s)));
    EXPECT_LT(quietPeak, peak / 2);

    const std::string path = (std::filesystem::temp_directory_path() / "voxelarena-test.wav").string();
    ASSERT_TRUE(writeWav(path, a));
    EXPECT_EQ(std::filesystem::file_size(path), 44 + a.size() * 2);

    std::ifstream in(path, std::ios::binary);
    char riff[4];
    in.read(riff, 4);
    EXPECT_EQ(std::string(riff, 4), "RIFF");
}

TEST(arena_effects, layered_models_split_into_animated_parts)
{
    VoxelModel m;
    ContentReport r;
    ASSERT_TRUE(loadVoxelModel(std::string(VOXELARENA_MODS_DIR) + "/base/models/player.vxl.json", m, r));

    std::vector<std::string> names;
    for (const auto& p : m.parts)
        names.push_back(p.name);

    EXPECT_EQ(m.parts.size(), 6u);
    EXPECT_NE(std::find(names.begin(), names.end(), "leg_l"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "head"), names.end());

    for (const auto& p : m.parts)
    {
        if (p.name == "leg_l")
            EXPECT_FLOAT_EQ(p.joint.y, 6.0f);   // the top of the leg (cells 0 to 5)
        if (p.name == "head")
            EXPECT_FLOAT_EQ(p.joint.y, 11.0f);  // the bottom of the head
    }

    // The dummy has one layer: one rigid part
    VoxelModel d;
    ASSERT_TRUE(loadVoxelModel(std::string(VOXELARENA_MODS_DIR) + "/base/models/dummy.vxl.json", d, r));
    EXPECT_EQ(d.parts.size(), 1u);
}
