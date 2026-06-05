#include "audio_filter.h"

#define FIR_TAPS 5
#define REVERB_DELAY_SAMPLES 4800  /* 150 ms aprox a 32 kHz */

static AudioFilterType current_filter = AUDIO_FILTER_NONE;

/* ---------- Utilidad de saturacion ---------- */

static int16_t clip_int16(int32_t x)
{
    if (x > 32767)
        return 32767;

    if (x < -32768)
        return -32768;

    return (int16_t)x;
}

/* ---------- Pasa-bajos FIR ---------- */

static int16_t fir_left[FIR_TAPS];
static int16_t fir_right[FIR_TAPS];
static int fir_index = 0;

static int16_t low_pass_fir_process(int16_t input, int16_t *buffer)
{
    int i;
    int32_t acc;

    buffer[fir_index] = input;

    acc = 0;

    for (i = 0; i < FIR_TAPS; i++)
    {
        acc += buffer[i];
    }

    return clip_int16(acc / FIR_TAPS);
}

/* ---------- Pasa-altos IIR simple ---------- */
/*
 * Ecuacion:
 * y[n] = alpha * ( y[n-1] + x[n] - x[n-1] )
 *
 * alpha cercano a 1 conserva mas frecuencias altas.
 * Se usa aritmetica entera con escala 1024.
 */

#define HP_ALPHA_NUM 950
#define HP_ALPHA_DEN 1024

static int16_t hp_prev_x_left = 0;
static int16_t hp_prev_y_left = 0;
static int16_t hp_prev_x_right = 0;
static int16_t hp_prev_y_right = 0;

static int16_t high_pass_iir_process(int16_t input,
                                     int16_t *prev_x,
                                     int16_t *prev_y)
{
    int32_t y;

    y = (int32_t)(*prev_y) + (int32_t)input - (int32_t)(*prev_x);
    y = (y * HP_ALPHA_NUM) / HP_ALPHA_DEN;

    *prev_x = input;
    *prev_y = clip_int16(y);

    return *prev_y;
}

/* ---------- Reverb simple ---------- */
/*
 * Reverb basico:
 * y[n] = x[n] + feedback * delay[n]
 *
 * Se usa un buffer circular con una muestra retrasada.
 */

#define REVERB_FEEDBACK_NUM 35
#define REVERB_FEEDBACK_DEN 100

static int16_t reverb_left[REVERB_DELAY_SAMPLES];
static int16_t reverb_right[REVERB_DELAY_SAMPLES];
static int reverb_index = 0;

static int16_t reverb_process(int16_t input, int16_t *buffer)
{
    int16_t delayed;
    int32_t output;

    delayed = buffer[reverb_index];

    output = (int32_t)input +
             ((int32_t)delayed * REVERB_FEEDBACK_NUM) / REVERB_FEEDBACK_DEN;

    buffer[reverb_index] = clip_int16(output);

    return clip_int16(output);
}

/* ---------- Pasa-banda IIR biquad ---------- */
/*
 * Filtro pasa-banda centrado aproximadamente en 1 kHz.
 * Diseñado para PCM de 32 kHz.
 *
 * Ecuacion:
 * y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2]
 *        - a1*y[n-1] - a2*y[n-2]
 *
 * Coeficientes normalizados:
 * fs = 32000 Hz
 * f0 = 1000 Hz
 * Q  = 0.8
 *
 * El resultado es un efecto tipo "radio/telefono", util para notar
 * rapidamente que el filtro esta funcionando.
 */

typedef struct
{
    float x1;
    float x2;
    float y1;
    float y2;
} BiquadState;

static BiquadState bp_left;
static BiquadState bp_right;

#define BP_B0  0.1086799475f
#define BP_B1  0.0000000000f
#define BP_B2 -0.1086799475f
#define BP_A1 -1.7483871752f
#define BP_A2  0.7826401049f

static int16_t band_pass_process(int16_t input, BiquadState *state)
{
    float x;
    float y;

    x = (float)input;

    y = (BP_B0 * x) +
        (BP_B1 * state->x1) +
        (BP_B2 * state->x2) -
        (BP_A1 * state->y1) -
        (BP_A2 * state->y2);

    state->x2 = state->x1;
    state->x1 = x;

    state->y2 = state->y1;
    state->y1 = y;

    return clip_int16((int32_t)y);
}


/* ---------- API publica ---------- */

void audio_filter_init(AudioFilterType type)
{
    int i;

    current_filter = type;
    // Inicializar estado del filtro pasa-bajos FIR
    fir_index = 0;
    reverb_index = 0;
    // Inicializar estado del filtro pasa-altos
    hp_prev_x_left = 0;
    hp_prev_y_left = 0;
    hp_prev_x_right = 0;
    hp_prev_y_right = 0;
    // Inicializar estado del biquad
    bp_left.x1 = 0.0f;
    bp_left.x2 = 0.0f;
    bp_left.y1 = 0.0f;
    bp_left.y2 = 0.0f;

    bp_right.x1 = 0.0f;
    bp_right.x2 = 0.0f;
    bp_right.y1 = 0.0f;
    bp_right.y2 = 0.0f;

    for (i = 0; i < FIR_TAPS; i++)
    {
        fir_left[i] = 0;
        fir_right[i] = 0;
    }

    for (i = 0; i < REVERB_DELAY_SAMPLES; i++)
    {
        reverb_left[i] = 0;
        reverb_right[i] = 0;
    }
}

void audio_filter_process_stereo(int16_t *left, int16_t *right)
{
    int16_t in_left;
    int16_t in_right;

    in_left = *left;
    in_right = *right;

    if (current_filter == AUDIO_FILTER_LOW_PASS_FIR)
    {
        *left = low_pass_fir_process(in_left, fir_left);
        *right = low_pass_fir_process(in_right, fir_right);

        fir_index++;

        if (fir_index >= FIR_TAPS)
            fir_index = 0;
    }
    else if (current_filter == AUDIO_FILTER_HIGH_PASS_IIR)
    {
        *left = high_pass_iir_process(in_left,
                                      &hp_prev_x_left,
                                      &hp_prev_y_left);

        *right = high_pass_iir_process(in_right,
                                       &hp_prev_x_right,
                                       &hp_prev_y_right);
    }
    else if (current_filter == AUDIO_FILTER_REVERB)
    {
        *left = reverb_process(in_left, reverb_left);
        *right = reverb_process(in_right, reverb_right);

        reverb_index++;

        if (reverb_index >= REVERB_DELAY_SAMPLES)
            reverb_index = 0;
    }
    else if (current_filter == AUDIO_FILTER_BAND_PASS)
    {
        *left = band_pass_process(in_left, &bp_left);
        *right = band_pass_process(in_right, &bp_right);
    }
    else
    {
        *left = in_left;
        *right = in_right;
    }
}