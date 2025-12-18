#include "SettingsDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(UiSettings::Tr(QStringLiteral("设置"), QStringLiteral("Settings")));
    setModal(true);
    resize(520, 520);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);

    languageBox_ = new QComboBox(this);
    languageBox_->addItem(UiSettings::LanguageLabel(UiSettings::Language::Auto),
                          static_cast<int>(UiSettings::Language::Auto));
    languageBox_->addItem(UiSettings::LanguageLabel(UiSettings::Language::ZhCN),
                          static_cast<int>(UiSettings::Language::ZhCN));
    languageBox_->addItem(UiSettings::LanguageLabel(UiSettings::Language::EnUS),
                          static_cast<int>(UiSettings::Language::EnUS));
    form->addRow(UiSettings::Tr(QStringLiteral("语言"), QStringLiteral("Language")), languageBox_);

    schemeBox_ = new QComboBox(this);
    schemeBox_->addItem(UiSettings::SchemeLabel(Theme::Scheme::Auto),
                        static_cast<int>(Theme::Scheme::Auto));
    schemeBox_->addItem(UiSettings::SchemeLabel(Theme::Scheme::Dark),
                        static_cast<int>(Theme::Scheme::Dark));
    schemeBox_->addItem(UiSettings::SchemeLabel(Theme::Scheme::Light),
                        static_cast<int>(Theme::Scheme::Light));
    schemeBox_->addItem(UiSettings::SchemeLabel(Theme::Scheme::HighContrast),
                        static_cast<int>(Theme::Scheme::HighContrast));
    form->addRow(UiSettings::Tr(QStringLiteral("主题"), QStringLiteral("Theme")), schemeBox_);

    fontScale_ = new QSpinBox(this);
    fontScale_->setRange(50, 200);
    fontScale_->setSingleStep(10);
    fontScale_->setSuffix(QStringLiteral("%"));
    form->addRow(UiSettings::Tr(QStringLiteral("字体缩放"), QStringLiteral("Font Scale")),
                 fontScale_);

    layout->addLayout(form);

    trayNotify_ =
        new QCheckBox(UiSettings::Tr(QStringLiteral("启用托盘通知"),
                                     QStringLiteral("Enable tray notifications")),
                      this);
    trayPreview_ =
        new QCheckBox(UiSettings::Tr(QStringLiteral("通知显示消息内容（默认关闭）"),
                                     QStringLiteral("Show message previews (default off)")),
                      this);
    trayPreview_->setToolTip(
        UiSettings::Tr(QStringLiteral("开启后托盘通知可能暴露消息内容，请谨慎。"),
                       QStringLiteral("Enabling previews may expose message contents.")));

    layout->addWidget(trayNotify_);
    layout->addWidget(trayPreview_);

    auto *proxyGroup =
        new QGroupBox(UiSettings::Tr(QStringLiteral("代理（SOCKS5）"), QStringLiteral("Proxy (SOCKS5)")),
                      this);
    auto *proxyLayout = new QVBoxLayout(proxyGroup);
    proxyLayout->setContentsMargins(12, 12, 12, 12);
    proxyLayout->setSpacing(8);

    proxyPathLabel_ = new QLabel(proxyGroup);
    proxyPathLabel_->setWordWrap(true);
    proxyPathLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::uiTextMuted().name()));
    proxyLayout->addWidget(proxyPathLabel_);

    auto *proxyForm = new QFormLayout();
    proxyForm->setLabelAlignment(Qt::AlignLeft);

    proxyType_ = new QComboBox(proxyGroup);
    proxyType_->addItem(UiSettings::Tr(QStringLiteral("无"), QStringLiteral("None")),
                        QStringLiteral("none"));
    proxyType_->addItem(QStringLiteral("SOCKS5"), QStringLiteral("socks5"));
    proxyForm->addRow(UiSettings::Tr(QStringLiteral("类型"), QStringLiteral("Type")), proxyType_);

    proxyHost_ = new QLineEdit(proxyGroup);
    proxyHost_->setPlaceholderText(QStringLiteral("127.0.0.1"));
    proxyForm->addRow(UiSettings::Tr(QStringLiteral("主机"), QStringLiteral("Host")), proxyHost_);

    proxyPort_ = new QSpinBox(proxyGroup);
    proxyPort_->setRange(0, 65535);
    proxyPort_->setValue(0);
    proxyForm->addRow(UiSettings::Tr(QStringLiteral("端口"), QStringLiteral("Port")), proxyPort_);

    proxyUser_ = new QLineEdit(proxyGroup);
    proxyForm->addRow(UiSettings::Tr(QStringLiteral("用户名"), QStringLiteral("Username")), proxyUser_);

    proxyPass_ = new QLineEdit(proxyGroup);
    proxyPass_->setEchoMode(QLineEdit::Password);
    proxyForm->addRow(UiSettings::Tr(QStringLiteral("密码"), QStringLiteral("Password")), proxyPass_);

    proxyLayout->addLayout(proxyForm);

    auto *proxyNote = new QLabel(
        UiSettings::Tr(QStringLiteral("提示：代理仅影响远程 TCP/TLS 的网络层转发，不改变端到端加密语义。"),
                       QStringLiteral("Note: proxy only affects transport routing, not E2EE.")),
        proxyGroup);
    proxyNote->setWordWrap(true);
    proxyNote->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::uiTextMuted().name()));
    proxyLayout->addWidget(proxyNote);

    layout->addWidget(proxyGroup);

    auto *note = new QLabel(
        UiSettings::Tr(QStringLiteral("安全提示：默认不在通知里显示消息内容。"),
                       QStringLiteral("Privacy: notifications hide message contents by default.")),
        this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::uiTextMuted().name()));
    layout->addWidget(note);

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(
        UiSettings::Tr(QStringLiteral("确定"), QStringLiteral("OK")));
    buttons->button(QDialogButtonBox::Cancel)->setText(
        UiSettings::Tr(QStringLiteral("取消"), QStringLiteral("Cancel")));
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (applyAndSave()) {
            accept();
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(trayNotify_, &QCheckBox::toggled, this, [this](bool on) {
        trayPreview_->setEnabled(on);
        if (!on) {
            trayPreview_->setChecked(false);
        }
    });

    connect(proxyType_, &QComboBox::currentIndexChanged, this, [this](int) {
        const bool enabled = proxyType_->currentData().toString() != QStringLiteral("none");
        proxyHost_->setEnabled(enabled);
        proxyPort_->setEnabled(enabled);
        proxyUser_->setEnabled(enabled);
        proxyPass_->setEnabled(enabled);
        if (!enabled) {
            proxyPort_->setValue(0);
        }
    });

    loadFromCurrent();
}

