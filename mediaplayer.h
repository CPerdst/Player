#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <QThread>
#include <QImage>
#include "framequeue.h"
#include "packetqueue.h"
#include <mutex>
#include <condition_variable>
#include "thread"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/time.h"
#include "SDL.h"
}

#define MAX_AUDIO_BUFFER_LENGTH (48000 * 4 * 2)
#define DEF_WANTED_SPEC_SAMPLES (1024)

/// VideoState 已经废弃
enum VideoState {
    NO_FILE_LOADED = 1,
    FILE_LOADED = 2,
    PLAYING = 3,
    STOPING = 4,
    STOPED = 5,
    EXITING = 6,
    EXITED = 7
};

class mediaplayer : public QThread
{
    Q_OBJECT
signals:
    void SIG_send_image(QImage image);
    void SIG_send_playtime(double time);

public:
    class MediaPlayerState{
    public:
        MediaPlayerState(mediaplayer* pThis): pThis(pThis){};
        virtual ~MediaPlayerState() = default;
        virtual int play(){
            return -1;
        };
        virtual int play(std::string filepath){
            return -1;
        };
        virtual int pause(){
            return -1;
        };
        virtual int select(std::string filepath){
            return -1;
        };
        virtual int end(){
            return -1;
        };

    protected:
        mediaplayer* pThis;
    };
    class NoFileLoadedState: public MediaPlayerState{
    public:
        NoFileLoadedState(mediaplayer* pThis): MediaPlayerState(pThis){};
        int select(std::string filepath) override{
            QString path = QString::fromStdString(filepath);
            pThis->setFile_path(path);
            pThis->setState(new FileLoadedState(pThis));
            return 0;
        };
        int play(std::string filepath) override{
            QString path = QString::fromStdString(filepath);
            pThis->setFile_path(path);
            pThis->initialized();
            pThis->start();
            pThis->setState(new PlayState(pThis));
            return 0;
        };
    };
    class FileLoadedState: public MediaPlayerState{
    public:
        FileLoadedState(mediaplayer* pThis): MediaPlayerState(pThis){};
        int play() override{
            pThis->initialized();
            pThis->start();
            pThis->setState(new PlayState(pThis));
            return 0;
        };
        int select(std::string filepath) override{
            QString path = QString::fromStdString(filepath);
            pThis->setFile_path(path);
            return 0;
        };
    };
    class PlayState: public MediaPlayerState{
    public:
        PlayState(mediaplayer* pThis): MediaPlayerState(pThis){};
        int select(std::string filepath) override{
            pThis->setRun_flag(false);
            std::async(std::launch::async, [&](){
                while(!pThis->run_flag()){
                    av_usleep(25000);
                }
                QString path = QString::fromStdString(filepath);
                pThis->setFile_path(path);
                pThis->setState(new FileLoadedState(pThis));
            });
            return 0;
        };
        int end() override{
            pThis->clear_recourse();
            pThis->setState(new NoFileLoadedState(pThis));
            return 0;
        };
        int pause() override{
            pThis->setPause_flag(true);
            pThis->setState(new PauseState(pThis));
            return 0;
        }
    };
    class PauseState: public MediaPlayerState{
    public:
        PauseState(mediaplayer* pThis): MediaPlayerState(pThis){};
        int play() override{
            pThis->setPause_flag(false);
            pThis->setState(new PlayState(pThis));
            return 0;
        };
        int end() override{
            pThis->clear_recourse();
            pThis->setState(new NoFileLoadedState(pThis));
            return 0;
        };
        int select(std::string filepath) override{
            pThis->setPause_flag(false);
            pThis->setRun_flag(false);
            std::async(std::launch::async, [&](){
                while(!pThis->run_flag()){
                    av_usleep(25000);
                };
                QString path = QString::fromStdString(filepath);
                pThis->setFile_path(path);
                pThis->setState(new FileLoadedState(pThis));
            });
            return 0;
        };
    };
    mediaplayer();
    int register_all();
    int initialized();
    void media_fetch_thread();
    void clear_recourse();
    int play();
    int stop();
    int quit_media_fetch();

    const QString &file_path() const;
    int setFile_path(const QString &newFile_path);

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
    VideoState state();
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
    void setState(const VideoState &state);
    void setState(MediaPlayerState* state);
    void notifyMeidaPlayThread();
    void setVt_running_flag(bool vt_running_flag);
    bool video_flag() const;
    bool audio_flag() const;

    bool vt_running_flag() const;

protected:
    void run() override;
    

public:

    std::unique_ptr<MediaPlayerState> &media_player_state();

    bool run_flag() const;
    void setRun_flag(bool run_flag);

    bool pause_flag() const;
    void setPause_flag(bool pause_flag);

    int audio_stream_idx() const;

    int audio_queue_size() const;
    int video_queue_size() const;

private:
    QString m_file_path;
    //    bool m_playing;
    //    bool m_stop;
    VideoState m_state; // 已经被弃用
    std::unique_ptr<MediaPlayerState> m_media_player_state;
    bool m_run_flag;
    bool m_pause_flag;
    std::mutex m_state_mtx;
    bool m_is_register_all;
    bool m_video_flag; // 用于标记是否有视频流
    bool m_audio_flag; // 用于标记是否有音频流
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
    bool m_vt_running_flag; // 用于标记mvt线程的状态 已经被弃用

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

    /**
     * 用于同步媒体包提取线程
     *
     * @brief m_media_player_mtx
     * @brief m_media_player_cond
     */
    std::mutex m_media_player_mtx;
    std::condition_variable m_media_player_cond_PLAYING_EXITING;

    SDL_AudioSpec wanted_spec, obtained_spec;
};

#endif // MEDIAPLAYER_H
