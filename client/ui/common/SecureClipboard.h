// Internal clipboard isolation for the client UI.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtGlobal>

class QApplication;

class SecureClipboard : public QObject {
    Q_OBJECT

public:
    static SecureClipboard *Install(QApplication &app);
    static SecureClipboard *instance();
    static void SetText(const QString &text);
    static QString GetText();

    void setSystemClipboardWriteEnabled(bool enabled);
    bool systemClipboardWriteEnabled() const;

    void setText(const QString &text);
    QString text() const;
    bool hasText() const;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    explicit SecureClipboard(QApplication &app);
    void clearInternal();
    bool handleCopy(QObject *obj, bool cut);
    bool handlePaste(QObject *obj);
    void handleAppStateChanged(Qt::ApplicationState state);

    QByteArray buffer_;
    bool hasData_{false};
    bool allowSystemWrite_{false};
    qint64 lastInternalCopyMs_{0};
    qint64 lastSystemCopyMs_{0};
};
