
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

#include <getopt.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include <err.h>
#include <fts.h>

#include <omp.h>

#include <gdbm.h>

#include <cv.h>
#include <cxcore.h>
#include <highgui.h>

#include "sift.h"
#include "imgfeatures.h"
#include "kdtree.h"

#include "util.h"
#include "tree.h"

/**
 * Contrast threshold for finding SIFT features
 *
 * default: 0.04, which yields lots and lots of points
 * recommended: 0.1
 */
#define SIFT_CONTRAST_THRESHOLD 0.1

/**
 * the maximum number of keypoint NN candidates to check during BBF search
 */
#define KDTREE_BBF_MAX_NN_CHKS 200

/**
 * threshold on squared ratio of distances between NN and 2nd NN
 *
 * This basically defines the neighbourhood size for matches
 */
#define NN_SQ_DIST_RATIO_THR 0.49

#define tprintf(...) {						\
	fprintf(stderr, "[%02d] ", omp_get_thread_num());	\
	fprintf(stderr, __VA_ARGS__);				\
    }

int verbose = 0;

struct match {
    char *file;
    int num_features;
    int num_matches;
    int percent;
};

const char *file_extensions[] = {".jpg", ".jpeg", ".png", ".bmp", ".gif", 0};

int match_compare(const void *a_, const void *b_)
{
    const struct match *a = a_, *b = b_;

    if(a->percent > b->percent)
        return 1;
    if(a->percent < b->percent)
        return -1;
    return 0;
}

void match_sort(struct match *matches, int num_matches)
{
    qsort(matches, num_matches, sizeof(*matches), match_compare);
}

/**
 * Apply SIFT algorithm to an image
 *
 * @return 0 on failure, nonzero on success
 */
int sift(const char *path, struct feature **features, int *num_features)
{
    IplImage *img;
    static const int MAX_SIZE = 800;

    img = cvLoadImage(path, 1);
    if(!img) {
        fprintf(stderr, "%s: I/O error\n", path);
        return 0;
    }

    if(img->width > MAX_SIZE || img->height > MAX_SIZE) {
        IplImage *scaled = cvCreateImage(
                               cvSize(img->width > MAX_SIZE ? MAX_SIZE : img->width, img->height > MAX_SIZE ? MAX_SIZE : img->height)
                               , img->depth, img->nChannels);

        if(!scaled) {
            fprintf(stderr, "%s: cvCreateImage failed (out of memory?)\n", path);
            cvReleaseImage(&img);
            return 0;
        }

        cvResize(img, scaled, CV_INTER_LINEAR);
        cvReleaseImage(&img);
        img = scaled;
    }

    (*num_features) = _sift_features(img, features, SIFT_INTVLS, SIFT_SIGMA, SIFT_CONTRAST_THRESHOLD,
                                     SIFT_CURV_THR, SIFT_IMG_DBL, SIFT_DESCR_WIDTH,
                                     SIFT_DESCR_HIST_BINS);
    cvReleaseImage(&img);

    if(!(*features) || !(*num_features)) {
        fprintf(stderr, "%s: No features found\n", path);
        return 0;
    }

    return 1;
}

void index_file(char *path, void* db_)
{
    GDBM_FILE db = (GDBM_FILE)db_;

    datum key = {path, strlen(path)+1};
    int skip;

    #pragma omp critical
    skip = gdbm_exists(db, key);
    if(skip) {
        free(path);
        return;
    }

    struct feature *features;
    int num_features;
    struct kd_node *kd_tree;
    char *data;
    size_t data_size;

    if(verbose)
        tprintf("%s\n", path);

    if(!sift(path, &features, &num_features)) {
        free(path);
        return;
    }

    if(verbose)
        tprintf("  %d features detected\n", num_features);

    kd_tree = kdtree_build(features, num_features);

    pack(features, num_features, kd_tree, &data, &data_size);

    free(features);
    kdtree_release(kd_tree);

    datum value = {data, data_size};

    #pragma omp critical
    gdbm_store(db, key, value, GDBM_REPLACE);

    free(data);
    free(path);
}

int update_index(const char *dir, GDBM_FILE db)
{
    return enumerate_files(dir, file_extensions, index_file, db);
}

int match_file(const char *file, GDBM_FILE db, struct match **pmatches, int *pnum_matches)
{
    struct match *matches = malloc(1);
    int num_matches = 0;
    struct feature *other_features;
    int num_other_features;

    if(!sift(file, &other_features, &num_other_features)) {
        return 0;
    }

    datum key = gdbm_firstkey(db);
    static const int bufsz = 64;
    struct {
        char *key;
        char *data;
    } buffer[bufsz];

    while(1) {
        int j;

        if(!key.dptr)
            break;

        for(j = 0; j < bufsz && key.dptr; j++, key = gdbm_nextkey(db, key)) {
            datum data = gdbm_fetch(db, key);
            buffer[j].key = key.dptr;
            buffer[j].data = data.dptr;
        }

        #pragma omp parallel for
        for(int k = 0; k < j; k++) {
            struct feature *features, *feat;
            struct kd_node *tree;

            unpack(buffer[k].data, &features, &tree);

            struct feature** nbrs;
            double d0, d1;
            int i, m = 0;
            for(i = 0; i < num_other_features; i++) {
                feat = other_features + i;
                if(kdtree_bbf_knn(tree, feat, 2, &nbrs, KDTREE_BBF_MAX_NN_CHKS) == 2) {
                    d0 = descr_dist_sq(feat, nbrs[0]);
                    d1 = descr_dist_sq(feat, nbrs[1]);
                    if(d0 < d1 * NN_SQ_DIST_RATIO_THR) {
                        m++;
                        other_features[i].fwd_match = nbrs[0];
                    }
                }
                free(nbrs);
            }

            int match_percent = (m*100) / num_other_features;
            if(match_percent > 10) {
                if(isatty(STDOUT_FILENO)) {
                    printf("%s: %d matches [%d %%]\n", buffer[k].key, m, (m*100)/num_other_features);
                } else {
                    puts(buffer[k].key);
                }

                struct match match = {
                    .file = strdup(buffer[k].key),
                    .num_features = num_other_features,
                    .num_matches = m,
                    .percent = match_percent
                };

                num_matches += 1;
                matches = realloc(matches, num_matches * sizeof(*matches));
                matches[num_matches-1] = match;
            }

            free(buffer[k].key);
            free(buffer[k].data);
        }
    }

    free(other_features);

    *pmatches = matches;
    *pnum_matches = num_matches;

    return 0;
}

