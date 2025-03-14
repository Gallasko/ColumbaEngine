#pragma once

#include "ECS/system.h"

#include <SDL_mixer.h>


namespace pg
{
    struct StartAudio
    { 
        StartAudio(const std::string song, int loops = -1) : song(song), loops(loops) {}
        StartAudio(const StartAudio& other) : song(other.song), loops(other.loops) {}
        std::string song; int loops = -1;
    };
    struct StopAudio {};
    struct PauseAudio {};
    struct ResumeAudio {};

    struct SetMasterVolume       { float volume; };
    struct SetMusicVolume        { float volume; };
    struct SetSoundEffectsVolume { float volume; };

    struct PlaySoundEffect { std::string effect; int loops = 0; int channel = -1; };

    struct AudioSystem : public System<Listener<StartAudio>, Listener<StopAudio>, Listener<PauseAudio>, Listener<ResumeAudio>, Listener<PlaySoundEffect>, Listener<SetMasterVolume>, Listener<SetMusicVolume>, Listener<SetSoundEffectsVolume>, InitSys>
    {
        virtual void init() override;

        void setNumberOfChannel(unsigned int channel);

        void closeSDLMixer();

        virtual void onEvent(const StartAudio& event) override;
        virtual void onEvent(const StopAudio& event) override;
        virtual void onEvent(const PauseAudio& event) override;
        virtual void onEvent(const ResumeAudio& event) override;

        virtual void onEvent(const PlaySoundEffect& event) override;

        virtual void onEvent(const SetMasterVolume& event) override       { masterVolume = event.volume;  updateVolume(); };
        virtual void onEvent(const SetMusicVolume& event) override        { musicVolume = event.volume;   updateVolume(); };
        virtual void onEvent(const SetSoundEffectsVolume& event) override { sEffectVolume = event.volume; updateVolume(); };

        virtual void execute() override;

        void updateVolume();

        virtual std::string getSystemName() const override { return "Audio System"; }

        Mix_Music* music = nullptr;

        bool soundEffectChannelEnable = false;
        unsigned int nbSoundEffectChannels = 0;
        Mix_Chunk **channels = nullptr;

        // Pourcentage of the master volume (Percentage calculated from MIX_MAX_VOLUME)
        float masterVolume = 1.0f;

        // Pourcentage of the main music volume
        float musicVolume = 0.1f;

        // Pourcentage of the sound effect volume
        float sEffectVolume = 0.1f;

        std::unordered_map<std::string, Mix_Chunk*> sfxDict;

        std::queue<PlaySoundEffect> sfxQueue;
    };
}