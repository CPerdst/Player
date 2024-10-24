#include "mediaplayer.h"
#include <QDebug>
#include <QFileInfo>
#include <QFileDialog>

extern "C"{
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
// #include "libavdevice/avdevice.h"
#include "libavutil/pixfmt.h"
#include "libavutil/imgutils.h"
}

SDL_AudioFormat av_format_to_sdl_formart(enum AVSampleFormat format){
    switch (format) {
        case AV_SAMPLE_FMT_U8: return AUDIO_U8;
        case AV_SAMPLE_FMT_S16: return AUDIO_S16SYS;
        case AV_SAMPLE_FMT_S16P: return AUDIO_S16SYS;
        case AV_SAMPLE_FMT_S32: return AUDIO_S32SYS;
        case AV_SAMPLE_FMT_S32P: return AUDIO_S32SYS;
        case AV_SAMPLE_FMT_FLT: return AUDIO_F32SYS;
        case AV_SAMPLE_FMT_FLTP: return AUDIO_F32SYS;
        default: return AUDIO_S16SYS;
    }
}

AVSampleFormat sdl_format_to_av_formart(SDL_AudioFormat format){
    switch (format) {
        case AUDIO_U8: return AV_SAMPLE_FMT_U8;
        case AUDIO_S16SYS: return AV_SAMPLE_FMT_S16;
        case AUDIO_S32SYS: return AV_SAMPLE_FMT_S32;
        case AUDIO_F32SYS: return AV_SAMPLE_FMT_FLT;
        default: return AV_SAMPLE_FMT_S16;
    }
}

void show_codec_context_information(AVCodec* codec, AVCodecContext* ctx, int idx){
    qDebug() << "----------CODEC CONTEXT INFO----------";
    qDebug() << codec->long_name;
    if(ctx->codec_type == AVMEDIA_TYPE_AUDIO){
        qDebug() << "Stream:        " << idx;
        qDebug() << "Sample Format: " << av_get_sample_fmt_name(ctx->sample_fmt);
        qDebug() << "Sample Size:   " << av_get_bytes_per_sample(ctx->sample_fmt);
        qDebug() << "Channels:      " << ctx->channels;
        qDebug() << "Float Output:  " << (av_sample_fmt_is_planar(ctx->sample_fmt) ? "yes" : "no");
        qDebug() << "Sample Rate:   " << ctx->sample_rate;
        qDebug() << "Audio TimeBase: " << av_q2d(ctx->time_base);
    }else if(ctx->codec_type == AVMEDIA_TYPE_VIDEO){
        qDebug() << "Stream:        " << idx;
        qDebug() << "Video Format: " << av_get_pix_fmt_name(ctx->pix_fmt);
        qDebug() << "Video Height: " << ctx->height;
        qDebug() << "Video Width: " << ctx->width;
        qDebug() << "Video Rate: " << av_q2d(ctx->framerate);
        qDebug() << "Video TimeBase: " << av_q2d(ctx->time_base);
    }
}

void show_frame_information(AVFrame*  frame){
    qDebug() << "----------FRAME INFO----------";
    qDebug() << "frame format: " << av_get_sample_fmt_name((enum AVSampleFormat)frame->format);
    qDebug() << "frame rate: " << frame->sample_rate;
    qDebug() << "frame channel layout: " << frame->channel_layout;
    qDebug() << "frame channels: " << frame->channels;
}

void show_spec_information(SDL_AudioSpec* spec){
    qDebug() << "----------SPEC INFO----------";
    qDebug() << "Spec Freq: " << spec->freq;
    qDebug() << "Spec Challels: " << spec->channels;
    qDebug() << "Spec Silence: " << spec->silence;
    qDebug() << "Spec format: " << QString::number(spec->format, 16).toUpper();
}

void audio_callback_with_frame(void *userdata, Uint8 * stream, int len);

void audio_callback_with_packet(void *userdata, Uint8 * stream, int len);

void video_thread(mediaplayer* pThis);

mediaplayer::mediaplayer()
    : m_state(NO_FILE_LOADED),
      m_media_player_state(new NoFileLoadedState(this)),
      m_run_flag(true),
      m_pause_flag(false),
      m_is_register_all(false),
      m_video_flag(false),
      m_audio_flag(false),
      m_video_stream_idx(-1),
      m_audio_stream_idx(-1),
      m_format_context(nullptr),
      m_codec_context_video(nullptr),
      m_codec_context_audio(nullptr),
      m_codec_video(nullptr),
      m_codec_audio(nullptr),
      m_audio_buffer_size(0),
      m_audio_buffer_cur(0),
      m_vt_running_flag(false),
      m_all_video_samples_count(0),
      m_play_samples_count(0)
{
    // 初始化m_audio_buffer
    memset(m_audio_buffer, 0, MAX_AUDIO_BUFFER_LENGTH);
}

