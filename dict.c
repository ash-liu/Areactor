/* Hash Tables Implementation.
 *
 * This file implements in memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

//#include "fmacros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <sys/time.h>
#include <ctype.h>

#include "dict.h"


/* Using dictEnableResize() / dictDisableResize() we make possible to
 * enable/disable resizing of the hash table as needed. This is very important
 * for Redis, as we use copy-on-write and don't want to move too much memory
 * around when there is a child performing saving operations.
 *
 * Note that even when dict_can_resize is set to 0, not all resizes are
 * prevented: an hash table is still allowed to grow if the ratio between
 * the number of elements and the buckets > dict_force_resize_ratio. */
static int dict_can_resize = 1;
static unsigned int dict_force_resize_ratio = 5;

/* -------------------------- private prototypes ---------------------------- */

static int _dictExpandIfNeeded(dict *ht);
static unsigned long _dictNextPower(unsigned long size);
static int _dictKeyIndex(dict *ht, const void *key);
static int _dictInit(dict *ht, dictType *type, void *privDataPtr);

/* -------------------------- hash functions -------------------------------- */

/* Thomas Wang's 32 bit Mix Function */
unsigned int dictIntHashFunction(unsigned int key)
{
    key += ~(key << 15);
    key ^=  (key >> 10);
    key +=  (key << 3);
    key ^=  (key >> 6);
    key += ~(key << 11);
    key ^=  (key >> 16);
    return key;
}

/* Identity hash function for integer keys */
unsigned int dictIdentityHashFunction(unsigned int key)
{
    return key;
}

static uint32_t dict_hash_function_seed = 5381;

void dictSetHashFunctionSeed(uint32_t seed) {
    dict_hash_function_seed = seed;
}

uint32_t dictGetHashFunctionSeed(void) {
    return dict_hash_function_seed;
}

/* MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your machine behaves -
 * 1. We can read a 4-byte value from any address without crashing
 * 2. sizeof(int) == 4
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 *
 * �㷨�ľ�����Ϣ���Բο� http://code.google.com/p/smhasher/
 */
unsigned int dictGenHashFunction(const void *key, int len) {
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    uint32_t seed = dict_hash_function_seed;
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    uint32_t h = seed ^ len;

    /* Mix 4 bytes at a time into the hash */
    const unsigned char *data = (const unsigned char *)key;

    while(len >= 4) {
        uint32_t k = *(uint32_t*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch(len) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0]; h *= m;
    };

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return (unsigned int)h;
}

/* And a case insensitive hash function (based on djb hash) */
unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len) {
    unsigned int hash = (unsigned int)dict_hash_function_seed;

    while (len--)
        hash = ((hash << 5) + hash) + (tolower(*buf++)); /* hash * 33 + c */
    return hash;
}

/* ----------------------------- API implementation ------------------------- */

/*
 * ���ù�ϣ��ĸ�������
 *
 * T = O(1)
 */
