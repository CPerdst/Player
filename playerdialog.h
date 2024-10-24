#ifndef PLAYERDIALOG_H
#define PLAYERDIALOG_H

#include <QMainWindow>
#include "mediaplayer.h"
#include "QImage"
#include "functional"
#include "QTimer"

QT_BEGIN_NAMESPACE
namespace Ui { class PlayerDialog; }
QT_END_NAMESPACE

class PlayerDialog : public QMainWindow
{
    Q_OBJECT

public:
    PlayerDialog(QWidget *parent = nullptr);
    ~PlayerDialog();

private slots:
    void on_pb_select_clicked();
    void SLOT_receive_image(QImage image);

    void on_pb_play_clicked();
    /**
     * 用于检测滑块事件
     * @brief on_hslider_process_sliderMoved
     * @brief on_hslider_process_sliderPressed
     * @brief on_hslider_process_sliderReleased
     */

    void on_hslider_process_sliderMoved(int position);
    void on_hslider_process_sliderPressed();
    void on_hslider_process_sliderReleased();

    void on_pb_stop_clicked();
    void SLOT_update_variables();
    void SLOT_receive_playtime(double time);

private:
    Ui::PlayerDialog *ui;
    mediaplayer m_media_player;
    bool m_playing;
    std::function<void(void)> m_select_callbacks;
    QTimer m_timer;

};
#endif // PLAYERDIALOG_H
