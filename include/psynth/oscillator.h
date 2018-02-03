/*
 *  oscillator.h
 *  PataterSynth
 *
 *  Created by Jaeden Amero on 2018-02-03.
 *  Copyright 2018. SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef OSCILLATOR_H
#define OSCILLATOR_H

#include <stddef.h>

enum OscillatorMode
{
    OM_SINE,
    OM_SQUARE,
    OM_TRIANGLE,
    OM_SAW,
};

void init_oscillator(void);
void generate_sine(short *buffer, size_t num_samples);
void generate_square(short *buffer, size_t num_samples);
void generate_saw(short *buffer, size_t num_samples);
void generate_triangle(short *buffer, size_t num_samples);

#endif