static void _dictReset(dictht *ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

/*
 * ����һ�����ֵ�
 *
 * T = O(1)
 */
dict *dictCreate(dictType *type,
        void *privDataPtr)
{
    // ����ռ�
    dict *d = malloc(sizeof(*d));

    // ��ʼ���ֵ�
    _dictInit(d,type,privDataPtr);

    return d;
}

/*
 * ��ʼ���ֵ�
 *
 * T = O(1)
 */
int _dictInit(dict *d, dictType *type,
        void *privDataPtr)
{
    // ��ʼ�� ht[0]
    _dictReset(&d->ht[0]);

    // ��ʼ�� ht[1]
    _dictReset(&d->ht[1]);

    // ��ʼ���ֵ�����
    d->type = type;
    d->privdata = privDataPtr;
    d->rehashidx = -1;
    d->iterators = 0;

    return DICT_OK;
}

/* Resize the table to the minimal size that contains all the elements,
 * but with the invariant of a USED/BUCKETS ratio near to <= 1 */
/*
 * ���ֵ���н������ýڵ���/Ͱ���ı��ʽӽ� <= 1 ��
 *
 * T = O(N)
 */
int dictResize(dict *d)
{
    int minimal;

    // ������ dict_can_resize Ϊ��
    // �����ֵ����� rehash ʱ����
    if (!dict_can_resize || dictIsRehashing(d)) return DICT_ERR;

    minimal = d->ht[0].used;

    if (minimal < DICT_HT_INITIAL_SIZE)
        minimal = DICT_HT_INITIAL_SIZE;

    return dictExpand(d, minimal);
}

/* Expand or create the hash table */
/*
 * ����һ���¹�ϣ������������������¶���֮һ��
 *  
 *   1) ����ֵ���� ht[0] Ϊ�գ����¹�ϣ��ֵ����
 *   2) ����ֵ���� ht[0] ��Ϊ�գ���ô���¹�ϣ��ֵ�� ht[1] ������ rehash ��ʶ
 *
 * T = O(N)
 */
int dictExpand(dict *d, unsigned long size)
{
    dictht n; /* the new hash table */
    
    // �����ϣ�����ʵ��С
    // O(N)
    unsigned long realsize = _dictNextPower(size);

    /* the size is invalid if it is smaller than the number of
     * elements already inside the hash table */
    if (dictIsRehashing(d) || d->ht[0].used > size)
        return DICT_ERR;

    /* Allocate the new hash table and initialize all pointers to NULL */
    // ��������ʼ���¹�ϣ��
    // O(N)
    n.size = realsize;
    n.sizemask = realsize-1;
    n.table = calloc(1, realsize*sizeof(dictEntry*));
    n.used = 0;

    /* Is this the first initialization? If so it's not really a rehashing
     * we just set the first hash table so that it can accept keys. */
    // ��� ht[0] Ϊ�գ���ô�����һ�δ����¹�ϣ����Ϊ
    // ���¹�ϣ������Ϊ ht[0] ��Ȼ�󷵻�
    if (d->ht[0].table == NULL) {
        d->ht[0] = n;
        return DICT_OK;
    }

    /* Prepare a second hash table for incremental rehashing */
    // ��� ht[0] ��Ϊ�գ���ô�����һ����չ�ֵ����Ϊ
    // ���¹�ϣ������Ϊ ht[1] ������ rehash ��ʶ
    d->ht[1] = n;
    d->rehashidx = 0;

    return DICT_OK;
}

/*
 * ִ�� N ������ʽ rehash ��
 *
 * ���ִ��֮���ϣ����Ԫ����Ҫ rehash ����ô���� 1 ��
 * �����ϣ����������Ԫ���Ѿ�Ǩ����ϣ���ô���� 0 ��
 *
 * ÿ�� rehash �����ƶ���ϣ��������ĳ�������ϵ���������ڵ㣬
 * ���Դ� ht[0] Ǩ�Ƶ� ht[1] �� key ���ܲ�ֹһ����
 *
 * T = O(N)
 */
int dictRehash(dict *d, int n) {
    if (!dictIsRehashing(d)) return 0;

    while(n--) {
        dictEntry *de, *nextde;

        // ��� ht[0] �Ѿ�Ϊ�գ���ôǨ�����
        // �� ht[1] ����ԭ���� ht[0]
        if (d->ht[0].used == 0) {

            // �ͷ� ht[0] �Ĺ�ϣ������
            free(d->ht[0].table);

            // �� ht[0] ָ�� ht[1]
            d->ht[0] = d->ht[1];

            // ��� ht[1] ��ָ��
            _dictReset(&d->ht[1]);

            // �ر� rehash ��ʶ
            d->rehashidx = -1;

            // ֪ͨ�����ߣ� rehash ���
            return 0;
        }

        /* Note that rehashidx can't overflow as we are sure there are more
         * elements because ht[0].used != 0 */
        assert(d->ht[0].size > (unsigned)d->rehashidx);
        // �ƶ����������׸���Ϊ NULL �����������
        while(d->ht[0].table[d->rehashidx] == NULL) d->rehashidx++;
        // ָ������ͷ
        de = d->ht[0].table[d->rehashidx];
        // �������ڵ�����Ԫ�ش� ht[0] Ǩ�Ƶ� ht[1]
        // ��ΪͰ�ڵ�Ԫ��ͨ��ֻ��һ�������߲�����ĳ���ض�����
        // ���Կ��Խ������������ O(1)
        while(de) {
            unsigned int h;

            nextde = de->next;

            /* Get the index in the new hash table */
            // ����Ԫ���� ht[1] �Ĺ�ϣֵ
            h = dictHashKey(d, de->key) & d->ht[1].sizemask;

            // ��ӽڵ㵽 ht[1] ������ָ��
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;

            // ���¼�����
            d->ht[0].used--;
            d->ht[1].used++;

            de = nextde;
        }

        // ����ָ��Ϊ NULL �������´� rehash ʱ����
        d->ht[0].table[d->rehashidx] = NULL;

        // ǰ������һ����
        d->rehashidx++;
    }

    // ֪ͨ�����ߣ�����Ԫ�صȴ� rehash
    return 1;
}

/*
 * �Ժ���Ϊ��λ�����ص�ǰʱ��
 *
 * T = O(1)
 */
long long timeInMilliseconds(void) {
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

/* Rehash for an amount of time between ms milliseconds and ms+1 milliseconds */
/*
 * �ڸ����������ڣ��� 100 ��Ϊ��λ�����ֵ���� rehash ��
 *
 * T = O(N)��N Ϊ�� rehash �� key-value ������
 */
int dictRehashMilliseconds(dict *d, int ms) {
    long long start = timeInMilliseconds();
    int rehashes = 0;

    while(dictRehash(d,100)) {
        rehashes += 100;
        if (timeInMilliseconds()-start > ms) break;
    }
    return rehashes;
}

/*
 * �����������Ļ�����һ��Ԫ�ش� ht[0] Ǩ���� ht[1]
 *
 * ����������������Һ͸��º��������ã��Ӷ�ʵ�ֽ���ʽ rehash ��
 *
 * T = O(1)
 */
static void _dictRehashStep(dict *d) {
    // ֻ��û�а�ȫ��������ʱ�򣬲��ܽ���Ǩ��
    // ������ܻ�����ظ�Ԫ�أ����߶�ʧԪ��
    if (d->iterators == 0) dictRehash(d,1);
}

/*
 * ��Ӹ��� key-value �Ե��ֵ�
 *
 * T = O(1)
 */
int dictAdd(dict *d, void *key, void *val)
{
    // ��� key ����ϣ�����ذ����� key �Ľڵ�
    dictEntry *entry = dictAddRaw(d,key);

    // ���ʧ�ܣ�
    if (!entry) return DICT_ERR;

    // ���ýڵ��ֵ
    dictSetVal(d, entry, val);

    return DICT_OK;
}

/* Low level add. This function adds the entry but instead of setting
 * a value returns the dictEntry structure to the user, that will make
 * sure to fill the value field as he wishes.
 *
 * This function is also directly exposed to the user API to be called
 * mainly in order to store non-pointers inside the hash value, example:
 *
 * entry = dictAddRaw(dict,mykey);
 * if (entry != NULL) dictSetSignedIntegerVal(entry,1000);
 *
 * Return values:
 *
 * If key already exists NULL is returned.
 * If key was added, the hash entry is returned to be manipulated by the caller.
 */
/*
 * ��� key ���ֵ�ĵײ�ʵ�֣����֮�󷵻��½ڵ㡣
 *
 * ��� key �Ѿ����ڣ����� NULL ��
 *
 * T = O(1)
 */
dictEntry *dictAddRaw(dict *d, void *key)
{
    int index;
    dictEntry *entry;
    dictht *ht;

    // ���Խ���ʽ�� rehash һ��Ԫ��
    if (dictIsRehashing(d)) _dictRehashStep(d);

    // ���ҿ�������Ԫ�ص�����λ��
    // ���Ԫ���Ѵ��ڣ� index Ϊ -1
    if ((index = _dictKeyIndex(d, key)) == -1)
        return NULL;

    /* Allocate the memory and store the new entry */
    // �����ð���Ԫ�ط����Ǹ���ϣ��
    ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0];
    // Ϊ��Ԫ�ط���ڵ�ռ�
    entry = malloc(sizeof(*entry));
    // �½ڵ�ĺ��ָ��ָ��ɵı�ͷ�ڵ�
    entry->next = ht->table[index];
    // �����½ڵ�Ϊ��ͷ
    ht->table[index] = entry;
    // �������нڵ�����
    ht->used++;

    /* Set the hash entry fields. */
    // ������ڵ�� key
    dictSetKey(d, entry, key);

    // �����½ڵ�
    return entry;
}

/* Add an element, discarding the old if the key already exists.
 * Return 1 if the key was added from scratch, 0 if there was already an
 * element with such key and dictReplace() just performed a value update
 * operation. */
/*
 * ���µ�ֵ���� key ԭ�е�ֵ��
 * 
 * ��� key �����ڣ���������ӵ���ϣ���С�
 *
 * ����������´����ģ����� 1 ����������Ǳ����µģ����� 0 ��
 *
 * T = O(1)
 */
int dictReplace(dict *d, void *key, void *val)
{
    dictEntry *entry, auxentry;

    /* Try to add the element. If the key
     * does not exists dictAdd will suceed. */
    // ���������Ԫ�ص���ϣ��
    // ֻҪ key �����ڣ���Ӿͻ�ɹ���
    // O(1)
    if (dictAdd(d, key, val) == DICT_OK)
        return 1;

    // ������ʧ�ܣ���ô˵��Ԫ���Ѿ�����
    // ��ȡ���Ԫ������Ӧ�Ľڵ�
    // O(1)
    entry = dictFind(d, key);

    /* Set the new value and free the old one. Note that it is important
     * to do that in this order, as the value may just be exactly the same
     * as the previous one. In this context, think to reference counting,
     * you want to increment (set), and then decrement (free), and not the
     * reverse. */
    auxentry = *entry;          // ָ���ֵ
    dictSetVal(d, entry, val);  // ������ֵ
    dictFreeVal(d, &auxentry);  // �ͷž�ֵ

    return 0;
}

/* dictReplaceRaw() is simply a version of dictAddRaw() that always
 * returns the hash entry of the specified key, even if the key already
 * exists and can't be added (in that case the entry of the already
 * existing key is returned.)
 *
 * See dictAddRaw() for more information. */
/*
 * ������ dictAddRaw() ��
 * dictReplaceRaw ����������ӽڵ㻹�Ǹ��½ڵ������£�
 * ������ key ����Ӧ�Ľڵ�
 *
 * T = O(1)
 */
dictEntry *dictReplaceRaw(dict *d, void *key) {
    // ����
    dictEntry *entry = dictFind(d,key);

    // û�ҵ�����ӣ��ҵ�ֱ�ӷ���
    return entry ? entry : dictAddRaw(d,key);
}

/*
 * �� key ���Ҳ�ɾ���ڵ�
 *
 * T = O(1)
 */
static int dictGenericDelete(dict *d, const void *key, int nofree)
{
    unsigned int h, idx;
    dictEntry *he, *prevHe;
    int table;

    // �ձ�
    if (d->ht[0].size == 0) return DICT_ERR; /* d->ht[0].table is NULL */

    // ����ʽ rehash
    if (dictIsRehashing(d)) _dictRehashStep(d);

    // �����ϣֵ
    h = dictHashKey(d, key);

    // ��������ϣ���в���
    for (table = 0; table <= 1; table++) {
        // ����ֵ
        idx = h & d->ht[table].sizemask;
        // �����������ж�Ӧ�ı�ͷ
        he = d->ht[table].table[idx];
        prevHe = NULL;
        // ��������
        // ��Ϊ�����Ԫ������ͨ��Ϊ 1 ������ά����һ����С�ı���
        // ��˿��Խ������������ O(1)
        while(he) {
            // �Ա�
            if (dictCompareKeys(d, key, he->key)) {
                /* Unlink the element from the list */
                if (prevHe)
                    prevHe->next = he->next;
                else
                    d->ht[table].table[idx] = he->next;
                // �ͷŽڵ�ļ���ֵ
                if (!nofree) {
                    dictFreeKey(d, he);
                    dictFreeVal(d, he);
                }
                // �ͷŽڵ�
                free(he);
                
                d->ht[table].used--;

                return DICT_OK;
            }
            prevHe = he;
            he = he->next;
        }

        // ����������ڽ��� rehash ��
        // ��ô������� ht[1] 
        if (!dictIsRehashing(d)) break;
    }

    return DICT_ERR; /* not found */
}

/*
 * ɾ����ϣ���е� key �������ͷű������ key �Ľڵ�
 *
 * T = O(1)
 */
int dictDelete(dict *ht, const void *key) {
    return dictGenericDelete(ht,key,0);
}

/*
 * ɾ����ϣ���е� key �����ǲ����ͷű������ key �Ľڵ�
 *
 * T = O(1)
 */
int dictDeleteNoFree(dict *ht, const void *key) {
    return dictGenericDelete(ht,key,1);
}

/* Destroy an entire dictionary */
/*
 * ���ٸ�����ϣ��
 *
 * T = O(N)
 */
int _dictClear(dict *d, dictht *ht)
{
    unsigned long i;

    /* Free all the elements */
    // ������ϣ������
    for (i = 0; i < ht->size && ht->used > 0; i++) {
        dictEntry *he, *nextHe;

        if ((he = ht->table[i]) == NULL) continue;
        // �ͷ����������ϵ�Ԫ��
        // ��Ϊ�����Ԫ������ͨ��Ϊ 1 ������ά����һ����С�ı���
        // ��˿��Խ������������ O(1)
        while(he) {
            nextHe = he->next;

            dictFreeKey(d, he);
            dictFreeVal(d, he);

            free(he);

            ht->used--;

            he = nextHe;
        }
    }

    /* Free the table and the allocated cache structure */
    free(ht->table);

    /* Re-initialize the table */
    _dictReset(ht);

    return DICT_OK; /* never fails */
}

/*
 * ��ղ��ͷ��ֵ�
 *
 * T = O(N)
 */
void dictRelease(dict *d)
{
    _dictClear(d,&d->ht[0]);
    _dictClear(d,&d->ht[1]);

    free(d);
}

/*
 * ���ֵ��в��Ҹ��� key ������Ľڵ�
 *
 * ��� key �����ڣ����� NULL
 *
 * T = O(1)
 */
dictEntry *dictFind(dict *d, const void *key)
{
    dictEntry *he;
    unsigned int h, idx, table;

    if (d->ht[0].size == 0) return NULL; /* We don't have a table at all */

    if (dictIsRehashing(d)) _dictRehashStep(d);
    
    // �����ϣֵ
    h = dictHashKey(d, key);
    // ��������ϣ���в���
    for (table = 0; table <= 1; table++) {
        // ����ֵ
        idx = h & d->ht[table].sizemask;
        // �ڵ�����
        he = d->ht[table].table[idx];
        // �������в���
        // ��Ϊ�����Ԫ������ͨ��Ϊ 1 ������ά����һ����С�ı���
        // ��˿��Խ������������ O(1)
        while(he) {
            // �ҵ�������
            if (dictCompareKeys(d, key, he->key))
                return he;

            he = he->next;
        }

        // ��� rehash �����ڽ�����
        // ��ô������� ht[1]
        if (!dictIsRehashing(d)) return NULL;
    }

    return NULL;
}

/*
 * �������ֵ��У� key ����Ӧ��ֵ value
 *
 * ��� key ���������ֵ䣬��ô���� NULL
 *
 * T = O(1)
 */
void *dictFetchValue(dict *d, const void *key) {
    dictEntry *he;

    he = dictFind(d,key);

    return he ? dictGetVal(he) : NULL;
}

/*
 * ���ݸ����ֵ䣬����һ������ȫ��������
 *
 * T = O(1)
 */
dictIterator *dictGetIterator(dict *d)
{
    dictIterator *iter = malloc(sizeof(*iter));

    iter->d = d;
    iter->table = 0;
    iter->index = -1;
    iter->safe = 0;
    iter->entry = NULL;
    iter->nextEntry = NULL;

    return iter;
}

/*
 * ���ݸ����ֵ䣬����һ����ȫ��������
 *
 * T = O(1)
 */
dictIterator *dictGetSafeIterator(dict *d) {
    dictIterator *i = dictGetIterator(d);

    i->safe = 1;
    return i;
}

/*
 * ���ص�����ָ��ĵ�ǰ�ڵ㡣
 *
 * ����ֵ��Ѿ�������ϣ����� NULL ��
 *
 * T = O(1)
 */
dictEntry *dictNext(dictIterator *iter)
{
    while (1) {
        if (iter->entry == NULL) {

            dictht *ht = &iter->d->ht[iter->table];

            // �ڿ�ʼ����֮ǰ�������ֵ� iterators ��������ֵ
            // ֻ�а�ȫ�������Ż����Ӽ���
            if (iter->safe &&
                iter->index == -1 &&
                iter->table == 0)
                iter->d->iterators++;

            // ��������
            iter->index++;

            // ��������Ԫ���������� ht->size ��ֵ
            // ˵��������Ѿ����������
            if (iter->index >= (signed) ht->size) {
                // �Ƿ���ŵ��� ht[1] ?
                if (dictIsRehashing(iter->d) && iter->table == 0) {
                    iter->table++;
                    iter->index = 0;
                    ht = &iter->d->ht[1];
                } else {
                // ���û�� ht[1] �������Ѿ��������� ht[1] ��������
                // ����
                    break;
                }
            }

            // ָ����һ�����Ľڵ�����
            iter->entry = ht->table[iter->index];

        } else {
            // ָ���������һ�ڵ�
            iter->entry = iter->nextEntry;
        }

        // ������ָ�� nextEntry��
        // ��Ӧ�Ե�ǰ�ڵ� entry ���ܱ��޸ĵ����
        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            iter->nextEntry = iter->entry->next;
            return iter->entry;
        }
    }
    return NULL;
}