int mediaplayer::register_all()
{
//    av_register_all();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        qDebug() << "SDL init failed\n";
        exit(0);
    }
    m_is_register_all = true;
    return 0;
}

int mediaplayer::initialized()
{
    int err;
//    clear_recourse();
    // 首先初始化一些变量
    m_play_samples_count = 0;
    m_all_video_samples_count = 0;
    memset(m_audio_buffer, 0, sizeof(m_audio_buffer));
    m_audio_buffer_cur = 0;
    m_audio_buffer_size = 0;
    m_video_packet_queue.clear();
    m_audio_packet_queue.clear();
    m_video_flag = false;
    m_audio_flag = false;
    m_video_stream_idx = -1;
    m_audio_stream_idx = -1;
    // 首先初始化媒体文件的格式上下文
    {
        m_format_context = avformat_alloc_context();
        if(avformat_open_input(&m_format_context, m_file_path.toStdString().c_str(), nullptr, nullptr) < 0){
            qDebug() << "avformat_open_input failed\n";
            exit(0);
        }
    }

    // 然后寻找格式上下文当中的流信息
    {
        if(avformat_find_stream_info(m_format_context, nullptr) < 0){
            qDebug() << "avformat_find_stream_info failed\n";
            exit(0);
        }
    }

    // 然后判断音视频流索引
    {
        for(unsigned int i = 0; i < m_format_context->nb_streams; i++){
            if(m_format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
                m_video_stream_idx = i;
            }else if(m_format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
                m_audio_stream_idx = i;
            }
        }
    }

    // 如果当前媒体文件中包含视频流，则初始化视频相关变量
    {
        if(m_video_stream_idx != -1){
            // 视频编解码器
            m_codec_video = avcodec_find_decoder(m_format_context->streams[m_video_stream_idx]->codecpar->codec_id);
            if(!m_codec_video){
                qDebug() << "m_codec_video not found\n";
                exit(0);
            }
            // 申请视频编解码器上下文
            m_codec_context_video = avcodec_alloc_context3(m_codec_video);
            if(!m_codec_context_video){
                qDebug() << "avcodec_alloc_context3 failed";
                exit(0);
            }
            // 复制编解码上下文参数
            err = avcodec_parameters_to_context(m_codec_context_video, m_format_context->streams[m_video_stream_idx]->codecpar);
            if(err < 0){
                qDebug() << "avcodec_parameters_to_context failed";
                exit(0);
            }
            // 设置一下context的time_base和rate
            m_codec_context_video->time_base = m_format_context->streams[m_video_stream_idx]->time_base;
            m_codec_context_video->framerate = m_format_context->streams[m_audio_stream_idx]->r_frame_rate;
            // 打开视频编解码器
            if(avcodec_open2(m_codec_context_video, m_codec_video, nullptr) < 0){
                qDebug() << "m_codec_context_audio avcodec_open2 failed\n";
                exit(0);
            }
            m_video_flag = true;
        }
    }

    // 如果当前媒体文件中包含音频流，则初始化音频相关变量
    {
        if(m_audio_stream_idx != -1){
            // 音频编解码器上下文
            m_codec_context_audio = m_format_context->streams[m_audio_stream_idx]->codec;
            // 音频编解码器
            m_codec_audio = avcodec_find_decoder(m_codec_context_audio->codec_id);
            if(!m_codec_audio){
                qDebug() << "m_codec_audio open failed\n";
                exit(0);
            }
            // 打开音频编解码器
            if(avcodec_open2(m_codec_context_audio, m_codec_audio, nullptr) < 0){
                qDebug() << "m_codec_context_audio avcodec_open2 failed\n";
                exit(0);
            }
            m_audio_flag = true;
            // SDL驱动设置
            wanted_spec.freq = m_codec_context_audio->sample_rate;
            wanted_spec.channels = m_codec_context_audio->channels;
            wanted_spec.silence = 0;
            wanted_spec.samples = DEF_WANTED_SPEC_SAMPLES;
            wanted_spec.format = av_format_to_sdl_formart(m_codec_context_audio->sample_fmt);
            wanted_spec.callback = audio_callback_with_packet;
            wanted_spec.userdata = this;
            show_spec_information(&wanted_spec);
        }
    }

    // 打印编解码器信息
    {
        show_codec_context_information(m_codec_video, m_codec_context_video, m_video_stream_idx);
        show_codec_context_information(m_codec_audio, m_codec_context_audio, m_audio_stream_idx);
    }

    return 0;
}

