import os
import time
import math
import sys
from signal import signal, SIGPIPE, SIG_DFL
signal(SIGPIPE,SIG_DFL)

nightly = '--nightly' in sys.argv
stopOnError = '--stop-on-error' in sys.argv

serialFileW = 'build/dbgpipe_osread'
serialFileR = 'build/dbgpipe_oswrite.txt'

w = open(serialFileW, 'wb', 0)

# bye-bye to file contents
open(serialFileR, 'wb').close()
r = open(serialFileR, 'rb', 0)

def textNormal():
    print('\033[0m', flush=True, end='')

def textRed():
    print('\033[91m', flush=True, end='')

def textGreen():
    print('\033[92m', flush=True, end='')

def textYellow():
    print('\033[93m', flush=True, end='')

def sendAck(w):
    global nightly
    bytes = bytearray()
    bytes.append(0xAA)  # start of packet
    bytes.append(0x00)  # type
    bytes.append(0x00)  # size high
    bytes.append(0x00)  # size mid
    bytes.append(0x01)  # size low
    bytes.append(0xBB)  # sync. byte (start of data)
    if nightly:
        bytes.append(0x67)  # DATA: initialise
    else:
        bytes.append(0x66)  # DATA: initialise
    bytes.append(0xCC)  # sync. byte (end of data)
    w.write(bytes)

sendAck(w)

currentTest = ''

saveState = None
tests_done = False
testStartTime = 0
testIsNightlyOnly = False

printedIntro = False


while True:
    aa = r.read(1)
    if len(aa) > 0:
        if aa[0] == 0xAA:
            time.sleep(0.6)
            if not printedIntro:
                printedIntro = True
                if nightly:
                    print('\n\nRunning test suite in nightly mode:')
                else:
                    print('\n\nRunning test suite:')
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

                if tests_done:
                    break

            elif data[0] == 0x33 or data[0] == 0x44 or data[0] == 0x34 or data[0] == 0x35 or data[0] == 0x45:
                saveState = bytearray()
                j = 8
                if data[0] == 0x33:
                    testIsNightlyOnly = data[1] == 0x1
                    for i in range(0, size - 8 - 96):
                        saveState.append(data[j])
                        j += 1
                    currentTest = ''
                    for i in range(96):
                        if data[j + i] != 0:
                            currentTest += chr(data[j + i])
                    
                else:
                    for i in range(0, size - 8):
                        saveState.append(data[j])
                        j += 1

                if data[0] == 0x33:
                    print('Starting test \'' + currentTest + '\'... ' + (' ' * (96 - len(currentTest))), end='', flush=True)
                    testStartTime = round(time.time() * 1000)

                if data[0] % 16 in [4, 5]:
                    ellapsedTime = round(time.time() * 1000) - testStartTime
                    ellapsedTime -= 600     # debug.py does a time.sleep(0.6), which adds to the reported test time (if you change this time, the tests times reported change with it). subtract it off to get more accurate estimate
                    if testIsNightlyOnly and not nightly:
                        textYellow()
                        print('skipped nightly')
                        textNormal()
                    else:
                        if data[0] % 16 == 4: 
                            textGreen()
                            print('passed', end='')
                        else:
                            textRed()
                            print('failed', end='')
                        textNormal()
                        if ellapsedTime < 1000:
                            timeStr = str(ellapsedTime) + ' ms'
                        else:
                            timeStr = str(str(int(round(ellapsedTime / 1000))) + '.' + str(int(round(ellapsedTime % 1000 / 10))) + ' s')
                        print(' ({})'.format(timeStr))
                        if data[0] % 16 == 5 and stopOnError:
                            print('\nStopping as --stop-on-error was set.')
                            break


                sendAck(w)      # causes reboot
                sendAck(w)      # to let it know we're ready


                if data[0] == 0x44:
                    print('All test cases are completed!')

                    # need to keep going until it reboots, as it needs to know that tests have finished
                    tests_done = True


w.close()
r.close()