/*
 * �ͷŵ�����
 *
 * T = O(1)
 */
void dictReleaseIterator(dictIterator *iter)
{
    if (iter->safe && !(iter->index == -1 && iter->table == 0))
        iter->d->iterators--;

    free(iter);
}

/*
 * ���ֵ��з���һ������ڵ㡣
 *
 * ������ʵ��������㷨��
 *
 * ����ֵ�Ϊ�գ����� NULL ��
 *
 * T = O(N)
 */
dictEntry *dictGetRandomKey(dict *d)
{
    dictEntry *he, *orighe;
    unsigned int h;
    int listlen, listele;

    // �ձ����� NULL
    if (dictSize(d) == 0) return NULL;

    // ����ʽ rehash
    if (dictIsRehashing(d)) _dictRehashStep(d);

    // ���ݹ�ϣ���ʹ�����������ӹ�ϣ������ѡһ���ǿձ�ͷ
    // O(N)
    if (dictIsRehashing(d)) {
        do {
            h = random() % (d->ht[0].size+d->ht[1].size);
            he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] :
                                      d->ht[0].table[h];
        } while(he == NULL);
    } else {
        do {
            h = random() & d->ht[0].sizemask;
            he = d->ht[0].table[h];
        } while(he == NULL);
    }

    /* Now we found a non empty bucket, but it is a linked
     * list and we need to get a random element from the list.
     * The only sane way to do so is counting the elements and
     * select a random index. */
    // �����ȡ�����е�����һ��Ԫ��
    // ����������
    // ��Ϊ�����Ԫ������ͨ��Ϊ 1 ����һ����С�ı���
    // ��������������Կ����� O(1)
    listlen = 0;
    orighe = he;
    while(he) {
        he = he->next;
        listlen++;
    }
    // �������ֵ
    listele = random() % listlen;

    // ȡ����Ӧ�ڵ�
    he = orighe;
    while(listele--) he = he->next;

    // ����
    return he;
}

