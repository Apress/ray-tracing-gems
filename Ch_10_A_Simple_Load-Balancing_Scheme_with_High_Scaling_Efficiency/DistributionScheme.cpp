// Pseudocode outlining the distribution scheme.

const unsigned n = image.width() * image.height();
const unsigned m = 1u << b;
const unsigned s = (n + m - 1) / m;
const unsigned bits = (sizeof(unsigned) * CHAR_BIT) - b;

// Assuming a relative speed of $w_k$, processor $k$ handles
// $\lfloor w_k m\rceil$ regions starting at index $base=\sum_{l=0}^{k-1}\lfloor s_l m\rceil.$

// On processor $k$, each pixel index $i$ in the contiguous block
// of $s\lfloor w_k m\rceil$ pixels is distributed across
// the image by this permutation:
const unsigned f = i / s;
const unsigned p = i % s;
const unsigned j = (reverse(f) >> bits) + p;

// Padding pixels are ignored.
if (j < n)
    image[j] = render(j);
