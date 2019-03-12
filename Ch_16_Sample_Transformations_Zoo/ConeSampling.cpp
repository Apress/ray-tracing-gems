float cosTheta = (1 - u[0]) + u[0] * cosThetaMax;
float sinTheta = sqrt(1 - cosTheta * cosTheta);
float phi = u[1] * 2 * M_PI;
x = cos(phi) * sinTheta;
y = sin(phi) * sinTheta;
z = cosTheta;
