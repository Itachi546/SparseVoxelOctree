#define EPSILON 10e-9f
#define MAX_ITERATIONS 1000
#define BRICK_SIZE 8
#define BRICK_SIZE2 (BRICK_SIZE * BRICK_SIZE)
#define BRICK_SIZE3 (BRICK_SIZE2 * BRICK_SIZE)
#define GRID_MARCH_MAX_ITERATION 100

#define REFLECT(p, c) (2.0f * c - p)
#define GET_MASK(p) (p >> 30)
#define GET_FIRST_CHILD(p) (p & 0x3fffffff)
#define InternalLeafNode 0
#define InternalNode 1
#define LeafNode 2
#define LeafNodeWithPtr 3