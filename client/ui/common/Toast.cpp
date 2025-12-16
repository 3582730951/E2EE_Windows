#include "Toast.h"

#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

#include "Theme.h"

namespace {

QColor AccentFor(Toast::Level level) {
    switch (level) {
        case Toast::Level::Success:
            return Theme::accentGreen();
        case Toast::Level::Warning:
            return Theme::accentOrange();
        case Toast::Level::Error:
            return Theme::uiDangerRed();
        case Toast::Level::Info:
        default:
            return Theme::uiAccentBlue();
    }
}

class ToastPopup final : public QWidget {
public:
    explicit ToastPopup(QWidget *host)
        : QWidget(host),
          label_(new QLabel(this)),
          opacityEffect_(new QGraphicsOpacityEffect(this)),
          fadeAnim_(new QPropertyAnimation(opacityEffect_, "opacity", this)) {
        setObjectName(QStringLiteral("mi_toast_popup"));
        setAttribute(Qt::WA_StyledBackground, true);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(14, 10, 14, 10);
        layout->setSpacing(10);

        label_->setWordWrap(true);
        label_->setTextFormat(Qt::PlainText);
        layout->addWidget(label_, 1);

        setGraphicsEffect(opacityEffect_);
        opacityEffect_->setOpacity(0.0);

        fadeAnim_->setDuration(140);
        fadeAnim_->setEasingCurve(QEasingCurve::OutCubic);
        connect(fadeAnim_, &QPropertyAnimation::finished, this, [this]() {
            if (opacityEffect_->opacity() <= 0.01) {
                hide();
            }
        });

        hideTimer_.setSingleShot(true);
        connect(&hideTimer_, &QTimer::timeout, this, [this]() { fadeOut(); });

        if (host) {
            host->installEventFilter(this);
        }
    }

    void showText(const QString &text, Toast::Level level, int durationMs) {
        label_->setText(text.trimmed());
        applyStyle(level);
        adjustSize();
        reposition();

        show();
        raise();
        fadeIn();

        const int ms = qMax(800, durationMs);
        hideTimer_.start(ms);
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (obj == parentWidget()) {
            if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
                reposition();
            }
        }
        return QWidget::eventFilter(obj, event);
    }

private:
    void applyStyle(Toast::Level level) {
        QColor bg = Theme::uiPanelBg();
        bg.setAlpha(245);
        QColor border = Theme::uiBorder();
        border.setAlpha(200);
        QColor accent = AccentFor(level);
        QColor text = Theme::uiTextMain();

        setStyleSheet(QStringLiteral(
                          "QWidget#mi_toast_popup { background: %1; border: 1px solid %2; "
                          "border-left: 4px solid %3; border-radius: 10px; }"
                          "QLabel { color: %4; font-size: 12px; }")
                          .arg(bg.name(QColor::HexArgb),
                               border.name(QColor::HexArgb),
                               accent.name(),
                               text.name()));
    }

    void reposition() {
        QWidget *host = parentWidget();
        if (!host) {
            return;
        }
        const int margin = 18;
        const int maxToastWidth = qMin(520, host->width() - margin * 2);
        if (maxToastWidth > 0) {
            label_->setMaximumWidth(maxToastWidth - 28);
        }
        adjustSize();
        const QSize s = sizeHint();
        resize(qMin(maxToastWidth, s.width()), s.height());
        const int x = (host->width() - width()) / 2;
        const int y = host->height() - height() - margin;
        move(qMax(margin, x), qMax(margin, y));
    }

    void fadeIn() {
        fadeAnim_->stop();
        fadeAnim_->setStartValue(opacityEffect_->opacity());
        fadeAnim_->setEndValue(1.0);
        fadeAnim_->start();
    }

    void fadeOut() {
        fadeAnim_->stop();
        fadeAnim_->setStartValue(opacityEffect_->opacity());
        fadeAnim_->setEndValue(0.0);
        fadeAnim_->start();
    }

    QLabel *label_;
    QGraphicsOpacityEffect *opacityEffect_;
    QPropertyAnimation *fadeAnim_;
    QTimer hideTimer_;
};

ToastPopup *EnsureToast(QWidget *host) {
    if (!host) {
        return nullptr;
    }
    if (auto *existing = host->findChild<QWidget *>(QStringLiteral("mi_toast_popup"))) {
        if (auto *toast = dynamic_cast<ToastPopup *>(existing)) {
            return toast;
        }
    }
    auto *toast = new ToastPopup(host);
    toast->hide();
    return toast;
}

}  // namespace

void Toast::Show(QWidget *parent, const QString &text, Level level, int durationMs) {
    QWidget *host = parent ? parent->window() : nullptr;
    if (!host) {
        return;
    }
    ToastPopup *toast = EnsureToast(host);
    if (!toast) {
        return;
    }
    toast->showText(text, level, durationMs);
}
