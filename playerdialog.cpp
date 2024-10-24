#include "playerdialog.h"
#include "ui_playerdialog.h"
#include <QFileDialog>
#include <QDebug>
#include "QTime"

extern "C"{
#include "libavutil/time.h"
}

PlayerDialog::PlayerDialog(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::PlayerDialog)
    , m_druggle_slider(false)
{
    ui->setupUi(this);
    // 初始化模块
    m_media_player.register_all();

    // 信号槽连接
    // 1、媒体控制模块发送解码好的视频图像 -> 显示图像到lb_show控件
    connect(&m_media_player, SIGNAL(SIG_send_image(QImage)), this, SLOT(SLOT_receive_image(QImage)));
    // 2、QTimer计时器打印播放器成员变量
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(SLOT_update_variables()));
    // 3、视频时钟更新
    connect(&m_media_player, SIGNAL(SIG_send_playtime(double)), this, SLOT(SLOT_receive_playtime(double)));

    // 初始化控件
    // 1、设置lb_show为全黑背景
    ui->lb_show->setScaledContents(true);
    qDebug() << ui->lb_show->width() << " " << ui->lb_show->height();
    QImage background = QImage(ui->lb_show->width(), ui->lb_show->height(), QImage::Format::Format_RGB32);
    background.fill(Qt::black);
    emit m_media_player.SIG_send_image(background);
    // 2、设置lb_time_show
    QTime time;
    time.setHMS(0, 0, 0);
    ui->lb_time_show->setText(time.toString());
    // 3、设置hslider_process的总位置(视频秒数)
    ui->hslider_process->setRange(1, 1000);
    // 4、开启计时器,10毫秒
    m_timer.start(10);

}

PlayerDialog::~PlayerDialog()
{
    delete ui;
}

void PlayerDialog::on_pb_select_clicked()
{
    QString tmp_filepath;
    tmp_filepath = QFileDialog::getOpenFileName(nullptr, nullptr, nullptr);
    QFileInfo fileinfo = QFileInfo(tmp_filepath);
    if(tmp_filepath.isNull() || \
            tmp_filepath.isEmpty() || \
            !fileinfo.isFile()){
        return;
    }
    m_media_player.media_player_state()->select(tmp_filepath.toStdString());
}

void PlayerDialog::SLOT_receive_image(QImage image)
{
    int w = this->ui->lb_show->width();
    int h = this->ui->lb_show->height();
    QPixmap pixmap = QPixmap::fromImage(image.scaled(w, h, Qt::KeepAspectRatio));
    this->ui->lb_show->setPixmap(pixmap);
    return;
}


void PlayerDialog::on_pb_play_clicked()
{
    if(dynamic_cast<mediaplayer::NoFileLoadedState*>(m_media_player.media_player_state().get())){
        QString tmp_filepath;
        tmp_filepath = QFileDialog::getOpenFileName(nullptr, nullptr, nullptr);
        QFileInfo fileinfo = QFileInfo(tmp_filepath);
        if(tmp_filepath.isNull() || \
                tmp_filepath.isEmpty() || \
                !fileinfo.isFile()){
            return;
        }
        m_media_player.media_player_state()->play(tmp_filepath.toStdString());
        return;
    }
    m_media_player.media_player_state()->play();
}

void PlayerDialog::on_hslider_process_sliderMoved(int position)
{
    return;
}

void PlayerDialog::on_hslider_process_sliderPressed()
{
    m_druggle_slider = true;
    return;
}

void PlayerDialog::on_hslider_process_sliderReleased()
{
    qDebug() << "Slider released!";
    m_druggle_slider = false;
    return;
}

void PlayerDialog::on_pb_stop_clicked()
{
    m_media_player.media_player_state()->pause();
}

void PlayerDialog::SLOT_update_variables()
{
    ui->lb_m_file_path->setText(QString("filepath: %1").arg(m_media_player.file_path()));

    if(dynamic_cast<mediaplayer::NoFileLoadedState*>(m_media_player.media_player_state().get())){
        ui->lb_m_state->setText(QString("state: %1").arg(QString("NoFileLoaded")));
    }else if(dynamic_cast<mediaplayer::FileLoadedState*>(m_media_player.media_player_state().get())){
        ui->lb_m_state->setText(QString("state: %1").arg(QString("FileLoaded")));
    }else if(dynamic_cast<mediaplayer::PlayState*>(m_media_player.media_player_state().get())){
        ui->lb_m_state->setText(QString("state: %1").arg(QString("Play")));
    }else if(dynamic_cast<mediaplayer::PauseState*>(m_media_player.media_player_state().get())){
        ui->lb_m_state->setText(QString("state: %1").arg(QString("Pause")));
    }
//    ui->lb_m_video_flag->setText(QString("video_flag: %1").arg(m_media_player.video_flag()));
//    ui->lb_m_audio_flag->setText(QString("audio_flag: %1").arg(m_media_player.audio_flag()));
    ui->lb_m_video_flag->setText(QString("audio_queue_size: %1").arg(m_media_player.audio_queue_size()));
    ui->lb_m_audio_flag->setText(QString("video_queue_size: %1").arg(m_media_player.video_queue_size()));
    ui->lb_m_audio_buffer_size->setText(QString("audio_buffer_size: %1").arg(m_media_player.audio_buffer_size()));
    ui->lb_m_audio_buffer_cur->setText(QString("audio_buffer_cur: %1").arg(m_media_player.audio_buffer_cur()));
    ui->lb_m_all_video_samples_count->setText(QString("all_samples_count: %1").arg(m_media_player.all_video_samples_count()));
    ui->lb_m_play_samples_count->setText(QString("play_samples_count: %1").arg(m_media_player.play_samples_count()));
}

void PlayerDialog::SLOT_receive_playtime(double time)
{
    int it = static_cast<int>(time);
    QTime t(0, 0, 0, 0);
    QTime tt = t.addSecs(it);
    ui->lb_time_show->setText(tt.toString());
    int res = (int)(((double)m_media_player.play_samples_count() / (double)m_media_player.all_video_samples_count()) * 1000) + 1;
//    qDebug() << res;
    if(!m_druggle_slider){
        ui->hslider_process->setValue(res);
    }
}
