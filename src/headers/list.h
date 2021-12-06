//
// Created by KoroLion on 12/6/2021.
//

#ifndef WEB_SERVER_LIST_H
#define WEB_SERVER_LIST_H

struct ListNode {
    void *ptr;

    struct ListNode *next;
};

void list_insert(struct ListNode **root_ptr, void *ptr);
void list_delete(struct ListNode **root_ptr, void *ptr);
void list_free(struct ListNode **root_ptr);

#endif //WEB_SERVER_LIST_H
