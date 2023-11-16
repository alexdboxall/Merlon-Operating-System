import os
import time
from signal import signal, SIGPIPE, SIG_DFL
signal(SIGPIPE,SIG_DFL)

serialFileW = 'build/dbgpipe_osread'
serialFileR = 'build/dbgpipe_oswrite.txt'

w = open(serialFileW, 'wb', 0)

# bye-bye to file contents
open(serialFileR, 'wb').close()
r = open(serialFileR, 'rb', 0)

def sendAck(w):
    bytes = bytearray()
    bytes.append(0xAA)  # start of packet
    bytes.append(0x00)  # type
    bytes.append(0x00)  # size high
    bytes.append(0x00)  # size mid
    bytes.append(0x01)  # size low
    bytes.append(0xBB)  # sync. byte (start of data)
    bytes.append(0x66)  # DATA: initialise
    bytes.append(0xCC)  # sync. byte (end of data)
    w.write(bytes)

sendAck(w)

currentTest = ''

saveState = None

while True:
    aa = r.read(1)
    if len(aa) > 0:
        if aa[0] == 0xAA:
            time.sleep(0.3)
            type_ = r.read(1)[0]
            size1 = r.read(1)[0]
            size2 = r.read(1)[0]
            size3 = r.read(1)[0]
            bb = r.read(1)[0]
            if bb != 0xBB:
                print('bb is', bb)
                break
            size = (size1 << 16) | (size2 << 8) | size3

            data = bytearray()
            for i in range(size):
                data.append(r.read(1)[0])

            cc = r.read(1)[0]
            if cc != 0xCC:
                print('cc is', cc)
                break

            if data[0] == 0x11:
                if saveState == None:
                    bytes = bytearray()
                    bytes.append(0xAA)  # start of packet
                    bytes.append(0x00)  # type
                    bytes.append(0x00)  # size high
                    bytes.append(0x00)  # size mid
                    bytes.append(0x01)  # size low
                    bytes.append(0xBB)  # sync. byte (start of data)
                    bytes.append(0x55)  # DATA: no save state
                    bytes.append(0xCC)  # sync. byte (end of data)
                    w.write(bytes)
                else:
                    bytes = bytearray()
                    bytes.append(0xAA)  # start of packet
                    bytes.append(0x00)  # type
                    # need +1 because there's a 0x22 byte that gets sent,
                    # plus 7 bytes of padding
                    bytes.append(((len(saveState) + 8) >> 16) & 0xFF)  # size mid
                    bytes.append(((len(saveState) + 8) >> 8) & 0xFF)  # size mid
                    bytes.append((len(saveState) + 8) & 0xFF)  # size low
                    bytes.append(0xBB)  # sync. byte (start of data)
                    bytes.append(0x22)  # 
                    bytes.append(0x00)  # 
                    bytes.append(0x00)  # 
                    bytes.append(0x00)  # 
                    bytes.append(0x00)  # 
                    bytes.append(0x00)  # 
                    bytes.append(0x00)  # 
                    bytes.append(0x00)  # 
                    for b in saveState:
                        bytes.append(b)
                    bytes.append(0xCC)  # sync. byte (end of data)
                    w.write(bytes)

            elif data[0] == 0x33 or data[0] == 0x44 or data[0] == 0x34 or data[0] == 0x35 or data[0] == 0x45:
                saveState = bytearray()
                j = 8
                if data[0] == 0x33:
                    for i in range(0, size - 8 - 96):
                        saveState.append(data[j])
                        j += 1
                    currentTest = ''
                    for i in range(96):
                        currentTest += chr(data[j + i])
                    
                else:
                    for i in range(0, size - 8):
                        saveState.append(data[j])
                        j += 1

                if data[0] == 0x33:
                    print('Starting test \'' + currentTest + '\'... ', end='', flush=True)

                if data[0] % 16 == 4:
                    print('passed')
                elif data[0] % 16 == 5:
                    print('failed')

                sendAck(w)      # causes reboot
                sendAck(w)      # to let it know we're ready


                if data[0] == 0x44:
                    print('All test cases are completed!')
                    break

w.close()
r.close()