/* ------------------------- private functions ------------------------------ */

/* Expand the hash table if needed */
/*
 * ������Ҫ����չ�ֵ�Ĵ�С
 * ��Ҳ���Ƕ� ht[0] ���� rehash��
 *
 * T = O(N)
 */
static int _dictExpandIfNeeded(dict *d)
{
    // �Ѿ��ڽ���ʽ rehash ���У�ֱ�ӷ���
    if (dictIsRehashing(d)) return DICT_OK;

    // �����ϣ��Ϊ�գ���ô������չΪ��ʼ��С
    // O(N)
    if (d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);

    /* If we reached the 1:1 ratio, and we are allowed to resize the hash
     * table (global setting) or we should avoid it but the ratio between
     * elements/buckets is over the "safe" threshold, we resize doubling
     * the number of buckets. */    
    // �����ϣ������ýڵ��� >= ��ϣ��Ĵ�С��
    // ��������������һ��Ϊ�棺
    //   1) dict_can_resize Ϊ��
    //   2) ���ýڵ������Թ�ϣ���С֮�ȴ��� 
    //      dict_force_resize_ratio
    // ��ô���� dictExpand �Թ�ϣ�������չ
    // ��չ���������Ϊ��ʹ�ýڵ���������
    // O(N)
    if (d->ht[0].used >= d->ht[0].size &&
        (dict_can_resize ||
         d->ht[0].used/d->ht[0].size > dict_force_resize_ratio))
    {
        return dictExpand(d, d->ht[0].used*2);
    }

    return DICT_OK;
}

