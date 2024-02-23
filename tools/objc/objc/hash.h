/* Hash tables for Objective C method dispatch.
   Copyright (C) 1993, 1995, 1996, 2004 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

/* As a special exception, if you link this library with files
   compiled with GCC to produce an executable, this does not cause
   the resulting executable to be covered by the GNU General Public License.
   This exception does not however invalidate any other reasons why
   the executable file might be covered by the GNU General Public License.  */

#pragma once

#include <stddef.h>
#include <string.h>
#include "objc.h"

typedef struct cache_node
{
  struct cache_node *next;	/* Pointer to next entry on the list.
				   NULL indicates end of list. */
  const void *key;		/* Key used to locate the value.  Used
				   to locate value when more than one
				   key computes the same hash
				   value. */
  void *value;			/* Value stored for the key. */
} *node_ptr;

typedef unsigned int (*hash_func_type) (void *, const void *);
typedef int (*compare_func_type) (const void *, const void *);

typedef struct cache
{
  /* Variables used to implement the hash itself.  */
  node_ptr *node_table; /* Pointer to an array of hash nodes.  */
  /* Variables used to track the size of the hash table so to determine
    when to resize it.  */
  unsigned int size; /* Number of buckets allocated for the hash table
			(number of array entries allocated for
			"node_table").  Must be a power of two.  */
  unsigned int used; /* Current number of entries in the hash table.  */
  unsigned int mask; /* Precomputed mask.  */

  /* Variables used to implement indexing through the hash table.  */

  unsigned int last_bucket; /* Tracks which entry in the array where
			       the last value was returned.  */
  /* Function used to compute a hash code given a key. 
     This function is specified when the hash table is created.  */
  hash_func_type    hash_func;
  /* Function used to compare two hash keys to see if they are equal.  */
  compare_func_type compare_func;
} *cache_ptr;


extern cache_ptr module_hash_table, class_hash_table;

cache_ptr objc_hash_new (unsigned int size,
			 hash_func_type hash_func,
			 compare_func_type compare_func);
                       
void objc_hash_delete (cache_ptr cache);
void objc_hash_add (cache_ptr *cachep, const void *key, void *value);
void objc_hash_remove (cache_ptr cache, const void *key);
node_ptr objc_hash_next (cache_ptr cache, node_ptr node);
void *objc_hash_value_for_key (cache_ptr cache, const void *key);
BOOL objc_hash_is_key_in_hash (cache_ptr cache, const void *key);

static inline unsigned int
objc_hash_ptr (cache_ptr cache, const void *key)
{
  return ((size_t)key / sizeof (void *)) & cache->mask;
}

static inline unsigned int 
objc_hash_string (cache_ptr cache, const void *key)
{
  unsigned int ret = 0;
  unsigned int ctr = 0;
  const char *ckey = (const char *) key;
        
  while (*ckey) {
    ret ^= *ckey++ << ctr;
    ctr = (ctr + 1) % sizeof (void *);
  }

  return ret & cache->mask;
}

static inline int 
objc_compare_ptrs (const void *k1, const void *k2)
{
  return (k1 == k2);
}

static inline int 
objc_compare_strings (const void *k1, const void *k2)
{
  if (k1 == k2)
    return 1;
  else if (k1 == 0 || k2 == 0)
    return 0;
  else
    return ! strcmp ((const char *) k1, (const char *) k2);
}
