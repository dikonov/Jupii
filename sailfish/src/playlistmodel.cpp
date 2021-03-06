/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QHash>
#include <QFile>
#include <QDataStream>

#include "playlistmodel.h"
#include "utils.h"
#include "filemetadata.h"
#include "settings.h"

PlayListModel::PlayListModel(QObject *parent) :
    ListModel(new FileItem, parent)
{
}

void PlayListModel::save()
{
    QStringList ids;

    for (auto item : m_list) {
        ids << item->id();
    }

    Settings::instance()->setLastPlaylist(ids);
}

bool PlayListModel::saveToFile(const QString& title)
{
    if (m_list.isEmpty()) {
        qWarning() << "Current playlist is empty";
        return false;
    }

    const QString dir = Settings::instance()->getPlaylistDir();

    if (dir.isEmpty())
        return false;

    QString name = title.trimmed();
    if (name.isEmpty()) {
        qWarning() << "Name is empty, so using default name";
        name = tr("Playlist");
    }

    const QString oname = name;

    QString path;
    for (int i = 0; i < 10; ++i) {
        name = oname + (i > 0 ? " " + QString::number(i) : "");
        path = dir + "/" + name + ".pls";
        if (!QFileInfo::exists(path))
            break;
    }

    Utils::instance()->writeToFile(path, makePlsData(name));

    return true;
}

QByteArray PlayListModel::makePlsData(const QString& name)
{
    QByteArray data;
    QTextStream sdata(&data);

    sdata << "[playlist]" << endl;
    sdata << "X-GNOME-Title=" << name << endl;
    sdata << "NumberOfEntries=" << m_list.size() << endl;

    int l = m_list.size();
    for (int i = 0; i < l; ++i) {
        auto pitem = static_cast<FileItem*>(m_list.at(i));
        sdata << "File" << i + 1 << "="
             << QUrl::fromLocalFile(pitem->path()).toString() << endl;
        /*if (Utils::typeFromId(pitem->id()) == "a") {
            sdata << "X-Jupii-Type" << i + 1 << "=Audio" << endl;
        }*/
    }

    return data;
}

void PlayListModel::load()
{
    QStringList ids = Settings::instance()->getLastPlaylist();
    QStringList n_ids;

    for (const auto& id : ids) {
        QString type = Utils::typeFromId(id);
        if (addId(id, type == "a" ?
                  ContentServer::TypeMusic :
                  ContentServer::TypeUnknown)) {
            n_ids << id;
        }
    }

    if (!n_ids.isEmpty())
        emit itemsLoaded(n_ids);
}

int PlayListModel::getPlayMode() const
{
    return m_playMode;
}

void PlayListModel::setPlayMode(int value)
{
    if (value != m_playMode) {
        m_playMode = value;
        emit playModeChanged();
    }
}

int PlayListModel::getActiveItemIndex() const
{
    return m_activeItemIndex;
}

void PlayListModel::addItems(const QStringList& paths)
{
    addItems(paths, false);
}

void PlayListModel::addItemsAsAudio(const QStringList& paths)
{
    addItems(paths, true);
}

void PlayListModel::addItems(const QStringList& paths, bool asAudio)
{
    qDebug() << "addItems:" << paths;

    QStringList n_ids;

    for (auto& path : paths) {
        // id = path/type/cookie e.g. /home/nemo/Music/track01.mp3/-/y6Dgh
        // types: "-" - default, "a" - audio
        QString id =
                path + "/" + (asAudio ? "a" : "-") + "/" + Utils::randString();
        if (addId(id, asAudio ? ContentServer::TypeMusic :
                  ContentServer::TypeUnknown)) {
            n_ids << id;
        }
    }

    if (!n_ids.isEmpty()) {
        emit itemsAdded(n_ids);

        if (Settings::instance()->getRememberPlaylist())
            save();
    }
}

bool PlayListModel::addId(const QString& id, ContentServer::Type type)
{
    qDebug() << "addId:" << id;

    QString path = Utils::pathFromId(id);

    QFileInfo file(path);

    if (!file.exists() || !file.isFile()) {
        qWarning() << "File" << path << "doesn't exist";
        return false;
    }

    if (find(id) != 0) {
        qWarning() << "Id" << id << "already added";
        emit error(E_FileExists);
        return false;
    }

    FileMetaData metaData;
    metaData.filename = file.fileName();
    metaData.type = type == ContentServer::TypeUnknown ?
                ContentServer::instance()->getContentType(path) :
                type;
    metaData.size = file.size();

    switch (metaData.type) {
    case ContentServer::TypeDir:
        metaData.icon = "image://theme/icon-m-file-folder";
        break;
    case ContentServer::TypeImage:
        metaData.icon = "image://theme/icon-m-file-image";
        break;
    case ContentServer::TypeMusic:
        metaData.icon = "image://theme/icon-m-file-audio";
        break;
    case ContentServer::TypeVideo:
        metaData.icon = "image://theme/icon-m-file-video";
        break;
    default:
        metaData.icon = "image://theme/icon-m-file-other";
    }

    appendRow(new FileItem(id,
                           metaData.filename,
                           path,
                           metaData.date,
                           metaData.type,
                           metaData.size,
                           metaData.filename,
                           metaData.icon,
                           false,
                           false,
                           this));
    return true;
}

void PlayListModel::setActiveId(const QString &id)
{
    if (id == activeId())
        return;

    const int len = m_list.length();
    for (int i = 0; i < len; ++i) {
        auto fi = static_cast<FileItem*>(m_list.at(i));

        bool new_active = fi->id() == id;

        if (fi->active() != new_active) {
            fi->setActive(new_active);
            if(new_active)
                setActiveItemIndex(i);
            emit activeItemChanged();
        }
    }
}

