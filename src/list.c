//
// Created by KoroLion on 12/6/2021.
//

#include "stdlib.h"

#include "headers/list.h"

void list_insert(struct ListNode **root_ptr, void *ptr) {
    struct ListNode *root = *root_ptr;
    if (root == NULL) {
        *root_ptr = malloc(sizeof(struct ListNode));
        root = *root_ptr;
        root->ptr = ptr;
        root->next = NULL;
        return;
    }

    while (root->next != NULL) {
        root = root->next;
    }

    root->next = malloc(sizeof(struct ListNode));
    root->next->ptr = ptr;
    root->next->next = NULL;
}

void list_delete(struct ListNode **root_ptr, void *ptr) {
    struct ListNode *root = *root_ptr;

    if (root == NULL) {
        return;
    }

    if (root->ptr == ptr) {
        struct ListNode *next = root->next;
        free(root);
        *root_ptr = next;
        return;
    }

    struct ListNode *prev = root;
    while (root->ptr != ptr) {
        prev = root;
        root = root->next;
        if (root == NULL) {
            return;
        }
    }
    prev->next = root->next;
    free(root);
}

void list_free(struct ListNode **root_ptr) {
    struct ListNode *root = *root_ptr;
    while (root != NULL) {
        struct ListNode *next = root->next;
        free(root);
        root = next;
    }

    *root_ptr = NULL;
}
