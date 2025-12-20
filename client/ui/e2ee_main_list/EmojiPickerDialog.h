// Emoji picker dialog for the chat composer.
#pragma once

#include <QDialog>

class QListView;
class QAbstractItemModel;

class EmojiPickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit EmojiPickerDialog(QWidget *parent = nullptr);

signals:
    void emojiSelected(const QString &emoji);

private:
    QListView *view_{nullptr};
    QAbstractItemModel *model_{nullptr};
};
