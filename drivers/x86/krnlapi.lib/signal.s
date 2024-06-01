
global x86UserHandleSignals
extern OsCommonSignalHandler

x86UserHandleSignals:
    call OsCommonSignalHandler
    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popad
    popfd
    ret
