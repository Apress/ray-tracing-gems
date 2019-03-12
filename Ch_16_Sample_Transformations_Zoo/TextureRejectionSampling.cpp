do {
    X = u();
    Y = u();
} while (u() > brightness(texture(X,Y))); // Brightness is [0,1].
