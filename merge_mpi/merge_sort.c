#include <stdlib.h>
#include <string.h>
#include "merge_sort.h"

static void merge(int *values, int *temporary, int left, int middle, int right) {
    int i = left;
    int j = middle;
    int k = left;

    while (i < middle && j < right) {
        if (values[i] <= values[j]) {
            temporary[k++] = values[i++];
        } else {
            temporary[k++] = values[j++];
        }
    }

    while (i < middle) temporary[k++] = values[i++];
    while (j < right) temporary[k++] = values[j++];

    for (i = left; i < right; ++i) values[i] = temporary[i];
}

static void merge_sort_recursive(int *values, int *temporary, int left, int right) {
    if (right - left <= 1) return;

    int middle = left + (right - left) / 2;
    merge_sort_recursive(values, temporary, left, middle);
    merge_sort_recursive(values, temporary, middle, right);
    merge(values, temporary, left, middle, right);
}

void merge_sort(int *values, int length) {
    if (values == NULL || length < 2) return;

    int *temporary = malloc((size_t)length * sizeof(int));
    if (temporary == NULL) {
        abort();
    }

    merge_sort_recursive(values, temporary, 0, length);
    free(temporary);
}
