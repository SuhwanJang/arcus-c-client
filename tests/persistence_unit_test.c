#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "libmemcached/memcached.h"

#define test_literal_param(X) (X), (strlen(X))
#define key_format "%s_key"

bool request_or_check;
memcached_st *mc;
memcached_return_t rc;
char key[32000];

typedef struct _test_st {
    char *command_name;
    memcached_return_t (*command)(const char *command_name);
} test_s;

static memcached_return_t value_compare(const char *value, const char *getvalue)
{
    assert(getvalue != NULL);
    if(strncmp(value, getvalue, strlen(value)) != 0) {
        printf("value mismatch. stored value = %.*s, get value = %.*s\n", (int)strlen(value), value, (int)strlen(getvalue), getvalue);
        return MEMCACHED_FAILURE;
    }
    return MEMCACHED_SUCCESS;
}

static memcached_return_t put_item(const char *key, const char *value) {
    rc = memcached_set(mc, test_literal_param(key), test_literal_param(value), 0, 0);
    return rc;
}

static memcached_return_t set(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";
    if (request_or_check) {
        rc = memcached_set(mc, test_literal_param(key), test_literal_param(value), 0, 0);
    } else {
        uint32_t flags;
        size_t string_length;
        char *get_value;
        get_value = memcached_get(mc, test_literal_param(key), &string_length, &flags, &rc);
        if (rc != MEMCACHED_SUCCESS) return rc;
        rc = value_compare(value, get_value);
    }
    return rc;
}

static memcached_return_t add(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";
    if (request_or_check) {
        rc = memcached_add(mc, test_literal_param(key), test_literal_param(value), 0, 0);
    } else {
        uint32_t flags;
        size_t string_length;
        char *get_value;
        get_value = memcached_get(mc, test_literal_param(key), &string_length, &flags, &rc);
        if (rc != MEMCACHED_SUCCESS) return rc;
        rc = value_compare(value, get_value);
    }
    return rc;
}
    
static memcached_return_t cas(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";
    char *ext = "xxx";

    unsigned int set= 1;
    memcached_behavior_set(mc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, set);
    if (request_or_check) {
        rc = put_item(key, value);
        if (rc != MEMCACHED_SUCCESS) {
            printf("failed put item.\n");
            return rc;
        }

        const char *keys[2] = { key, NULL};
        size_t key_lengths[2] = {strlen(key), 0 };
        memcached_mget(mc, keys, key_lengths, 1);
        memcached_result_st result_obj;
        memcached_result_st *result = memcached_result_create(mc, &result_obj);
        if (result == NULL) {
            printf("failed memcached_result_create()\n");
            return MEMCACHED_CLIENT_ERROR;
        }
        result = memcached_fetch_result(mc, &result_obj, &rc);
        if (rc != MEMCACHED_SUCCESS) {
            printf("failed memcahced_fetch_result().\n");
            return rc;
        }
        uint64_t cas = memcached_result_cas(result);
        rc = memcached_cas(mc, test_literal_param(key), test_literal_param(ext), 0, 0, cas);
        memcached_result_free(&result_obj);
    } else {
        uint32_t flags;
        size_t string_length;
        char *get_value;
        get_value = memcached_get(mc, test_literal_param(key), &string_length, &flags, &rc);
        if (rc != MEMCACHED_SUCCESS) return rc;
        rc = value_compare(ext, get_value);
    }
    set= 0;
    memcached_behavior_set(mc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, set);
    return rc;
}

static memcached_return_t delete(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";

    if (request_or_check) {
        rc = put_item(key, value);
        if (rc != MEMCACHED_SUCCESS) {
            printf("failed put item.\n");
            return rc;
        }
        rc = memcached_delete(mc, test_literal_param(key), 0); 
    } else {
        uint32_t flags;
        size_t string_length;
        char *string;
        string = memcached_get(mc, test_literal_param(key), &string_length, &flags, &rc);
        if (rc == MEMCACHED_NOTFOUND) {
            rc = MEMCACHED_SUCCESS;
        }
    }
        return rc;
}

