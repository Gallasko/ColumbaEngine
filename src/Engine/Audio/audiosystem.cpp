#include "stdafx.h"

#include "audiosystem.h"

#include <algorithm>

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Audio";

        unsigned int BASE_NUMBER_OF_CHANNELS = 6;
    }

    void AudioSystem::init()
    {
        LOG_THIS_MEMBER(DOM);

        // if (not Mix_Init(MIX_INIT_MP3))
        // {
        //     printf("Error audio: %s\n", Mix_GetError());
        // }

        if (Mix_OpenAudio(96000, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 1024) < 0)
        {
            printf("Error audio: %s\n", Mix_GetError());
            LOG_ERROR(DOM, "Erreur initialisation SDL_mixer : " << Mix_GetError());
            return;
        }

        printf("Audio is initialized\n");

        audioOpen = true;

        // A game may have asked for more channels before the device was open
        setNumberOfChannel(std::max(BASE_NUMBER_OF_CHANNELS, requestedChannels));
    }

    void AudioSystem::setNumberOfChannel(unsigned int channel)
    {
        LOG_THIS_MEMBER(DOM);

        // Before init the mixer is not open: remember the request, init applies it
        if (not audioOpen)
        {
            requestedChannels = channel;
            return;
        }

        if (channel == nbSoundEffectChannels and channels)
            return;

        LOG_INFO(DOM, "Sound effect channels: " << nbSoundEffectChannels << " -> " << channel);

        // Shrinking: stop the channels that go away before the mixer forgets them
        for (unsigned int i = channel; i < nbSoundEffectChannels; i++)
            Mix_HaltChannel(i);

        // Keep what was playing on the channels that stay, clear the new ones
        auto resized = new Mix_Chunk*[channel];

        for (unsigned int i = 0; i < channel; i++)
            resized[i] = (channels and i < nbSoundEffectChannels) ? channels[i] : nullptr;

        delete[] channels;
        channels = resized;

        Mix_AllocateChannels(static_cast<int>(channel));

        nbSoundEffectChannels = channel;
        requestedChannels = channel;

        soundEffectChannelEnable = channel > 0;
    }

    void AudioSystem::closeSDLMixer()
    {
        LOG_THIS_MEMBER(DOM);

        Mix_HaltMusic();
        Mix_HaltChannel(-1);

        // 2. Free all loaded effects (owned by sfxDict)
        for (auto &pair : sfxDict)
        {
            Mix_FreeChunk(pair.second);
        }

        sfxDict.clear();

        // 3. Free the music
        if (music)
            Mix_FreeMusic(music);

        // 4. Close SDL_mixer
        Mix_CloseAudio();
        Mix_Quit();

        delete[] channels;

        channels = nullptr;
        nbSoundEffectChannels = 0;
        soundEffectChannelEnable = false;
        audioOpen = false;
    }

    void AudioSystem::onEvent(const StartAudio& event)
    {
        LOG_THIS_MEMBER(DOM);

        if (music != nullptr)
        {
            Mix_HaltMusic();
            Mix_FreeMusic(music);
        }

        music = Mix_LoadMUS(event.song.c_str());

        if (music == nullptr)
        {
            printf("Error audio: %s\n", Mix_GetError());
            LOG_ERROR(DOM, "Erreur chargement de la musique : " << Mix_GetError());
            return;
        }

        Mix_PlayMusic(music, event.loops);

        // Todo
        Mix_VolumeMusic(MIX_MAX_VOLUME * masterVolume * musicVolume);
        // Mix_VolumeMusic(MIX_MAX_VOLUME);
    }

    void AudioSystem::onEvent(const StopAudio&)
    {
        LOG_THIS_MEMBER(DOM);

        // Stop the music
        Mix_HaltMusic();
    }

    void AudioSystem::onEvent(const PauseAudio&)
    {
        LOG_THIS_MEMBER(DOM);

        // If music is not pause
        if (Mix_PausedMusic() == 0)
        {
            // We pause it
            Mix_PauseMusic();
        }
    }

    void AudioSystem::onEvent(const ResumeAudio&)
    {
        LOG_THIS_MEMBER(DOM);

        // If music is in pause
        if (Mix_PausedMusic() == 1)
        {
            // We resume it
            Mix_ResumeMusic();
        }
    }

    void AudioSystem::onProcessEvent(const PlaySoundEffect& event)
    {
        LOG_THIS_MEMBER(DOM);

        if (not soundEffectChannelEnable)
        {
            LOG_ERROR(DOM, "Sound effects are not enabled");
            return;
        }

        auto it = sfxDict.find(event.effect);

        Mix_Chunk *effect;

        if (it != sfxDict.end())
        {
            effect = it->second;
        }
        else
        {
            effect = Mix_LoadWAV(event.effect.c_str());
            sfxDict.emplace(event.effect, effect);
        }

        if (effect == nullptr)
        {
            LOG_ERROR(DOM, "Could not load effect: " << event.effect.c_str());
            return;
        }

        Mix_VolumeChunk(effect, MIX_MAX_VOLUME * masterVolume * sEffectVolume);

        auto channel = Mix_PlayChannel(event.channel, effect, event.loops);

        if (channel == -1)
        {
            // This is not an error just not enough channel so we skip the sfx
            LOG_MILE(DOM, "Could not play effect: " << event.effect.c_str());
        }
        else if (static_cast<unsigned int>(channel) >= nbSoundEffectChannels)
        {
            LOG_ERROR(DOM, "Error in channel given by Mix_PlayChannel, given:  " << channel << " when only " << nbSoundEffectChannels << " are available !");
        }
        else
        {
            channels[channel] = effect;
        }
    }

    void AudioSystem::updateVolume()
    {
        LOG_THIS_MEMBER(DOM);

        Mix_VolumeMusic(MIX_MAX_VOLUME * masterVolume * musicVolume);

        for (unsigned int i = 0; i < nbSoundEffectChannels; i++)
        {
            if (channels[i] != nullptr)
                Mix_VolumeChunk(channels[i], MIX_MAX_VOLUME * masterVolume * sEffectVolume);
        }
    }

    void AudioSystem::execute()
    {
    }

}