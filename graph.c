#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define EPS 2
#define DBL_MIN -10000
#define DBL_MAX 10000

static void repeat_str(const char *str, int times, char *ret) {
    int len = strlen(str);
    printf("%d\n", len);
    for (int i = 0; i < times; i++) {
        strncpy(ret + i*len, str, len);
    }
    ret[len*times] = '\0';
}

static double *TheArray;
static int cmp(const void *a, const void *b){
    int ia = *(int *)a;
    int ib = *(int *)b;
    return (TheArray[ia] > TheArray[ib]) - (TheArray[ia] < TheArray[ib]);
}

static double l2_norm(double *u, double *v, int dim) {
    double norm = 0;
    for (int i = 0; i < dim; i++) {
        norm += (u[i] - v[i]) * (u[i] - v[i]);
    }
    return sqrt(norm);
}

static void construct_fully_connected_matrix(double *points, int lines, int dim, double *ret) {
    for (int i = 0; i < lines; ++i) {
        for (int j = 0; j < lines; ++j) {
            ret[i*lines + j] = l2_norm(&points[i*dim], &points[j*dim], dim);
            printf("%lf ", ret[i*lines + j]);
        }
        printf("\n");
    }
}

static void construct_eps_neighborhood_matrix(double *points, int lines, int dim, int *ret) {
    for (int i = 0; i < lines; ++i) {
        for (int j = 0; j < lines; ++j) {
            ret[i*lines + j] = l2_norm(&points[i*dim], &points[j*dim], dim) < EPS;
            printf("%d ", ret[i*lines + j]);
        }
        printf("\n");
    }
}

static void calculate_diagonal_degree_matrix(double * weighted_adj_matrix, int n, double *ret){
    for (int i = 0; i < n; i++) {
        double d_i = 0;
        for (int j = 0; j < n;j++) {
            d_i += weighted_adj_matrix[i*n+ j];
        }
        ret[i] = d_i;
        printf("%lf ", ret[i]);
    }
    printf("\n");
}

static void construct_normalized_laplacian_sym_matrix(double *weighted_adj_matrix, int num_points, double *ret){
    double sqrt_inv_degree_matrix[num_points];  // '1-d' array
    calculate_diagonal_degree_matrix(weighted_adj_matrix, num_points, sqrt_inv_degree_matrix); //load degree_matrix temporarily in sqrt_inv_degree_matrix
    for (int i =0; i < num_points; i++){
        sqrt_inv_degree_matrix[i] = 1.0/sqrt(sqrt_inv_degree_matrix[i]);
    }
    // compute D^(-1/2) W,  not sure if this code is optimal yet, how to avoid "jumping row"?  process one row each time enccourage spatial locality
    // but with this trick we avoid *0.0
    for (int i = 0; i < num_points; i++){
        for(int j = 0; j < num_points; j++){
            ret[i*num_points + j] = sqrt_inv_degree_matrix[i] * weighted_adj_matrix[i*num_points + j];
        }
    }
    // compute (D^(-1/2)*W)*D^(-1/2)
    for (int i = 0; i < num_points; i++) {
        for (int j = 0; j < num_points; j++) {
            if (i == j) {
                ret[i*num_points + j] = 1.0 - ret[i*num_points + j] * sqrt_inv_degree_matrix[j];
            } else {
                ret[i*num_points + j] = -ret[i*num_points + j] * sqrt_inv_degree_matrix[j];
            }
            printf("%lf ", ret[i*num_points + j]);
        }
        printf("\n");
    }
}

static void construct_normalized_laplacian_rw_matrix(double *weighted_adj_matrix, int num_points, double *ret) {
    double inv_degree_matrix[num_points];
    calculate_diagonal_degree_matrix(weighted_adj_matrix, num_points, inv_degree_matrix); //load degree_matrix temporarily in sqrt_inv_degree_matrix
    for (int i = 0; i < num_points; i++){
        inv_degree_matrix[i] = 1.0/inv_degree_matrix[i];
    }
    for (int i = 0; i < num_points; i++) {
        for (int j = 0; j < num_points; j++) {
            if (i == j) {
                ret[i*num_points + j] = 1.0 - inv_degree_matrix[i] * weighted_adj_matrix[i*num_points + j];
            } else {
                ret[i*num_points + j] = -inv_degree_matrix[i] * weighted_adj_matrix[i*num_points + j];
            }
            printf("%lf ", ret[i*num_points + j]);
        }
        printf("\n");
    }
}

static void construct_unnormalized_laplacian(double *graph, int n, double *ret) {
    // double* degrees = (double *)malloc(n * n * sizeof(double));
    double degrees[n];
    calculate_diagonal_degree_matrix(graph, n, degrees);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            ret[i*n+j] = ((i == j) ? degrees[i] : 0) - graph[i*n+j];
            printf("%f ", ret[i*n+j]);
        }
        printf("\n");
    }
}