static memcached_return_t append(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";
    char *ext = "xxx";

    if (request_or_check) {
        rc = put_item(key, value);
        if (rc != MEMCACHED_SUCCESS) {
            printf("failed put item.\n");
            return rc;
        }
        rc = memcached_append(mc, test_literal_param(key), test_literal_param(ext), 0, 0);
    } else {
        char expected_value[50];
        sprintf(expected_value, "%s%s", value, ext);
        uint32_t flags;
        size_t string_length;
        char *get_value;
        get_value = memcached_get(mc, test_literal_param(key), &string_length, &flags, &rc);
        if (rc != MEMCACHED_SUCCESS) return rc;
        rc = value_compare(expected_value, get_value);
    }
        return rc;
}

static memcached_return_t prepend(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";
    char *ext = "xxx";

    if (request_or_check) {
        rc = put_item(key, value);
        if (rc != MEMCACHED_SUCCESS) {
            printf("failed put item.\n");
            return rc;
        }
        rc = memcached_prepend(mc, test_literal_param(key), test_literal_param(ext), 0, 0); 
    } else {
        char expected_value[50];
        sprintf(expected_value, "%s%s", ext, value);
        uint32_t flags;
        size_t string_length;
        char *get_value;
        get_value = memcached_get(mc, test_literal_param(key), &string_length, &flags, &rc);
        if (rc != MEMCACHED_SUCCESS) return rc;
        rc = value_compare(expected_value, get_value);
    }
    return rc;
}

static memcached_return_t replace(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";
    char *ext = "xxx";

    if (request_or_check) {
        rc = put_item(key, value);
        if (rc != MEMCACHED_SUCCESS) {
            printf("failed put item.\n");
            return rc;
        }
        rc = memcached_replace(mc, test_literal_param(key), test_literal_param(ext), 0, 0); 
    } else {
        char *expected_value = ext;
        uint32_t flags;
        size_t string_length;
        char *get_value;
        get_value = memcached_get(mc, test_literal_param(key), &string_length, &flags, &rc);
        if (rc != MEMCACHED_SUCCESS) return rc;
        rc = value_compare(expected_value, get_value);
    }
    return rc;
}

static memcached_return_t increment(const char *command)
{
    sprintf(key, key_format, command);
    int ivalue = 1000;
    char value[10];
    sprintf(value, "%d", ivalue);
    int iext = 10;

    if (request_or_check) {
        rc = put_item(key, value);
        if (rc != MEMCACHED_SUCCESS) {
            printf("failed put item.\n");
            return rc;
        }
        uint64_t result = 0;
        rc = memcached_increment(mc, test_literal_param(key), iext, &result); 
    } else {
        int inc_res = ivalue + iext;
        char expected_value[50];
        sprintf(expected_value, "%d", inc_res);
        uint32_t flags;
        size_t string_length;
        char *get_value;
        get_value = memcached_get(mc, test_literal_param(key), &string_length, &flags, &rc);
        if (rc != MEMCACHED_SUCCESS) return rc;
        rc = value_compare(expected_value, get_value);
    }
    return rc;
}

static memcached_return_t decrement(const char *command)
{
    sprintf(key, key_format, command);
    int ivalue = 1000;
    char value[10];
    sprintf(value, "%d", ivalue);
    int iext = 10;

    if (request_or_check) {
        rc = put_item(key, value);
        if (rc != MEMCACHED_SUCCESS) {
            printf("failed put item.\n");
            return rc;
        }
        uint64_t result = 0;
        rc = memcached_decrement(mc, test_literal_param(key), iext, &result); 
    } else {
        int dec_res = ivalue - iext;
        char expected_value[50];
        sprintf(expected_value, "%d", dec_res);
        uint32_t flags;
        size_t string_length;
        char *get_value;
        get_value = memcached_get(mc, test_literal_param(key), &string_length, &flags, &rc);
        if (rc != MEMCACHED_SUCCESS) return rc;
        rc = value_compare(expected_value, get_value);

    }
    return rc;
}

