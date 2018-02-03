/*
 *  oscillator.c
 *  PataterSynth
 *
 *  Created by Jaeden Amero on 2018-02-03.
 *  Copyright 2018. SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "psynth/oscillator.h"
#include "psynth/psynth.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>

/* TODO
 *  - Support alternative polynomials for polyblep
 *  - Implement minBLEP
 *  - Implement duty cycle control(especially for square)
 *  - Figure out how to change notes and still maintain bandlimiting(i.e.
 *    without introducing discontinuities).
 *  - Check triangle wave over carefully.looks not right.integrator needs
 *    tuning.
 *  - Check phase is correct.Appears our sine wave is not quite right for
 *    phase 0. sin(0) should be zero, but we get slightly less than 1.
 *  - Is it possible to do an integer only implmementation of polyblep ? how
 *    about minblep ?
 *  - Use dither in float to int conversion
 */

enum OscillatorMode mode;
double fundamental;
double phase = 0;
double phase_step;
double prev = 0;
#ifndef M_2PI
static const double M_2PI = 2.0 * M_PI;
#endif

/* 0 to skip bandlimiting, 1 to use polyblep bandlimiting */
static const int polyblep_mode = 1;

void init_oscillator(void)
{
    fundamental = 2637.0; /* MIDI #100, E8 */
    fundamental = 440.0; /* MIDI #69, A5 */
    phase_step = fundamental * M_2PI / samples_per_sec;
}

/*static*/ double polyblep_narrow(double t)
{
    double dt = phase_step / M_2PI;

    if (t < dt) /* 0 <= t < 1 */
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    else if (t > 1.0 - dt) /* -1 < t < 0 */
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    else
    {
        return 0.0;
    }
}

/*static*/ double polyblep(double t)
{
    double dt = phase_step / M_2PI;

    if (t <= dt) /* 0 < t <= 1 */
    {
        t /= dt;
        return t - t * t * 0.5 - 0.5;
    }
    else if (t > 1.0 - dt) /* -1 <= t <= 0  XXX Check this condition */
    {
        t = (t - 1.0) / dt; /* XXX Check this */
        return t * t * 0.5 + t + 0.5;
    }
    else
    {
        return 0.0;
    }
}

/*static*/ short float_to_int_round(double f)
{
    /* Make the float value positive by adding 1.0. */
    /* Assumes -1 <= f <= 1 */
    f += 1.0;

    /* 0 <= f <= 2 */
    f *= 32767.5;

    /* 0 <= f <= 65535 */
    /* Add 0.5 to make the C cast's truncation into rounding. */
    f += 0.5;

    /* 0.5 <= f <= 65535.5 */
    size_t u = (size_t)f;

    /* 0 <= u <= 65535 */
    short s = (short)(u - 32768);

    /* -32768 <= s <= 32767 */
    return s;
}

/*static*/ short crand_dither(double f)
{
    /*
    Dithering is a way to randomly toggle the results between 4 and 5 so that
    80% of the time it ended up on 5 then it would average 4.8 over the long
    run but would have random, non-repeating error in the result.

    If the signal being dithered is to undergo further processing, then it
    should be processed with a triangular-type dither that has an amplitude of
    two quantisation steps; for example, so that the dither values computed
    range from, say, -1 to +1, or 0 to 2.[12] This is the "lowest power ideal"
    dither, in that it does not introduce noise modulation (which would
    manifest as a constant noise floor), and completely eliminates the harmonic
    distortion from quantisation. If a colored dither is used instead at these
    intermediate processing stages, then frequency content may "bleed" into
    other frequency ranges that are more noticeable, which could become
    distractingly audible.

    If the signal being dithered is to undergo no further processing -- if it
    is being dithered to its final result for distribution -- then a "colored"
    dither or noise shaping is appropriate. This can effectively lower the
    audible noise level, by putting most of that noise in a frequency range
    where it is less critical.

    https://en.wikipedia.org/wiki/Dither
    */
    double whole = floor(f);
    double fraction = f - whole;
    double fake_uniform_rand = (double)rand() / (RAND_MAX + 1);

    /* Make the float value positive by adding 1.0. */
    /* Assumes -1 <= f <= 1 */
    f += 1.0;

    /* 0 <= f <= 2 */
    f *= 32767.5;

    /* 0 <= f <= 65535 */
    short s = (short)(f - 32768.0);

    return s + (fake_uniform_rand < fraction);
}

static short float_to_int(double f)
{
#if SHOULD_DITHER
    return dither(f);
#else
    return float_to_int_round(f);
#endif
}

void generate_sine(short *buffer, size_t num_samples)
{
    size_t count = 0;
    while (num_samples--)
    {
        /* */
        double sample = sin(phase);
        *buffer++ = float_to_int(sample);
        /* */
        count++;

        phase += phase_step;

        /* Consider faster way to do floating point mod */
        while (phase >= M_2PI)
        {
            phase -= M_2PI;
        }
    }
}

void generate_square(short *buffer, size_t num_samples)
{
    while (num_samples--)
    {
        /* */
        double t = phase / M_2PI;
        double sample = phase < M_PI ? 1.0 : -1.0;

        if (polyblep_mode) // Refactor to remove conditional here
        {
            sample += polyblep_narrow(t);
            sample -= polyblep_narrow(fmod(t + 0.5, 1.0));
        }
        *buffer++ = float_to_int(sample);
        /* */

        // TODO refactor phase accumulator
        phase += phase_step;

        /* Consider faster way to do floating point mod */
        while (phase >= M_2PI)
        {
            phase -= M_2PI;
        }
    }
}

void generate_saw(short *buffer, size_t num_samples)
{
    while (num_samples--)
    {
        /* */
        double t = phase / M_2PI;
        double sample =
            (phase / M_PI) - 1.0; /* XXX better with or without -1? */

        if (polyblep_mode)
        {
            sample -= polyblep_narrow(t);
        }
        *buffer++ = float_to_int(sample);
        /* */

        /* TODO refactor phase accumulator */
        phase += phase_step;

        /* Consider faster way to do floating point mod */
        while (phase >= M_2PI)
        {
            phase -= M_2PI;
        }
    }
}

void generate_triangle(short *buffer, size_t num_samples)
{
    while (num_samples--)
    {
        /* */
        double t = phase / M_2PI;
        double sample;

        if (polyblep_mode) // Refactor to remove conditional here
        {
            sample = phase < M_PI ? 1.0 : -1.0; // XXX same as square

            sample += polyblep_narrow(t);
            sample -= polyblep_narrow(fmod(t + 0.5, 1.0));

            /* Use a leaky integrator on the quasi-bandlimited square to make
             * triangle. */
            sample = phase_step * sample + (1 - phase_step) * prev;
            prev = sample;
        }
        else
        {
            sample = -1.0 + (phase / M_PI);
            sample = 2.0 * (fabs(sample) - 0.5);
        }
        *buffer++ = float_to_int(sample);
        /* */

        /* TODO refactor phase accumulator */
        phase += phase_step;

        /* Consider faster way to do floating point mod */
        while (phase >= M_2PI)
        {
            phase -= M_2PI;
        }
    }
}
