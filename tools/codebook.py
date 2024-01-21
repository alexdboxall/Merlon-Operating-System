
import os
lines = 0
asserts = 0
comments = 0
testing = 0
platlines = 0
kernel = 0
ext = ['c', 'h', 'cpp', 'hpp', 'asm', 's']

def convertLine(line, line_num, big):
    formatStr = '{:6}' if big else '{:5d}'
    if line == '':  # i.e. just a newline
        return [(formatStr + '\n').format(line_num), line_num + 1]
    
    line = line.rstrip()
    out = ''
    continuation = False
    while len(line) > 0:
        if continuation:
            out += '  --> '
        else:
            out += (formatStr + ' ').format(line_num)
        out += line[0:100] + '\n'
        line = line[100:]
        continuation = True
    line_num += 1

    return [out, line_num]

def convertFile(filename, lines, line_num, big):
    out = '\n'
    for _ in range(107 if big else 106): out += '-'
    out += '\n'
    out += ' ' + filename + '\n\n'
    for line in lines:
        t = convertLine(line, line_num, big)
        out += t[0]
        line_num = t[1]
    return [out, line_num]

def getText(kernel_only, no_drv, big):
    start_dir = os.getcwd() + '/kernel' if kernel_only else os.getcwd()

    output = ''
    good_files = []
    for path, subdirs, files in os.walk(start_dir):
        if 'tools' in path or 'tools' in subdirs: continue
        if not big:
            if 'debug' in path or 'debug' in subdirs: continue
        if 'build' in path or 'build' in subdirs: continue
        if 'toolchain' in path or 'toolchain' in subdirs: continue
        if '/toolchain' in path or '/toolchain' in subdirs: continue
        if '\\toolchain' in path or '\\toolchain' in subdirs: continue
        if 'toolchain/' in path or 'toolchain/' in subdirs: continue
        if 'toolchain\\' in path or 'toolchain\\' in subdirs: continue
        if no_drv:
            if 'drivers' in path or 'drivers' in subdirs: continue

        for name in files:
            if name.split('.')[-1] in ext:
                n = os.path.join(path, name)
                if 'acpi' in n and not big:
                    continue
                good_files.append(n)
    
    line_num = 1
    for file in reversed(good_files):
        t = convertFile(file.replace(os.getcwd() + '/', ''), open(file, 'r').read().split('\n'), line_num, big)
        output += t[0]
        line_num = t[1]

    return output
                
open('bookkrnl.txt', 'w').write(getText(True , False, False))
open('bookos.txt'  , 'w').write(getText(False, False, False))
open('bookosnd.txt', 'w').write(getText(False, True , False))
#open('bookbig.txt', 'w').write(getText(False, False, True))