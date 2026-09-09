/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtTest

import "qrc:/qml/src/gui" as Gui

Item {
    width: 640
    height: 480

    Component {
        id: assistantWindowComponent

        Gui.AssistantWindow {}
    }

    TestCase {
        name: "AssistantWindow"
        when: windowShown

        function test_headlineIsBrandNeutral() {
            const assistantWindow = assistantWindowComponent.createObject(null)
            verify(assistantWindow !== null)
            compare(assistantWindow.headline, "Assistant")
            verify(assistantWindow.headline.indexOf("Nextcloud") === -1)
            assistantWindow.destroy()
        }
    }
}
