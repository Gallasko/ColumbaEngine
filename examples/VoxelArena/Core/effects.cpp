#include "effects.h"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace arena
{
    namespace
    {
        constexpr float TAU = 6.28318530718f;

        uint32_t xorshift(uint32_t& s)
        {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            return s;
        }

        float wave(const std::string& kind, float phase)
        {
            const float p = phase - std::floor(phase);

            if (kind == "square")
                return p < 0.5f ? 1.0f : -1.0f;
            if (kind == "saw")
                return 2.0f * p - 1.0f;
            if (kind == "triangle")
                return 4.0f * std::fabs(p - 0.5f) - 1.0f;

            return std::sin(phase * TAU);
        }

        void put32(std::ofstream& out, uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); }
        void put16(std::ofstream& out, uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); }
    }

    std::vector<int16_t> synthesise(const SoundRecipe& r, uint32_t seed, float loudness)
    {
        uint32_t rng = seed * 2654435761u + 12345u;
        if (rng == 0)
            rng = 1;

        // Each variant a little higher or lower
        const float jitter = 1.0f + r.pitchJitter * ((static_cast<float>(xorshift(rng) % 2001) / 1000.0f) - 1.0f);
        const float f0 = std::max(1.0f, r.frequency * jitter);
        const float f1 = std::max(1.0f, r.frequencyEnd * jitter);

        const int count = std::max(1, static_cast<int>(r.duration * SAMPLE_RATE));
        const int fade = std::min(count / 4, SAMPLE_RATE / 200);   // 5 ms at the end: no click
        const float smoothing = 1.0f - std::clamp(r.lowpass, 0.0f, 1.0f) * 0.97f;

        std::vector<int16_t> out(static_cast<size_t>(count));
        float phase = 0.0f;
        float filtered = 0.0f;

        for (int i = 0; i < count; ++i)
        {
            const float t = static_cast<float>(i) / SAMPLE_RATE;
            const float k = t / std::max(1e-4f, r.duration);

            // Glide (exponential, as the ear hears pitch)
            phase += f0 * std::pow(f1 / f0, k) / SAMPLE_RATE;

            float env = t < r.attack ? t / std::max(1e-5f, r.attack) : std::exp(-(t - r.attack) / std::max(1e-4f, r.decay));

            if (i >= count - fade)
                env *= static_cast<float>(count - 1 - i) / static_cast<float>(std::max(1, fade));

            const float noise = static_cast<float>(xorshift(rng) % 20001) / 10000.0f - 1.0f;
            const float x = wave(r.wave, phase) * r.tone + noise * (1.0f - r.tone);

            filtered += (x - filtered) * smoothing;

            const float v = std::clamp(filtered * env * r.volume * loudness, -1.0f, 1.0f);
            out[static_cast<size_t>(i)] = static_cast<int16_t>(v * 32000.0f);
        }

        return out;
    }

    bool writeWav(const std::string& path, const std::vector<int16_t>& samples)
    {
        std::ofstream out(path, std::ios::binary);

        if (not out)
            return false;

        const uint32_t bytes = static_cast<uint32_t>(samples.size() * sizeof(int16_t));

        out.write("RIFF", 4);
        put32(out, 36 + bytes);
        out.write("WAVE", 4);
        out.write("fmt ", 4);
        put32(out, 16);                 // PCM header size
        put16(out, 1);                  // PCM
        put16(out, 1);                  // mono
        put32(out, SAMPLE_RATE);
        put32(out, SAMPLE_RATE * 2);    // byte rate
        put16(out, 2);                  // block align
        put16(out, 16);                 // bits per sample
        out.write("data", 4);
        put32(out, bytes);
        out.write(reinterpret_cast<const char*>(samples.data()), bytes);

        return static_cast<bool>(out);
    }

    std::map<std::string, SoundRecipe> readSounds(const Json& tree, ContentReport& report)
    {
        std::map<std::string, SoundRecipe> out;

        if (not tree.isObject())
            return out;

        for (const auto& [name, j] : tree.object)
        {
            SoundRecipe r;
            r.name = name;
            r.file = j.str("file", "");
            r.wave = j.str("wave", r.wave);
            r.duration = static_cast<float>(j.num("duration", r.duration));
            r.frequency = static_cast<float>(j.num("frequency", r.frequency));
            r.frequencyEnd = static_cast<float>(j.num("frequencyEnd", r.frequency));
            r.tone = static_cast<float>(j.num("tone", r.tone));
            r.attack = static_cast<float>(j.num("attack", r.attack));
            r.decay = static_cast<float>(j.num("decay", r.decay));
            r.lowpass = static_cast<float>(j.num("lowpass", r.lowpass));
            r.volume = static_cast<float>(j.num("volume", r.volume));
            r.variants = std::clamp(static_cast<int>(j.num("variants", r.variants)), 1, 8);
            r.pitchJitter = static_cast<float>(j.num("pitchJitter", r.pitchJitter));

            if (r.wave != "sine" and r.wave != "square" and r.wave != "saw" and r.wave != "triangle")
                report.warn("sounds.json", j.line, "sound \"" + name + "\": unknown wave \"" + r.wave + "\" (sine, square, saw, triangle)");

            if (r.duration <= 0.0f or r.duration > 5.0f)
                report.error("sounds.json", j.line, "sound \"" + name + "\": duration must be between 0 and 5 s");
            else
                out[name] = r;
        }

        return out;
    }

    std::map<std::string, EffectDef> readEffects(const Json& tree, const std::map<std::string, SoundRecipe>& sounds, ContentReport& report)
    {
        std::map<std::string, EffectDef> out;

        if (not tree.isObject())
            return out;

        for (const auto& [name, j] : tree.object)
        {
            EffectDef e;
            e.sound = j.str("sound", "");
            e.shake = static_cast<float>(j.num("shake", 0.0));

            if (not e.sound.empty() and not sounds.count(e.sound))
                report.error("effects.json", j.line, "effect \"" + name + "\" plays the sound \"" + e.sound + "\", which sounds.json does not have");

            if (const Json* p = j.find("particles"))
            {
                ParticlePreset& pp = e.particles;
                pp.count = std::clamp(static_cast<int>(p->num("count", 0)), 0, 200);
                pp.speed = static_cast<float>(p->num("speed", pp.speed));
                pp.up = static_cast<float>(p->num("up", pp.up));
                pp.size = static_cast<float>(p->num("size", pp.size));
                pp.life = static_cast<float>(p->num("life", pp.life));
                pp.gravity = static_cast<float>(p->num("gravity", pp.gravity));

                if (const Json* c = p->find("color"))
                {
                    if (c->isString() and c->string == "event")
                        pp.eventColor = true;
                    else if (c->isArray() and c->array.size() >= 3)
                        pp.color = glm::vec3(c->array[0].number, c->array[1].number, c->array[2].number) / 255.0f;
                    else
                        report.error("effects.json", c->line, "effect \"" + name + "\": particle color is [r, g, b] or \"event\"");
                }
            }

            out[name] = e;
        }

        return out;
    }
}
