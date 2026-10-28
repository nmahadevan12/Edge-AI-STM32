/* Host-side check: gcc -O2 -I ml/artifacts -I fw fw/test_features.c fw/wake_features.c -lm
 * Compares the C features/score with what Python produced for the same clip. */
 #include <stdio.h>
 #include <math.h>
 #include "wake_word_model.h"
 #include "wake_tables.h"
 #include "wake_test_vector.h"
 #include "wake_features.h"
 
 int main(void)
 {
     float feat[WAKE_N_FEATURES], worst = 0.0f;
     int i, worst_i = 0;
 
     Wake_ComputeFeatures(WAKE_TEST_WAVE, feat);
     for (i = 0; i < WAKE_N_FEATURES; i++) {
         float err = fabsf(feat[i] - WAKE_TEST_FEATURES[i]);
         float tol = 1e-3f + 1e-3f * fabsf(WAKE_TEST_FEATURES[i]);
         float r = err / tol;
         if (r > worst) { worst = r; worst_i = i; }
     }
     float score = Wake_Score(WAKE_TEST_WAVE);
     printf("worst feature #%d: C=%g py=%g (error/tolerance = %.3f)\n",
            worst_i, feat[worst_i], WAKE_TEST_FEATURES[worst_i], worst);
     printf("score: C=%g py=%g diff=%g\n", score, WAKE_TEST_SCORE, score - WAKE_TEST_SCORE);
     puts(worst <= 1.0f && fabsf(score - WAKE_TEST_SCORE) < 0.05f ? "PASS" : "FAIL");
     return 0;
 }