void mediaplayer::media_fetch_thread()
{
    // 首先创建相关变量
    AVPacket* pkt = av_packet_alloc();
    // 首先判断是否有视频流
    if(!m_video_flag){
        // 设置视频背景为黑色
        QImage image = QImage(1920, 1280, QImage::Format::Format_RGB32);
        image.fill(Qt::black);
        emit SIG_send_image(image);
    }else{
        // 否则开启视频处理线程
        m_vt = new std::thread([](void (*func)(mediaplayer* pThis), mediaplayer* pThis){pThis->setVt_running_flag(true);func(pThis);}, video_thread, this);
        m_vt->detach();
    }
    // 然后判断是否包含音频流
    if(m_audio_flag){
        // 尝试设置相关spec
        if(SDL_OpenAudio(&wanted_spec, &obtained_spec) < 0){
            qDebug() << "failed to open the audio device\n";
            exit(0);
        }
        show_spec_information(&obtained_spec);
        // 设置wanted_frame
        m_wanted_frame.channel_layout = av_get_default_channel_layout(obtained_spec.channels);
        m_wanted_frame.format = sdl_format_to_av_formart(obtained_spec.format);
        m_wanted_frame.sample_rate = obtained_spec.freq;
        m_wanted_frame.channels = obtained_spec.channels;
        // 查看frame信息
        show_frame_information(&m_wanted_frame);
        // 开启SDL音频
        SDL_PauseAudio(0);
    }

    /** @note
      这里会出现一些问题，当PakcetQueue缓冲长度太小的时候，可能会出现
      avcodec_read_frame读取的数据只有视频帧，然后就会导致视频队列占满
      但是音频队列缺没有数据，此时，音频时钟就不会进行增加，导致视频队列的
      视频帧无法被视频线程消费，进而进一步引发m_video_packet_queue.packet_queue_put(pkt);
      会卡死再条件变量的等待函数中，最终引发下面while循环卡死。
      **/

    // 循环从format_context中提取packet，然后根据idx放到对应queue
    {
        while(av_read_frame(m_format_context, pkt) >= 0){
            // 只允许media_fetch_thread访问state()
//            if(state() == STOPING){
//                // 睡50毫秒
//                av_usleep(25000);
//            }else if(state() == EXITING){
//                // 退出循环，清理资源
//                break;
//            }
            if(!run_flag()){
                break;
            }
            while(pause_flag()){
               av_usleep(25000);
            }

            // 首先需要查看当前播放器状态
            /*if(state() == STOPING){
                // 暂停态：停止音频播放，并进入睡眠状态，等待被唤醒
                std::unique_lock<std::mutex> lock(m_media_player_mtx);
                SDL_PauseAudio(1);
                setState(STOPED);
                m_media_player_cond_PLAYING_EXITING.wait(lock, [&](){return ((state() == PLAYING) || (state() == EXITING));});
                if(state() == EXITING){
                    break;
                }
                SDL_PauseAudio(0);
            }else if(state() == EXITING){
                // 如果状态为退出，那就跳出循环，清理资源
                break;
            }*/
            if(pkt->stream_index == m_video_stream_idx){
                // 将packet放入m_video_packet_queue中，然后视频线程会自动消费
                m_video_packet_queue.packet_queue_put(pkt);
                av_packet_unref(pkt);
            }else if(pkt->stream_index == m_audio_stream_idx){
                m_audio_packet_queue.packet_queue_put(pkt);
                av_packet_unref(pkt);
            }else{
                // 释放pkt中的数据
                av_packet_unref(pkt);
            }
        }
    }

    /// 退出之前清理资源（vt线程，SDL音频线程，pkt）

    // 首先向m_vt函数中添加一个结束包
//    {
//        const char* exit_str = "exit";
//        pkt->data = (uint8_t *)av_malloc(sizeof(exit_str) + 1);
//        pkt->size = sizeof(exit_str) + 1;
//        memcpy(pkt->data, (const void*)exit_str, 4);
//        m_video_packet_queue.packet_queue_put(pkt);
//        // 等待m_vt线程的退出
//        while(true){
//            if(!vt_running_flag()){
//                break;
//            }
//            // 睡眠50毫秒
//            av_usleep(50000);
//        }
//    }
    // 等待m_vt线程先退出
    {
        setRun_flag(false);
        while(true){
            if(!vt_running_flag()){
                break;
            }
            av_usleep(25000);
        }
    }

    // 停止音频播放，并关闭音频设备
    {
        SDL_PauseAudio(1);
        SDL_CloseAudio();
    }

    // 释放 packet 资源
    {
        if(pkt){
            // 首先释放pkt->data
            if(pkt->data){
                av_free(pkt->data);
            }
            av_packet_unref(pkt);
            av_packet_free(&pkt);
        }
    }

    // 清理剩余资源并设置EXITED标志
    {
//        clear_recourse();
//        setState(EXITED);
    }

    // 执行end事件
    {
        media_player_state()->end();
    }
    qDebug() << "media player thread exit.";
}

