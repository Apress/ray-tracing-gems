return { mu + sigma * sqrt(-2 * log(1-u[0])) * cos(2*M_PI*u[1]),
         mu + sigma * sqrt(-2 * log(1-u[0])) * sin(2*M_PI*u[1])) };
