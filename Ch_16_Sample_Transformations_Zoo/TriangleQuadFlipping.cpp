alpha = u[0];
beta = u[1];
if (alpha + beta > 1) {
    alpha = 1-alpha;
    beta = 1-beta;
}
gamma = 1-beta-alpha;
P = alpha*P0 + beta*P1 + gamma*P2;
