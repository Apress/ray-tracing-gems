// Image assembly

// Map the pixel index $i$ to the permuted pixel index $j$.
const unsigned f = i / s;
const unsigned p = i % s;
const unsigned j = (reverse(f) >> bits) + p;

// The permutation is involutory:
// pixel $j$ permutates back to pixel $i$.
// The in-place reverse permutation swaps permutation pairs.
if (j > i)
    swap(image[i],image[j]);
