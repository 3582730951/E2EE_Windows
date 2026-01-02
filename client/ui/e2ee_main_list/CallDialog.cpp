#include "CallDialog.h"

#include <QCloseEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVideoFrame>
#include <QVideoSink>
#include <QImage>
#include <QElapsedTimer>

#include "../common/Toast.h"
#include "../common/UiSettings.h"

class CallDialog::VideoFrameWidget : public QWidget {
public:
    explicit VideoFrameWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(&sink_, &QVideoSink::videoFrameChanged, this,
                &VideoFrameWidget::handleFrame);
        frame_timer_.start();
    }

    QVideoSink *sink() { return &sink_; }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(18, 18, 18));
        if (frame_.isNull()) {
            return;
        }
        const QSize target = frame_.size().scaled(rect().size(), Qt::KeepAspectRatio);
        QRect drawRect(QPoint(0, 0), target);
        drawRect.moveCenter(rect().center());
        p.drawImage(drawRect, frame_);
    }

private:
    void handleFrame(const QVideoFrame &frame) {
        if (frame_timer_.isValid() && frame_timer_.elapsed() < min_interval_ms_) {
            return;
        }
        frame_timer_.restart();
        if (!frame.isValid()) {
            frame_ = QImage();
            update();
            return;
        }
        const QImage img = frame.toImage();
        frame_ = img;
        update();
    }

    QVideoSink sink_;
    QImage frame_;
    QElapsedTimer frame_timer_;
    int min_interval_ms_{33};
};

CallDialog::CallDialog(mi::client::ClientCore &core, QWidget *parent)
    : QDialog(parent), controller_(core, this) {
    setWindowTitle(UiSettings::Tr(QStringLiteral("通话"), QStringLiteral("Call")));
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMinimumSize(420, 280);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    status_label_ = new QLabel(this);
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    status_label_->setText(QStringLiteral(" "));
    root->addWidget(status_label_);

    video_container_ = new QWidget(this);
    auto *videoLayout = new QGridLayout(video_container_);
    videoLayout->setContentsMargins(0, 0, 0, 0);

    remote_view_ = new VideoFrameWidget(video_container_);
    local_view_ = new VideoFrameWidget(video_container_);
    local_view_->setFixedSize(160, 120);
    local_view_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    videoLayout->addWidget(remote_view_, 0, 0);

    auto *overlay = new QWidget(video_container_);
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    auto *overlayLayout = new QVBoxLayout(overlay);
    overlayLayout->setContentsMargins(0, 0, 8, 8);
    overlayLayout->addStretch();
    auto *overlayRow = new QHBoxLayout();
    overlayRow->addStretch();
    overlayRow->addWidget(local_view_);
    overlayLayout->addLayout(overlayRow);
    videoLayout->addWidget(overlay, 0, 0);

    root->addWidget(video_container_, 1);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(12);
    accept_btn_ = new QPushButton(UiSettings::Tr(QStringLiteral("接受"),
                                                 QStringLiteral("Accept")),
                                  this);
    decline_btn_ = new QPushButton(UiSettings::Tr(QStringLiteral("拒绝"),
                                                  QStringLiteral("Decline")),
                                   this);
    hangup_btn_ = new QPushButton(UiSettings::Tr(QStringLiteral("挂断"),
                                                 QStringLiteral("Hang up")),
                                  this);
    btnRow->addStretch();
    btnRow->addWidget(accept_btn_);
    btnRow->addWidget(decline_btn_);
    btnRow->addWidget(hangup_btn_);
    btnRow->addStretch();
    root->addLayout(btnRow);

    connect(accept_btn_, &QPushButton::clicked, this, &CallDialog::acceptCall);
    connect(decline_btn_, &QPushButton::clicked, this, &CallDialog::declineCall);
    connect(hangup_btn_, &QPushButton::clicked, this, &CallDialog::hangupCall);
    connect(&controller_, &CallController::callStateChanged, this,
            &CallDialog::updateUiState);

    controller_.setLocalVideoSink(local_view_->sink());
    controller_.setRemoteVideoSink(remote_view_->sink());
    applyVideoVisibility();
    updateUiState();
}

bool CallDialog::startOutgoing(const QString &peer,
                               const QString &callIdHex,
                               bool video,
                               QString &outError) {
    peer_ = peer.trimmed();
    call_id_ = callIdHex.trimmed();
    video_ = video;
    incoming_ = false;
    applyVideoVisibility();
    if (!controller_.Start(peer_, call_id_, true, video_, outError)) {
        return false;
    }
    updateUiState();
    return true;
}

void CallDialog::showIncoming(const QString &peer,
                              const QString &callIdHex,
                              bool video) {
    peer_ = peer.trimmed();
    call_id_ = callIdHex.trimmed();
    video_ = video;
    incoming_ = true;
    applyVideoVisibility();
    updateUiState();
    show();
    raise();
    activateWindow();
}

void CallDialog::closeEvent(QCloseEvent *event) {
    controller_.Stop();
    emit callEnded();
    QDialog::closeEvent(event);
}

void CallDialog::acceptCall() {
    if (controller_.isActive()) {
        incoming_ = false;
        updateUiState();
        return;
    }
    QString err;
    if (!controller_.Start(peer_, call_id_, false, video_, err)) {
        Toast::Show(this,
                    err.isEmpty()
                        ? UiSettings::Tr(QStringLiteral("加入通话失败"),
                                         QStringLiteral("Failed to join call"))
                        : err,
                    Toast::Level::Error);
        return;
    }
    incoming_ = false;
    updateUiState();
}

void CallDialog::declineCall() {
    close();
}

void CallDialog::hangupCall() {
    close();
}

void CallDialog::updateUiState() {
    const bool active = controller_.isActive();
    const QString peer = peer_.isEmpty() ? QStringLiteral("…") : peer_;
    const QString callType =
        video_
            ? UiSettings::Tr(QStringLiteral("视频通话"),
                             QStringLiteral("Video call"))
            : UiSettings::Tr(QStringLiteral("语音通话"),
                             QStringLiteral("Voice call"));

    if (incoming_ && !active) {
        status_label_->setText(
            UiSettings::Tr(QStringLiteral("来自 %1 的%2邀请").arg(peer, callType),
                           QStringLiteral("Incoming %1 from %2").arg(callType, peer)));
    } else if (active) {
        status_label_->setText(
            UiSettings::Tr(QStringLiteral("与 %1 %2中").arg(peer, callType),
                           QStringLiteral("%1 with %2").arg(callType, peer)));
    } else {
        status_label_->setText(
            UiSettings::Tr(QStringLiteral("正在呼叫 %1…").arg(peer),
                           QStringLiteral("Calling %1…").arg(peer)));
    }

    accept_btn_->setVisible(incoming_ && !active);
    decline_btn_->setVisible(incoming_ && !active);
    hangup_btn_->setVisible(!incoming_ || active);
}

void CallDialog::applyVideoVisibility() {
    if (video_container_) {
        video_container_->setVisible(video_);
    }
    if (!video_) {
        setMinimumSize(360, 200);
    } else {
        setMinimumSize(520, 360);
    }
}