void mediaplayer::clear_recourse()
{
    if(m_format_context){
        avformat_close_input(&m_format_context);
        avformat_free_context(m_format_context);
        m_format_context = nullptr;
    }
    if(m_codec_context_video){
        avcodec_close(m_codec_context_video);
        avcodec_free_context(&m_codec_context_video);
        m_codec_context_video = nullptr;
    }
//    if(m_codec_context_audio){
//        avcodec_close(m_codec_context_audio);
//        avcodec_free_context(&m_codec_context_audio);
//        m_codec_context_audio = nullptr;
//    }
    m_play_samples_count = 0;
    m_all_video_samples_count = 0;
    memset(m_audio_buffer, 0, sizeof(m_audio_buffer));
    m_audio_buffer_cur = 0;
    m_audio_buffer_size = 0;
    m_video_packet_queue.clear();
    m_audio_packet_queue.clear();
    m_video_flag = false;
    m_audio_flag = false;
    m_video_stream_idx = -1;
    m_audio_stream_idx = -1;
    m_run_flag = true;
    m_pause_flag = false;
}

int mediaplayer::play()
{
    /* 首先，media_player 一共有七种状态
     * （NO_FILE_LOADED、FILE_LOADED、PLAYING、STOPING、STOPED、EXITING、EXITED）
     * 如果想要播放视频
     * 1、首先要确保已经选取文件
     * 2、需要确保选取的文件路径不为空且是文件路径
     * 3、
     */
    QString tmp_filepath;
//    std::unique_lock<std::mutex> lock(m_state_mtx);
    auto now_state = state();
    if(now_state == NO_FILE_LOADED){
        tmp_filepath = QFileDialog::getOpenFileName(nullptr, nullptr, nullptr);
        QFileInfo fileinfo = QFileInfo(tmp_filepath);
        if(tmp_filepath.isNull() || \
                tmp_filepath.isEmpty() || \
                !fileinfo.isFile()){
            return 0;
        }
        m_file_path = tmp_filepath;
    }
    if(now_state == PLAYING || now_state == EXITING){
        return -1;
    }
    if(now_state == STOPING){
        setState(PLAYING);
        return 0;
    }
    initialized();
    setState(PLAYING);
    start();
    return 0;
}

int mediaplayer::stop()
{
    if(state() != PLAYING){
        return 0;
    }
    setState(STOPING);
    return 0;
}

int mediaplayer::quit_media_fetch()
{
    setState(EXITING);
    notifyMeidaPlayThread();
    while(state() != EXITED){
        av_usleep(20000);
    }
    return 0;
}

const QString &mediaplayer::file_path() const
{
    return m_file_path;
}

int mediaplayer::setFile_path(const QString &newFile_path)
{
    m_file_path = newFile_path;
    // 确保状态操作一次完成
    /*auto now_state = state();
    if(now_state == EXITED || \
            now_state == NO_FILE_LOADED || \
            now_state == FILE_LOADED){
        m_file_path = newFile_path;
        setState(FILE_LOADED);
        return 0;
    }
    if(now_state == PLAYING || now_state == STOPING){
        setState(EXITING);
        while(state() != EXITED){
            av_usleep(20000);
        }
        m_file_path = newFile_path;
        setState(FILE_LOADED);
        return 0;
    }
    if(now_state == EXITING){
        return -1;
    }*/
    return -1;
}

void mediaplayer::run()
{
    media_fetch_thread();
}

std::unique_ptr<mediaplayer::MediaPlayerState> &mediaplayer::media_player_state()
{
    return m_media_player_state;
}

bool mediaplayer::run_flag() const
{
    return m_run_flag;
}

void mediaplayer::setRun_flag(bool run_flag)
{
    m_run_flag = run_flag;
}

bool mediaplayer::pause_flag() const
{
    return m_pause_flag;
}

void mediaplayer::setPause_flag(bool pause_flag)
{
    m_pause_flag = pause_flag;
}

bool mediaplayer::vt_running_flag() const
{
    return m_vt_running_flag;
}

bool mediaplayer::audio_flag() const
{
    return m_audio_flag;
}

bool mediaplayer::video_flag() const
{
    return m_video_flag;
}

void mediaplayer::setVt_running_flag(bool vt_running_flag)
{
    m_vt_running_flag = vt_running_flag;
}

void mediaplayer::setState(const VideoState &state)
{
    m_state = state;
}

void mediaplayer::setState(MediaPlayerState* state){
    m_media_player_state.reset(state);
}

void mediaplayer::notifyMeidaPlayThread()
{
    m_media_player_cond_PLAYING_EXITING.notify_one();
}

int mediaplayer::video_stream_idx() const
{
    return m_video_stream_idx;
}

AVFormatContext *mediaplayer::format_context() const
{
    return m_format_context;
}

long long mediaplayer::all_video_samples_count() const
{
    return m_all_video_samples_count;
}

void mediaplayer::setAll_video_samples_count(long long all_video_samples_count)
{
    m_all_video_samples_count = all_video_samples_count;
}

long long mediaplayer::play_samples_count() const
{
    return m_play_samples_count;
}

