/*
 * Dynamically Resized Array
 *
 * Authors: Alfredo K. Kojima <kojima@windowmaker.org>
 *          Dan Pascu         <dan@windowmaker.org>
 *
 * This code is released to the Public Domain, but
 * proper credit is always appreciated :)
 */


#include <stdlib.h>
#include <string.h>

#include "WUtil.h"


#define INITIAL_SIZE 8
#define RESIZE_INCREMENT 8


typedef struct W_Array {
    void **items;		 /* the array data */
    int itemCount;	         /* # of items in array */
    int allocSize;	         /* allocated size of array */
    WMFreeDataProc *destructor;  /* the destructor to free elements */
} W_Array;


WMArray*
WMCreateArray(int initialSize)
{
    return WMCreateArrayWithDestructor(initialSize, NULL);
}


WMArray*
WMCreateArrayWithDestructor(int initialSize, WMFreeDataProc *destructor)
{
    WMArray *array;

    array = wmalloc(sizeof(WMArray));

    if (initialSize == 0) {
        initialSize = INITIAL_SIZE;
    }

    array->items = wmalloc(sizeof(void*) * initialSize);

    array->itemCount = 0;
    array->allocSize = initialSize;
    array->destructor = destructor;

    return array;
}


void
WMEmptyArray(WMArray *array)
{
    if (array->destructor) {
        while (array->itemCount > 0) {
            array->itemCount--;
            array->destructor(array->items[array->itemCount]);
        }
    }
    /*memset(array->items, 0, array->itemCount * sizeof(void*));*/
    array->itemCount = 0;
}


void
WMFreeArray(WMArray *array)
{
    WMEmptyArray(array);
    free(array->items);
    free(array);
}


int
WMGetArrayItemCount(WMArray *array)
{
    return array->itemCount;
}


int
WMAppendArray(WMArray *array, WMArray *other)
{
    if (other->itemCount == 0)
        return 1;

    if (array->itemCount + other->itemCount > array->allocSize) {
        array->allocSize += other->allocSize;
        array->items = wrealloc(array->items, sizeof(void*)*array->allocSize);
    }

    memcpy(array->items+array->itemCount, other->items,
           sizeof(void*)*other->itemCount);
    array->itemCount += other->itemCount;

    return 1;
}


int
WMAddToArray(WMArray *array, void *item)
{
    if (array->itemCount >= array->allocSize) {
        array->allocSize += RESIZE_INCREMENT;
        array->items = wrealloc(array->items, sizeof(void*)*array->allocSize);
    }
    array->items[array->itemCount] = item;

    array->itemCount++;

    return 1;
}


int
WMInsertInArray(WMArray *array, int index, void *item)
{
    wassertrv(index >= 0 && index <= array->itemCount, 0);

    if (array->itemCount >= array->allocSize) {
        array->allocSize += RESIZE_INCREMENT;
        array->items = wrealloc(array->items, sizeof(void*)*array->allocSize);
    }
    if (index < array->itemCount) {
        memmove(array->items+index+1, array->items+index,
                sizeof(void*)*(array->itemCount-index));
    }
    array->items[index] = item;

    array->itemCount++;

    return 1;
}


void*
WMReplaceInArray(WMArray *array, int index, void *item)
{
    void *old;

    wassertrv(index >= 0 && index <= array->itemCount, NULL);

    if (index == array->itemCount) {
        WMAddToArray(array, item);
        return NULL;
    }

    old = array->items[index];
    array->items[index] = item;

    return old;
}


int
WMDeleteFromArray(WMArray *array, int index)
{
    wassertrv(index >= 0 && index < array->itemCount, 0);

    if (array->destructor) {
        array->destructor(array->items[index]);
    }

    if (index < array->itemCount-1) {
        memmove(array->items+index, array->items+index+1,
                sizeof(void*)*(array->itemCount-index-1));
    }

    array->itemCount--;

    return 1;
}


int
WMRemoveFromArray(WMArray *array, void *item)
{
    int i;

    for (i = 0; i < array->itemCount; i++) {
        if (array->items[i] == item) {
            return WMDeleteFromArray(array, i);
        }
    }

    return 0;
}


void*
WMGetFromArray(WMArray *array, int index)
{
    if (index < 0 || index >= array->itemCount)
        return NULL;

    return array->items[index];
}


void*
WMPopFromArray(WMArray *array)
{
    array->itemCount--;

    return array->items[array->itemCount];
}


int
WMFindInArray(WMArray *array, WMMatchDataProc *match, void *cdata)
{
    int i;

    if (match!=NULL) {
        for (i = 0; i < array->itemCount; i++) {
            if ((*match)(array->items[i], cdata))
                return i;
        }
    } else {
        for (i = 0; i < array->itemCount; i++) {
            if (array->items[i] == cdata)
                return i;
        }
    }

    return WANotFound;
}


int
WMCountInArray(WMArray *array, void *item)
{
    int i, count;

    for (i=0, count=0; i<array->itemCount; i++) {
        if (array->items[i] == item)
            count++;
    }

    return count;
}


void
WMSortArray(WMArray *array, WMCompareDataProc *comparer)
{
    qsort(array->items, array->itemCount, sizeof(void*), comparer);
}


void
WMMapArray(WMArray *array, void (*function)(void*, void*), void *data)
{
    int i;

    for (i=0; i<array->itemCount; i++) {
        (*function)(array->items[i], data);
    }
}

