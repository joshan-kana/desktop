/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QCoreApplication>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

class AssistantWindowQmlTestSetup : public QObject
{
    Q_OBJECT

public:
    AssistantWindowQmlTestSetup()
    {
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
    }

public Q_SLOTS:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImportPath(QCoreApplication::applicationDirPath());
        engine->addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
        engine->addImportPath(QStringLiteral("qrc:/qml/theme"));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(assistantwindow, AssistantWindowQmlTestSetup)

#include "assistantwindowqmltestrunner.moc"
