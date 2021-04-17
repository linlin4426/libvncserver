#ifndef LIBVNCSERVER_RFBSTATISTICS_H
#define LIBVNCSERVER_RFBSTATISTICS_H

#define LIBVNCSERVER_STATISTICS_ENABLED

#include <time.h>

typedef struct rfbStatistics {
    //encoding
    uint32_t last_frame;
    uint32_t average_frame_qp;
    uint32_t encode_ts_start_ms;
    uint32_t encode_ts_end_ms;
    //networking
    uint32_t tx_ts_start_ms;
    uint32_t tx_ts_end_ms;
} rfbStatistics;

typedef struct rfbStatisticsMsg {
    uint8_t type; //rfbStatisticsUpdate
    uint8_t padding[3];
    rfbStatistics statistics;
} rfbStatisticsMsg;

uint64_t getTimeNowMs();

#define sz_rfbStatisticsMsg sizeof(rfbStatisticsMsg)

#endif //LIBVNCSERVER_RFBSTATISTICS_H