static memcached_return_t lop_create(const char *command)
{
    sprintf(key, key_format, command);

    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);
        memcached_coll_create_attrs_set_overflowaction(&attributes, OVERFLOWACTION_ERROR);
        // CREATED
        rc = memcached_lop_create(mc, test_literal_param(key), &attributes);
    } else {
        memcached_coll_attrs_st attrs;
        rc = memcached_get_attrs(mc, test_literal_param(key), &attrs);
    }
    return rc;
}

static memcached_return_t lop_insert(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";

    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);
        // CREATED_STORED
        rc = memcached_lop_insert(mc, test_literal_param(key), 0, test_literal_param(value), &attributes);
    } else {
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        rc = memcached_lop_get(mc, test_literal_param(key), 0, false/*with delete*/, false/*drop_if_empty*/, result);
        if (rc != MEMCACHED_SUCCESS) return rc;
        const char *get_value = memcached_coll_result_get_value(result, 0);
        rc = value_compare(value, get_value);
        memcached_coll_result_free(result);
    }
    return rc;
}

static memcached_return_t lop_delete(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";

    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);

        // CREATED_STORED
        rc = memcached_lop_insert(mc, test_literal_param(key), 0, test_literal_param(value), &attributes);
        if (rc != MEMCACHED_SUCCESS) {
            printf("lop create & insert failed.\n");
            return rc;
        }
        // DELETE
        rc = memcached_lop_delete(mc, test_literal_param(key), 0, false /* drop if empty */); 
    } else {
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        rc = memcached_lop_get(mc, test_literal_param(key), 0, false/*with delete*/, false/*drop_if_empty*/, result);
        if (rc == MEMCACHED_NOTFOUND_ELEMENT) rc = MEMCACHED_SUCCESS;
    }
    return rc;
}

static memcached_return_t sop_create(const char *command)
{
    sprintf(key, key_format, command);

    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);
        memcached_coll_create_attrs_set_overflowaction(&attributes, OVERFLOWACTION_ERROR);
        // CREATED
        rc = memcached_sop_create(mc, test_literal_param(key), &attributes);
    } else {
        memcached_coll_attrs_st attrs;
        rc = memcached_get_attrs(mc, test_literal_param(key), &attrs);
    }

    return rc;
}

static memcached_return_t sop_insert(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";

    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);

        // CREATED_STORED
        rc = memcached_sop_insert(mc, test_literal_param(key), test_literal_param(value), &attributes);
    } else {
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        rc = memcached_sop_get(mc, test_literal_param(key), maxcount, false, false, result); 
        if (rc != MEMCACHED_SUCCESS) return rc;
        const char *get_value = memcached_coll_result_get_value(result, 0);
        rc = value_compare(value, get_value);
        memcached_coll_result_free(result);
    }
    return rc;
}

static memcached_return_t sop_delete(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";

    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);

        // CREATED_STORED
        rc = memcached_sop_insert(mc, test_literal_param(key), test_literal_param(value), &attributes);
        if (rc != MEMCACHED_SUCCESS) {
            printf("sop create & insert failed.\n");
            return rc;
        }

        // DELETE
        rc = memcached_sop_delete(mc, test_literal_param(key), test_literal_param(value), false /* drop if empty */); 
    } else {
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        rc = memcached_sop_get(mc, test_literal_param(key), maxcount, false/*with delete*/, false/*drop_if_empty*/, result);
        if (rc == MEMCACHED_NOTFOUND_ELEMENT) rc = MEMCACHED_SUCCESS;
    }
    return rc;
}

static memcached_return_t mop_create(const char *command)
{
    sprintf(key, key_format, command);

    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);
        memcached_coll_create_attrs_set_overflowaction(&attributes, OVERFLOWACTION_ERROR);
        // CREATED
        rc = memcached_mop_create(mc, test_literal_param(key), &attributes);
    } else {
        memcached_coll_attrs_st attrs;
        rc = memcached_get_attrs(mc, test_literal_param(key), &attrs);
    }
    return rc;
}

