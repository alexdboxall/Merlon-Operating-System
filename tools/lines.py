
import os
lines = 0
asserts = 0
comments = 0
testing = 0
platlines = 0
kernel = 0
ext = ['c', 'h', 'cpp', 'hpp', 'asm', 's']

for path, subdirs, files in os.walk(os.getcwd()):
    if '\\.git' in path: continue
    if '\\.git' in subdirs: continue
    if '\\.vs' in path: continue
    if '\\.vs' in subdirs: continue
    if '\\Debug' in path: continue
    if '\\Debug' in subdirs: continue
    if '\\build' in path: continue
    if '\\build' in subdirs: continue
    if 'toolchain' in path or 'toolchain' in subdirs: continue
    if '/toolchain' in path or '/toolchain' in subdirs: continue
    if '\\toolchain' in path or '\\toolchain' in subdirs: continue
    if 'toolchain/' in path or 'toolchain/' in subdirs: continue
    if 'toolchain\\' in path or 'toolchain\\' in subdirs: continue

    for name in files:
        if name.split('.')[-1] in ext:
            n = os.path.join(path, name)
            if 'acpi' in n:
                continue
            lns = open(n, 'r').read().split('\n')
            j = len(lns)
            for l in lns:
                if l.startswith('* ') or l.replace('\t', '').lstrip().startswith('* ') or l.replace('\t', '    ').find('   * ') != -1:
                    comments += 1
                if l.find('assert') != -1:
                    asserts += 1
            lines += j
            if n.find('kernel') != -1 and n.find('debug') == -1:
                kernel += j
            if n.find('debug/tests/') != -1 or n.find('debug\\tests\\') != -1:
                testing += j
            if n.find('arch/') != -1 or n.find('arch\\') != -1 or n.find('x86/') != -1 or n.find('x86\\') != -1:
                platlines += j
        
print('{} lines (of those, {}, or {}% are platform specific)'.format(lines, platlines, round(platlines * 100 / lines, 1)))
print('{} lines come from the kernel (non-tests)'.format(kernel))
print('{}% asserts, '.format(round(asserts * 100 / lines, 1)), '{}% comments, '.format(round(comments * 100 / lines, 1)), '{}% tests'.format(round(testing * 100 / lines, 1)))
print('(does not include ACPICA)')