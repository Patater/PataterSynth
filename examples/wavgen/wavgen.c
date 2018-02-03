/*
 *  wavgen.c
 *  PataterSynth
 *
 *  Created by Jaeden Amero on 2018-02-03.
 *  Copyright 2018. SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "psynth/oscillator.h"
#include "psynth/psynth.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TODO
 *   wav file stuff
 *   - Support stereo audio
 *   - Support big endian machines
 *   - Pack the structs
 *   - Support non-PCM wav formats
 *   - Align size of chunks if they are not word (16-bit) aligned by adding
 *     padding byte zero to end of them
 */

static const size_t BUFFER_SIZE = 256 * 1024;
uint8_t *buffer;
size_t buffer_i;
size_t num_sample_bytes_written;

struct riff_chunk
{
    char name[4];
    uint32_t num_bytes;
};

enum WavFormat
{
    WF_UNKNOWN = 0,
    WF_PCM = 1,
    WF_ADPCM = 2,
    WF_ALAW = 6,
    WF_MULAW = 7,
    WF_EXPERIMENTAL = 65535
};

struct wav_data
{
    char type[4]; /* "WAVE" */
};

struct fmt_data
{
    uint16_t format;
    uint16_t num_channels;
    uint32_t samples_per_sec;
    uint32_t avg_bytes_per_sec;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

int flush(FILE *f)
{
    fwrite(buffer, buffer_i, 1, f);
    fflush(f);
    buffer_i = 0;

    return 0;
}

/* Buffered output stream
 * Return non-zero on fail, zero on success.
 */
int write(FILE *f, void *b, size_t num_bytes)
{
    if (num_bytes >= BUFFER_SIZE)
    {
        /* Can't write such big thing (through buffer, but could do direct to
         * file) */
        fprintf(stderr, "Too big of write\n");
        exit(1);
    }

    /* If we don't have room in the buffer right now, flush it first to make
     * room. */
    if (buffer_i + num_bytes >= BUFFER_SIZE)
    {
        flush(f);
    }

    memcpy(buffer + buffer_i, b, num_bytes);
    buffer_i += num_bytes;

    return 0;
}

/* Always 16-bit signed little endian (for now) */
int write_samples(FILE *f, int16_t *s, size_t num_samples)
{
    size_t num_bytes = num_samples * sizeof(*s);
    write(f, s, num_bytes);
    num_sample_bytes_written += num_bytes;

    return 0;
}

/* Don't call this if you plan to write more data. Call after you are done,
 * because we don't reset the fseek. */
int write_wav_header(FILE *f)
{
    struct riff_chunk riff_chunk;
    struct riff_chunk data_chunk;
    struct riff_chunk fmt_chunk;
    struct fmt_data fmt;
    struct wav_data wav;

    /* "data" */
    data_chunk.name[0] = 'd';
    data_chunk.name[1] = 'a';
    data_chunk.name[2] = 't';
    data_chunk.name[3] = 'a';
    data_chunk.num_bytes = num_sample_bytes_written;

    /* "fmt " */
    fmt_chunk.name[0] = 'f';
    fmt_chunk.name[1] = 'm';
    fmt_chunk.name[2] = 't';
    fmt_chunk.name[3] = ' ';
    fmt_chunk.num_bytes = sizeof(fmt);
    fmt.format = WF_PCM;
    fmt.num_channels = 1; /* Mono */
    fmt.samples_per_sec = 44100;
    static const size_t sample_size_bytes = 2; /* 16-bit (2 bytes) */
    const size_t frame_size =
        fmt.num_channels
        * sample_size_bytes; /* frame includes one sample from all channels */
    fmt.avg_bytes_per_sec = fmt.samples_per_sec * frame_size;
    fmt.block_align = (uint16_t)frame_size;
    fmt.bits_per_sample = sample_size_bytes * 8; /* 8 bits per byte */

    /* "WAVE" */
    wav.type[0] = 'W';
    wav.type[1] = 'A';
    wav.type[2] = 'V';
    wav.type[3] = 'E';

    /* "RIFF" */
    riff_chunk.name[0] = 'R';
    riff_chunk.name[1] = 'I';
    riff_chunk.name[2] = 'F';
    riff_chunk.name[3] = 'F';
    riff_chunk.num_bytes = sizeof(riff_chunk) + sizeof(wav) + sizeof(fmt_chunk)
                           + sizeof(fmt) + sizeof(data_chunk)
                           + data_chunk.num_bytes;

    /* Flush any samples in our buffer */
    flush(f);

    /* Write out wav file header. TODO could optimize this to be one fwrite...
     */
    fseek(f, 0, SEEK_SET); /* Seek to beginning of file */
    fwrite(&riff_chunk, sizeof(riff_chunk), 1, f);
    fwrite(&wav, sizeof(wav), 1, f);
    fwrite(&fmt_chunk, sizeof(fmt_chunk), 1, f);
    fwrite(&fmt, sizeof(fmt), 1, f);
    fwrite(&data_chunk, sizeof(data_chunk), 1, f);

    /* RIFF 0x056244
     * fmt  0x000010
     * data 0x056220 */

    return 0;
}

int main()
{
    FILE *file;

    file = fopen("out.wav", "wb");
    if (!file)
    {
        fprintf(stderr, "Failed to open file for writing\n");
        return 1;
    }

    buffer = (uint8_t *)malloc(BUFFER_SIZE);
    if (!buffer)
    {
        fprintf(stderr, "Failed to allocate buffer\n");
        return 1;
    }

    /* Generate some cool sounds */
    size_t num_samples = samples_per_sec * 2;
    int16_t *sample_buf = (int16_t *)malloc(num_samples * sizeof(*sample_buf));
    if (!sample_buf)
    {
        fprintf(stderr, "Failed to allocate sample buffer\n");
        return 1;
    }

    init_oscillator();

    /* XXX How to properly volume control this generator? can't just divide by
     * 2, as that would introduce quantization error... generator has to know
     * the output volume it should make? we could do everything with floating
     * point internally as well, and then quantize once for output. */
    generate_sine(sample_buf, num_samples);
    write_samples(file, sample_buf, num_samples);
    memset(sample_buf, 0, samples_per_sec * sizeof(*sample_buf) / 5);
    write_samples(file, sample_buf, samples_per_sec / 5);

    generate_square(sample_buf, num_samples);
    write_samples(file, sample_buf, num_samples);
    memset(sample_buf, 0, samples_per_sec * sizeof(*sample_buf) / 5);
    write_samples(file, sample_buf, samples_per_sec / 5);

    generate_saw(sample_buf, num_samples);
    write_samples(file, sample_buf, num_samples);
    memset(sample_buf, 0, samples_per_sec * sizeof(*sample_buf) / 5);
    write_samples(file, sample_buf, samples_per_sec / 5);

    generate_triangle(sample_buf, num_samples);
    write_samples(file, sample_buf, num_samples);
    memset(sample_buf, 0, samples_per_sec * sizeof(*sample_buf) / 5);
    write_samples(file, sample_buf, samples_per_sec / 5);

    /* Write out to file */
    write_wav_header(file);
    fclose(file);
    free(buffer);

    return 0;
}
