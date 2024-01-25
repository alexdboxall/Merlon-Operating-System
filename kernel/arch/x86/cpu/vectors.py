
output = '; Do not edit - this file was generated automatically!\n\n'
output += 'section .data\n'
output += 'global isr_vectors\n'
output += 'isr_vectors:\n'
for i in range(256):
    output += '    dd isrx{}\n'.format(i)

output += '\nsection .text\n'
output += 'extern InterruptCommonHandler\n'

for i in range(256):
    output += 'isrx{}:\n'.format(i)

    if i < 32:
        if not (i == 8 or (i >= 10 and i <= 14) or i == 17):
            output += '    push byte 0\n'
        output += '    push byte {}\n'.format(i)
        output += '    jmp short thunk{}\n\n'.format(i // 32)
    else:
        output += '    push byte 0\n'
        if i < 128:
            output += '    push byte {}\n'.format(i)
        else:
            output += '    push byte {}\n'.format(i)
        output += '    jmp short thunk{}\n\n'.format(i // 32)

    if i % 32 == 15:
        output += 'thunk{}:\n'.format(i // 32)
        output += '    jmp InterruptCommonHandler\n\n'

open('vectors.s', 'w').write(output)