/*
 * �����ϣ�����ʵ���
 *
 * ��� size С�ڵ��� DICT_HT_INITIAL_SIZE ��
 * ��ô���� DICT_HT_INITIAL_SIZE ��
 * �������ֵΪ��һ�� >= size �Ķ����ݡ�
 *
 * T = O(N)
 */
static unsigned long _dictNextPower(unsigned long size)
{
    unsigned long i = DICT_HT_INITIAL_SIZE;

    if (size >= LONG_MAX) return LONG_MAX;
    while(1) {
        if (i >= size)
            return i;
        i *= 2;
    }
}

/* Returns the index of a free slot that can be populated with
 * an hash entry for the given 'key'.
 * If the key already exists, -1 is returned.
 *
 * Note that if we are in the process of rehashing the hash table, the
 * index is always returned in the context of the second (new) hash table. */
/*
 * ���ظ��� key ���Թ�ϣ�������ŵ�������
 *
 * ��� key �Ѿ������ڹ�ϣ������ -1 ��
 *
 * ������ִ�� rehash ��ʱ��
 * ���ص� index ����Ӧ���ڵڶ������µģ���ϣ��
 *
 * T = O(1)
 */
static int _dictKeyIndex(dict *d, const void *key)
{
    unsigned int h, idx, table;
    dictEntry *he;

    // �������Ҫ�����ֵ������չ
    if (_dictExpandIfNeeded(d) == DICT_ERR)
        return -1;

    // ���� key �Ĺ�ϣֵ
    h = dictHashKey(d, key);

    // ��������ϣ���н��в��Ҹ��� key
    for (table = 0; table <= 1; table++) {

        // ���ݹ�ϣֵ�͹�ϣ��� sizemask 
        // ����� key ���ܳ����� table �����е��ĸ�����
        idx = h & d->ht[table].sizemask;

        // �ڽڵ���������Ҹ��� key
        // ��Ϊ�����Ԫ������ͨ��Ϊ 1 ������һ����С�ı���
        // ���Կ��Խ������������ O(1) ������
        he = d->ht[table].table[idx];
        while(he) {
            // key �Ѿ�����
            if (dictCompareKeys(d, key, he->key))
                return -1;

            he = he->next;
        }

        // ��һ�ν������е�����ʱ��˵���Ѿ������� d->ht[0] ��
        // ��ʱ�����ϣ���� rehash ���У���û�б�Ҫ���� d->ht[1]
        if (!dictIsRehashing(d)) break;
    }

    return idx;
}

