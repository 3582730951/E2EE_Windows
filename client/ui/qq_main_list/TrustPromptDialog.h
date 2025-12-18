#pragma once

#include <QString>

class QWidget;

bool PromptTrustWithSas(QWidget *parent,
                        const QString &title,
                        const QString &description,
                        const QString &fingerprintHex,
                        const QString &sasShown,
                        QString &outSasInput,
                        const QString &entityLabel = QString(),
                        const QString &entityValue = QString());

