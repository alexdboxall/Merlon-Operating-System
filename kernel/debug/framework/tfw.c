
/*
 * tfw - Testing Framework
 */

#include <debug/tfw.h>
#include <debug/hostio.h>
#include <string.h>
#include <log.h>
#include <assert.h>
#include <panic.h>
#include <debug/tfw_tests.h>

#ifndef NDEBUG

#define RESULT_NOT_STARTED  0
#define RESULT_IN_PROGRESS  1
#define RESULT_SUCCESS      2
#define RESULT_FAILURE      3
#define RESULT_SKIPPED      4

struct host_state {
    int test_num;
    uint8_t test_results[MAX_TWF_TESTS];
};

static struct host_state test_state;
static struct tfw_test registered_tests[MAX_TWF_TESTS];
static int num_tests_registered = 0;
static bool nightly_mode = false;

#define PACKET_BUFFER_SIZE (MAX_TWF_TESTS + 256)
uint8_t packet_buffer[PACKET_BUFFER_SIZE];

static bool all_tests_done = false;

/*
 * Send a 0x11 byte to host to ask for current data.
 * Host sends back a packet with 0x22, then the data at data + 8 onwards.
 * It may also send back 0x55, in which case it means it the start of the run (no data yet).
 */
static void GetHostState() {
    if (all_tests_done) {
        return;
    }
    
    packet_buffer[0] = 0x11;
    DbgWritePacket(DBGPKT_TFW, packet_buffer, 1);

    while (true) {
        int type;
        int size = PACKET_BUFFER_SIZE - 32;
        DbgReadPacket(&type, packet_buffer, &size);
        if (size < 1024 && type == DBGPKT_TFW) {
            if (packet_buffer[0] == 0x55) {
                test_state.test_num = 0;
                inline_memset(test_state.test_results, 0, sizeof(test_state.test_results));
                break;
            }
            if (packet_buffer[0] == 0x66 || packet_buffer[0] == 0x67) {
                nightly_mode = packet_buffer[0] == 0x67;
                continue;
            }
            assert(packet_buffer[0] == 0x22);
            test_state = *((struct host_state*) (void*) (packet_buffer + 8));
            break;
        }
    }
}

static void ReadAck(void) {
    int type;
    int size = 10;
    uint8_t d[32];
    DbgReadPacket(&type, d, &size);
    assert(d[0] == 0x66 || d[0] == 0x67);
}

/*
 * To set the host state, send a 0x33/0x44, then the data at +8.
 * 0x33 - starting a test
 * 0x34 - finished test successfully
 * 0x35 - finidhed test unsuccessfully
 * 0x44 - finished last test successfully
 * 0x45 - finidhed last test unsuccessfully
 */
static void SetHostState(int code) {
    if (all_tests_done) {
        return;
    }

    assert(sizeof(struct host_state) < 4096 - 8);

    inline_memset(packet_buffer, 0, sizeof(test_state) + 8 + (code == 0x33 ? MAX_NAME_LENGTH : 0));
    packet_buffer[0] = code;
    inline_memcpy(packet_buffer + 8, &test_state, sizeof(test_state));

    if (code == 0x33) {
        strncpy((char*) (packet_buffer + 8 + sizeof(test_state)), registered_tests[test_state.test_num].name, MAX_NAME_LENGTH);
    }
    packet_buffer[1] = registered_tests[test_state.test_num].nightly_only ? 1 : 0;

    DbgWritePacket(DBGPKT_TFW, packet_buffer, sizeof(test_state) + 8 + (code == 0x33 ? MAX_NAME_LENGTH : 0));
    ReadAck();

    if (code >> 4 == 0x4) {
        LogWriteSerial("\n\nFINISHED ALL TESTS:\n");
        for (int i = 0; i < num_tests_registered; ++i) {
            LogWriteSerial("    %s - %s\n", registered_tests[i].name, test_state.test_results[i] == RESULT_SUCCESS ? "passed" : "failed");
        }
        LogWriteSerial("\n");
        all_tests_done = true;
    }
}

static bool in_test = false;

bool IsInTfwTest(void) {
    return in_test;
}

void RegisterTfwTest(const char* name, int start_point, void (*code)(struct tfw_test*, size_t), int expected_panic, size_t context) {
    struct tfw_test test;
    inline_memset(test.name, 0, MAX_NAME_LENGTH);
    strncpy(test.name, name, MAX_NAME_LENGTH - 1);
    test.code = code;
    test.expected_panic_code = expected_panic;
    test.start_point = start_point;
    test.context = context;
    test.nightly_only = false;
    registered_tests[num_tests_registered++] = test;
}

void RegisterNightlyTfwTest(const char* name, int start_point, void (*code)(struct tfw_test*, size_t), int expected_panic, size_t context) {
    RegisterTfwTest(name, start_point, code, expected_panic, context);
    registered_tests[num_tests_registered - 1].nightly_only = true;
}

void FinishedTfwTest(int panic_code) {
    in_test = false;

    bool success = registered_tests[test_state.test_num].expected_panic_code == panic_code;
    LogWriteSerial("FinishedTfwTest: finished test %d, expected %d vs. actual %d\n", test_state.test_num, registered_tests[test_state.test_num].expected_panic_code, panic_code);

    test_state.test_results[test_state.test_num] = success ? RESULT_SUCCESS : RESULT_FAILURE;

    if (registered_tests[test_state.test_num].nightly_only && !nightly_mode) {
        test_state.test_results[test_state.test_num] = RESULT_SKIPPED;
    }

    test_state.test_num++;
    if (test_state.test_num >= num_tests_registered) {
        SetHostState(success ? 0x44 : 0x45);
    } else {
        SetHostState(success ? 0x34 : 0x35);
    }
}

void MarkTfwStartPoint(int id) {
    LogWriteSerial("Reached TFW_SP %d\n", id);

    if (test_state.test_num >= num_tests_registered || all_tests_done) {
        return;
    }

    if (registered_tests[test_state.test_num].start_point == id) {
        test_state.test_results[test_state.test_num] = RESULT_IN_PROGRESS;
        SetHostState(0x33);
        in_test = true;
        if (!registered_tests[test_state.test_num].nightly_only || nightly_mode) {
            LogWriteSerial("MarkTfwStartPoint: running test %d\n", test_state.test_num);
            registered_tests[test_state.test_num].code(registered_tests + test_state.test_num, registered_tests[test_state.test_num].context);
        }
        Panic(PANIC_UNIT_TEST_OK);
    }
}

void RegisterTfwTests(void) {
    RegisterTfwInitTests();
    RegisterTfwIrqlTests();
    RegisterTfwWaitTests();
    RegisterTfwSemaphoreTests();
    RegisterTfwPhysTests();
    RegisterTfwAVLTreeTests();
    RegisterTfwHeapAdtTests(); 
}

void InitTfw(void) {
    RegisterTfwTests();
    ReadAck();
    GetHostState();
    test_state.test_results[test_state.test_num] = RESULT_NOT_STARTED;
    MarkTfwStartPoint(TFW_SP_INITIAL);
}

#endif