/*
 * ��������ֵ�
 *
 * T = O(N)
 */
void dictEmpty(dict *d) {
    _dictClear(d,&d->ht[0]);
    _dictClear(d,&d->ht[1]);
    d->rehashidx = -1;
    d->iterators = 0;
}

/*
 * �� rehash ��ʶ
 *
 * T = O(1)
 */
void dictEnableResize(void) {
    dict_can_resize = 1;
}

/*
 * �ر� rehash ��ʶ
 *
 * T = O(1)
 */
void dictDisableResize(void) {
    dict_can_resize = 0;
}

#if 0

/* The following is code that we don't use for Redis currently, but that is part
of the library. */

/* ----------------------- Debugging ------------------------*/

#define DICT_STATS_VECTLEN 50
static void _dictPrintStatsHt(dictht *ht) {
    unsigned long i, slots = 0, chainlen, maxchainlen = 0;
    unsigned long totchainlen = 0;
    unsigned long clvector[DICT_STATS_VECTLEN];

    if (ht->used == 0) {
        printf("No stats available for empty dictionaries\n");
        return;
    }

    for (i = 0; i < DICT_STATS_VECTLEN; i++) clvector[i] = 0;
    for (i = 0; i < ht->size; i++) {
        dictEntry *he;

        if (ht->table[i] == NULL) {
            clvector[0]++;
            continue;
        }
        slots++;
        /* For each hash entry on this slot... */
        chainlen = 0;
        he = ht->table[i];
        while(he) {
            chainlen++;
            he = he->next;
        }
        clvector[(chainlen < DICT_STATS_VECTLEN) ? chainlen : (DICT_STATS_VECTLEN-1)]++;
        if (chainlen > maxchainlen) maxchainlen = chainlen;
        totchainlen += chainlen;
    }
    printf("Hash table stats:\n");
    printf(" table size: %ld\n", ht->size);
    printf(" number of elements: %ld\n", ht->used);
    printf(" different slots: %ld\n", slots);
    printf(" max chain length: %ld\n", maxchainlen);
    printf(" avg chain length (counted): %.02f\n", (float)totchainlen/slots);
    printf(" avg chain length (computed): %.02f\n", (float)ht->used/slots);
    printf(" Chain length distribution:\n");
    for (i = 0; i < DICT_STATS_VECTLEN-1; i++) {
        if (clvector[i] == 0) continue;
        printf("   %s%ld: %ld (%.02f%%)\n",(i == DICT_STATS_VECTLEN-1)?">= ":"", i, clvector[i], ((float)clvector[i]/ht->size)*100);
    }
}