void PlayListModel::resetToBeActive()
{
    const int len = m_list.length();
    for (int i = 0; i < len; ++i) {
        auto fi = static_cast<FileItem*>(m_list.at(i));
        fi->setToBeActive(false);
    }
}

void PlayListModel::setToBeActiveIndex(int i)
{
    if (i < m_list.length()) {
        auto fi = static_cast<FileItem*>(m_list.at(i));
        fi->setToBeActive(true);
    }
}

void PlayListModel::setActiveUrl(const QUrl &url)
{
    if (!url.isEmpty()) {
        auto cs = ContentServer::instance();
        setActiveId(cs->idFromUrl(url));
    }
}

void PlayListModel::clear()
{
    bool active_removed = false;
    if (m_activeItemIndex > -1) {
        auto fi = static_cast<FileItem*>(m_list.at(m_activeItemIndex));
        if (fi->active())
            active_removed = true;
    }

    if(rowCount() > 0) {
        removeRows(0, rowCount());
    }

    setActiveItemIndex(-1);

    if (Settings::instance()->getRememberPlaylist())
        save();

    if(active_removed)
        emit activeItemChanged();
}

QString PlayListModel::activeId() const
{
    if (m_activeItemIndex > -1) {
        auto fi = m_list.at(m_activeItemIndex);
        return fi->id();
    }

    return QString();
}

QString PlayListModel::firstId() const
{
    if (m_list.length() > 0)
        return m_list.first()->id();

    return QString();
}

QString PlayListModel::secondId() const
{
    if (m_list.length() > 1)
        return m_list.at(1)->id();

    return QString();
}

void PlayListModel::setActiveItemIndex(int index)
{
    if (m_activeItemIndex != index) {
        m_activeItemIndex = index;
        emit activeItemIndexChanged();
    }
}

bool PlayListModel::remove(const QString &id)
{
    int index = indexFromId(id);
    if (index < 0)
        return false;

    auto fi = static_cast<FileItem*>(m_list.at(index));

    bool active_removed = false;
    if (fi->active())
        active_removed = true;

    bool ok = removeRow(index);

    if (ok) {
        if (m_activeItemIndex > -1) {
            if (index == m_activeItemIndex)
                setActiveItemIndex(-1);
            else if (index < m_activeItemIndex)
                setActiveItemIndex(m_activeItemIndex - 1);
        }

        if (Settings::instance()->getRememberPlaylist())
            save();

        emit itemRemoved();

        if(active_removed)
            emit activeItemChanged();
    }

    return ok;
}

QString PlayListModel::nextActiveId() const
{
    if (m_activeItemIndex < 0)
        return QString();

    int l = m_list.length();
    if (l == 0)
        return QString();

    if (m_playMode == PM_RepeatOne)
        return m_list.at(m_activeItemIndex)->id();

    int nextIndex = m_activeItemIndex + 1;

    if (nextIndex < l) {
        return m_list.at(nextIndex)->id();
    } else if (m_playMode == PM_RepeatAll)
        return m_list.first()->id();

    return QString();
}

QString PlayListModel::prevActiveId() const
{
    if (m_activeItemIndex < 0)
        return QString();

    const int l = m_list.length();
    if (l == 0)
        return QString();

    if (m_playMode == PM_RepeatOne)
        return m_list.at(m_activeItemIndex)->id();

    int prevIndex = m_activeItemIndex - 1;

    if (prevIndex < 0) {
        if (m_playMode == PM_RepeatAll)
            prevIndex = l - 1;
        else
            return QString();
    }

    return m_list.at(prevIndex)->id();
}


QString PlayListModel::nextId(const QString &id) const
{
    const int l = m_list.length();
    if (l == 0)
        return QString();

    if (m_playMode == PM_RepeatOne)
        return id;

    bool nextFound = false;

    for (auto li : m_list) {
        if (nextFound)
            return li->id();
        if(li->id() == id)
            nextFound = true;
    }

    if (nextFound && m_playMode == PM_RepeatAll)
        return m_list.first()->id();

    return QString();
}

FileItem::FileItem(const QString &id,
                   const QString &name,
                   const QString &path,
                   const QString &date,
                   ContentServer::Type type,
                   const qint64 size,
                   const QString &title,
                   const QString &icon,
                   bool active,
                   bool toBeActive,
                   QObject *parent) :
    ListItem(parent),
    m_id(id),
    m_name(name),
    m_path(path),
    m_date(date),
    m_type(type),
    m_size(size),
    m_title(title),
    m_icon(icon),
    m_active(active),
    m_tobeactive(toBeActive)
{}

QHash<int, QByteArray> FileItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[PathRole] = "path";
    names[DateRole] = "date";
    names[TypeRole] = "type";
    names[SizeRole] = "size";
    names[TitleRole] = "title";
    names[IconRole] = "icon";
    names[ActiveRole] = "active";
    names[ToBeActiveRole] = "toBeActive";
    return names;
}

QVariant FileItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case NameRole:
        return name();
    case PathRole:
        return path();
    case DateRole:
        return date();
    case TypeRole:
        return type();
    case SizeRole:
        return size();
    case TitleRole:
        return title();
    case IconRole:
        return icon();
    case ActiveRole:
        return active();
    case ToBeActiveRole:
        return toBeActive();
    default:
        return QVariant();
    }
}

void FileItem::setActive(bool value)
{
    setToBeActive(false);

    if (m_active != value) {
        m_active = value;
        emit dataChanged();
    }
}

void FileItem::setToBeActive(bool value)
{
    if (m_tobeactive != value) {
        m_tobeactive = value;
        emit dataChanged();
    }
}
