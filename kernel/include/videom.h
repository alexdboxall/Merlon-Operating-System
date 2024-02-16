#pragma once

#include <stddef.h>
#include <stdint.h>

@protocol VideoDriving
-(void) drawCharacter:(char)c bg:(uint32_t)bg fg:(uint32_t)fg;
-(void) clearScreen:(uint32_t)bg withFg:(uint32_t)fg;
@end