void SettingsDialog::setClientConfigPath(const QString &path) {
    const QString p = path.trimmed();
    clientConfigPath_ = p.isEmpty() ? detectConfigPath() : p;
    loadProxyFromConfig();
}

void SettingsDialog::loadFromCurrent() {
    const UiSettings::Settings s = UiSettings::current();

    const auto setComboByData = [](QComboBox *box, int v) {
        const int idx = box->findData(v);
        if (idx >= 0) {
            box->setCurrentIndex(idx);
        }
    };

    setComboByData(languageBox_, static_cast<int>(s.language));
    setComboByData(schemeBox_, static_cast<int>(s.scheme));
    fontScale_->setValue(s.fontScalePercent);
    trayNotify_->setChecked(s.trayNotifications);
    trayPreview_->setChecked(s.trayPreview);
    trayPreview_->setEnabled(s.trayNotifications);

    loadProxyFromConfig();
}

SettingsDialog::ProxySnapshot SettingsDialog::proxySnapshotFromWidgets() const {
    ProxySnapshot snap;
    snap.type = proxyType_ ? proxyType_->currentData().toString().trimmed().toLower()
                           : QStringLiteral("none");
    snap.host = proxyHost_ ? proxyHost_->text().trimmed() : QString();
    snap.port = proxyPort_ ? proxyPort_->value() : 0;
    snap.username = proxyUser_ ? proxyUser_->text() : QString();
    snap.password = proxyPass_ ? proxyPass_->text() : QString();
    return snap;
}

QString SettingsDialog::detectConfigPath() const {
    const auto resolve = [](const QString &name) -> QString {
        if (name.isEmpty()) {
            return {};
        }
        const QFileInfo info(name);
        if (info.isAbsolute() && QFile::exists(name)) {
            return name;
        }
        if (QFile::exists(name)) {
            return name;
        }
        const QString candidate =
            QCoreApplication::applicationDirPath() + QStringLiteral("/") + name;
        if (QFile::exists(candidate)) {
            return candidate;
        }
        return name;
    };

    const QString p1 = resolve(QStringLiteral("client_config.ini"));
    if (QFile::exists(p1)) {
        return p1;
    }
    const QString p2 = resolve(QStringLiteral("config.ini"));
    if (QFile::exists(p2)) {
        return p2;
    }
    return p1.isEmpty() ? QStringLiteral("client_config.ini") : p1;
}

