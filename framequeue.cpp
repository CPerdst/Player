#include "framequeue.h"
#include "QDebug"

extern "C"
{
#include "libavutil/time.h"
}

FrameQueue::FrameQueue():
    m_size(0),
    m_length(0)
{

}

void FrameQueue::frame_queue_put(AVFrame* frame)
{
    // 这里直接将 frame push 到 m_queue 中
    AVFrame* tmp = av_frame_alloc();
//    memcpy(tmp, frame, sizeof(AVFrame));
    av_frame_ref(tmp, frame);
    std::lock_guard<std::mutex> lock(m_mtx);
    m_queue.push(tmp);
    m_size += 1;
    m_length += frame->linesize[0];
}

/**
 * @brief FrameQueue::frame_queue_get
 * @param is_block
 * @return 成功则返回一个AVFrame对象，否则返回nullptr
 */

AVFrame* FrameQueue::frame_queue_get(bool is_block){
    // 看是否为空
    while(m_queue.empty()){
        if(is_block){
            // 如果m_queue没有元素，并且允许阻塞，则解锁并睡眠1毫秒
            av_usleep(1000);
        }else{
            // 否则直接返回nullptr
            return nullptr;
        }
    }
    std::unique_lock<std::mutex> lock(m_mtx);
    AVFrame* tmp = m_queue.front();
    m_queue.pop();
    m_size -= 1;
    m_length -= tmp->linesize[0];
    return tmp;
}

uint64_t FrameQueue::size() const
{
    return m_size;
}

uint64_t FrameQueue::length() const
{
    return m_length;
}