void mediaplayer::setPlay_samples_count(long long play_samples_count)
{
    m_play_samples_count = play_samples_count;
    emit SIG_send_playtime(m_play_samples_count * av_q2d(m_codec_context_audio->time_base));
}

AVCodecContext *mediaplayer::codec_context_video() const
{
    return m_codec_context_video;
}

AVCodecContext *mediaplayer::codec_context_audio() const
{
    return m_codec_context_audio;
}

PakcetQueue &mediaplayer::video_packet_queue()
{
    return m_video_packet_queue;
}

PakcetQueue &mediaplayer::audio_packet_queue()
{
    return m_audio_packet_queue;
}

const AVFrame &mediaplayer::wanted_frame() const
{
    return m_wanted_frame;
}

VideoState mediaplayer::state()
{
    return m_state;
}

FrameQueue &mediaplayer::audio_frame_queue()
{
    return m_audio_frame_queue;
}

void mediaplayer::setAudio_buffer_cur(int newAudio_buffer_cur)
{
    m_audio_buffer_cur = newAudio_buffer_cur;
}

uint8_t *mediaplayer::audio_buffer()
{
    return m_audio_buffer;
}

void mediaplayer::setAudio_buffer_size(int newAudio_buffer_size)
{
    m_audio_buffer_size = newAudio_buffer_size;
}

int mediaplayer::audio_buffer_cur() const
{
    return m_audio_buffer_cur;
}

int mediaplayer::audio_buffer_size() const
{
    return m_audio_buffer_size;
}

/**
 * 用于从m_audio_frame_queue中提取frame的内容填充到m_audio_buffer缓冲区
 *
 * @note 此函数会通过对比frame的与m_wanted_frame中的format选择性进行样本
 *             转换
 *
 * @brief decode_audio_frame
 * @param pThis
 * @return 解码成功返回0，否则返回负数错误码
 */

int decode_audio_frame(mediaplayer* pThis, uint8_t* buffer){
    // 本函数会将pThis中的m_audio_frame_queue帧队列中的帧数据
    // 存放到pThis中的m_audio_buffer之中，并且维护pThis中m_audio_buffer_cur与m_audio_buffer_size
    // 首先，需要从m_audio_frame_queue中拿一个帧
    AVFrame* tmp;
    int data_size = -1;
    tmp = pThis->audio_frame_queue().frame_queue_get(0);
    if(!tmp){
        return -1;
    }
    // 拿到帧之后，判断是否进行重采样或者格式转换
    AVFrame wanted_frame = pThis->wanted_frame();
    SwrContext* swr_ctx = NULL;
//    AVSampleFormat target_format = av_get_planar_sample_fmt((AVSampleFormat)tmp->format);

//    if(target_format == AV_SAMPLE_FMT_NONE){
//        qDebug() << "target_format is AV_SAMPLE_FMT_NONE";
//        exit(0);
//    }
//    show_frame_information(tmp);
    if(wanted_frame.format != tmp->format){
        // 说明是planar格式，或者需求格式不符，需要被转码
        swr_ctx = swr_alloc_set_opts(NULL, wanted_frame.channel_layout,
                                     (AVSampleFormat)wanted_frame.format,
                                     wanted_frame.sample_rate,
                                     tmp->channel_layout,
                                     (AVSampleFormat)tmp->format,
                                     tmp->sample_rate,
                                     0, NULL);
        if(!swr_ctx || swr_init(swr_ctx) < 0){
            qDebug() << "SwrContext initialized failed";
            return -1;
        }
    }

    if(swr_ctx){
        int convert_size = swr_convert(swr_ctx,
                    &buffer,
                    MAX_AUDIO_BUFFER_LENGTH,
                    (const uint8_t **)tmp->data,
                    tmp->nb_samples);
        data_size = av_samples_get_buffer_size(
                NULL,
                wanted_frame.channels,
                convert_size,
                (AVSampleFormat)wanted_frame.format,
                0
            );
        swr_free(&swr_ctx);
    }else{
        // 直接复制
        data_size = tmp->nb_samples;
        memcpy(buffer, tmp->extended_data[0], data_size);
    }
    return data_size;
}

void audio_callback_with_frame(void *userdata, Uint8 * stream, int len){
    mediaplayer* pThis = (mediaplayer*)userdata;
    uint8_t* audio_buffer = pThis->audio_buffer();
    int decode_size = 0;
    // 只要len不为零，就拷贝
    while(len){
        // 首先，判断m_audio_buffer当中的数据是否已经使用完
        if(pThis->audio_buffer_cur() >= pThis->audio_buffer_size()){
            decode_size = decode_audio_frame(pThis, audio_buffer);
            if(decode_size < 0){
                // 没有获取成功，可能是因为队列中没有元素，或者是因为其他错误导致
                if(len > 0)
                    memset(stream, 0, len);
                break;
            }
            pThis->setAudio_buffer_cur(0);
            pThis->setAudio_buffer_size(decode_size);
        }
        // 如果没有用完，就把数据拷贝到stream
        int remain_size = pThis->audio_buffer_size() - pThis->audio_buffer_cur();
        remain_size = remain_size > len ? len : remain_size;
        memcpy(stream, audio_buffer + pThis->audio_buffer_cur(), remain_size);
        stream += remain_size;
        len -= remain_size;
        pThis->setAudio_buffer_cur(pThis->audio_buffer_cur() + remain_size);
    }
    return;
}