static memcached_return_t mop_insert(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";
    char *field = "field";

    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);

        // CREATED_STORED
        rc= memcached_mop_insert(mc, test_literal_param(key), test_literal_param(field), test_literal_param(value), &attributes);
    } else {
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        rc = memcached_mop_get(mc, test_literal_param(key), test_literal_param(field), false, false, result); 
        if (rc != MEMCACHED_SUCCESS) return rc;
        const char *get_value = memcached_coll_result_get_value(result, 0);
        rc = value_compare(value, get_value);
        memcached_coll_result_free(result);
    }
    return rc;
}

static memcached_return_t mop_delete(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";
    char *field = "field";

    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);

        // CREATED_STORED
        rc = memcached_mop_insert(mc, test_literal_param(key), test_literal_param(field), test_literal_param(value), &attributes);
        // DELETE
        rc = memcached_mop_delete(mc, test_literal_param(key), test_literal_param(field), false /* drop if empty */); 
    } else {
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        rc = memcached_mop_get(mc, test_literal_param(key), test_literal_param(field), false, false, result); 
        if (rc == MEMCACHED_NOTFOUND_ELEMENT) rc = MEMCACHED_SUCCESS;
    }
    return rc;
}

static memcached_return_t mop_update(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";
    char *field = "field";
    char *ext = "xxx";

    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);

        // CREATED_STORED
        rc = memcached_mop_insert(mc, test_literal_param(key), test_literal_param(field), test_literal_param(value), &attributes);
        if (rc != MEMCACHED_SUCCESS) {
            printf("mop create & insert failed.\n");
            return rc;
        }

        // UPDATE
        rc = memcached_mop_update(mc, test_literal_param(key), test_literal_param(field), test_literal_param(ext)); 
    } else {
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        rc = memcached_mop_get(mc, test_literal_param(key), test_literal_param(field), false, false, result); 
        if (rc != MEMCACHED_SUCCESS) return rc;
        const char *get_value = memcached_coll_result_get_value(result, 0);
        rc = value_compare(ext, get_value);
        memcached_coll_result_free(result);
    }
        return rc;
}

static memcached_return_t btree_create(const char *command)
{
    sprintf(key, key_format, command);

    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);
        memcached_coll_create_attrs_set_overflowaction(&attributes, OVERFLOWACTION_ERROR);
        // CREATED
        rc = memcached_bop_create(mc, test_literal_param(key), &attributes);
    } else {
        memcached_coll_attrs_st attrs;
        rc = memcached_get_attrs(mc, test_literal_param(key), &attrs);
    }
    return rc;
}

static memcached_return_t btree_insert(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";

    uint64_t bkey_1 = 1;
    uint64_t bkey_2 = 2;
    char *eflag = "0x3F";
    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);

        // CREATED_STORED
        rc = memcached_bop_insert(mc, test_literal_param(key), bkey_1, (unsigned char*)eflag, 1, test_literal_param(value), &attributes);
        if (rc != MEMCACHED_SUCCESS) return rc;
        /* no eflag filters. */
        rc = memcached_bop_insert(mc, test_literal_param(key), bkey_2, NULL, 0, test_literal_param(value), &attributes);
    } else {
        /* check bkey_1 */
        /*  bop get with eflag fileter. */
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        memcached_coll_eflag_filter_st efilter;
        memcached_coll_eflag_filter_init(&efilter, 0,
                                   (unsigned char *)eflag, 1,
                                   MEMCACHED_COLL_COMP_EQ);
        rc = memcached_bop_get(mc, test_literal_param(key), bkey_1, &efilter, false, false, result);
        if (rc != MEMCACHED_SUCCESS) return rc;

        const char *get_value = memcached_coll_result_get_value(result, 0);
        rc = value_compare(value, get_value);
        if (rc != MEMCACHED_SUCCESS) return rc;

        /* check bkey_2 */
        result = memcached_coll_result_create(mc, NULL);
        rc = memcached_bop_get(mc, test_literal_param(key), bkey_2, NULL, false, false, result);
        if (rc != MEMCACHED_SUCCESS) return rc;

        get_value = memcached_coll_result_get_value(result, 0);
        rc = value_compare(value, get_value);
        memcached_coll_result_free(result);
    }
        return rc;
}

