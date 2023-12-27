#pragma once

struct video_driver {
    void (*putchar)(char);
    void (*puts)(char*);
};

void InitVideoConsole(struct video_driver driver);
