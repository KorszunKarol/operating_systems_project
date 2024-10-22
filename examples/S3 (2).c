/*
* Solution S3 Operating Systems - Threads
* Year 2023-24
*
* @author: Ferran Castañé
*
*/

#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 1
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>



#define ERROR_MSG_NUM_ARGS "ERROR: Incorrect number of arguments\nPlease provide the input file name\nUsage: ./S3 <data_file>\n"
#define ERROR_MSG_FILE_NOT_FOUND "ERROR: File not found\n"

#define printF(x) write(1, x, strlen(x))




typedef struct {
    double *data;
    int size;
    double result;
} thread_data;



/**
 * Reads from fd file descriptor until cEnd is found or is EOF. This character is not included to the array, which is NULL
 * terminated. All the data is stored in dynamic memory
 * @param fd file descriptor to read from
 * @param cEnd delimiting character
 * @return a pointer to an array of characters
 */char *read_until(int fd, char end) {
    char *string = NULL;
    char c = '\0';
    int i = 0, size;

    while (1) {
        size = read(fd, &c, sizeof(char));
        if (string == NULL) {
            string = (char *)malloc(sizeof(char));
        }
        if (c != end && size > 0) {
            string = (char *)realloc(string, sizeof(char) * (i + 2));
            string[i++] = c;
        } else {
            break;
        }
    }
    string[i] = '\0';
    return string;
}


/**
 * Compares two double values for the qsort function
 * @param a pointer to the first value
 * @param b pointer to the second value
 * @return 1 if a > b, -1 if a < b, 0 if a == b
 */
int compare(const void *a, const void *b) {
    double A = *(double *)a;
    double B = *(double *)b;
    return (A > B) - (A < B);
}

/**
 * Sorts the data in ascending order using the quicksort algorithm
 * @param data array of data
 * @param size size of the array
 * @return a pointer to the sorted array
 */
double *sort_data(double *data, int size) {
    double *sorted_data = (double *)malloc(sizeof(double) * size);
    memcpy(sorted_data, data, sizeof(double) * size);
    qsort(sorted_data, size, sizeof(double), compare);
    return sorted_data;
}



/**
 * Calculates the mean of the data
 * @param arg pointer to a thread_data structure
 */
void *calculate_mean(void *arg) {
    thread_data *data = (thread_data *)arg; // Cast the argument to the correct type of data structure
    double sum = 0.0;
    for (int i = 0; i < data->size; i++) {
        sum += data->data[i];
    }
    data->result = sum / data->size; // Store the result in the data structure
    return NULL;
}

/**
 * Calculates the median of the data
 * @param arg pointer to a thread_data structure
 */

void *find_median(void *arg){
    thread_data * data = (thread_data *)arg;
    double median = 0.0;
    double *sorted_data =  sort_data(data->data, data->size);
    if (data->size % 2 == 0){
        median = (sorted_data[data->size / 2] + sorted_data[data->size / 2 - 1]) / 2;
    } else {
        median = sorted_data[data->size / 2];
    }
    data->result = median;
    free(sorted_data);
    return NULL;

}

/**
 * Calculates the variance of the data
 * @param arg pointer to a thread_data structure
 */
void *find_variance(void *args){
    thread_data * data = (thread_data *)args;
    double variance = 0.0;
    double mean = data->result;

    for (int i = 0; i < data->size; i++){
        variance += pow(data->data[i] - mean, 2);
    }
    variance = variance / data->size;
    data->result = variance;
    return NULL;
}


/**
 * Finds the maximum value in the data
 * @param arg pointer to a thread_data structure
 */
void *find_max(void *arg) {
    thread_data *data = (thread_data *)arg;
    double max = data->data[0];
    for (int i = 1; i < data->size; i++) {
        if (data->data[i] > max) {
            max = data->data[i];
        }
    }
    data->result = max;
    return NULL;
}

/**
 * Finds the minimum value in the data
 * @param arg pointer to a thread_data structure
 */

void *find_min(void *arg){
    thread_data * data = (thread_data *)arg;
    double min = data->data[0];
    for (int i = 1; i < data->size; i++){
        if (data->data[i] < min){
            min = data->data[i];
        }
    }
    data->result = min;
    return NULL;
}


double *read_file(int fd, int *size) {
    double *data = NULL;
    char *num_str;

    while (1) {
        num_str = read_until(fd, '\n');
        if (strlen(num_str) == 0) {
            free(num_str);
            break;
        }
        data = (double *)realloc(data, sizeof(double) * ((*size) + 1));
        data[(*size)++] = atof(num_str);
        free(num_str);
    }
    close(fd);
    return data;
}

/**
 * Main function
 * @param argc number of arguments
 * @param argv array of arguments
 */
int main(int argc, char *argv[]) {
    pthread_t threads[5];
    thread_data tdata[5];
    int size = 0;
    char *buffer = NULL;

    if (argc != 2) {
        printF(ERROR_MSG_NUM_ARGS);
        exit(1);
    }

    // Ignore Ctrl+C (SIGINT)
    signal(SIGINT, SIG_IGN);

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        printF(ERROR_MSG_FILE_NOT_FOUND);
        exit(1);
    }

    double *data = read_file(fd, &size);

    for(int i= 0; i < 5; i++){
        tdata[i].data = data;
        tdata[i].size = size;

        switch (i)
        {
        case 0:
            pthread_create(&threads[i], NULL, calculate_mean, &tdata[i]);
            break;
        case 1:
            pthread_create(&threads[i], NULL, find_median, &tdata[i]);
            break;
        case 2:
            pthread_create(&threads[i], NULL, find_max, &tdata[i]);
            break;
        case 3:
            pthread_create(&threads[i], NULL, find_min, &tdata[i]);
            break;
        case 4:
            pthread_join(threads[0], NULL);
            tdata[i].result = tdata[0].result;
            pthread_create(&threads[i], NULL, find_variance, &tdata[i]);
            break;
        default:
            printF("Error\n");
            break;
        }
    }

    for(int i = 1; i < 5; i++){
        pthread_join(threads[i], NULL);
    }


    asprintf(&buffer, "Mean: %f\n", tdata[0].result);
    printF(buffer);
    free(buffer);


    asprintf(&buffer, "Median: %f\n", tdata[1].result);
    printF(buffer);
    free(buffer);

    asprintf(&buffer, "Maximum value: %f\n", tdata[2].result);
    printF(buffer);
    free(buffer);

    asprintf(&buffer, "Minimum value: %f\n", tdata[3].result);
    printF(buffer);
    free(buffer);

    asprintf(&buffer, "Variance: %f\n", tdata[4].result);
    printF(buffer);
    free(buffer);

    free(data);


    return 0;
}
