#include "packetqueue.h"
#include "QDebug"

extern "C"{
#include "libavutil/time.h"
}

PakcetQueue::PakcetQueue():
    m_packet_size(0),
    m_packet_all_size(0),
    m_packet_length(0),
    m_max_packet_count(MAX_QUEUE_COUNT)
{

}

int PakcetQueue::packet_queue_put(AVPacket *packet)
{
    int err;
    AVPacket* tmp_packet = av_packet_alloc();
    err = av_packet_ref(tmp_packet, packet);
    if(err < 0){
        // 直接丢弃此包
        qDebug() << "av_packet_ref failed";
        exit(0);
    }

    std::unique_lock<std::mutex> lock(m_packet_mtx);
    if(m_packet_queue.size() >= m_max_packet_count){
        // 如果当前队列已经满了，丢弃对头元素，然后再将本帧插入
        m_packet_cond.wait(lock, [&](){return m_packet_queue.size() < m_max_packet_count;});
//        AVPacket* tmp = m_packet_queue.front();
//        // 释放资源
//        av_packet_unref(tmp);
//        av_packet_free(&tmp);
//        m_packet_queue.pop();
    }

    m_packet_queue.push(tmp_packet);
    m_packet_size++;
    m_packet_all_size++;
    m_packet_length += tmp_packet->size;
    return 0;
}

AVPacket *PakcetQueue::packet_queue_get(bool is_block)
{
    AVPacket* tmp_packet;
    while(m_packet_queue.empty()){
        if(is_block){
            av_usleep(100);
        }else{
            return nullptr;
        }
    }
    {
        std::unique_lock<std::mutex> lock(m_packet_mtx);
        tmp_packet = m_packet_queue.front();
        m_packet_queue.pop();
        m_packet_size--;
        if(!tmp_packet){
            return nullptr;
        }
        m_packet_length -= tmp_packet->size;
    }
    // 唤醒等待线程
    m_packet_cond.notify_one();
    return tmp_packet;
}

uint64_t PakcetQueue::size() const
{
    return m_packet_size;
}

uint64_t PakcetQueue::length() const
{
    return m_packet_length;
}

void PakcetQueue::clear()
{
    std::unique_lock<std::mutex> lock(m_packet_mtx);
    while(!m_packet_queue.empty()){
        m_packet_queue.pop();
    }
    m_packet_size = 0;
    m_packet_length = 0;
    m_packet_all_size = 0;
}