int decode_audio_packet(mediaplayer* pThis, uint8_t* buffer){
    int err;
    int err_count = 0;
    AVPacket* packet;
    AVFrame* frame = av_frame_alloc();
    const AVFrame& wanted_frame = pThis->wanted_frame();
    SwrContext* swr_ctx = nullptr;
    int convert_data;
    int all_convert_data = 0;
    // 首先拿一个pakcet
    packet = pThis->audio_packet_queue().packet_queue_get(0);
    if(!packet){
        // 表示没有拿到packet，直接返回-1
        av_frame_free(&frame);
        swr_free(&swr_ctx);
        return -1;
    }
    // 否则扔到解码器进行解码
    do{
        err = avcodec_send_packet(pThis->codec_context_audio(), packet);
        if(err < 0){
            if(err == AVERROR(EAGAIN)){
                // 尝试睡眠1毫秒，然后再重新send
                av_usleep(1000);
                continue;
            }else{
                // 发生未知错误，重新获取packet进行解码，并将错误计数器进行加一，如果错误次数超过10次，表示失败
                if(err_count >= 10){
                    // 释放资源
                    av_packet_unref(packet);
                    av_packet_free(&packet);
                    av_frame_free(&frame);
                    swr_free(&swr_ctx);
                    return -1;
                }
                av_packet_unref(packet);
                packet = pThis->audio_packet_queue().packet_queue_get(0);
                err_count++;
                continue;
            }
        }
        break;
    }while(true);
    // 然后开始从解码器中拿取帧
    do{
        err = avcodec_receive_frame(pThis->codec_context_audio(), frame);
        if(err < 0){
            if(err == AVERROR(EAGAIN)){
                // 表示当前已经没有帧可以拿了
                break;
            }else if(err == AVERROR_EOF){
                // 表示当前解码器已经被刷新，直接退出
                break;
            }
            // 表示解码器出现了未知错误，退出进程
            qDebug() << "avcodec_receive_frame failed";
            // 清理资源
            swr_free(&swr_ctx);
            exit(0);
        }
        // 否则表示成功拿到一帧解码后的数据
        // 首先，判断当前frame格式是否为pThis->m_wanted_frame的格式
        // @note 如果frame格式是planer格式，则一定会初始化转码器
        if(frame->format != wanted_frame.format){
            // 初始化swr转码器
            swr_ctx = swr_alloc_set_opts(NULL, \
                                         wanted_frame.channel_layout,
                                         (AVSampleFormat)wanted_frame.format,
                                         wanted_frame.sample_rate,
                                         frame->channel_layout,
                                         (AVSampleFormat)frame->format,
                                         frame->sample_rate,
                                         0, NULL);
            if(!swr_ctx || swr_init(swr_ctx) < 0){
                qDebug() << "swr_alloc_set_opts failed";
                swr_free(&swr_ctx);
                exit(0);
            }
        }
        // 根据转码器初始化结果对frame进行转码
        if(swr_ctx){
            // 如果需要进行转码
            convert_data = swr_convert(swr_ctx, &buffer, MAX_AUDIO_BUFFER_LENGTH,
                                       (const uint8_t **)frame->extended_data, frame->nb_samples);
            if(convert_data < 0){
                // 转码出错，忽略本帧
                av_frame_unref(frame);
                continue;
            }
            // 否则转码成功，获取转码之后的数据长度
            int convert_data_size = av_samples_get_buffer_size(NULL,
                                                               wanted_frame.channels,
                                                               convert_data,
                                                               (AVSampleFormat)wanted_frame.format,
                                                               1);
            if(convert_data_size < 0){
                qDebug() << "av_samples_get_buffer_size failed";
                swr_free(&swr_ctx);
                exit(0);
            }
            all_convert_data += convert_data_size;
        }else{
            // 不需要转码的话，直接复制
            memcpy(buffer + all_convert_data, frame->extended_data[0], frame->nb_samples);
            all_convert_data += frame->nb_samples; // 这里是因为如果不需要转码，则一定不是planer样本格式，所以单声道样本数nb_samples就是frame总样本数
        }
    }while(err >= 0);

    // 使用完毕，释放packet
    av_packet_unref(packet);
    av_frame_unref(frame);
    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&swr_ctx);
    return all_convert_data;
}