void dictPrintStats(dict *d) {
    _dictPrintStatsHt(&d->ht[0]);
    if (dictIsRehashing(d)) {
        printf("-- Rehashing into ht[1]:\n");
        _dictPrintStatsHt(&d->ht[1]);
    }
}

/* ----------------------- StringCopy Hash Table Type ------------------------*/

static unsigned int _dictStringCopyHTHashFunction(const void *key)
{
    return dictGenHashFunction(key, strlen(key));
}

static void *_dictStringDup(void *privdata, const void *key)
{
    int len = strlen(key);
    char *copy = malloc(len+1);
    DICT_NOTUSED(privdata);

    memcpy(copy, key, len);
    copy[len] = '\0';
    return copy;
}

static int _dictStringCopyHTKeyCompare(void *privdata, const void *key1,
        const void *key2)
{
    DICT_NOTUSED(privdata);

    return strcmp(key1, key2) == 0;
}

static void _dictStringDestructor(void *privdata, void *key)
{
    DICT_NOTUSED(privdata);

    free(key);
}

dictType dictTypeHeapStringCopyKey = {
    _dictStringCopyHTHashFunction, /* hash function */
    _dictStringDup,                /* key dup */
    NULL,                          /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    NULL                           /* val destructor */
};

/* This is like StringCopy but does not auto-duplicate the key.
 * It's used for intepreter's shared strings. */
dictType dictTypeHeapStrings = {
    _dictStringCopyHTHashFunction, /* hash function */
    NULL,                          /* key dup */
    NULL,                          /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    NULL                           /* val destructor */
};

/* This is like StringCopy but also automatically handle dynamic
 * allocated C strings as values. */
dictType dictTypeHeapStringCopyKeyValue = {
    _dictStringCopyHTHashFunction, /* hash function */
    _dictStringDup,                /* key dup */
    _dictStringDup,                /* val dup */
    _dictStringCopyHTKeyCompare,   /* key compare */
    _dictStringDestructor,         /* key destructor */
    _dictStringDestructor,         /* val destructor */
};
#endif
