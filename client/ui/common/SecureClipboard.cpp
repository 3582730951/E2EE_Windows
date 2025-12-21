#include "SecureClipboard.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextCursor>

namespace {

SecureClipboard *g_instance = nullptr;

QString NormalizeSelectedText(QString text) {
    text.replace(QChar::ParagraphSeparator, QChar('\n'));
    text.replace(QChar::LineSeparator, QChar('\n'));
    return text;
}

}  // namespace

SecureClipboard *SecureClipboard::Install(QApplication &app) {
    if (!g_instance) {
        g_instance = new SecureClipboard(app);
        app.installEventFilter(g_instance);
    }
    return g_instance;
}

SecureClipboard *SecureClipboard::instance() {
    return g_instance;
}

void SecureClipboard::SetText(const QString &text) {
    if (auto *inst = SecureClipboard::instance()) {
        inst->setText(text);
        return;
    }
    if (auto *cb = QGuiApplication::clipboard()) {
        cb->setText(text);
    }
}

QString SecureClipboard::GetText() {
    if (auto *inst = SecureClipboard::instance()) {
        return inst->text();
    }
    if (auto *cb = QGuiApplication::clipboard()) {
        return cb->text();
    }
    return {};
}

SecureClipboard::SecureClipboard(QApplication &app) : QObject(&app) {
    connect(&app, &QGuiApplication::applicationStateChanged,
            this, &SecureClipboard::handleAppStateChanged);
    if (auto *cb = QGuiApplication::clipboard()) {
        connect(cb, &QClipboard::dataChanged, this, [this]() {
            if (QGuiApplication::applicationState() == Qt::ApplicationActive) {
                ownsSystem_ = true;
            }
        });
    }
}

void SecureClipboard::setText(const QString &text) {
    clearInternal();
    if (!text.isEmpty()) {
        buffer_ = text.toUtf8();
        hasData_ = true;
    }
    ownsSystem_ = true;
    clearSystemClipboard();
}

QString SecureClipboard::text() const {
    return hasData_ ? QString::fromUtf8(buffer_) : QString();
}

bool SecureClipboard::hasText() const {
    return hasData_;
}

bool SecureClipboard::eventFilter(QObject *obj, QEvent *event) {
    if (!obj || !event) {
        return QObject::eventFilter(obj, event);
    }
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::Copy)) {
            if (handleCopy(obj, false)) {
                return true;
            }
        }
        if (keyEvent->matches(QKeySequence::Cut)) {
            if (handleCopy(obj, true)) {
                return true;
            }
        }
        if (keyEvent->matches(QKeySequence::Paste)) {
            if (handlePaste(obj)) {
                return true;
            }
        }
    }
    if (event->type() == QEvent::ContextMenu) {
        auto *ctx = static_cast<QContextMenuEvent *>(event);
        if (auto *line = qobject_cast<QLineEdit *>(obj)) {
            QMenu menu(line);
            QAction *cut = menu.addAction(QStringLiteral("Cut"));
            QAction *copy = menu.addAction(QStringLiteral("Copy"));
            QAction *paste = menu.addAction(QStringLiteral("Paste"));
            QAction *selectAll = menu.addAction(QStringLiteral("Select All"));
            bool canPaste = !text().isEmpty();
            if (!canPaste) {
                if (auto *cb = QGuiApplication::clipboard()) {
                    canPaste = !cb->text().isEmpty();
                }
            }
            cut->setEnabled(!line->isReadOnly() && line->hasSelectedText());
            copy->setEnabled(line->hasSelectedText());
            paste->setEnabled(!line->isReadOnly() && canPaste);
            selectAll->setEnabled(!line->text().isEmpty());
            QAction *picked = menu.exec(ctx->globalPos());
            if (picked == cut) {
                handleCopy(obj, true);
            } else if (picked == copy) {
                handleCopy(obj, false);
            } else if (picked == paste) {
                handlePaste(obj);
            } else if (picked == selectAll) {
                line->selectAll();
            }
            return true;
        }
        if (auto *plain = qobject_cast<QPlainTextEdit *>(obj)) {
            QMenu menu(plain);
            QAction *cut = menu.addAction(QStringLiteral("Cut"));
            QAction *copy = menu.addAction(QStringLiteral("Copy"));
            QAction *paste = menu.addAction(QStringLiteral("Paste"));
            QAction *selectAll = menu.addAction(QStringLiteral("Select All"));
            const bool hasSel = plain->textCursor().hasSelection();
            bool canPaste = !text().isEmpty();
            if (!canPaste) {
                if (auto *cb = QGuiApplication::clipboard()) {
                    canPaste = !cb->text().isEmpty();
                }
            }
            cut->setEnabled(!plain->isReadOnly() && hasSel);
            copy->setEnabled(hasSel);
            paste->setEnabled(!plain->isReadOnly() && canPaste);
            selectAll->setEnabled(!plain->document()->isEmpty());
            QAction *picked = menu.exec(ctx->globalPos());
            if (picked == cut) {
                handleCopy(obj, true);
            } else if (picked == copy) {
                handleCopy(obj, false);
            } else if (picked == paste) {
                handlePaste(obj);
            } else if (picked == selectAll) {
                plain->selectAll();
            }
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void SecureClipboard::clearInternal() {
    if (!buffer_.isEmpty()) {
        buffer_.fill('\0');
    }
    buffer_.clear();
    hasData_ = false;
}

void SecureClipboard::clearSystemClipboard() {
    if (auto *cb = QGuiApplication::clipboard()) {
        cb->setText(QString());
    }
}

bool SecureClipboard::handleCopy(QObject *obj, bool cut) {
    if (auto *line = qobject_cast<QLineEdit *>(obj)) {
        const QString selected = line->selectedText();
        if (!selected.isEmpty()) {
            setText(selected);
            if (cut) {
                const int start = line->selectionStart();
                line->setSelection(start, selected.size());
                line->insert(QString());
            }
        }
        return !selected.isEmpty();
    }
    if (auto *plain = qobject_cast<QPlainTextEdit *>(obj)) {
        QTextCursor cursor = plain->textCursor();
        if (!cursor.hasSelection()) {
            return false;
        }
        const QString selected = NormalizeSelectedText(cursor.selectedText());
        if (!selected.isEmpty()) {
            setText(selected);
            if (cut) {
                cursor.removeSelectedText();
                plain->setTextCursor(cursor);
            }
            return true;
        }
        return false;
    }
    return false;
}

bool SecureClipboard::handlePaste(QObject *obj) {
    QString content = text();
    if (content.isEmpty()) {
        if (auto *cb = QGuiApplication::clipboard()) {
            content = cb->text();
        }
    }
    if (content.isEmpty()) {
        return false;
    }
    if (auto *line = qobject_cast<QLineEdit *>(obj)) {
        line->insert(content);
        return true;
    }
    if (auto *plain = qobject_cast<QPlainTextEdit *>(obj)) {
        QTextCursor cursor = plain->textCursor();
        cursor.insertText(content);
        plain->setTextCursor(cursor);
        return true;
    }
    return false;
}

void SecureClipboard::handleAppStateChanged(Qt::ApplicationState state) {
    if (state == Qt::ApplicationActive) {
        return;
    }
    if (ownsSystem_) {
        clearSystemClipboard();
        ownsSystem_ = false;
    }
    clearInternal();
}
