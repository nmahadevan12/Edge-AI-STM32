#ifndef WAKE_FEATURES_H
#define WAKE_FEATURES_H

/* No <stdint.h> on purpose: main.c defines its own uint32_t etc. and mixing
 * the two gives pointer-type conflicts. Everything here is float. */

/* wave: WAKE_N_SAMPLES raw samples (same units as the "y" column of the CSV
 * dumps). feat: WAKE_N_FEATURES output floats, same order as
 * waveform_to_features() in ml/train_wake_word.py. */
void  Wake_ComputeFeatures(const float *wave, float *feat);

/* Logit. > 0 means "hey STM". */
float Wake_Score(const float *wave);

#endif