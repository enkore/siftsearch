#include <stdlib.h>

#include <cv.h>
#include <cxcore.h>
#include <highgui.h>

#include "sift.h"
#include "imgfeatures.h"
#include "kdtree.h"

/**
 * Stored in a dbm database
 *
 * Each value is sift_data + features + kd_tree
 *
 * Size: sizeof(sift_data) + num_features * sizeof(feature) + tree_size * sizeof(kd_node)
 */
struct sift_data {
    int num_features;
    int tree_size;
};

void pack(struct feature *features, int num_features, struct kd_node *kd_tree, char **data, size_t *data_size);
void unpack(char *data, struct feature **features, struct kd_node **tree);
