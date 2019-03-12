a = 1 - 2*u[0];
b = sqrt(1 - a*a);
phi = 2*M_PI*u[1];
x = n_x + b*cos(phi);
y = n_y + b*sin(phi);
z = n_z + a;
pdf = a / M_PI;
