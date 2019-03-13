// more comments on code at http://psgraphics.blogspot.com/2011/01/improved-code-for-concentric-map.html

a = 2*u[0] - 1;
b = 2*u[1] - 1;
// special case to avoid division by zero, noted by Greg Ward
if ( b == 0 ) {
    b = 1;
}
if (a*a > b*b) {
    r = R*a;
    phi = (M_PI/4)*(b/a);
} else {
    r = R*b;
    phi = (M_PI/2) - (M_PI/4)*(a/b);
}
X = r*cos(phi);
Y = r*sin(phi);
