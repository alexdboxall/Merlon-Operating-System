
// see objc/objc-api.h for these things

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define nil ((id) 0)
#define Nil ((Class) 0)
typedef id (*IMP)(id, SEL, ...);

struct objc_ivar {
    const char* name;
    const char* type;
    int offset;
};

struct objc_ivar_list {
    int count;
    struct objc_ivar ivars[1];
};

struct objc_class {
    struct objc_class* class_pointer;
    struct objc_class* super_class;
    const char* name;
    long version;
    size_t info;
    long instance_size;
    struct objc_ivar_list* ivars;
    struct objc_method_list* methods;
    void* dtable;
    struct objc_class* subclass_list;
    struct objc_class* sibling_class;
    struct objc_protocol_list* protocols;
    void* gc_object_type;
};

struct objc_protocol {
    struct objc_class* class;
    char* name;
    struct objc_protocol_list* protocols;
    struct objc_method_description_list* instance_methods;
    struct objc_method_description_list* class_methods; 
}; 

struct objc_method {
    SEL name;
    const char* types;
    IMP imp;
};

struct objc_method_list {
    struct objc_method_list* next;
    int count;
    struct objc_method list[1]; 
};

struct objc_protocol_list {
    struct objc_protocol_list* next;
    size_t count;
    struct objc_protocol* list[1];
};

struct objc_category {
    const char* category_name;
    const char* class_name;
    struct objc_method_list* instance_methods;
    struct objc_method_list* class_methods;
    struct objc_protocol_list* protocols;
};

struct objc_static_instances
{
  char* class_name;
  id instances[0];      // NULL TERMINATED
};

struct objc_symtab {
    size_t reserved_1;
    SEL reserved_2;
    uint16_t num_classes;
    uint16_t num_categories;
    void* ptrs[1];      // num_classes ptrs to calles, then num_categories
                        // pointers to categories, then NULL terminated array
                        // of objc_static_instances.
};

struct objc_module {
    size_t version;
    size_t size;
    const char* name;
    struct objc_symtab* symbols;
};

void __objc_class_name_Protocol() {
    // TODO: args, return value, etc.
}

void __objc_exec_class(struct objc_module* module) {
    // needs to register the classes from executable / dll

    struct objc_symtab* symtab = module->symbols;
    int i = 0;
    for (; i < symtab->num_classes; ++i) {
        struct objc_class* class = symtab->ptrs[i];
        // TODO: add the class to a hash table
    }
}

Class objc_get_class(const char* name) {
    // TODO: retreive the class from a hash table
    return 0;
}

IMP objc_msg_lookup(id obj, SEL sel) {
   return 0;
}

@interface Box {
    double length;    // Length of a box
    double breadth;   // Breadth of a box
    double height;    // Height of a box
}
@end

@implementation Box

-(id)init {
    length = 1.0;
    breadth = 1.0;
    return self;
}

-(double) volume {
    return length*breadth*height;
}

-(double) getLengthWhenMultipliedBy:(double)multiply andAdding:(double)add {
    return length * multiply + add;
}
@end

@protocol VideoDriver
-(void) drawCharacter:(char)c x:(int)x y:(int)y;
-(void) clearScreen;
@end

@interface DefaultVideo : Box<VideoDriver>
-(void) drawCharacter:(char)c x:(int)x y:(int)y;
-(void) clearScreen;
@end 

@implementation DefaultVideoDriver
-(void) drawCharacter:(char)c x:(int)x y:(int)y {

}

-(void) clearScreen {
    for (int y = 0; y < 25; ++y) {
        for (int x = 0; x < 80; ++x) {
            [self drawCharacter: ' ' x:x y:y]
        }
    }
}
@end

@interface VgaVideo : Box<VideoDriver>
-(void) drawCharacter:(char)c x:(int)x y:(int)y;
-(void) clearScreen;
@end 


int main(int argc, const char** argv) {
    Box* box = [[Box alloc] init];
    double x = [box getLengthWhenMultipliedBy: 3 andAdding: 5];
    return [box volume] + x;
}