void audio_callback_with_packet(void *userdata, Uint8 * stream, int len){
    int decode_size;
    int remain_size;
    mediaplayer* pThis = (mediaplayer*)userdata;
    Uint8* audio_buffer = pThis->audio_buffer();
    int per_sample_size = av_get_bytes_per_sample(pThis->codec_context_audio()->sample_fmt) * pThis->codec_context_audio()->channels;
    while(len){
        if(pThis->pause_flag()){
            memset(stream, 0, len);
            break;
        }
        if(pThis->audio_buffer_cur() >= pThis->audio_buffer_size()){
            // 如果音频缓冲区没有可用数据了，则解码新数据
            decode_size = decode_audio_packet(pThis, audio_buffer);
            if(decode_size < 0){
                // 解码失败，剩余部分直接静音
                memset(stream, 0, len);
                break;
            }
            // 解码成功，设置cur和size
            pThis->setAudio_buffer_cur(0);
            pThis->setAudio_buffer_size(decode_size);
            // 然后维护一下音频时钟
            pThis->setPlay_samples_count(pThis->play_samples_count() + decode_size / per_sample_size);
        }
        remain_size = pThis->audio_buffer_size() - pThis->audio_buffer_cur();
        remain_size = remain_size < len ? remain_size : len;
        memcpy(stream, audio_buffer + pThis->audio_buffer_cur(), remain_size);
        stream += remain_size;
        len -= remain_size;
        pThis->setAudio_buffer_cur(pThis->audio_buffer_cur() + remain_size);
    }
}

