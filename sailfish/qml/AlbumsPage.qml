/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.AlbumModel 1.0

Page {
    id: root

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge

    property bool _doPop: false

    signal accepted(var songs);

    Component.onCompleted: {
        albumModel.filter = ""
    }

    function doPop() {
        if (pageStack.busy)
            _doPop = true;
        else
            pageStack.pop(pageStack.previousPage(root))
    }

    Connections {
        target: pageStack
        onBusyChanged: {
            if (!pageStack.busy && root._doPop)
                pageStack.pop();
        }
    }

    AlbumModel {
        id: albumModel

        onSongsQueryResult: {
            root.accepted(songs);
            root.doPop()
        }

        onFilterChanged: console.log("Filter: " + filter)
    }

    SilicaListView {
        id: listView

        anchors.fill: parent
        currentIndex: -1

        model: albumModel

        header: SearchField {
            width: parent.width
            placeholderText: qsTr("Search album")

            onActiveFocusChanged: {
                if (activeFocus) {
                    listView.currentIndex = -1
                }
            }

            onTextChanged: {
                albumModel.filter = text.toLowerCase().trim()
            }
        }

        delegate: DoubleListItem {
            title.text: model.title
            subtitle.text: qsTr("%n track(s)", "", model.count)
            icon.source: model.image
            defaultIcon.source: "image://theme/graphic-grid-playlist?" + (highlighted ?
                                    Theme.highlightColor : Theme.primaryColor)

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Select tracks")
                    onClicked: {
                        selectTracks()
                    }
                }

                MenuItem {
                    text: qsTr("Add all tracks")
                    onClicked: {
                        albumModel.querySongs(model.id)
                    }
                }
            }

            onClicked: {
                selectTracks()
            }

            function selectTracks() {
                var dialog = pageStack.push(Qt.resolvedUrl("TracksPage.qml"),{albumId: model.id})
                dialog.accepted.connect(function() {
                    root.accepted(dialog.selectedPaths)
                    root.doPop()
                })
            }
        }

        ViewPlaceholder {
            enabled: listView.count == 0
            text: qsTr("No albums")
        }
    }

    VerticalScrollDecorator {
        flickable: listView
    }
}
