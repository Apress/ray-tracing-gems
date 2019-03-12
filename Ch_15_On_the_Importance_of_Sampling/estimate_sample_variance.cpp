float estimate_sample_variance(float samples[], int n) {
   float sum = 0, sum_sq = 0;
   for (int i = 0; i < n; ++i) {
     sum += samples[i];
     sum_sq += samples[i] * samples[i];
   }
   return sum_sq / (n * (n - 1))) -
          sum * sum / ((n - 1) * n * n);
}
