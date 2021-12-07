//
// Created by KoroLion on 12/5/2021.
//
#include "stdio.h"
#include "stdlib.h"

#define BUFFER_SIZE 2048

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: hash PATH_TO_FILE\n");
        return -1;
    }

    const char *fpath = argv[1];

    FILE *f = fopen(fpath, "rb");
    if (f == NULL) {
        perror("Unable to open file!");
        return -1;
    }

    unsigned hash = 0;
    char buffer[BUFFER_SIZE];
    int count;

    while ((count = fread(buffer, sizeof(char), sizeof(buffer), f)) > 0) {
        for (int i = 0; i < count; i++) {
            hash += buffer[i];
        }
    }

    fclose(f);

    printf("%08x\n", hash);

    return 0;
}