#include "soundplayer.h"
#include <stdlib.h>
#include <AL/al.h>
#include <AL/alc.h>

struct sound_player {
    ALCdevice* device;       /* Used playback device */
    ALCcontext* context;     /* Used audio render context */
    ALCenum last_error_code; /* Code from the last error that occured */
};

static void check_al_error()
{
    ALCenum error = alGetError();
    if (error != AL_NO_ERROR) {
        /* TODO */
    }
}

sound_player_t* sound_player_create()
{
    struct sound_player* sp = malloc(sizeof(struct sound_player));
    sp->device = alcOpenDevice(0);
    if (!sp->device)
        check_al_error();

    sp->context = alcCreateContext(sp->device, 0);
    if (!alcMakeContextCurrent(sp->context))
        check_al_error();
    return sp;
}

void sound_player_destroy(sound_player_t* sp)
{
    alcMakeContextCurrent(0);
    alcDestroyContext(sp->context);
    alcCloseDevice(sp->device);
    free(sp);
}

void get_devices(sound_player_t* sp)
{
    (void)sp;
    /*
    // Names of the playback devices available
    std::vector<std::string> playbackDevices;

    // Check if device enumeration is possible
    ALboolean enumeration;
    enumeration = alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT");
    if (enumeration == AL_FALSE) // When this happens listing audio devices only return the default device
        return playbackDevices;
    else
    {
        // String devices are separated with a null char and the list is terminated by two null chars
        const ALCchar* devices = alcGetString(nullptr, ALC_DEVICE_SPECIFIER);
        const ALCchar* device = devices;
        const ALCchar* next = devices + 1;
        size_t len = 0;

        while (device && *device != '\0' && *next != '\0')
        {
            playbackDevices.push_back(device);
            len = strlen(device);
            device += (len + 1);
            next += (len + 2);
        }
    }
    return playbackDevices;
    */
}

void set_playback_device(const char* device)
{
    (void)device;
    /* TODO */
}

float get_master_volume(sound_player_t* sp)
{
    (void)sp;
    /* TODO */
    return 0.0f;
}

void set_master_volume(sound_player_t* sp, float vol)
{
    (void)sp;
    (void)vol;
    /* TODO */
}

void adjust_volume(sound_player_t* sp, float percent)
{
    (void)sp;
    (void)percent;
    /* TODO */
}

void mute(sound_player_t* sp, int mute)
{
    (void)sp;
    (void)mute;
    /* TODO */
}

int is_mute(sound_player_t* sp)
{
    (void)sp;
    /* TODO */
    return 0;
}

static int al_fmt_from_info(short channels, short bits_per_sample)
{
    int is_stereo = (channels > 1);
    switch (bits_per_sample) {
        case 16:
            if (is_stereo)
                return AL_FORMAT_STEREO16;
            else
                return AL_FORMAT_MONO16;
        case 8:
            if (is_stereo)
                return AL_FORMAT_STEREO8;
            else
                return AL_FORMAT_MONO8;
        default:
            return -1;
    }
}

void play(sound_player_t* sp, struct sound* snd)
{
    (void)sp;
    // Create audio source
    ALuint source;
    alGenSources(1, &source);

    // Set its properties
    alSourcef(source, AL_PITCH, 1);
    alSourcef(source, AL_GAIN, 1);
    alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alSourcei(source, AL_LOOPING, AL_FALSE);

    // Generate audio buffer object
    ALuint buffer;
    alGenBuffers(1, &buffer);

    // Load pcm data into buffer
    alBufferData(
        buffer,
        al_fmt_from_info(snd->channels, snd->bits_per_sample),
        snd->data, snd->data_sz,
        snd->samplerate);

    // Bind the source with its buffer
    alSourcei(source, AL_BUFFER, buffer);

    // Play
    alSourcePlay(source);

    /*
    // Cleanup thread
    std::thread t(
        [source, buffer, finishCb]()
        {
            // Wait till playing finishes
            ALint sourceState;
            alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
            while (sourceState == AL_PLAYING)
            {
                alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            // Release resources
            alDeleteSources(1, &source);
            alDeleteBuffers(1, &buffer);

            // Call finish callback
            finishCb();
        }
    );
    t.detach();
    */
}
