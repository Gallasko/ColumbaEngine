Audio
=====

Audio (``src/Engine/Audio/``) is fully event-driven on top of SDL2_mixer. An ``AudioSystem`` is registered automatically at window creation, so in game code you only ever send events — no setup, no handles to manage.

Playing sounds
--------------

.. code-block:: cpp

    #include "Audio/audiosystem.h"

    // music: one stream, loops = -1 means forever
    ecsRef->sendEvent(StartAudio{"res/audio/music.ogg", -1});
    ecsRef->sendEvent(StopAudio{});
    ecsRef->sendEvent(PauseAudio{});
    ecsRef->sendEvent(ResumeAudio{});

    // sound effects: loops = 0 means play once, channel = -1 means first free channel
    ecsRef->sendEvent(PlaySoundEffect{"res/audio/hit.ogg"});
    ecsRef->sendEvent(PlaySoundEffect{"res/audio/rain.ogg", /*loops*/ 3, /*channel*/ 2});

    // volume: floats in 0.0 - 1.0; effective volume is master x (music | sfx)
    ecsRef->sendEvent(SetMasterVolume{0.8f});
    ecsRef->sendEvent(SetMusicVolume{0.3f});
    ecsRef->sendEvent(SetSoundEffectsVolume{0.6f});

PgScript gets the same surface via the audio module: ``playAudio``, ``stopAudio``, ``pauseAudio``, ``resumeAudio``, ``playSoundEffect``.

The model
---------

- **Music** is a single stream (``Mix_Music``); starting a new track frees the previous one. Not cached.
- **Sound effects** play on mixing channels — **6 by default** (``AudioSystem::setNumberOfChannel`` to change). If every channel is busy, the effect is *silently skipped*, not queued — size your channel count for your worst-case burst.
- Effect chunks are **cached by path** after first load, so repeated ``PlaySoundEffect`` events are cheap. First play of a large file does disk IO — consider firing a muted play at load time if that matters.
- ``PlaySoundEffect`` is processed as a queued event (in the audio system's own frame slot); the others are handled at event-drain time.
- Device: opened at 96 kHz with a 1024-sample buffer. OGG and WAV are the safe formats (MP3 support depends on the SDL2_mixer build — ``Mix_Init(MIX_INIT_MP3)`` is currently not called).

Volumes default to ``master 1.0, music 0.1, sfx 0.1`` — turn them up in your init if things sound quiet.

Platform notes
--------------

- **Emscripten**: same API, same constants; paths get the leading ``/`` like all web asset paths (``"/res/audio/hit.ogg"``). Browsers block audio until the first user gesture — start music from a click/keypress, not from ``init()``.
- **Shutdown ordering** (engine programmers): the ``AudioSystem`` is owned by the ECS, but ``closeSDLMixer()`` must run *before* ``delete ecs`` — the window teardown does this ordering on purpose (see the comment in ``window.cpp``); reproduce it in any custom shutdown path or ASan will flag a use-after-free.

Current limitations
-------------------

No spatial/positional audio, no mixing graph or effects bus, no crossfades — the surface is deliberately small. For a jam game: one music stream + cached one-shot effects covers the standard needs; anything fancier is game code today.