static void construct_knn_matrix(double *points, int lines, int dim, int k, int *ret) {
    double vals[lines];
    int indices[lines];

    for (int i = 0; i < lines; ++i) {
        for(int ii = 0; ii < lines; ++ii) {
            indices[ii] = ii;
        }
        for (int j = 0; j < lines; ++j) {
            ret[i*lines + j] = 0.0;
            vals[j] = l2_norm(&points[i*dim], &points[j*dim], dim);
        }
        TheArray = vals;
        size_t len = sizeof(vals) / sizeof(*vals);
        qsort(indices, len, sizeof(*indices), cmp);
        for (int s = 0; s < k+1; ++s) {
            ret[i*lines + indices[s]] = (indices[s] != i) ? 1.0 : 0;
        }
    }

    for(int i = 0; i < lines; ++i) {
        for(int j = 0; j < lines; ++j) {
            printf("%d ", ret[i*lines + j]);
        }
        printf("\n");
    }
}
/*---- K-Means util methods ---------------------------------------------- */

struct cluster {
    int mean;
    int size;
    double *points;
};

static double init_means(double *points, int lines, int k, double *ret) {
    // find min/max bounds for each dimension
    int bounds[k*2];
    for (int i = 0; i < 2*k; i++) {  // row represents dimension
        bounds[i] = (i%2==0) ? DBL_MAX : DBL_MIN;  // first column represents min bound, second column max bound
    } // Right you need to set to opposite value !!
    for (int i = 0; i < lines; i++) { // each line is a point
        for (int j = 0; j < k; j++) {
            bounds[j*2] = (points[i*k + j] < bounds[j*2]) ? points[i*k + j] : bounds[j*2];
            bounds[j*2+1] = (points[i*k + j] > bounds[j*2+1]) ? points[i*k + j] : bounds[j*2+1];
        }
    }
    // generate k random means stores row-wise
    for (int i = 0; i < k; i++) {
        srand((i+1)*100*time(0));
        printf("Center %d: ( ", i);
        for (int j = 0; j < k; j++) {

            ret[i*k + j] = (rand() % (bounds[j * k + 1] - bounds[j * k] + 1)) + bounds[j * k];
            printf("%lf ", ret[i*k + j]);
        }
        printf(")\n");
    }
}

static double compute_mean_of_one_dimension(double *points, int size, int k, int dimension) {
    int sum = 0;
    for (int i = 0; i < size; i++) { // for all points
            sum += points[i*k + dimension]; // .. select one dimension
    }
    return (size > 0) ? (sum/size) : 0;
}

static double reset_means(struct cluster *clusters, int k, double *ret) {
    for (int i = 0; i < k; i++) { // re-compute the means (ret) for each cluster
        printf("Center %d: ( ", i);
        for (int j = 0; j < k; j++) {
            ret[i*k + j] = (clusters[j].size > 0) ? compute_mean_of_one_dimension(clusters[i].points, clusters[i].size, k, j) : clusters[j].mean;
            printf("%lf ", ret[i*k + j]);
        }
        printf(")\n");
    }

}

static int find_nearest_cluster_index(double *point, double *means, int k) {
    // use l2_norm
    double gap = l2_norm(point, &means[0], k);
    int index = 0;
    for (int i = 1; i < k; i++) { // for every cluster check abs distance to point and take the minimal
        double norm = l2_norm(point, &means[i*k], k);
        if(norm < gap) {
            gap = norm;
            index = i;
        }
    }
    return index;
}

static void map_to_nearest_cluster(double *points, int lines, int k, double *means, struct cluster *ret) {
   // potentially all points can be in the same cluster
    // find nearest cluster for each point = line
    int index_nn[lines];
    for (int j = 0; j < lines; j++) {
       index_nn[j] = find_nearest_cluster_index(&points[j * k], means, k); // find nearest mean for this point = line
    }
    for (int i = 0; i < k; i++) { // construct cluster one after another
        double tmp[lines*k];
        int cluster_size = 0; // keep tract of cluster size in # of points
        for (int j = 0; j < lines; j++) {
            if (index_nn[j] == i) {
                for (int e = 0; e < k; e++) {
                    tmp[cluster_size * k + e] = points[j * k + e];
                }
                cluster_size++;
            }
        } // done with point j

        for (int j = 0; j < cluster_size*k; j++) { // copy data
            ret[i].points[j] = tmp[j];
        }
        ret[i].mean = means[i];
        ret[i].size = cluster_size;
    } // done with cluster i

}


