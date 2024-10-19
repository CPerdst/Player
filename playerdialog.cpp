#include "playerdialog.h"
#include "ui_playerdialog.h"
#include <QFileDialog>
#include <QDebug>
#include "QTime"

PlayerDialog::PlayerDialog(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::PlayerDialog)
{
    ui->setupUi(this);
    // 初始化模块
    m_media_player.register_all();

    // 信号槽连接
    // 1、媒体控制模块发送解码好的视频图像 -> 显示图像到lb_show控件
    connect(&m_media_player, SIGNAL(SIG_send_image(QImage)), this, SLOT(SLOT_receive_image(QImage)));

    // 初始化控件
    // 1、设置lb_show为全黑背景
    qDebug() << ui->lb_show->width() << " " << ui->lb_show->height();
    QImage background = QImage(ui->lb_show->width(), ui->lb_show->height(), QImage::Format::Format_RGB32);
    background.fill(Qt::black);
    emit m_media_player.SIG_send_image(background);
    // 2、设置lb_time_show
    QTime time;
    time.setHMS(0, 0, 0);
    ui->lb_time_show->setText(time.toString());
    // 3、设置hslider_process的总位置(视频秒数)
    ui->hslider_process->setDisabled(true);

}

PlayerDialog::~PlayerDialog()
{
    delete ui;
}

void PlayerDialog::on_pb_select_clicked()
{
    // 首先判断mediaplayer是否正在播放视频
    if(m_media_player.is_playing()){
        // 如果正在播放媒体文件，关闭并重置标志位并清理资源
        m_media_player.clear_recourse();
    }
    // 选择音频文件
    QString path = QFileDialog::getOpenFileName();
    qDebug() << path;
    m_media_player.setFile_path(path);
}

void PlayerDialog::SLOT_receive_image(QImage image)
{
    int w = this->ui->lb_show->width();
    int h = this->ui->lb_show->height();
    QPixmap pixmap = QPixmap::fromImage(image.scaled(w, h, Qt::IgnoreAspectRatio));
    this->ui->lb_show->setPixmap(pixmap);
    return;
}


void PlayerDialog::on_pb_play_clicked()
{
    if(m_media_player.is_playing()){
        return;
    }
    QFileInfo fileinfo(m_media_player.file_path());
    if(m_media_player.file_path().isEmpty() ||
            !fileinfo.exists() || !fileinfo.isFile()){
        // 如果m_file_path是空的，或者不存在，或者不是一个文件而是一个目录，那就重新选择一个文件
        QString tmp_file_path = QFileDialog::getOpenFileName(nullptr, nullptr, nullptr, nullptr);
        m_media_player.setFile_path(tmp_file_path);
    }
    // 初始化相关av资源
    m_media_player.initialized();
    // 开始播放
    m_media_player.start();
}

void PlayerDialog::on_hslider_process_sliderMoved(int position)
{
    qDebug() << "Slider moved to: " << position;
    return;
}

void PlayerDialog::on_hslider_process_sliderPressed()
{
    qDebug() << "Slider pressed!";
    return;
}

void PlayerDialog::on_hslider_process_sliderReleased()
{
    qDebug() << "Slider released!";
    return;
}
