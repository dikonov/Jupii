/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QVariant>
#include <QSettings>
#include <QByteArray>

#include "taskexecutor.h"

class Settings:
        public QObject,
        public TaskExecutor
{
    Q_OBJECT
    Q_PROPERTY (int port READ getPort WRITE setPort NOTIFY portChanged)
    Q_PROPERTY (QString lastDir READ getLastDir WRITE setLastDir NOTIFY lastDirChanged)
    Q_PROPERTY (bool ssdpIpEnabled READ getSsdpIpEnabled WRITE setSsdpIpEnabled NOTIFY ssdpIpEnabledChanged)
    Q_PROPERTY (bool showAllDevices READ getShowAllDevices WRITE setShowAllDevices NOTIFY showAllDevicesChanged)
    Q_PROPERTY (int forwardTime READ getForwardTime WRITE setForwardTime NOTIFY forwardTimeChanged)
    Q_PROPERTY (bool imageSupported READ getImageSupported WRITE setImageSupported NOTIFY imageSupportedChanged)
    Q_PROPERTY (bool rememberPlaylist READ getRememberPlaylist WRITE setRememberPlaylist NOTIFY rememberPlaylistChanged)
    Q_PROPERTY (QStringList lastPlaylist READ getLastPlaylist WRITE setLastPlaylist NOTIFY lastPlaylistChanged)
    Q_PROPERTY (bool useDbusVolume READ getUseDbusVolume WRITE setUseDbusVolume NOTIFY useDbusVolumeChanged)

public:
    static Settings* instance();

    void setPort(int value);
    int getPort();

    void setForwardTime(int value);
    int getForwardTime();

    void setShowAllDevices(bool value);
    bool getShowAllDevices();

    void setImageSupported(bool value);
    bool getImageSupported();

    void setRememberPlaylist(bool value);
    bool getRememberPlaylist();

    void setUseDbusVolume(bool value);
    bool getUseDbusVolume();

    void setFavDevices(const QHash<QString,QVariant> &devs);
    void addFavDevice(const QString &id);
    void removeFavDevice(const QString &id);
    Q_INVOKABLE void asyncAddFavDevice(const QString &id);
    Q_INVOKABLE void asyncRemoveFavDevice(const QString &id);
    bool readDeviceXML(const QString& id, QByteArray& xml);
    QHash<QString, QVariant> getFavDevices();

    QString getLastDir();
    void setLastDir(const QString& value);

    void setSsdpIpEnabled(bool value);
    bool getSsdpIpEnabled();

    QStringList getLastPlaylist();
    void setLastPlaylist(const QStringList& value);

    QByteArray getKey();
    QByteArray resetKey();

    QString getCacheDir();
    QString getPlaylistDir();

signals:
    void portChanged();
    void favDevicesChanged();
    void lastDirChanged();
    void lastPlaylistChanged();
    void showAllDevicesChanged();
    void forwardTimeChanged();
    void imageSupportedChanged();
    void rememberPlaylistChanged();
    void useDbusVolumeChanged();
    void ssdpIpEnabledChanged();

private:
    QSettings settings;
    static Settings* inst;

    explicit Settings(QObject* parent = 0);
    bool writeDeviceXML(const QString& id, QString &url);
};

#endif // SETTINGS_H
