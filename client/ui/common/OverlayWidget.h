// Overlay widget to show semi-transparent reference images.
#pragma once

#include <QLabel>
#include <QWidget>

class OverlayWidget : public QWidget {
    Q_OBJECT

public:
    explicit OverlayWidget(QWidget *parent = nullptr);
    void setOverlayImage(const QString &path);
    bool isOverlayVisible() const;

public slots:
    void toggle();
    void showOverlay();
    void hideOverlay();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void refreshPixmap();

    QLabel *m_label;
    QString m_path;
    QPixmap m_source;
};
