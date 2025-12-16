#pragma once

#include <QDialog>
#include <QString>

#include "UiSettings.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    void setClientConfigPath(const QString &path);

private:
    void loadFromCurrent();
    bool applyAndSave();
    void loadProxyFromConfig();
    bool saveProxyToConfig(QString &outError);
    QString detectConfigPath() const;

    struct ProxySnapshot {
        QString type;
        QString host;
        int port{0};
        QString username;
        QString password;
    };
    ProxySnapshot proxySnapshotFromWidgets() const;

    QComboBox *languageBox_{nullptr};
    QComboBox *schemeBox_{nullptr};
    QSpinBox *fontScale_{nullptr};
    QCheckBox *trayNotify_{nullptr};
    QCheckBox *trayPreview_{nullptr};

    QString clientConfigPath_;
    QLabel *proxyPathLabel_{nullptr};
    QComboBox *proxyType_{nullptr};
    QLineEdit *proxyHost_{nullptr};
    QSpinBox *proxyPort_{nullptr};
    QLineEdit *proxyUser_{nullptr};
    QLineEdit *proxyPass_{nullptr};
    ProxySnapshot loadedProxy_{};
};
