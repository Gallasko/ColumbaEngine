#include "audiosystem.h"

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

        setNumberOfChannel(BASE_NUMBER_OF_CHANNELS);
    }

    void AudioSystem::setNumberOfChannel(unsigned int channel)
    {
        LOG_THIS_MEMBER(DOM);

        if (nbSoundEffectChannels < channel)
        {
            for (unsigned int i = channel; i < nbSoundEffectChannels; i++)
            {
                LOG_INFO(DOM, "Removing channel: " << i);

                Mix_HaltChannel(i);
                Mix_FreeChunk(channels[i]);
            }
        }

        if (nbSoundEffectChannels == 0)
        {
            LOG_INFO(DOM, "Increasing sound effect channel to size: " << channel);

            channels = new Mix_Chunk*[channel];

            // Set unallocated channels to nullptr
            for (unsigned int i = 0; i < channel; i++)
            {
                channels[i] = nullptr;
            }
        }
        else if (nbSoundEffectChannels > channel)
        {
            LOG_INFO(DOM, "Increasing sound effect channel to size: " << channel);

            auto tmp = new Mix_Chunk*[channel];

            memcpy(tmp, channels, nbSoundEffectChannels * sizeof(Mix_Chunk*));

            delete channels;

            channels = tmp;

            // Set unallocated channels to nullptr
            for (unsigned int i = nbSoundEffectChannels; i < channel; i++)
            {
                channels[i] = nullptr;
            }
        }

        Mix_AllocateChannels(channel);

        nbSoundEffectChannels = channel;

        soundEffectChannelEnable = true;
    }

    void AudioSystem::closeSDLMixer()
    {
        LOG_THIS_MEMBER(DOM);

        for (unsigned int i = 0; i < nbSoundEffectChannels; i++)
        {
            if (channels[i] != nullptr)
                Mix_FreeChunk(channels[i]);
        }

        if (soundEffectChannelEnable)
            delete[] channels;

        Mix_FreeMusic(music);
        Mix_CloseAudio();
        Mix_Quit();
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
        else if (static_cast<unsigned int>(channel) > nbSoundEffectChannels)
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