/*
** Copyright (c) 2002-2016, Erik de Castro Lopo <erikd@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/libsndfile/libsamplerate/blob/master/COPYING
*/

#include <math.h>
#include "samplerate.h"
#include "common.h"

static SRC_STATE* psrc_set_converter(int converter_type, int channels,
                                     int* error);


SRC_STATE* src_new(int converter_type, int channels, int* error) {
    return psrc_set_converter(converter_type, channels, error);
}

SRC_STATE* src_delete(SRC_STATE* state) {
    if (state) {
        state->vt->close(state);
    }
    return NULL;
}

int src_process(SRC_STATE* state, SRC_DATA* data) {
    int error;

    if (state == NULL) {
        return SRC_ERR_BAD_STATE;
    }
    if (state->mode != SRC_MODE_PROCESS) {
        return SRC_ERR_BAD_MODE;
    }
    // Check for valid SRC_DATA first.
    if (data == NULL) {
        return SRC_ERR_BAD_DATA;
    }
    // And that data_in and data_out are valid.
    if ((data->data_in == NULL && data->input_frames > 0) ||
        (data->data_out == NULL && data->output_frames > 0)) {
        return SRC_ERR_BAD_DATA_PTR;
    }
    // Check src_ratio is in range.
    if (is_bad_src_ratio(data->src_ratio)) {
        return SRC_ERR_BAD_SRC_RATIO;
    }
    if (data->input_frames < 0) {
        data->input_frames = 0;
    }
    if (data->output_frames < 0) {
        data->output_frames = 0;
    }

    if (data->data_in < data->data_out) {
        if (data->data_in + data->input_frames * state->channels >
            data->data_out)
        {
            return SRC_ERR_DATA_OVERLAP;
        }
    }
    else if (data->data_out + data->output_frames * state->channels >
             data->data_in) {
        return SRC_ERR_DATA_OVERLAP;
    }

    // Set the input and output counts to zero.
    data->input_frames_used = 0;
    data->output_frames_gen = 0;

    // Special case for when last_ratio has not been set.
    if (state->last_ratio < (1.0 / SRC_MAX_RATIO)) {
        state->last_ratio = data->src_ratio;
    }

    // Now process.
    if (fabs(state->last_ratio - data->src_ratio) < 1e-15) {
        error = state->vt->const_process(state, data);
    } else {
        error = state->vt->vari_process(state, data);
    }

    return error;
}

/*------------------------------------------------------------------------------
**	Simple interface for performing a single conversion from input buffer to
**	output buffer at a fixed conversion ratio.
*/
int src_simple(SRC_DATA* src_data, int converter, int channels) {
    int error;
    SRC_STATE* src_state = src_new(converter, channels, &error);
    if (src_state == NULL) {
        return error;
    }
    src_data->end_of_input = 1; // Only one buffer worth of input.
    error = src_process(src_state, src_data);
    src_delete(src_state);
    return error;
}

/*------------------------------------------------------------------------------
**	Private functions.
*/

static SRC_STATE* psrc_set_converter(int converter_type, int channels,
                                     int* error)
{
    SRC_ERROR temp_error;
    SRC_STATE *state;

    switch (converter_type) {
#ifdef ENABLE_SINC_BEST_CONVERTER
        case SRC_SINC_BEST_QUALITY:
            state = sinc_state_new(converter_type, channels, &temp_error);
            break;
#endif
#ifdef ENABLE_SINC_MEDIUM_CONVERTER
        case SRC_SINC_MEDIUM_QUALITY:
            state = sinc_state_new(converter_type, channels, &temp_error);
            break;
#endif
#ifdef ENABLE_SINC_FAST_CONVERTER
        case SRC_SINC_FASTEST:
            state = sinc_state_new(converter_type, channels, &temp_error);
            break;
#endif
        case SRC_ZERO_ORDER_HOLD:
            state = zoh_state_new(channels, &temp_error);
            break;
        case SRC_LINEAR:
            state = linear_state_new(channels, &temp_error);
            break;
        default:
            temp_error = SRC_ERR_BAD_CONVERTER;
            state = NULL;
            break;
    }

    if (error) {
        *error = (int) temp_error;
    }

    return state;
}
