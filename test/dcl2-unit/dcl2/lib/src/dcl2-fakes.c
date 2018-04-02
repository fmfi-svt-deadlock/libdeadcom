DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(transmitBytes, uint8_t*, uint8_t);
FAKE_VOID_FUNC(mutexInit, void*);
FAKE_VOID_FUNC(mutexLock, void*);
FAKE_VOID_FUNC(mutexUnlock, void*);
FAKE_VOID_FUNC(condvarInit,  void*);
FAKE_VALUE_FUNC(bool, condvarWait, void*, uint32_t);
FAKE_VOID_FUNC(condvarSignal, void*);
FAKE_VALUE_FUNC(int, yahdlc_frame_data, yahdlc_control_t*, const char*, unsigned int, char*,
                unsigned int*);
FAKE_VALUE_FUNC(int, yahdlc_get_data, yahdlc_state_t*, yahdlc_control_t*, const char*, unsigned int,
                char*, unsigned int*);

#define FFF_FAKES_LIST(FAKE)        \
    FAKE(transmitBytes)             \
    FAKE(mutexInit)                 \
    FAKE(mutexLock)                 \
    FAKE(mutexUnlock)               \
    FAKE(condvarInit)               \
    FAKE(condvarWait)               \
    FAKE(condvarSignal)             \
    FAKE(yahdlc_frame_data)         \
    FAKE(yahdlc_get_data)


DeadcomL2ThreadingVMT t = {
    &mutexInit,
    &mutexLock,
    &mutexUnlock,
    &condvarInit,
    &condvarWait,
    &condvarSignal
};

/* A fake framing implementation that produces inspectable frames in the following format:
 *
 * +-----------------------------+-------------+--------------------------------------------+
 * | control_structure           | message_len | message_bytes                              |
 * | (sizeof(yahdlc_control_t))  | 4 bytes     | message_len                                |
 * +-----------------------------+-------------+--------------------------------------------+
 */
int frame_data_fake_impl(yahdlc_control_t *control, const char *data, unsigned int data_len,
                         char *output_frame, unsigned int *output_frame_len) {

    if (output_frame) {
        // Control struct
        memcpy(output_frame, control, sizeof(yahdlc_control_t));
    }
    *output_frame_len = sizeof(yahdlc_control_t);

    if (data_len != 0) {
        if (output_frame) {
            // Message length
            output_frame[(*output_frame_len)++] = (data_len >> 24) & 0xFF;
            output_frame[(*output_frame_len)++] = (data_len >> 16) & 0xFF;
            output_frame[(*output_frame_len)++] = (data_len >>  8) & 0xFF;
            output_frame[(*output_frame_len)++] = (data_len >>  0) & 0xFF;
        } else {
            *output_frame_len += 4;
        }

        if (output_frame) {
            // The message itself
            memcpy(output_frame+(*output_frame_len), data, data_len);
        }
        *output_frame_len += data_len;
    }
}