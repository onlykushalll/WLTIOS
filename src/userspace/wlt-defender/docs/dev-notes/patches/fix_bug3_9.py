import sys
f = sys.argv[1]
s = open(f).read()

# BUG 3 fix: pack the itree_node_t struct so C layout matches Python struct.pack
old = """typedef struct {
    int32_t feature;
    double  threshold;
    int32_t left;
    int32_t right;
    int32_t size;
} itree_node_t;"""
new = """/* BUG 3 fix: __attribute__((packed)) so the C layout matches the Python
 * struct.pack('<idiii') format (24 bytes, no padding). Without packed, the
 * double forces 4 bytes of padding -> 28 bytes -> garbled reads. */
typedef struct __attribute__((packed)) {
    int32_t feature;
    double  threshold;
    int32_t left;
    int32_t right;
    int32_t size;
} itree_node_t;"""
assert old in s, "itree_node_t struct not found"
s = s.replace(old, new)

# BUG 9 fix: add n_nodes bounds check in g_iforest_score
# Store n_nodes in the struct + check node < n_nodes in the walk loop.
old_struct = """struct g_iforest {
    itree_node_t *nodes;
    int32_t *tree_roots;
    uint32_t n_trees;
    uint32_t max_depth;
    uint32_t n_features;
    double threshold;
};"""
new_struct = """struct g_iforest {
    itree_node_t *nodes;
    int32_t *tree_roots;
    uint32_t n_trees;
    uint32_t max_depth;
    uint32_t n_features;
    size_t   n_nodes;   /* BUG 9: bounds check for tree-walk safety */
    double threshold;
};"""
assert old_struct in s, "g_iforest struct not found"
s = s.replace(old_struct, new_struct)

# store n_nodes in g_iforest_load (after reading nodes)
old_load = """    if (fread(ifo->nodes, node_sz, n_nodes, f) != n_nodes) {
        free(ifo->nodes); free(ifo->tree_roots); free(ifo); fclose(f); return NULL;
    }
    fclose(f);"""
new_load = """    if (fread(ifo->nodes, node_sz, n_nodes, f) != n_nodes) {
        free(ifo->nodes); free(ifo->tree_roots); free(ifo); fclose(f); return NULL;
    }
    ifo->n_nodes = n_nodes;  /* BUG 9: store for bounds checking */
    fclose(f);"""
assert old_load in s, "load fread block not found"
s = s.replace(old_load, new_load)

# bounds check in the walk loop
old_walk = """        while (node >= 0 && f->nodes[node].feature != -1 && depth < f->max_depth + 16) {
            int feat = f->nodes[node].feature;"""
new_walk = """        while (node >= 0 && (size_t)node < f->n_nodes &&
               f->nodes[node].feature != -1 && depth < f->max_depth + 16) {
            int feat = f->nodes[node].feature;"""
assert old_walk in s, "walk loop not found"
s = s.replace(old_walk, new_walk)

open(f,'w').write(s)
print("BUG 3 + BUG 9 fixed: packed struct + n_nodes bounds check")
