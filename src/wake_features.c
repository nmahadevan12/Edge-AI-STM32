/* On-device port of waveform_to_features() from ml/train_wake_word.py.
 * Window, mel filterbank, DCT and delta (Savitzky-Golay) matrices come from
 * wake_tables.h so they are bit-for-bit what librosa used in training.
 * Build with -lm. */
 #include <math.h>
 #include "wake_word_model.h"
 #include "wake_tables.h"
 #include "wake_features.h"
 
 #define TOP_DB   80.0f
 #define AMIN     1e-10f
 #define ZCR_THR  1e-10f
 
 static float xn[WAKE_N_SAMPLES];                 /* peak-normalised wave   */
 static float mel_db[WAKE_N_FRAMES][WAKE_N_MELS]; /* log-mel, per frame     */
 static float mfcc[WAKE_N_MFCC][WAKE_N_FRAMES];
 static float rms_f[WAKE_N_FRAMES];
 static float zcr_f[WAKE_N_FRAMES];
 
 /* mean / population std (ddof = 0, like numpy) of v[0..n-1] */
 static void mean_std(const float *v, int n, float *mean, float *std)
 {
     float s = 0.0f, ss = 0.0f;
     int i;
     for (i = 0; i < n; i++) s += v[i];
     s /= (float)n;
     for (i = 0; i < n; i++) { float d = v[i] - s; ss += d * d; }
     *mean = s;
     *std  = sqrtf(ss / (float)n);
 }
 
 /* sample of the zero-padded signal, centre=True: index may be out of range */
 static float padded_zero(int idx)
 {
     return (idx < 0 || idx >= WAKE_N_SAMPLES) ? 0.0f : xn[idx];
 }
 
 /* librosa zero_crossing_rate pads with edge values */
 static float padded_edge(int idx)
 {
     if (idx < 0) idx = 0;
     if (idx >= WAKE_N_SAMPLES) idx = WAKE_N_SAMPLES - 1;
     return xn[idx];
 }
 
 void Wake_ComputeFeatures(const float *wave, float *feat)
 {
     int f, k, n, j, c, t, o = 0;
     float peak = 0.0f, max_db = -1e30f;
 
     /* 1. peak normalise */
     for (n = 0; n < WAKE_N_SAMPLES; n++) {
         float a = fabsf(wave[n]);
         if (a > peak) peak = a;
     }
     peak += 1e-9f;
     for (n = 0; n < WAKE_N_SAMPLES; n++) xn[n] = wave[n] / peak;
 
     /* 2. STFT power -> mel -> dB, one frame at a time */
     for (f = 0; f < WAKE_N_FRAMES; f++) {
         float fr[WAKE_N_FFT];
         float pw[WAKE_N_BINS];
         int start = f * WAKE_HOP - WAKE_N_FFT / 2;
 
         for (n = 0; n < WAKE_N_FFT; n++)
             fr[n] = WAKE_HANN[n] * padded_zero(start + n);
 
         for (k = 0; k < WAKE_N_BINS; k++) {
             float re = 0.0f, im = 0.0f;
             for (n = 0; n < WAKE_N_FFT; n++) {
                 int tw = (k * n) & (WAKE_N_FFT - 1);   /* N_FFT is a power of 2 */
                 re += fr[n] * WAKE_TW_COS[tw];
                 im -= fr[n] * WAKE_TW_SIN[tw];
             }
             pw[k] = re * re + im * im;
         }
 
         for (j = 0; j < WAKE_N_MELS; j++) {
             float m = 0.0f;
             for (k = 0; k < WAKE_N_BINS; k++) m += WAKE_MEL[j][k] * pw[k];
             if (m < AMIN) m = AMIN;
             mel_db[f][j] = 10.0f * log10f(m);
             if (mel_db[f][j] > max_db) max_db = mel_db[f][j];
         }
     }
     /* power_to_db(top_db=80): floor relative to the loudest cell in the clip */
     for (f = 0; f < WAKE_N_FRAMES; f++)
         for (j = 0; j < WAKE_N_MELS; j++)
             if (mel_db[f][j] < max_db - TOP_DB) mel_db[f][j] = max_db - TOP_DB;
 
     /* 3. DCT-II -> MFCC */
     for (c = 0; c < WAKE_N_MFCC; c++)
         for (f = 0; f < WAKE_N_FRAMES; f++) {
             float s = 0.0f;
             for (j = 0; j < WAKE_N_MELS; j++) s += WAKE_DCT[c][j] * mel_db[f][j];
             mfcc[c][f] = s;
         }
 
     /* 4. summary stats of mfcc, delta, delta2 (same order as the Python) */
     for (c = 0; c < WAKE_N_MFCC; c++)
         mean_std(mfcc[c], WAKE_N_FRAMES, &feat[0 * WAKE_N_MFCC + c],
                                           &feat[1 * WAKE_N_MFCC + c]);
 
     for (c = 0; c < WAKE_N_MFCC; c++) {
         float d1[WAKE_N_FRAMES], d2[WAKE_N_FRAMES];
         for (f = 0; f < WAKE_N_FRAMES; f++) {
             float a = 0.0f, b = 0.0f;
             for (t = 0; t < WAKE_N_FRAMES; t++) {
                 a += WAKE_D1[f][t] * mfcc[c][t];
                 b += WAKE_D2[f][t] * mfcc[c][t];
             }
             d1[f] = a; d2[f] = b;
         }
         mean_std(d1, WAKE_N_FRAMES, &feat[2 * WAKE_N_MFCC + c],
                                      &feat[3 * WAKE_N_MFCC + c]);
         mean_std(d2, WAKE_N_FRAMES, &feat[4 * WAKE_N_MFCC + c],
                                      &feat[5 * WAKE_N_MFCC + c]);
     }
     o = 6 * WAKE_N_MFCC;
 
     /* 5. RMS (zero padded) and zero-crossing rate (edge padded), per frame */
     for (f = 0; f < WAKE_N_FRAMES; f++) {
         int start = f * WAKE_HOP - WAKE_N_FFT / 2;
         float e = 0.0f;
         int crossings = 0, prev_neg = 0;
 
         for (n = 0; n < WAKE_N_FFT; n++) {
             float v = padded_zero(start + n);
             e += v * v;
         }
         rms_f[f] = sqrtf(e / (float)WAKE_N_FFT);
 
         for (n = 0; n < WAKE_N_FFT; n++) {
             float v = padded_edge(start + n);
             int neg = (v < -ZCR_THR);          /* |v| <= thr counts as +0 */
             if (n > 0 && neg != prev_neg) crossings++;
             prev_neg = neg;
         }
         /* librosa averages over all N_FFT entries (entry 0 is always 0) */
         zcr_f[f] = (float)crossings / (float)WAKE_N_FFT;
     }
     mean_std(rms_f, WAKE_N_FRAMES, &feat[o + 0], &feat[o + 1]);
     mean_std(zcr_f, WAKE_N_FRAMES, &feat[o + 2], &feat[o + 3]);
 }
 
 float Wake_Score(const float *wave)
 {
     float feat[WAKE_N_FEATURES];
     float s = WAKE_BIAS;
     int i;
 
     Wake_ComputeFeatures(wave, feat);
     for (i = 0; i < WAKE_N_FEATURES; i++) s += WAKE_WEIGHTS[i] * feat[i];
     return s;
 }