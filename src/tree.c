
#include "tree.h"

int tree_calc_size(const struct kd_node *node)
{
    if(!node)
        return 0;

    return 1 + tree_calc_size(node->kd_left) + tree_calc_size(node->kd_right);
}

void tree_copy_nodes(const struct feature *features, const struct kd_node *node, struct kd_node **compressed, const struct kd_node *root)
{
    // Make features pointer relative to start of features block
    // Casting both pointers to char* ensures that we're working on byte offsets.
    (*compressed)->features = (struct feature*)((char*)node->features - (char*)features);
    (*compressed)->n = node->n;
    (*compressed)->ki = node->ki;
    (*compressed)->kv = node->kv;
    (*compressed)->leaf = node->leaf;

    // Save current node pointer for kd_right, after left subtree has
    // been copied
    struct kd_node *this = *compressed;

    if(node->kd_left) {
        (*compressed)->kd_left = 1 + (struct kd_node*)((char*)(*compressed) - (char*)root);
	*compressed += 1;
        tree_copy_nodes(features, node->kd_left, compressed, root);
    }

    if(node->kd_right) {
        this->kd_right = 1 + (struct kd_node*)((char*)(*compressed) - (char*)root);
	*compressed += 1;
        tree_copy_nodes(features, node->kd_right, compressed, root);
    }
}

/**
 * Create packed (memory contiguus, relative pointers) copy of tree
 *
 * Pointers to kd_nodes become relative to the root node, pointers to features become
 * array indices to the features array.
 */
struct kd_node *tree_pack(const struct feature *features, const struct kd_node *tree, int *tree_size)
{
    *tree_size = tree_calc_size(tree);

    // calloc() sets kd_left/kd_right implicitly to 0, which
    // simplifies tree_copy_nodes
    struct kd_node *compressed = calloc(*tree_size, sizeof(struct kd_node));
    struct kd_node *current = compressed;

    tree_copy_nodes(features, tree, &current, current);

    return compressed;
}

/**
 * packs features and kd_tree into one contiguus memory region
 */
void pack(struct feature *features, int num_features, struct kd_node *kd_tree, char **data, size_t *data_size)
{
    struct sift_data sd = {.num_features = num_features};
    struct kd_node *compressed_tree = tree_pack(features, kd_tree, &sd.tree_size);
    *data_size = sizeof(struct sift_data) + sd.num_features * sizeof(struct feature) + sd.tree_size * sizeof(struct kd_node);
    *data = malloc(*data_size);

    memcpy((*data), &sd, sizeof(struct sift_data));
    memcpy((*data) + sizeof(struct sift_data), features, sd.num_features * sizeof(struct feature));
    memcpy((*data) + sizeof(struct sift_data) + sd.num_features * sizeof(struct feature), compressed_tree, sd.tree_size * sizeof(struct kd_node));

    free(compressed_tree);
}

void tree_unpack(const struct feature *features,  struct kd_node *node, const struct kd_node *root)
{
    // Make features pointer absolute again
    // Casting both pointers to char* ensures that we're working on byte offsets.
    node->features = (struct feature*)((char*)node->features + (size_t)features);

    if(node->kd_left) {
        node->kd_left = (struct kd_node*)((char*)node->kd_left + (size_t)root);
        tree_unpack(features, node->kd_left, root);
    }

    if(node->kd_right) {
        node->kd_right = (struct kd_node*)((char*)node->kd_right + (size_t)root);
        tree_unpack(features, node->kd_right, root);
    }
}

/**
 * unpacks packed data in-place
 */
void unpack(char *data, struct feature **features, struct kd_node **tree)
{
    struct sift_data *sd = (struct sift_data*)data;
    *features = (struct feature*)(data + sizeof(struct sift_data));
    *tree = (struct kd_node*)(data + sizeof(struct sift_data) + sd->num_features * sizeof(struct feature));

    tree_unpack(*features, *tree, *tree);
}
