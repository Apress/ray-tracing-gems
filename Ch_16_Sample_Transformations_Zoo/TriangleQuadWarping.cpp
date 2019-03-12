beta = 1-sqrt(u[0]);
gamma = (1-beta)*u[1];
alpha = 1-beta-gamma;
P = alpha*P0 + beta*P1 + gamma*P2;