/*
 * K-Means Algorithm
 *
 *   1. Choose the number of clusters(K) and obtain the data points: Done
 *   2. Place the centroids c_1, c_2, ..... c_k randomly in [min..max]: Done
 *   3. Repeat steps 4 and 5 until convergence or until the end of a fixed number of iterations
 *   4. for each data point x_i:
 *          - find the nearest centroid(c_1, c_2 .. c_k)
 *          - assign the point to that cluster
 *   5. for each cluster j = 1..k
 *          - new centroid = mean of all points assigned to that cluster
 *   6. End
 *
 */
static void K_means(double *points, int lines, int k, int max_iter, struct cluster *ret) {
    int i = 0;
    double means[k*k];
    while (i < max_iter) {
        (i == 0) ? init_means(&points[0], lines, k, means) : reset_means(ret, k, means);
        // post condition: means is up-to-date
        map_to_nearest_cluster(points, lines, k, means, ret);
        // TODO: need to break before max_iter if all means are stable
        i++;
    }
    // print clusters: Cluster i : (1,2) (4,5) etc.
    for (int j = 0; j < k; j++) {
        printf("Cluster %d: ", j);
        for(int e = 0; e < ret[j].size; e++) {
            printf("( ");
            for (int f = 0; f < k; f++) {
                printf("%lf ", ret[j].points[e * k + f]);
            }
            printf(")  ");
        }
        printf("\n");
    }
}

/*------------------------------------------------------------------------ */

/*
 * The file that the program reads from is stored in the following format, assuming that
 * we are using n d-dimensional datapoints:
 * <d>\n
 * <Dim. 0 of point 0> <Dim. 1 of point 0> <Dim. 2 of point 0> ... <Dim. d of point 0>\n
 * <Dim. 0 of point 1> <Dim. 1 of point 1> <Dim. 2 of point 1> ... <Dim. d of point 1>\n
 *                           ........................
 * <Dim. 0 of point n-1> <Dim. 1 of point n-1> <Dim. 2 of point n-1> ... <Dim. d of point n-1>\n
 */

int main(int argc, char *argv[]) {

    FILE *fp;
    fp = fopen("points.txt", "r");

    // Count the number of lines in the file
    int lines = 0;
    while (EOF != (fscanf(fp, "%*[^\n]"), fscanf(fp,"%*c")))
        ++lines;
    --lines;  // Subtract one because it starts with the dimension
    printf("Read %d lines\n", lines);

    // Find the dimension
    rewind(fp);
    int dim;
    fscanf(fp, "%d\n", &dim);

    // Read the points
    char fmt[4*dim + 1];
    repeat_str("%lf ", dim, fmt);
    fmt[4*dim-1] = '\n';
    fmt[4*dim] = '\0';
    printf("Dimension = %d, fmt = %s", dim, fmt);
    double points[lines][2];
    for (int i = 0; i < lines; ++i) {

        fscanf(fp, fmt, &points[i][0], &points[i][1]);
    }

    // for (int i = 0; i < lines; ++i) {
    //     printf(fmt, points[i][0], points[i][1]);
    // }

    // Construct the matrices and print them
    // fully-connected matrix
    printf("Fully connected matrix:\n");
    double fully_connected[lines][lines];
    construct_fully_connected_matrix((double *) points, lines, dim, (double *) fully_connected);
    // epsilon neighborhood matrix

    printf("\nEps neighborhood matrix:\n");
    int eps_neighborhood[lines][lines];
    construct_eps_neighborhood_matrix((double *) points, lines, dim, (int *) eps_neighborhood);
    // Skip KNN matrix since too annoying to compute

    printf("\nKNN matrix:\n");
    int k = 2;
    int knn_graph[lines][lines];
    construct_knn_matrix((double *) points, lines, dim, k,(int *) knn_graph);

    printf("\nUnnormalized Laplacian:\n");
    // compute unnormalized laplacian
    double laplacian[lines][lines];
    construct_unnormalized_laplacian((double *) fully_connected, lines, (double *) laplacian);

    printf("\nRW Normalized Laplacian\n");
    // compute normalized rw laplacian
    double l_rw[lines][lines];
    construct_normalized_laplacian_rw_matrix((double *) fully_connected, lines, (double *) l_rw);

    printf("\nSymmetric Normalized Laplacian\n");
    // compute normalized rw laplacian
    double l_sym[lines][lines];
    construct_normalized_laplacian_sym_matrix((double *) fully_connected, lines, (double *) l_sym);

    printf("\nK-means Clustering\n");
    // U (8x2) is the data in points.txt for now => k = 2
    k = 2; // number of cluster <=> # columns of U
    struct cluster clusters[k];
    for (int i = 0; i < k; i++) {
        clusters[i].size = 0;
        clusters[i].points = (double *) malloc(lines * sizeof(double));
    }
    // try with different max_iter
    K_means((double *) points, lines, k, 10, clusters);
    return 0;
}