/* Force linking of classes required by Objective C runtime.
   Copyright (C) 1997 Free Software Foundation, Inc.
   Contributed by Ovidiu Predescu (ovidiu@net-community.com).

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

/* As a special exception, if you link this library with files compiled with
   GCC to produce an executable, this does not cause the resulting executable
   to be covered by the GNU General Public License. This exception does not
   however invalidate any other reasons why the executable file might be
   covered by the GNU General Public License.  */

#include "objc/Object.h"
#include "objc/NXConstStr.h"

#define __USE_FIXED_PROTOTYPES__
#include <panic.h>
#include <heap.h>
#include <log.h>
#include "objc/runtime.h"

static objc_error_handler _objc_error_handler = NULL;

void objc_error(id object, int code, const char *fmt, ...) {
    va_list ap;
    va_start (ap, fmt);
    objc_verror (object, code, fmt, ap);
    va_end (ap);
}

void objc_verror(id object, int code, const char *fmt, va_list ap) {
    BOOL result = NO;

    if (_objc_error_handler) {
        result = (*_objc_error_handler) (object, code, fmt, ap);
    } else {
        LogWriteSerial("objc_verror: %s\n", fmt);
        LogWriteSerialVa(fmt, ap, false);
        PanicEx(PANIC_UNKNOWN, "OBJC ERROR A");
    }

    if (!result) {
        PanicEx (PANIC_UNKNOWN, "OBJC ERROR C");
    }
}

objc_error_handler objc_set_error_handler (objc_error_handler func) {
    objc_error_handler temp = _objc_error_handler;
    _objc_error_handler = func;
    return temp;
}

void* objc_malloc(size_t size) {
    void* res = (void*) (*_objc_malloc) (size);
    if (!res) {
        objc_error(nil, OBJC_ERR_MEMORY, "Virtual memory exhausted\n");
    }
    return res;
}

void* objc_atomic_malloc(size_t size) {
    void* res = (void*) (*_objc_atomic_malloc) (size);
    if (!res) {
        objc_error (nil, OBJC_ERR_MEMORY, "Virtual memory exhausted\n");
    }
    return res;
}

void* objc_valloc(size_t size) {
    void* res = (void*) (*_objc_valloc) (size);
    if (!res) {
        objc_error (nil, OBJC_ERR_MEMORY, "Virtual memory exhausted\n");
    }
    return res;
}

void* objc_realloc(void *mem, size_t size) {
    void* res = (void*) (*_objc_realloc) (mem, size);
    if (!res) {
        objc_error (nil, OBJC_ERR_MEMORY, "Virtual memory exhausted\n");
    }
    return res;
}

void* objc_calloc(size_t nelem, size_t size) {
    void* res = (void*) (*_objc_calloc) (nelem, size);
    if (!res) {
        objc_error (nil, OBJC_ERR_MEMORY, "Virtual memory exhausted\n");
    }
    return res;
}

void objc_free(void* mem) {
    (*_objc_free) (mem);
}

void* calloc(int a, int b) {
    return AllocHeapZero(a * b);
}

objc_DECLARE void *(*_objc_malloc) (size_t) = AllocHeap;
objc_DECLARE void *(*_objc_atomic_malloc) (size_t) = AllocHeap;
objc_DECLARE void *(*_objc_valloc) (size_t) = AllocHeap;
objc_DECLARE void *(*_objc_realloc) (void *, size_t) = ReallocHeap;
objc_DECLARE void *(*_objc_calloc) (size_t, size_t) = calloc;
objc_DECLARE void (*_objc_free) (void *) = FreeHeap;

@implementation NXConstantString
-(const char *) cString
{
    return (c_string);
}

-(unsigned int) length
{
    return (len);
}
@end

void __objc_linking(void) {
    [Object name];
}

void __objc_generate_gc_type_description(Class) {

}

void class_ivar_set_gcinvisible(Class, const char *, BOOL) {

}

id nil_method(id receiver, SEL) {
    return receiver;
}


id __objc_object_alloc (Class);
id __objc_object_dispose (id);
id __objc_object_copy (id);

id (*_objc_object_alloc) (Class)   = __objc_object_alloc;
id (*_objc_object_dispose) (id)    = __objc_object_dispose;
id (*_objc_object_copy) (id)       = __objc_object_copy;

id class_create_instance(Class class) {
    id new = nil;
    if (CLS_ISCLASS (class)) {
        new = (*_objc_object_alloc) (class);
    }
    if (new != nil) {
        memset (new, 0, class->instance_size);
        new->class_pointer = class;
    }
    return new;
}

id object_copy(id object) {
    if ((object != nil) && CLS_ISCLASS(object->class_pointer)) {
        return (*_objc_object_copy)(object);
    } else {
        return nil;
    }
}

id object_dispose(id object) {
    if ((object != nil) && CLS_ISCLASS (object->class_pointer)) {
        if (_objc_object_dispose) {
            (*_objc_object_dispose)(object);
        } else {
            objc_free(object);
        }
    }
    return nil;
}

id __objc_object_alloc(Class class) {
    return (id) objc_malloc(class->instance_size);
}

id __objc_object_dispose(id object) {
    objc_free(object);
    return 0;
}

id __objc_object_copy(id object) {
    id copy = class_create_instance (object->class_pointer);
    memcpy (copy, object, object->class_pointer->instance_size);
    return copy;
}