void print_usage(FILE* f)
{
    fprintf(f,
            "siftsearch\n"
            "Yet another image search tool\n"
            "Copyright (c) 2014 Marian Beermann, GPLv3 license\n"

            "\nUsage:\n"
            "\tsiftsearch --help\n"
            "\tsiftsearch [--db <file>] [--clean] --index <directory>\n"
            "\tsiftsearch [--db <file>] [--exec <command>] <file> ...\n"
            "\tsiftsearch [--db <file>] --dump\n"

            "\nOptions:\n"
            "    --db <file>\n"
            "\tSet database file to use.\n"
            "    --index <directory>\n"
            "\tCreate or update database from all images in the given directory\n"
            "\t, recursing subdirectories.\n"
            "    --clean\n"
            "\tDiscard existing database contents, if any. Not effective without\n"
            "\t--index.\n"
            "    --exec <command>\n"
            "\tExecute <command> with absolute paths to found files sorted by ascending\n"
            "\tmatch percentage.\n"
            "    --help\n"
            "\tDisplay this help message.\n"
            "    --dump\n"
            "\tList all indexed files and exit.\n");
}

int main(int argc, char **argv)
{
    GDBM_FILE db;
    char *dbfile = "sift.db";
    int dbflags = 0;
    char *index_dir = 0;
    char *exec = 0;
    (void)exec;
    int dump = 0;

    struct option long_options[] = {
        {"index",required_argument, 0,  'i'},
        {"clean",no_argument,       &dbflags, GDBM_NEWDB},
        {"db",   required_argument, 0,  'b'},
        {"exec", required_argument, 0,  'e'},
        {"dump", no_argument,       &dump, 1},
        {"help", no_argument,       0,  'h'},
        {"verbose",no_argument,     &verbose, 1},
        {0,      0,                 0,   0}
    };

    while(1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "", long_options, &option_index);
        if(c == -1)
            break;

        switch (c) {
        case 'i':
            index_dir = optarg;
            break;
        case 'b':
            dbfile = optarg;
            break;
        case 'e':
            exec = optarg;
            break;
        case 'h':
            print_usage(stderr);
            return 0;
        case 0: // Flags
            break;
        default:
            return 1;
        }
    }

    if(index_dir) {
        db = gdbm_open(dbfile, 1024, GDBM_WRCREAT|dbflags, 0644, 0);
        if(!db) {
            fprintf(stderr, "Can't open '%s': %s\n", dbfile, gdbm_strerror(gdbm_errno));
            return 1;
        }

        printf("Indexing '%s'\n", index_dir);
        update_index(index_dir, db);
        gdbm_close(db);
    }

    // Subsequent operations require no write access
    db = gdbm_open(dbfile, 1024, GDBM_READER, 0, 0);
    if(!db) {
        fprintf(stderr, "Can't open '%s': %s\n", dbfile, gdbm_strerror(gdbm_errno));
        return 1;
    }

    if(dump) {
        // This functionality doesn't seem to be available easily
        // using gdbmtool or similar thus I added it here
        datum key = gdbm_firstkey(db);

        while(key.dptr) {
            datum nextkey = gdbm_nextkey(db, key);
            puts(key.dptr);
            free(key.dptr);
            key = nextkey;
        }

        gdbm_close(db);
        return 0;
    }

    // At least space for argv[0] and terminating NUL
    char **exec_files = malloc(2*sizeof(char*));
    int num_exec_files = 1;
    exec_files[0] = exec;

    for(int i = optind; i < argc; i++) {
        struct match *matches;
        int num_matches = 0;

        fprintf(stderr, "Matching '%s'...\n", argv[i]);
        match_file(argv[i], db, &matches, &num_matches);

        num_exec_files += num_matches;
        exec_files = realloc(exec_files, (num_exec_files+1)*sizeof(char*));
        match_sort(matches, num_matches);

        for(int j = 0; j < num_matches; j++) {
            exec_files[num_exec_files - num_matches + j] = matches[j].file;
        }
    }

    if(exec) {
	if(verbose) {
	    for(int i = 0; i < num_exec_files; i++)
		printf("%s ", exec_files[i]);
	    printf("\n");
	}

        gdbm_close(db);

        // terminate array with NUL
        exec_files[num_exec_files] = 0;

        execvp(exec, exec_files);
    }

    for(int i = 0; i < num_exec_files; i++)
        free(exec_files[i]);
    free(exec_files);

    gdbm_close(db);
    return 0;
}
