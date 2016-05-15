#include "assets/sound/soundload.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* The "RIFF" chunk descriptor */
struct wav_desc
{
    uint8_t riff[4];
    uint32_t size;
    uint8_t wave[4];
};

/* Describes the format of the sound info in the data sub-chunk */
struct wav_format
{
    uint8_t id[4];
    uint32_t size;
    uint16_t format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

/* Indicates the size of the sound info and contains the raw sound data */
struct wav_chunk
{
    uint8_t id[4];
    uint32_t size;
    uint8_t* data;
};

#define member_size(type, member) sizeof(((type *)0)->member)

#define parse_field(strct, field, inst, offset) \
    memcpy(                                     \
        &inst.field,                            \
        begin + offset,                         \
        member_size(strct, field)               \
    );

struct sound* sound_from_wav(const unsigned char* data, size_t sz) {
    (void) sz;

    /* Parse header */
    unsigned char* begin = (unsigned char*)data;

    /* Parse wav desc section */
    struct wav_desc wdesc;
    memset(&wdesc, 0, sizeof(struct wav_desc));
    parse_field(struct wav_desc, riff, wdesc, 0) /* Should be equal to "RIFF" */
    parse_field(struct wav_desc, size, wdesc, 4) /* Should be equal to the size of the entire file in bytes minus 8 bytes for the two fields already parsed */
    parse_field(struct wav_desc, wave, wdesc, 8) /* Should be equal to "WAVE" */

    /* Parse wav format section */
    struct wav_format wfmt;
    memset(&wfmt, 0, sizeof(struct wav_format));
    parse_field(struct wav_format, id, wfmt, 12)              /* Should be equal to "fmt " (note the space) */
    parse_field(struct wav_format, size, wfmt, 16)            /* Should be equal to 16 for PCM. This is the size of the rest of the subchunk which follows this number */
    parse_field(struct wav_format, format, wfmt, 20)          /* Should be equal to 1 for PCM. Other values indicate compression. */
    parse_field(struct wav_format, channels, wfmt, 22)        /* Mono = 1, Stereo = 2 */
    parse_field(struct wav_format, sample_rate, wfmt, 24)     /* 8000, 44100, etc. */
    parse_field(struct wav_format, byte_rate, wfmt, 28)       /* == SampleRate * NumChannels * BitsPerSample / 8 */
    parse_field(struct wav_format, block_align, wfmt, 32)     /* == NumChannels * BitsPerSample / 8 */
    parse_field(struct wav_format, bits_per_sample, wfmt, 34) /* 8 bits = 8, 16 bits = 16, etc. */

    /* Advance pointer */
    begin += 36;

    /* Skip extra parameters that might exist */
    if (wfmt.size != 16)
    {
        uint16_t extraParamSz;
        memcpy((uint8_t*)(&extraParamSz), begin, 2);
        begin += 2;
    }

    /* Parse wav chunk section */
    struct wav_chunk wchunk;
    memset(&wchunk, 0, sizeof(struct wav_chunk));
    parse_field(struct wav_chunk, id, wchunk, 0); /* Should be equal to "data" */
    parse_field(struct wav_chunk, size, wchunk, 4); /* == NumSamples * NumChannels * BitsPerSample / 8 (number of bytes in data) */
    wchunk.data = begin + 8;

    /* Fill and return sound object */
    struct sound* snd = malloc(sizeof(struct sound));
    snd->channels = wfmt.channels;
    snd->samplerate = wfmt.sample_rate;
    snd->bits_per_sample = wfmt.bits_per_sample;
    snd->data_sz = wchunk.size;
    snd->data = malloc(snd->data_sz);
    memcpy(snd->data, wchunk.data, snd->data_sz);
    return snd;
}
