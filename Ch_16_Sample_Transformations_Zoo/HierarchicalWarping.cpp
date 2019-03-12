node = root;
while (!node.isLeaf) {
    if (u < node.probLeft) {
        u /= node.probLeft;
        node = node.left;
    } else {
        u /= (1.0 - node.probLeft);
        node = node.right;
    }
}
// Ok. We have found a leaf with the correct probability!
