/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Jeff Epler for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <math.h>
#include "py/runtime.h"
#include "shared-module/synthio/Note.h"
#include "shared-bindings/synthio/Note.h"
#include "shared-bindings/synthio/__init__.h"

static int32_t round_float_to_int(mp_float_t f) {
    return (int32_t)(f + MICROPY_FLOAT_CONST(0.5));
}

mp_float_t common_hal_synthio_note_get_frequency(synthio_note_obj_t *self) {
    return self->frequency;
}

void common_hal_synthio_note_set_frequency(synthio_note_obj_t *self, mp_float_t value_in) {
    mp_float_t val = mp_arg_validate_float_range(value_in, 0, 32767, MP_QSTR_frequency);
    self->frequency = val;
    self->frequency_scaled = synthio_frequency_convert_float_to_scaled(val);
}

mp_float_t common_hal_synthio_note_get_amplitude(synthio_note_obj_t *self) {
    return self->amplitude;
}

void common_hal_synthio_note_set_amplitude(synthio_note_obj_t *self, mp_float_t value_in) {
    mp_float_t val = mp_arg_validate_float_range(value_in, 0, 1, MP_QSTR_amplitude);
    self->amplitude = val;
    self->amplitude_scaled = round_float_to_int(val * 32767);
}

mp_float_t common_hal_synthio_note_get_tremolo_depth(synthio_note_obj_t *self) {
    return self->tremolo_descr.amplitude;
}

void common_hal_synthio_note_set_tremolo_depth(synthio_note_obj_t *self, mp_float_t value_in) {
    mp_float_t val = mp_arg_validate_float_range(value_in, 0, 1, MP_QSTR_tremolo_depth);
    self->tremolo_descr.amplitude = val;
    self->tremolo_state.amplitude_scaled = round_float_to_int(val * 32767);
}

mp_float_t common_hal_synthio_note_get_tremolo_rate(synthio_note_obj_t *self) {
    return self->tremolo_descr.frequency;
}

void common_hal_synthio_note_set_tremolo_rate(synthio_note_obj_t *self, mp_float_t value_in) {
    mp_float_t val = mp_arg_validate_float_range(value_in, 0, 60, MP_QSTR_tremolo_rate);
    self->tremolo_descr.frequency = val;
    if (self->sample_rate != 0) {
        self->tremolo_state.dds = synthio_frequency_convert_float_to_dds(val, self->sample_rate);
    }
}

mp_float_t common_hal_synthio_note_get_vibrato_depth(synthio_note_obj_t *self) {
    return self->vibrato_descr.amplitude;
}

void common_hal_synthio_note_set_vibrato_depth(synthio_note_obj_t *self, mp_float_t value_in) {
    mp_float_t val = mp_arg_validate_float_range(value_in, 0, 1, MP_QSTR_vibrato_depth);
    self->vibrato_descr.amplitude = val;
    self->vibrato_state.amplitude_scaled = round_float_to_int(val * 32767);
}

mp_float_t common_hal_synthio_note_get_vibrato_rate(synthio_note_obj_t *self) {
    return self->vibrato_descr.frequency;
}

void common_hal_synthio_note_set_vibrato_rate(synthio_note_obj_t *self, mp_float_t value_in) {
    mp_float_t val = mp_arg_validate_float_range(value_in, 0, 60, MP_QSTR_vibrato_rate);
    self->vibrato_descr.frequency = val;
    if (self->sample_rate != 0) {
        self->vibrato_state.dds = synthio_frequency_convert_float_to_dds(val, self->sample_rate);
    }
}

mp_obj_t common_hal_synthio_note_get_envelope_obj(synthio_note_obj_t *self) {
    return self->envelope_obj;
}

void common_hal_synthio_note_set_envelope(synthio_note_obj_t *self, mp_obj_t envelope_in) {
    if (envelope_in != mp_const_none) {
        mp_arg_validate_type(envelope_in, (mp_obj_type_t *)&synthio_envelope_type_obj, MP_QSTR_envelope);
    }
    self->envelope_obj = envelope_in;
}

mp_obj_t common_hal_synthio_note_get_waveform_obj(synthio_note_obj_t *self) {
    return self->waveform_obj;
}

void common_hal_synthio_note_set_waveform(synthio_note_obj_t *self, mp_obj_t waveform_in) {
    if (waveform_in == mp_const_none) {
        memset(&self->waveform_buf, 0, sizeof(self->waveform_buf));
    } else {
        mp_buffer_info_t bufinfo_waveform;
        synthio_synth_parse_waveform(&bufinfo_waveform, waveform_in);
        self->waveform_buf = bufinfo_waveform;
    }
    self->waveform_obj = waveform_in;
}

void synthio_note_recalculate(synthio_note_obj_t *self, int32_t sample_rate) {
    if (sample_rate == self->sample_rate) {
        return;
    }
    self->sample_rate = sample_rate;

    if (self->envelope_obj != mp_const_none) {
        synthio_envelope_definition_set(&self->envelope_def, self->envelope_obj, sample_rate);
    }

    synthio_lfo_set(&self->tremolo_state, &self->tremolo_descr, sample_rate);
    self->tremolo_state.offset_scaled = 32768 - self->tremolo_state.amplitude_scaled;
    synthio_lfo_set(&self->vibrato_state, &self->vibrato_descr, sample_rate);
    self->vibrato_state.offset_scaled = 32768;
}

void synthio_note_start(synthio_note_obj_t *self, int32_t sample_rate) {
    synthio_note_recalculate(self, sample_rate);
    self->phase = 0;
}

uint32_t synthio_note_envelope(synthio_note_obj_t *self) {
    return self->amplitude_scaled;
}

uint32_t synthio_note_step(synthio_note_obj_t *self, int32_t sample_rate, int16_t dur, uint16_t *loudness) {
    int tremolo_value = synthio_lfo_step(&self->tremolo_state, dur);
    int vibrato_value = synthio_lfo_step(&self->vibrato_state, dur);
    *loudness = (*loudness * tremolo_value) >> 15;
    uint32_t frequency_scaled = ((uint64_t)self->frequency_scaled * vibrato_value) >> 15;
    return frequency_scaled;
}