static memcached_return_t btree_ext_insert(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";

    char *bkey = "0xA7";
    char *eflag = "0x3F";
    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);

        // CREATED_STORED
        rc = memcached_bop_ext_insert(mc, test_literal_param(key), (unsigned char*)bkey, 1, (unsigned char*)eflag, 1, test_literal_param(value), &attributes);
    } else {
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        memcached_coll_eflag_filter_st efilter;
        memcached_coll_eflag_filter_init(&efilter, 0,
                                   (const unsigned char *)eflag, 1,
                                   MEMCACHED_COLL_COMP_EQ);
        rc = memcached_bop_ext_get(mc, test_literal_param(key), (unsigned char*)bkey, 1, &efilter, false, false, result);
        if (rc != MEMCACHED_SUCCESS) return rc;

        const char *get_value = memcached_coll_result_get_value(result, 0);
        rc = value_compare(value, get_value);
        memcached_coll_result_free(result);
    }
    return rc;
}

static memcached_return_t btree_delete(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";

    uint64_t bkey = 1;
    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);

        // CREATED_STORED
        rc= memcached_bop_insert(mc, test_literal_param(key), bkey, NULL, 0, test_literal_param(value), &attributes);
        if (rc != MEMCACHED_SUCCESS) {
            printf("bop create & insert failed.\n");
            return rc;
        }

        // DELETE
        rc = memcached_bop_delete(mc, test_literal_param(key), bkey, NULL, false /* drop if empty */); 
    } else {
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        rc = memcached_bop_get(mc, test_literal_param(key), bkey, NULL, false, false, result); 
        if (rc == MEMCACHED_NOTFOUND_ELEMENT) rc = MEMCACHED_SUCCESS;
    }
    return rc;
}

static memcached_return_t btree_update(const char *command)
{
    sprintf(key, key_format, command);
    char *value = "value";
    char *ext = "xxx";

    uint64_t bkey = 1;
    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);

        // CREATED_STORED
        rc= memcached_bop_insert(mc, test_literal_param(key), bkey, NULL, 0, test_literal_param(value), &attributes);
        if (rc != MEMCACHED_SUCCESS) {
            printf("bop create & insert failed.\n");
            return rc;
        }

        // UPDATE
        rc = memcached_bop_update(mc, test_literal_param(key), bkey, NULL, test_literal_param(ext)); 
    } else {
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        rc = memcached_bop_get(mc, test_literal_param(key), bkey, NULL, false, false, result); 
        if (rc != MEMCACHED_SUCCESS) return rc;
        const char *get_value = memcached_coll_result_get_value(result, 0); 
        rc = value_compare(ext, get_value);
        memcached_coll_result_free(result);
    }
        return rc;
}

static memcached_return_t btree_incr(const char *command)
{
    sprintf(key, key_format, command);
    int ivalue = 1000;
    char value[10];
    sprintf(value, "%d", ivalue);
    int iext = 10;

    uint64_t bkey = 1;
    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);

        // CREATED_STORED
        rc= memcached_bop_insert(mc, test_literal_param(key), bkey, NULL, 0, test_literal_param(value), &attributes);
        if (rc != MEMCACHED_SUCCESS) {
            printf("bop create & insert failed.\n");
            return rc;
        }

        uint64_t result = 0;
        // INCREMENT
        rc = memcached_bop_incr(mc, test_literal_param(key), bkey, iext, &result); 
    } else {
        int inc_res = ivalue + iext; 
        char expected_value[50];
        sprintf(expected_value, "%d", inc_res);
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        rc = memcached_bop_get(mc, test_literal_param(key), bkey, NULL, false, false, result); 
        if (rc != MEMCACHED_SUCCESS) return rc;

        const char *get_value = memcached_coll_result_get_value(result, 0); 
        rc = value_compare(expected_value, get_value);
        memcached_coll_result_free(result);
    }
        return rc;
}

