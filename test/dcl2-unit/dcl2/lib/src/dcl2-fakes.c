DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(bool, transmitBytes, const uint8_t*, size_t, void*);
FAKE_VALUE_FUNC(bool, mutexInit, void*);
FAKE_VALUE_FUNC(bool, mutexLock, void*);
FAKE_VALUE_FUNC(bool, mutexUnlock, void*);
FAKE_VALUE_FUNC(bool, condvarInit,  void*);
FAKE_VALUE_FUNC(bool, condvarWait, void*, uint32_t, bool*);
FAKE_VALUE_FUNC(bool, condvarSignal, void*);
FAKE_VALUE_FUNC(int, yahdlc_frame_data, yahdlc_control_t*, const uint8_t*, size_t, uint8_t*,
                size_t*);
FAKE_VALUE_FUNC(int, yahdlc_get_data, yahdlc_state_t*, yahdlc_control_t*, const uint8_t*, size_t,
                uint8_t*, size_t*);

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


DeadcomL2ThreadingMethods t = {
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
int frame_data_fake_impl(yahdlc_control_t *control, const uint8_t *data, size_t data_len,
                         uint8_t *output_frame, size_t *output_frame_len) {

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

    return 0;
}
