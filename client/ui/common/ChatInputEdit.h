// Pinyin-capable input edit for chat composer.
#pragma once

#include <QPlainTextEdit>

class ChatInputEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit ChatInputEdit(QWidget *parent = nullptr);

    bool isComposing() const;
    bool isNativeComposing() const;
    bool imeEnabled() const;
    void setImeEnabled(bool enabled);
    bool commitDefaultCandidate();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    class CandidatePopup;

    void handleToggleIme();
    bool handleCompositionKey(QKeyEvent *event);
    void startComposition(const QString &ch);
    void updateCompositionText();
    void updateCandidates();
    void commitCandidate(int index);
    void cancelComposition(bool keepText);
    void showPopup();
    void hidePopup();
    void updatePopupPosition();
    void ensurePopup();

    bool imeEnabled_{false};
    bool composing_{false};
    bool nativeComposing_{false};
    QString composition_;
    QStringList candidates_;
    int candidateIndex_{0};
    int compStart_{0};
    int compLength_{0};
    CandidatePopup *popup_{nullptr};
};
