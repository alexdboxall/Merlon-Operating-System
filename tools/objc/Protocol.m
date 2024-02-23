/* This file contains the implementation of class Protocol.
   Copyright (C) 1993, 2004 Free Software Foundation, Inc.

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

#include "objc/Protocol.h"
#include "objc/objc-api.h"

struct objc_method_description_list {
    int count;
    struct objc_method_description list[1];
};

@implementation Protocol
{
@private
    char *protocol_name;
    struct objc_protocol_list* protocol_list;
    struct objc_method_description_list* instance_methods; 
    struct objc_method_description_list* class_methods; 
}

- (const char*) name
{
    return protocol_name;
}

- (BOOL) conformsTo: (Protocol*) aProtocolObject
{
    if (aProtocolObject == nil) {
        return NO;
    }

    if (!strcmp(aProtocolObject->protocol_name, self->protocol_name)) {
        return YES;
    }

    struct objc_protocol_list* proto_list;
    for (proto_list = protocol_list; proto_list; proto_list = proto_list->next) {
        for (size_t i = 0; i < proto_list->count; ++i) {
            if ([proto_list->list[i] conformsTo: aProtocolObject]) {
                return YES;
            }
        }
    }

    return NO;
}

- (struct objc_method_description *) descriptionForInstanceMethod:(SEL)aSel
{
    const char* name = sel_get_name(aSel);

    if (instance_methods) {
        for (int i = 0; i < instance_methods->count; i++) {
	        if (!strcmp ((char*)instance_methods->list[i].name, name)) {
                return &(instance_methods->list[i]);
            } 
        }
    }

    struct objc_protocol_list* proto_list;
    for (proto_list = protocol_list; proto_list; proto_list = proto_list->next) {
        for (size_t j = 0; j < proto_list->count; j++) {
            struct objc_method_description* result;
	        if ((result = [proto_list->list[j] descriptionForInstanceMethod: aSel])) {
	            return result;
            }
	    }
    }

    return NULL;
}

- (struct objc_method_description *) descriptionForClassMethod:(SEL)aSel;
{
    const char* name = sel_get_name(aSel);

    if (class_methods) {
        for (int i = 0; i < class_methods->count; i++){
            if (!strcmp ((char*)class_methods->list[i].name, name)) {
                return &(class_methods->list[i]);
            }
        }
    }

    struct objc_protocol_list* proto_list;
    for (proto_list = protocol_list; proto_list; proto_list = proto_list->next) {
        for (size_t j = 0; j < proto_list->count; j++) {
            struct objc_method_description* result;
	        if ((result = [proto_list->list[j] descriptionForClassMethod: aSel])) {
                return result;
            }
	    }
    }

    return NULL;
}

- (unsigned) hash
{
    int hash = 0;
    for (int index = 0; protocol_name[index]; ++index) {
        hash = (hash << 4) ^ (hash >> 28) ^ protocol_name[index];
    }
    return (hash ^ (hash >> 10) ^ (hash >> 20));
}

- (BOOL) isEqual: (id)obj
{
    if (obj == self) {
        return YES;
    }

    if ([obj isKindOf: [Protocol class]]) {
        if (!strcmp(protocol_name, ((Protocol *)obj)->protocol_name)) {
            return YES;
        }
    }

    return NO;
}
@end