void SettingsDialog::loadProxyFromConfig() {
    if (!proxyType_ || !proxyHost_ || !proxyPort_ || !proxyUser_ || !proxyPass_ ||
        !proxyPathLabel_) {
        return;
    }

    if (clientConfigPath_.trimmed().isEmpty()) {
        clientConfigPath_ = detectConfigPath();
    }
    proxyPathLabel_->setText(
        UiSettings::Tr(QStringLiteral("配置文件：%1"), QStringLiteral("Config file: %1"))
            .arg(clientConfigPath_));

    QString type = QStringLiteral("none");
    QString host;
    int port = 0;
    QString user;
    QString pass;

    if (QFile::exists(clientConfigPath_)) {
        QSettings cfg(clientConfigPath_, QSettings::IniFormat);
        cfg.beginGroup(QStringLiteral("proxy"));
        type = cfg.value(QStringLiteral("type"), QStringLiteral("none")).toString().trimmed().toLower();
        host = cfg.value(QStringLiteral("host")).toString().trimmed();
        port = cfg.value(QStringLiteral("port"), 0).toInt();
        user = cfg.value(QStringLiteral("username")).toString();
        pass = cfg.value(QStringLiteral("password")).toString();
        cfg.endGroup();
    }

    const bool isSocks5 = (type == QStringLiteral("socks5") || type == QStringLiteral("socks"));
    const QString normalizedType = isSocks5 ? QStringLiteral("socks5") : QStringLiteral("none");
    const int idx = proxyType_->findData(normalizedType);
    if (idx >= 0) {
        proxyType_->setCurrentIndex(idx);
    }
    proxyHost_->setText(host);
    proxyPort_->setValue(port < 0 ? 0 : (port > 65535 ? 65535 : port));
    proxyUser_->setText(user);
    proxyPass_->setText(pass);

    loadedProxy_ = proxySnapshotFromWidgets();
    const bool enabled = normalizedType != QStringLiteral("none");
    proxyHost_->setEnabled(enabled);
    proxyPort_->setEnabled(enabled);
    proxyUser_->setEnabled(enabled);
    proxyPass_->setEnabled(enabled);
}

bool SettingsDialog::saveProxyToConfig(QString &outError) {
    outError.clear();
    if (!proxyType_ || !proxyHost_ || !proxyPort_ || !proxyUser_ || !proxyPass_) {
        outError = UiSettings::Tr(QStringLiteral("代理控件未初始化"),
                                  QStringLiteral("Proxy widgets not initialized"));
        return false;
    }
    if (clientConfigPath_.trimmed().isEmpty()) {
        clientConfigPath_ = detectConfigPath();
    }

    const ProxySnapshot snap = proxySnapshotFromWidgets();
    if (snap.type == QStringLiteral("socks5")) {
        if (snap.host.trimmed().isEmpty() || snap.port <= 0 || snap.port > 65535) {
            outError = UiSettings::Tr(QStringLiteral("代理配置不完整：请填写 host/port。"),
                                      QStringLiteral("Proxy config incomplete: host/port required."));
            return false;
        }
    }

    QSettings cfg(clientConfigPath_, QSettings::IniFormat);
    cfg.beginGroup(QStringLiteral("proxy"));
    cfg.setValue(QStringLiteral("type"), snap.type);
    cfg.setValue(QStringLiteral("host"), snap.host);
    cfg.setValue(QStringLiteral("port"), snap.port);
    cfg.setValue(QStringLiteral("username"), snap.username);
    cfg.setValue(QStringLiteral("password"), snap.password);
    cfg.endGroup();
    cfg.sync();
    if (cfg.status() != QSettings::NoError) {
        outError = UiSettings::Tr(QStringLiteral("写入配置文件失败：%1"),
                                  QStringLiteral("Failed to write config: %1"))
                       .arg(clientConfigPath_);
        return false;
    }

    loadedProxy_ = snap;
    return true;
}

bool SettingsDialog::applyAndSave() {
    const UiSettings::Settings prev = UiSettings::current();
    UiSettings::Settings next = prev;
    next.language = static_cast<UiSettings::Language>(languageBox_->currentData().toInt());
    next.scheme = static_cast<Theme::Scheme>(schemeBox_->currentData().toInt());
    next.fontScalePercent = fontScale_->value();
    next.trayNotifications = trayNotify_->isChecked();
    next.trayPreview = trayPreview_->isChecked();

    const bool languageChanged = next.language != prev.language;
    const bool schemeChanged = next.scheme != prev.scheme;
    const bool fontChanged = next.fontScalePercent != prev.fontScalePercent;

    UiSettings::setCurrent(next);
    UiSettings::Save(next);

    if (qApp && fontChanged) {
        Theme::setFontScalePercent(next.fontScalePercent);
        qApp->setFont(Theme::defaultFont(10));
    }

    const ProxySnapshot newProxy = proxySnapshotFromWidgets();
    const bool proxyChanged =
        newProxy.type != loadedProxy_.type || newProxy.host != loadedProxy_.host ||
        newProxy.port != loadedProxy_.port || newProxy.username != loadedProxy_.username ||
        newProxy.password != loadedProxy_.password;
    if (proxyChanged) {
        QString proxyErr;
        if (!saveProxyToConfig(proxyErr)) {
            QMessageBox::warning(
                this, UiSettings::Tr(QStringLiteral("保存失败"), QStringLiteral("Save Failed")),
                proxyErr.isEmpty()
                    ? UiSettings::Tr(QStringLiteral("代理配置保存失败。"),
                                     QStringLiteral("Failed to save proxy settings."))
                    : proxyErr);
            return false;
        }
    }

    if (languageChanged || schemeChanged || proxyChanged) {
        const QString msg = UiSettings::Tr(
            QStringLiteral("语言/主题/代理等设置可能需要重启或重新连接后生效。"),
            QStringLiteral("Some settings may take effect after restart/reconnect."));
        QMessageBox::information(this, UiSettings::Tr(QStringLiteral("提示"), QStringLiteral("Info")),
                                 msg);
    }

    return true;
}
