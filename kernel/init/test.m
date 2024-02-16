#include <objc/objc/Object.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <log.h>

@interface Box: Object {
    int length;    // Length of a box
    int breadth;   // Breadth of a box
    int height;    // Height of a box
}
@end

@implementation Box

-(id)init {
    length = 1.0;
    breadth = 1.0;
    return self;
}

-(int) volume {
    return length*breadth*height;
}

-(int) getLengthWhenMultipliedBy:(int)multiply andAdding:(int)add {
    return length * multiply + add;
}
@end

@protocol VideoDriver
-(void) drawCharacter:(char)c x:(int)x y:(int)y;
-(void) clearScreen;
@end

@interface DefaultVideoDriver : Box<VideoDriver>
-(void) drawCharacter:(char)c x:(int)x y:(int)y;
-(void) clearScreen;
@end 

@implementation DefaultVideoDriver
-(void) drawCharacter:(char)c x:(int)x y:(int)y {
    (void) c;
    (void) x;
    (void) y;
}

-(void) clearScreen {
    for (int y = 0; y < 25; ++y) {
        for (int x = 0; x < 80; ++x) {
            [self drawCharacter: ' ' x:x y:y];
        }
    }
}
@end

@interface VgaVideo : Box<VideoDriver>
-(void) drawCharacter:(char)c x:(int)x y:(int)y;
-(void) clearScreen;
@end 

#include <arch.h>

int ObjcTest(void) {
    ArchCallGlobalConstructors();

    Box* box = [[Box alloc] init];
    LogWriteSerial("Allocated a Box* here: 0x%X\n", box);
    int x = [box getLengthWhenMultipliedBy: 3 andAdding: 5];
    LogWriteSerial("Length * 3 + 5 = %d\n", x);
    LogWriteSerial("Volume: %d\n", [box volume]);
    return [box volume] + x;
}