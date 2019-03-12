s = 0;
do {
    s -= log(1 - u()) / kappa_max;
} while (kappa(s) < u() * kappa_max);
