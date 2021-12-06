//
// Created by KoroLion on 12/3/2021.
//

#include "assert.h"

#include "headers/test.h"

#include "headers/http_utils.h"
#include "headers/list.h"

void test_get_ext() {
    char *ext;

    ext = get_ext("wolf.txt");
    assert(strncmp("txt", ext, strlen(ext)) == 0);
    free(ext);

    ext = get_ext("complex/path/wolf.jpeg");
    assert(strncmp("jpeg", ext, strlen(ext)) == 0);
    free(ext);

    ext = get_ext("wolf.backup.zip");
    assert(strncmp("zip", ext, strlen(ext)) == 0);
    free(ext);

    ext = get_ext("no_ext");
    assert(strncmp("", ext, strlen(ext)) == 0);
    free(ext);

    ext = get_ext(".only_ext");
    assert(strncmp("only_ext", ext, strlen(ext)) == 0);
    free(ext);
}

void test_urldecode() {
    int res_len = 2048;
    char *res = malloc(res_len * sizeof(char));

    urldecode(res, "wolf");
    assert(strncmp("wolf", res, res_len) == 0);

    strncpy(res, "wolf%20lion%20qwer", 18 + 1);
    urldecode(res, res);
    assert(strncmp("wolf lion qwer", res, res_len) == 0);

    urldecode(res, "wolf%20lion%20qwer");
    assert(strncmp("wolf lion qwer", res, res_len) == 0);

    urldecode(res, "%D1%80%D1%83%D1%81%D1%81%D0%BA%D0%B8%D0%B5%20%D0%B1%D1%83%D0%BA%D0%B2%D1%8B");
    assert(strncmp("русские буквы", res, res_len) == 0);

    urldecode(res, "https%3A%2F%2Fwolf.wolf%2Fwolf%3Fwolf%3Dwqerwrfsadf%20sadf%20sadfasd%20fdas%20fsadfsad%26lion%3D%D1%8B%D0%B2%D0%B0%D0%BB%D1%84%D1%8B%D0%B2%D0%BB%D1%8C%D0%B0%D0%B4%D1%84%D1%8B%20%D1%8C%D0%BB%D0%B4%D0%B0%D1%84%D1%8B%D0%B2%D1%8C%D1%82%D0%BB%D1%8C%D1%82%D0%B0%D1%84%D1%8B%D0%B2%D0%BB%D1%82%D1%8C%D0%B0%D0%BB%D1%8B%D1%84%D0%B2%D1%82");
    assert(strncmp("https://wolf.wolf/wolf?wolf=wqerwrfsadf sadf sadfasd fdas fsadfsad&lion=ывалфывльадфы ьлдафывьтльтафывлтьалыфвт", res, res_len) == 0);

    urldecode(res, "%BZ1%D0%B2");
    assert(strncmp("%BZ1в", res, res_len) == 0);

    free(res);
}

void test_list() {
    char *s1 = "wolf", *s2 = "lion", *s3 = "white";

    struct ListNode *l = NULL;

    list_insert(&l, s1);
    assert(strncmp(l->ptr, s1, strlen(s1)) == 0);
    assert(l->next == NULL);

    list_insert(&l, s2);
    assert(strncmp(l->ptr, s1, strlen(s1)) == 0);
    assert(strncmp(l->next->ptr, s2, strlen(s2)) == 0);
    assert(l->next->next == NULL);

    list_insert(&l, s3);
    assert(strncmp(l->ptr, s1, strlen(s1)) == 0);
    assert(strncmp(l->next->ptr, s2, strlen(s2)) == 0);
    assert(strncmp(l->next->next->ptr, s3, strlen(s3)) == 0);
    assert(l->next->next->next == NULL);

    list_delete(&l, (void*)123);
    assert(strncmp(l->ptr, s1, strlen(s1)) == 0);
    assert(strncmp(l->next->ptr, s2, strlen(s2)) == 0);
    assert(strncmp(l->next->next->ptr, s3, strlen(s3)) == 0);
    assert(l->next->next->next == NULL);

    list_delete(&l, s2);
    assert(strncmp(l->ptr, s1, strlen(s1)) == 0);
    assert(strncmp(l->next->ptr, s3, strlen(s3)) == 0);
    assert(l->next->next == NULL);

    list_delete(&l, s1);
    assert(strncmp(l->ptr, s3, strlen(s3)) == 0);
    assert(l->next == NULL);

    list_free(&l);
    assert(l == NULL);
}

void test() {
    test_get_ext();

    test_urldecode();

    test_list();
}
