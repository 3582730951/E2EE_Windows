#pragma once

#include <QString>

#include <cstddef>
#include <cstring>

namespace mi::ui {

constexpr char kCallVoicePrefix[] = "[call]voice:";
constexpr char kCallVideoPrefix[] = "[call]video:";

struct CallInvite {
    bool ok{false};
    bool video{false};
    QString callId;
};

inline bool IsHexChar(const QChar &ch) {
    const ushort c = ch.unicode();
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

inline bool IsValidCallId(const QString &callId) {
    if (callId.size() != 32) {
        return false;
    }
    for (const auto &ch : callId) {
        if (!IsHexChar(ch)) {
            return false;
        }
    }
    return true;
}

inline CallInvite ParseCallInvite(const QString &text) {
    CallInvite invite;
    if (text.startsWith(QString::fromLatin1(kCallVoicePrefix))) {
        invite.ok = true;
        invite.video = false;
        invite.callId = text.mid(static_cast<int>(std::strlen(kCallVoicePrefix)));
    } else if (text.startsWith(QString::fromLatin1(kCallVideoPrefix))) {
        invite.ok = true;
        invite.video = true;
        invite.callId = text.mid(static_cast<int>(std::strlen(kCallVideoPrefix)));
    }
    invite.callId = invite.callId.trimmed();
    if (!IsValidCallId(invite.callId)) {
        invite.ok = false;
        invite.callId.clear();
    }
    return invite;
}

inline QString BuildCallInvitePayload(bool video, const QString &callId) {
    return QString::fromLatin1(video ? kCallVideoPrefix : kCallVoicePrefix) + callId;
}

}  // namespace mi::ui