static memcached_return_t btree_decr(const char *command)
{
    sprintf(key, key_format, command);
    int ivalue = 1000;
    char value[10];
    sprintf(value, "%d", ivalue);
    int iext = 10;

    uint64_t bkey = 1;
    uint32_t flags = 0;
    int32_t exptime = 0;
    uint32_t maxcount = 3000;

    if (request_or_check) {
        memcached_coll_create_attrs_st attributes;
        memcached_coll_create_attrs_init(&attributes, flags, exptime, maxcount);

        // CREATED_STORED
        rc= memcached_bop_insert(mc, test_literal_param(key), bkey, NULL, 0, test_literal_param(value), &attributes);
        if (rc != MEMCACHED_SUCCESS) {
            printf("bop create & insert failed.\n");
            return rc;
        }

        uint64_t result = 0;
        // DECREMENT
        rc = memcached_bop_decr(mc, test_literal_param(key), bkey, iext, &result); 
    } else {
        int dec_res = ivalue - iext; 
        char expected_value[50];
        sprintf(expected_value, "%d", dec_res);
        memcached_coll_result_st *result = memcached_coll_result_create(mc, NULL);
        rc = memcached_bop_get(mc, test_literal_param(key), bkey, NULL, false, false, result); 
        if (rc != MEMCACHED_SUCCESS) return rc;

        const char *get_value = memcached_coll_result_get_value(result, 0); 
        rc = value_compare(expected_value, get_value);
        memcached_coll_result_free(result);
    }
    return rc;
}

static memcached_return_t flush_all(const char *command)
{
    rc = MEMCACHED_SUCCESS;
    if (request_or_check) {
        rc = memcached_flush(mc, 0);
    }
    return rc;
}

static memcached_return_t flush_prefix(const char *command)
{
    rc = MEMCACHED_SUCCESS;
    if (request_or_check) {
        rc = memcached_flush_by_prefix(mc, "prefix", 6, 0);
    }
    return rc;
}

static test_s tests[] = {
  {"flush_all", flush_all },
  {"flush_prefix", flush_prefix },
  {"set", set },
  {"add", add },
  {"cas", cas },
  {"delete", delete },
  {"append", append },
  {"prepend", prepend },
  {"replace", replace },
  {"increment", increment },
  {"decrement", decrement },
  {"lop_create", lop_create },
  {"lop_insert", lop_insert },
  {"lop_delete", lop_delete },
  {"sop_create", sop_create },
  {"sop_insert", sop_insert },
  {"sop_delete", sop_delete },
  {"mop_create", mop_create },
  {"mop_insert", mop_insert },
  {"mop_delete", mop_delete },
  {"mop_update", mop_update },
  {"btree_create", btree_create },
  {"btree_insert", btree_insert },
  {"btree_ext_insert", btree_ext_insert },
  {"btree_delete", btree_delete },
  {"btree_update", btree_update },
  {"btree_incr", btree_incr },
  {"btree_decr", btree_decr }
};

static int check_arguement(int argc, char **argv) 
{
    if (argc != 2) {
        printf("need 2 arguements. eg.\"filename request(or check)\"\n");
        return -1;
    }
    if (strncmp(argv[1], "request", 7) == 0) {
        request_or_check = true;
    } else if (strncmp(argv[1], "check", 5) == 0) {
        request_or_check = false;
    } else {
        printf("second argument must be \"request\" or \"check\".\n");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (check_arguement(argc, argv) < 0) { 
        return -1;
    }
    /* Create the memcached object */
    if (NULL == (mc = memcached_create(NULL)))
        return -1;

    /* Add the server's address */
    if (MEMCACHED_SUCCESS != memcached_server_add(mc, "127.0.0.1", 11446))
        return -1;

    int i;
    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        printf("test command = %s\n", tests[i].command_name);
        rc = tests[i].command(tests[i].command_name);
        if (rc != MEMCACHED_SUCCESS) {
            printf("test failed. command=%s, mode=%s, key=%s, error=%s\n",
                   tests[i].command_name, (request_or_check) ? "request" : "check", key, memcached_strerror(NULL, rc));
            return -1;
        }
        printf("completed\n");
    }
    printf("done to %s update command.", (request_or_check) ? "request" : "check");

    return 0;
}

