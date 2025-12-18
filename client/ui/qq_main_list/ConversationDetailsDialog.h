// Conversation details + shared files dialog.
#pragma once

#include <QDialog>
#include <QtGlobal>
#include <QString>
#include <QVector>

class BackendAdapter;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QToolButton;

class ConversationDetailsDialog : public QDialog {
    Q_OBJECT

public:
    enum class StartPage {
        Info = 0,
        Files = 1,
    };

    explicit ConversationDetailsDialog(BackendAdapter *backend,
                                       const QString &conversationId,
                                       const QString &title,
                                       bool isGroup,
                                       QWidget *parent = nullptr);

    void setStartPage(StartPage page);

private:
    struct FileRow {
        QString messageId;
        QString name;
        QString sender;
        qint64 size{0};
        qint64 timestampSec{0};
        bool outgoing{false};
    };

    void buildUi();
    void ensureMembersLoaded();
    void ensureFilesLoaded();
    void reloadMembers();
    void reloadFiles();
    void saveSelectedFile();

    BackendAdapter *backend_{nullptr};
    QString conversationId_;
    QString title_;
    bool isGroup_{false};

    bool membersLoaded_{false};
    bool filesLoaded_{false};

    QToolButton *infoBtn_{nullptr};
    QToolButton *filesBtn_{nullptr};
    QStackedWidget *stack_{nullptr};

    QLabel *idValue_{nullptr};
    QLabel *typeValue_{nullptr};
    QLabel *membersHint_{nullptr};
    QListWidget *membersList_{nullptr};
    QPushButton *refreshMembersBtn_{nullptr};

    QLabel *filesHint_{nullptr};
    QTableWidget *filesTable_{nullptr};
    QPushButton *refreshFilesBtn_{nullptr};
    QPushButton *saveFileBtn_{nullptr};

    QVector<FileRow> files_;
};