void video_thread(mediaplayer* pThis){
    int err;
    double play_time;
    double video_caculate_play_time;
    int decode_count = 0;
    int get_packet_count = 0;
    AVPacket* packet;
    std::vector<AVPacket*> packets_in_decoder(100);
    bool decode_success_flag = false;
    AVFrame* frame = av_frame_alloc();
    AVFrame* frameRGB = av_frame_alloc();
    AVCodecContext* codec_context_video = pThis->codec_context_video();
    double audio_time_base = av_q2d(pThis->codec_context_audio()->time_base);
    double video_time_base = av_q2d(pThis->format_context()->streams[pThis->video_stream_idx()]->time_base);
    // 初始化SWS
    SwsContext* sws_ctx = sws_getContext(codec_context_video->width,
                                         codec_context_video->height,
                                         codec_context_video->pix_fmt,
                                         codec_context_video->width,
                                         codec_context_video->height,
                                         AV_PIX_FMT_RGB32,
                                         SWS_BILINEAR, NULL, NULL, NULL);
    if(!sws_ctx){
        qDebug() << "sws_getContext failed";
        exit(0);
    }
    const Uint8* output_buffer = (Uint8*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB32,
                                                                      codec_context_video->width,
                                                                      codec_context_video->height,
                                                                      1));
    if(!output_buffer){
        qDebug() << "av_malloc failed";
        exit(0);
    }
    err = av_image_fill_arrays(frameRGB->data,
                               frameRGB->linesize,
                               output_buffer,
                               AV_PIX_FMT_RGB32,
                               codec_context_video->width,
                               codec_context_video->height,
                               1);
    if(err < 0){
        qDebug() << "av_image_fill_arrays failed";
        exit(0);
    }

    qDebug() << QString("%1%2%3%4").arg("frame_pts: ", -12).arg("play_time", -12).arg("video_caculate_play_time", -30).arg("play_frame: ", -12);
    while(true){
//        if(pThis->state() == EXITING){
//            break;
//        }
//        while(pThis->state() == STOPING){
//            // 睡25毫秒
//            av_usleep(25000);
//        }
        if(!pThis->run_flag()){
            break;
        }
        while(pThis->pause_flag()){
            av_usleep(25000);
        }
        // 首先从m_video_packet_queue中获取一个pakcet
        packet = pThis->video_packet_queue().packet_queue_get(0);
        while(!packet){
            // 如果因为队列为空，导致没有拿到packet
            packet = pThis->video_packet_queue().packet_queue_get(0);
            av_usleep(1000);
            get_packet_count++;
            // 下面需要先实现音视频同步才可以
//            if(get_packet_count >= 300){
//                // 说明出了问题，退出清理资源
//                break;
//            }
        }
        // 判断是否是完毕包，是则退出线程
//        if(memcmp(packet->data, "exit", 4) == 0){
//            // 收到结束包，直接退出线程
//            break;
//        }
        // 拿到一个packet之后，发送至解码器
        err = avcodec_send_packet(codec_context_video, packet);
        if(err < 0){
            if(err == AVERROR(EAGAIN)){
                // 说明当前解码器缓冲区已满（正常情况下不可能，因为只有一个视频thread会sendpakcet到解码器）
                break;
            }else if(err == AVERROR_EOF){
                // 表示解码器已经被刷新，直接退出线程
                return;
            }
            // 否则表示avcodec_send_packet出现未知错误，直接退出
            qDebug() << "avcodec_send_packet failed";
            exit(0);
        }
        // 否则表示packet已经成功送入解码器，开始从解码器中提取frmae
        // 首先将packet存放到packets_in_decoder，因为解码器可能会由于
        // 数据不足而导致返回EAGAIN错误码，从而继续推送一个新packet，
        // 如果不将前几个packet存起来，可能会导致内存泄漏
        packets_in_decoder.push_back(packet);
        do{
//            if(pThis->state() == EXITING){
//                break;
//            }
            if(!pThis->run_flag()){
                break;
            }
            while(pThis->pause_flag()){
                av_usleep(25000);
            }
            err = avcodec_receive_frame(codec_context_video, frame);
            if(err < 0){
                if(err == AVERROR(EAGAIN)){
                    // 说明当前解码器缓冲区没有解码好的帧
                    // 此时判断一下解码次数
                    if(decode_count == 1){
                         // 表示已经解码过一次，进行下一次取包解码操作
                        decode_count = 0;
                        decode_success_flag = true;
                        break;
                    }
                    // 否则表示刚刚送入解码器的packet解码成功，需要新推送一个packet
//                    av_usleep(1000);
//                    continue;
                    decode_success_flag = false;
                    break;
                }else if(err == AVERROR_EOF){
                    // 表示解码器已经被刷新，直接退出线程
                    return;
                }
                // 否则表示avcodec_receive_frame出现未知错误，直接退出
                qDebug() << "avcodec_receive_frame failed";
                exit(0);
            }
            // 否则解码成功，开始进行显示
            // 首先，开始进行转码
            err = sws_scale(sws_ctx,
                      (uint8_t const *const *)frame->data,
                      frame->linesize,
                      0, codec_context_video->height,
                      frameRGB->data,
                      frameRGB->linesize);
            if(err  < 0){
                qDebug() << "sws_scale failed";
                exit(0);
            }
            // 转码成功后，将内容转成QImage
            QImage image = QImage((uchar *)output_buffer, frame->width, frame->height, QImage::Format::Format_RGB32).copy();
            // 基于音频时钟做一下等待
            while(true){
//                if(pThis->state() == EXITING){
//                    break;
//                }
                if(!pThis->run_flag()){
                    break;
                }
                while(pThis->pause_flag()){
                    av_usleep(25000);
                }
                // 1、获取已播放样本数
                long long play_samples_count = pThis->play_samples_count();
                const char* state = (video_caculate_play_time > play_time) ? "1" : (video_caculate_play_time < play_time - audio_time_base * DEF_WANTED_SPEC_SAMPLES ? "2" : "3");
                // 2、通过“已播放样本数”计算“已播放时间”
                play_time = play_samples_count * audio_time_base;
                video_caculate_play_time = frame->pts * video_time_base;
                qDebug() << QString("%1%2%3%4").arg(frame->pts, -12).arg(play_time, -12).arg(video_caculate_play_time, -30).arg(state, -12);
                if(video_caculate_play_time > play_time){
                    // 如果视频帧播放时间大于当亲媒体播放时间，则等待1ms
                    av_usleep(1000);
                    continue;
                }else{
                    // 如果视频帧播放时间小于当亲媒体播放时间，有两种情况
                    if(video_caculate_play_time < play_time - audio_time_base * DEF_WANTED_SPEC_SAMPLES * 2){
                        // 如果严重落后媒体播放时间，则直接丢弃此帧

                    }else{
                        // 否则直接播放
                        emit pThis->SIG_send_image(image);
                    }
                    break;
                }
            }

            decode_count++;
            // 首先清理frame内部数据
            av_frame_unref(frame);
        }while(true);
        if(decode_success_flag){
            // 只有解码并显示成功的时候，才会去释放packets_in_decoder中的packets
            for(auto i : packets_in_decoder){
                if(i){
                    // 释放packet包内数据
                    av_packet_unref(i);
                    av_packet_free(&i);
                }
            }
            // 然后清空vector容器
            packets_in_decoder.clear();
        }
    }
    // 释放资源
    av_frame_unref(frame);
    av_frame_free(&frame);
    if(output_buffer){
        av_free((void*)output_buffer);
    }
    av_frame_unref(frameRGB);
    av_frame_free(&frameRGB);
    if(sws_ctx){
        sws_freeContext(sws_ctx);
    }
    // 释放packets_in_decoder中残留packet资源
    for (auto pkt : packets_in_decoder) {
        if (pkt) {
            av_packet_unref(pkt);
            av_packet_free(&pkt); // 如果需要释放 AVPacket 结构体本身
        }
    }
    packets_in_decoder.clear();
    // 设置退出标记为true
    pThis->setVt_running_flag(false);
    qDebug() << "vt thread returnd.";
}
