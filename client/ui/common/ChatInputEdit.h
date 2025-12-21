// Pinyin-capable input edit for chat composer.
#pragma once

#include <QMargins>
#include <QPlainTextEdit>

class ChatInputEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    enum class InputMode {
        Chinese = 0,
        English = 1,
    };

    explicit ChatInputEdit(QWidget *parent = nullptr);
    ~ChatInputEdit() override;

    bool isComposing() const;
    bool isNativeComposing() const;
    bool imeEnabled() const;
    void setImeEnabled(bool enabled);
    bool commitDefaultCandidate();
    InputMode inputMode() const;
    bool isChineseMode() const;
    void setInputMode(InputMode mode);
    void setInputViewportMargins(int left, int top, int right, int bottom);
    QMargins inputViewportMargins() const;

signals:
    void inputModeChanged(bool chinese);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    class CandidatePopup;

    void handleToggleIme();
    void toggleInputMode();
    void applyInputMode(InputMode mode);
    bool handleCompositionKey(QKeyEvent *event);
    bool handleEnglishSuggestionKey(QKeyEvent *event);
    void startComposition(const QString &ch);
    void updateCompositionText();
    void updateCandidates();
    void commitCandidate(int index);
    void cancelComposition(bool keepText);
    void updateEnglishSuggestions();
    void commitEnglishCandidate(int index);
    void cancelEnglishSuggestions();
    void showPopup();
    void hidePopup();
    void updatePopupPosition();
    void ensurePopup();

    bool imeEnabled_{true};
    bool composing_{false};
    bool nativeComposing_{false};
    InputMode inputMode_{InputMode::Chinese};
    QString composition_;
    QStringList candidates_;
    int candidateIndex_{0};
    int compStart_{0};
    int compLength_{0};
    bool shiftPressed_{false};
    bool shiftUsed_{false};
    bool englishSuggesting_{false};
    QString englishPrefix_;
    QStringList englishCandidates_;
    int englishCandidateIndex_{0};
    int englishStart_{0};
    int englishLength_{0};
    CandidatePopup *popup_{nullptr};
};
