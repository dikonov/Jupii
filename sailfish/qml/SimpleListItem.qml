/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

ListItem {
    id: root

    property alias title: _title
    property alias icon: _icon
    property alias defaultIcon: _dicon

    contentHeight: Theme.itemSizeMedium

    anchors {
        left: parent.left
        right: parent.right
    }

    Image {
        id: _icon
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }

        height: Theme.iconSizeMedium
        width: Theme.iconSizeMedium

        Image {
            id: _dicon
            anchors.fill: parent
            visible: icon.status !== Image.Ready
        }
    }

    Label {
        id: _title

        truncationMode: TruncationMode.Fade

        anchors {
            left: _icon.status !== Image.Ready &&
                  _dicon.status !== Image.Ready ? parent.left : _icon.right
            right: parent.right
            leftMargin: Theme.horizontalPageMargin;
            rightMargin: Theme.horizontalPageMargin;
            verticalCenter: parent.verticalCenter
        }

        color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
    }
}
