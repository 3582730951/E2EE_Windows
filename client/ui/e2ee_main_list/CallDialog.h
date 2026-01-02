#pragma once

#include <QDialog>

#include "CallController.h"

class QLabel;
class QPushButton;
class QVBoxLayout;

class CallDialog : public QDialog {
    Q_OBJECT

public:
    explicit CallDialog(mi::client::ClientCore &core, QWidget *parent = nullptr);

    bool startOutgoing(const QString &peer, const QString &callIdHex, bool video, QString &outError);
    void showIncoming(const QString &peer, const QString &callIdHex, bool video);

    bool hasActiveCall() const { return controller_.isActive(); }
    QString activeCallId() const { return controller_.activeCallId(); }
    QString activeCallPeer() const { return controller_.activeCallPeer(); }

signals:
    void callEnded();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void acceptCall();
    void declineCall();
    void hangupCall();
    void updateUiState();

private:
    void applyVideoVisibility();

    class VideoFrameWidget;

    CallController controller_;
    QString peer_;
    QString call_id_;
    bool video_{false};
    bool incoming_{false};
    QLabel *status_label_{nullptr};
    QWidget *video_container_{nullptr};
    VideoFrameWidget *remote_view_{nullptr};
    VideoFrameWidget *local_view_{nullptr};
    QPushButton *accept_btn_{nullptr};
    QPushButton *decline_btn_{nullptr};
    QPushButton *hangup_btn_{nullptr};
};
