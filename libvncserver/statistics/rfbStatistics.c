#include <stdint.h>
#include "rfbStatistics.h"
#include "rfb/rfb.h"

uint64_t getTimeNowMs() {
    struct timespec now = {};
    clock_gettime( CLOCK_MONOTONIC_RAW, &now );
    uint64_t now_ns = (uint64_t)now.tv_sec * UINT64_C(1000) + (uint64_t)now.tv_nsec/1000000;
    return now_ns;
}

rfbBool rfbSendStatistics(rfbClientPtr cl) {
    rfbBool result = TRUE;
    rfbStatisticsMsg msg = {0};
    msg.type = rfbStatisticsUpdate;
    msg.statistics = cl->rfbStatistics;
    msg.statistics.last_frame = Swap32IfLE(msg.statistics.last_frame);
    msg.statistics.average_frame_qp = Swap32IfLE(msg.statistics.average_frame_qp);
    msg.statistics.encode_ts_start_ms = Swap32IfLE(msg.statistics.encode_ts_start_ms);
    msg.statistics.encode_ts_end_ms = Swap32IfLE(msg.statistics.encode_ts_end_ms);
    msg.statistics.tx_ts_start_ms = Swap32IfLE(msg.statistics.tx_ts_start_ms);
    msg.statistics.tx_ts_end_ms = Swap32IfLE(msg.statistics.tx_ts_end_ms);

    memcpy(&cl->updateBuf[cl->ublen], &msg, sz_rfbStatisticsMsg);
    cl->ublen += sz_rfbStatisticsMsg;
    if(!rfbSendUpdateBuf(cl)) {
        rfbErr("rfbStatistics: could not send statistics.\n");
        result = FALSE;
        goto error;
    }

error:
    return result;
}