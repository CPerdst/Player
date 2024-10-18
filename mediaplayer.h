#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <QThread>
#include <QImage>
#include "framequeue.h"
#include "packetqueue.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavdevice/avdevice.h"
#include "libavutil/time.h"
#include "libavutil/pixfmt.h"
#include "libavutil/imgutils.h"
#include "SDL.h"
}

#define MAX_AUDIO_BUFFER_LENGTH (48000 * 4 * 2)
#define DEF_WANTED_SPEC_SAMPLES (1024)

enum VideoState {
    PLAYING = 1,
    STOPING = 2
};

class mediaplayer : public QThread
{
    Q_OBJECT
signals:
    void SIG_send_image(QImage image);

public:
    mediaplayer();
    int register_all();
    int initialized();
    void play();
    bool is_playing();
    void clear_recourse();

    const QString &file_path() const;
    void setFile_path(const QString &newFile_path);

    /**
     * @brief audio_buffer_size
     * @return 音频缓冲区长度
     */
    int audio_buffer_size() const;

    /**
     * @brief audio_buffer_cur
     * @return 音频缓冲区当前指针
     */
    int audio_buffer_cur() const;

    /**
     * 设置音频缓冲区长度
     *
     * @brief setAudio_buffer_size
     * @param newAudio_buffer_size
     */
    void setAudio_buffer_size(int newAudio_buffer_size);

    /**
     * 设置音频缓冲区指针
     *
     * @brief setAudio_buffer_cur
     * @param newAudio_buffer_cur
     */
    void setAudio_buffer_cur(int newAudio_buffer_cur);

    uint8_t* audio_buffer();

    FrameQueue &audio_frame_queue(); /* 已经被废弃，现在使用audio_packet_queue */

    VideoState state() const;

    const AVFrame &wanted_frame() const;

    PakcetQueue &audio_packet_queue();

    PakcetQueue &video_packet_queue();

    AVCodecContext *codec_context_audio() const;

    AVCodecContext *codec_context_video() const;

    long long play_samples_count() const;
    void setPlay_samples_count(long long play_samples_count);

    long long all_video_samples_count() const;
    void setAll_video_samples_count(long long all_video_samples_count);

    AVFormatContext *format_context() const;

    int video_stream_idx() const;

protected:
    void run() override;

private:
    QString m_file_path;
    //    bool m_playing;
    //    bool m_stop;
    VideoState m_state;
    bool m_is_register_all;
    bool m_video_flag;
    bool m_audio_flag;
    int m_video_stream_idx;
    int m_audio_stream_idx;
    AVFormatContext* m_format_context;
    AVCodecContext* m_codec_context_video;
    AVCodecContext* m_codec_context_audio;
    AVCodec* m_codec_video;
    AVCodec* m_codec_audio;
    AVFrame m_wanted_frame;
    FrameQueue m_audio_frame_queue; /* 已经被废弃，现在使用m_audio_packet_queue */
    /**
     * @brief m_audio_packet_queue
     * @todo 存放提取的音频packet，音频回调获取packet解码成frame
     */
    PakcetQueue m_audio_packet_queue;
    /**
     * @brief m_video_packet_queue
     * @todo 存放提取的视频packet，视频线程获取packet解码成frame显示
     */
    PakcetQueue m_video_packet_queue;

    /**
     * @brief m_audio_buffer
     * @todo 用于填充音频回调缓冲区
     */
    uint8_t m_audio_buffer[MAX_AUDIO_BUFFER_LENGTH * 3 / 2];
    int m_audio_buffer_size; // 音频缓冲区内容长度
    int m_audio_buffer_cur; // 音频缓冲区当前指针

    std::thread* m_vt; // 用于解码并显示视频帧的线程

    /**
     * all_video_samples_count 用于记录当前媒体文件中音频样本总数
     * play_samples_count 用于记录当前 mediaplayer 播放样本总数
     * 播放时长 = play_samples_count * (1 / sample_rate)
     * 媒体文件总时长= all_video_samples_count * (1 / sample_rate)
     *
     * @brief m_all_video_samples_count
     * @brief m_play_samples_count
     */
    long long m_all_video_samples_count; // 记录媒体文件中音频样本总数（相当于总时间）
    long long m_play_samples_count; // 记录当前播放样本总数（用于维护音频时钟）
};

#endif // MEDIAPLAYER_H
