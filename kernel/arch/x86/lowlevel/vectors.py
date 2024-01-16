output = '; Do not edit - this file was generated automatically!\n\n'
for i in range(256):
    output += 'global isr{}\n'.format(i)
    output += 'isr{}:\n'.format(i)

    # IRQ8, IRQ10-14 and IRQ17 push an error code. For consistency,
    # we'll make the other ones push a dummy error code.
    if not (i == 8 or (i >= 10 and i <= 14) or i == 17):
        output += '    push byte 0\n'
    output += '    push byte {}\n'.format(i)
    output += '    jmp int_common_handler\n\n'

output += 'global isr_vectors\n'
output += 'isr_vectors:\n'
for i in range(256):
    output += '    dd isr{}\n'.format(i)

open('vectors.s', 'w').write(output)
