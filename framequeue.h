#ifndef FRAMEQUEUE_H
#define FRAMEQUEUE_H

#include <mutex>
#include <condition_variable>
#include <queue>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "libavutil/time.h"
#include "libavutil/pixfmt.h"
#include "libavutil/imgutils.h"
#include "SDL.h"
}

class FrameQueue
{
public:
    FrameQueue();
    void frame_queue_put(AVFrame* frame);
    AVFrame *frame_queue_get(bool is_block);

    uint64_t size() const;
    uint64_t length() const;

private:
    // frame variables
    std::queue<AVFrame*> m_queue;
    uint64_t m_size;
    uint64_t m_length;
    std::mutex m_mtx;
    std::condition_variable m_cond;
};

#endif // FRAMEQUEUE_H
