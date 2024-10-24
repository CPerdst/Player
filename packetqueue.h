#ifndef PACKETQUEUE_H
#define PACKETQUEUE_H

#include <mutex>
#include <condition_variable>
#include <queue>

extern "C"
{
#include "libavcodec/avcodec.h"
}

#define MAX_QUEUE_COUNT 50

class PakcetQueue
{
public:
    PakcetQueue();
    int packet_queue_put(AVPacket* packet);
    AVPacket *packet_queue_get(bool is_block);

    uint64_t size() const;
    uint64_t length() const;

    void clear();

private:
    std::queue<AVPacket*> m_packet_queue;
    uint64_t m_packet_size;
    uint64_t m_packet_all_size;
    uint64_t m_packet_length;
    uint64_t m_max_packet_count;
    std::mutex m_packet_mtx;
    std::condition_variable m_packet_cond;
};
#endif // PACKETQUEUE_H
