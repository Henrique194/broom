/*
** Copyright (c) 2002-2016, Erik de Castro Lopo <erikd@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/libsndfile/libsamplerate/blob/master/COPYING
*/

/*
** API documentation is available here:
**     http://libsndfile.github.io/libsamplerate/api.html
*/

#ifndef SAMPLERATE_H
#define SAMPLERATE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Opaque data type SRC_STATE. */
typedef struct SRC_STATE_tag SRC_STATE;

/* SRC_DATA is used to pass data to src_simple() and src_process(). */
typedef struct
{
    const float *data_in;
    float *data_out;

    long input_frames;
    long output_frames;
    long input_frames_used;
    long output_frames_gen;

    int end_of_input;

    double src_ratio;
} SRC_DATA;

/*
** User supplied callback function type for use with src_callback_new()
** and src_callback_read(). First parameter is the same pointer that was
** passed into src_callback_new(). Second parameter is pointer to a
** pointer. The user supplied callback function must modify *data to
** point to the start of the user supplied float array. The user supplied
** function must return the number of frames that **data points to.
*/
typedef long (*src_callback_t)(void *cb_data, float **data);

/*
**	Standard initialisation function : return an anonymous pointer to the
**	internal state of the converter. Choose a converter from the enums below.
**	Error returned in *error.
*/
SRC_STATE* src_new(int converter_type, int channels, int *error);

/*
**	Cleanup all internal allocations.
**	Always returns NULL.
*/
SRC_STATE* src_delete(SRC_STATE *state);

/*
**	Standard processing function.
**	Returns non zero on error.
*/
int src_process(SRC_STATE *state, SRC_DATA *data);

/*
**	Simple interface for performing a single conversion from input buffer to
**	output buffer at a fixed conversion ratio.
**	Simple interface does not require initialisation as it can only operate on
**	a single buffer worth of audio.
*/
int src_simple(SRC_DATA *data, int converter_type, int channels);

/*
** The following enums can be used to set the interpolator type
** using the function src_set_converter().
*/
enum
{
    SRC_SINC_BEST_QUALITY = 0,
    SRC_SINC_MEDIUM_QUALITY = 1,
    SRC_SINC_FASTEST = 2,
    SRC_ZERO_ORDER_HOLD = 3,
    SRC_LINEAR = 4,
};


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* SAMPLERATE_H */
