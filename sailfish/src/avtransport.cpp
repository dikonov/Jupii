/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QUrl>
#include <QThread>
#include <QGuiApplication>
#include <QTime>

#include "avtransport.h"
#include "directory.h"
#include "devicemodel.h"
#include "utils.h"
#include "contentserver.h"
#include "taskexecutor.h"


AVTransport::AVTransport(QObject *parent) :
    Service(parent),
    m_seekTimer(parent)
{
    QObject::connect(this, &AVTransport::transportStateChanged,
                     this, &AVTransport::transportStateHandler);
    QObject::connect(this, &AVTransport::currentURIChanged,
                     this, &AVTransport::trackChangedHandler);
    QObject::connect(this, &AVTransport::preControlableChanged,
                     this, &AVTransport::controlableChangedHandler);

    m_seekTimer.setInterval(500);
    m_seekTimer.setSingleShot(true);
    QObject::connect(&m_seekTimer, &QTimer::timeout, this, &AVTransport::seekTimeout);
}

void AVTransport::changed(const QString &name, const QVariant &_value)
{
    if (_value.canConvert(QVariant::Int)) {
        int value = _value.toInt();

        if (name == "TransportState") {
            if (m_transportState != value) {
                m_transportState = value;
                emit transportStateChanged();
                emit preControlableChanged();
            }
        } else if (name == "TransportStatus") {
            if (m_transportStatus != value) {
                m_transportStatus = value;
                emit transportStatusChanged();
                emit preControlableChanged();
            }
        } else if (name == "NumberOfTracks") {
            if (m_numberOfTracks != value) {
                m_numberOfTracks = value;
                emit numberOfTracksChanged();
            }
        } else if (name == "CurrentTrack") {
            if (m_currentTrack != value) {
                m_currentTrack = value;
                emit currentTrackChanged();
            }
        } else if (name == "CurrentTrackDuration") {
            if (m_currentTrackDuration != value) {
                m_currentTrackDuration = value;
                emit currentTrackDurationChanged();
            }
        } else if (name == "RelativeTimePosition") {
            if (!m_seekTimer.isActive()) {
                if (m_relativeTimePosition != value) {
                    m_relativeTimePosition = value;
                    m_oldTimePosition = value;
                    emit relativeTimePositionChanged();
                }
            }
        } else if (name == "AbsoluteTimePosition") {
            if (m_absoluteTimePosition != value) {
                m_absoluteTimePosition = value;
                emit absoluteTimePositionChanged();
            }
        }
    }

    if (_value.canConvert(QVariant::String)) {
        QString value = _value.toString();

        if (name == "AVTransportURI" || name == "CurrentTrackURI") {
            if (m_currentURI != value) {

                if (m_blockEmitUriChanged) {
                    qDebug() << "currentURI change blocked";
                    return;
                }

                m_currentURI = value;

                if (m_emitCurrentUriChanged) {
                    qDebug() << "pending currentURI change exists";
                    return;
                }

                m_emitCurrentUriChanged = true;

                startTask([this]() {
                    tsleep();
                    if (m_emitCurrentUriChanged) {
                        m_emitCurrentUriChanged = false;
                        qDebug() << "emitting currentURI change";
                        emit currentURIChanged();
                        emit preControlableChanged();
                    }
                });
            }
        }

        if (name == "NextAVTransportURI") {
            if (m_nextURI != value) {

                if (m_blockEmitUriChanged) {
                    qDebug() << "nextURI change blocked";
                    return;
                }

                m_nextURI = value;

                if (m_emitNextUriChanged) {
                    qDebug() << "pending nextURI change exists";
                    return;
                }

                m_emitNextUriChanged = true;

                qDebug() << "nextURI change delayed";

                startTask([this]() {
                    tsleep();
                    if (m_emitNextUriChanged) {
                        m_emitNextUriChanged = false;
                        qDebug() << "emiting nextURIChanged";
                        emit nextURIChanged();
                        emit transportActionsChanged();
                        //emit preControlableChanged();
                    }
                });
            }
        }
    }
}

UPnPClient::Service* AVTransport::createUpnpService(const UPnPClient::UPnPDeviceDesc &ddesc,
                                                    const UPnPClient::UPnPServiceDesc &sdesc)
{
    return new UPnPClient::AVTransport(ddesc, sdesc);
}

void AVTransport::postInit()
{
    qDebug() << "--> UPDATE postInit";
    update();
}

void AVTransport::postDeInit()
{
    qDebug() << "postDeInit";

    m_currentURI.clear();
    m_nextURI.clear();
    m_transportState = Unknown;
    m_transportStatus = TPS_Unknown;
    m_numberOfTracks = 0;
    m_currentTrack = 0;
    m_currentTrackDuration = 0;
    m_relativeTimePosition  = 0;
    m_absoluteTimePosition = 0;
    m_speed = 1;
    m_currentClass.clear();
    m_currentTitle.clear();
    m_currentAuthor.clear();
    m_currentDescription.clear();
    m_currentAlbumArtURI.clear();
    m_currentAlbum.clear();
    m_currentTransportActions = 0;
    m_futureSeek = 0;

    emit currentURIChanged();
    emit nextURIChanged();
    emit transportStateChanged();
    emit transportStatusChanged();
    emit currentTrackChanged();
    emit currentTrackDurationChanged();
    emit relativeTimePositionChanged();
    emit absoluteTimePositionChanged();
    emit speedChanged();
    emit currentMetaDataChanged();
    emit currentAlbumArtChanged();
    emit transportActionsChanged();
    emit preControlableChanged();
}


std::string AVTransport::type() const
{
    return "urn:schemas-upnp-org:service:AVTransport:1";
}

void AVTransport::timerEvent()
{
    if (!m_seekTimer.isActive()) {
        fakeUpdateRelativeTimePosition();
    }
}

UPnPClient::AVTransport* AVTransport::s()
{
    if (m_ser == nullptr) {
        qWarning() << "AVTransport service is not inited";
        //emit error(E_NotInited);
        return nullptr;
    }

    return static_cast<UPnPClient::AVTransport*>(m_ser);
}

void AVTransport::transportStateHandler()
{
    emit transportActionsChanged();
    emit preControlableChanged();
    //qDebug() << "--> aUPDATE transportStateHandler";
    asyncUpdate();
}

void AVTransport::handleApplicationStateChanged(Qt::ApplicationState state)
{
    //qDebug() << "State changed:" << state;
    //qDebug() << "--> aUPDATE handleApplicationStateChanged";

    Q_UNUSED(state)
    asyncUpdate();
}

void AVTransport::trackChangedHandler()
{
    emit transportActionsChanged();
    //qDebug() << "--> aUPDATE trackChangedHandler";
    asyncUpdate();
}

void AVTransport::controlableChangedHandler()
{
    qDebug() << "controlableChangedHandler";

    if (m_pendingControlableSignal) {
        qDebug() << "Pending controlable signal exists";
        return;
    }

    qDebug() << "New pending controlable signal";
    m_pendingControlableSignal = true;

    if (!startTask([this]() {
        tsleep(2000);
        m_pendingControlableSignal = false;
        qDebug() << "Emit delayed controlable signal";
        emit controlableChanged();
    })) {
        m_pendingControlableSignal = false;
    }
}

int AVTransport::getTransportState()
{
    return m_transportState;
}

int AVTransport::getTransportStatus()
{
    return m_transportStatus;
}

int AVTransport::getPlayMode()
{
    return m_playmode;
}

int AVTransport::getNumberOfTracks()
{
    return m_numberOfTracks;
}

int AVTransport::getCurrentTrack()
{
    return m_currentTrack;
}

int AVTransport::getCurrentTrackDuration()
{
    return m_currentTrackDuration;
}

int AVTransport::getRelativeTimePosition()
{
    return m_relativeTimePosition;
}

int AVTransport::getSpeed()
{
    return m_speed;
}

int AVTransport::getAbsoluteTimePosition()
{
    if (m_absoluteTimePosition > m_currentTrackDuration) {
        qWarning() << "Track position is greater that currentTrackDuration";
        return m_currentTrackDuration;
    }
    return m_absoluteTimePosition;
}

QString AVTransport::getCurrentURI()
{
    return m_currentURI;
}

QString AVTransport::getNextURI()
{
    return m_nextURI;
}

QString AVTransport::getCurrentPath()
{
    auto cs = ContentServer::instance();
    return cs->pathFromUrl(m_currentURI);
}

QString AVTransport::getNextPath()
{
    auto cs = ContentServer::instance();
    return cs->pathFromUrl(m_nextURI);
}

AVTransport::Type AVTransport::getCurrentType()
{
    // Reference: http://upnp.org/specs/av/UPnP-av-ContentDirectory-v1-Service.pdf

    const QStringList list = m_currentClass.split('.');

    if (list.size() > 2) {
        const QString c = list.at(2);
        if (c == "audioItem")
            return T_Audio;
        if (c == "videoItem")
            return T_Video;
        if (c == "imageItem")
            return T_Image;
    }

    qWarning() << "Unknown meta data class:" << m_currentClass;
    return T_Unknown;
}

QString AVTransport::getCurrentClass()
{
    return m_currentClass;
}

QString AVTransport::getCurrentTitle()
{
    return m_currentTitle;
}

QString AVTransport::getCurrentAuthor()
{
    return m_currentAuthor;
}

QString AVTransport::getCurrentDescription()
{
    return m_currentDescription;
}

QString AVTransport::getCurrentAlbumArtURI()
{
    return m_currentAlbumArtURI;
}

QString AVTransport::getCurrentAlbum()
{
    return m_currentAlbum;
}

bool AVTransport::getNextSupported()
{
    return (m_currentTransportActions & UPnPClient::AVTransport::TPA_Next) &&
            !m_nextURI.isEmpty();
}

bool AVTransport::getPauseSupported()
{
    return m_currentTransportActions & UPnPClient::AVTransport::TPA_Pause;
}

bool AVTransport::getPlaySupported()
{
    return (m_currentTransportActions & UPnPClient::AVTransport::TPA_Play) &&
            !m_currentURI.isEmpty();
}

bool AVTransport::getPreviousSupported()
{
    return m_currentTransportActions & UPnPClient::AVTransport::TPA_Previous;
}

bool AVTransport::getSeekSupported()
{
    if (m_transportState == Playing)
        return m_currentTransportActions & UPnPClient::AVTransport::TPA_Seek;
    else
        return false;
}

bool AVTransport::getStopSupported()
{
    return m_currentTransportActions & UPnPClient::AVTransport::TPA_Stop;
}

bool AVTransport::getPlayable()
{
    return getInited() && m_transportStatus == TPS_Ok &&
           getPlaySupported() &&
           (m_transportState == Stopped ||
            m_transportState == PausedPlayback ||
            m_transportState == PausedRecording);
}

bool AVTransport::getStopable()
{
    return getInited() && m_transportStatus == TPS_Ok &&
           m_transportState == Playing &&
           !m_currentURI.isEmpty() &&
           (getPauseSupported() || getStopSupported());
}

bool AVTransport::getControlable()
{
    return getPlayable() || getStopable();
}

void AVTransport::setLocalContent(const QString &cid, const QString &nid)
{
    //qDebug() << "1| cid, nid:" << cid << nid;

    if (!Utils::instance()->checkNetworkIf()) {
        qWarning() << "Can't find valid network interface";
        emit error(E_LostConnection);
        return;
    }

    //qDebug() << ">>> setLocalContent thread:" << QThread::currentThreadId();

    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this, cid, nid](){
        auto cs = ContentServer::instance();

        bool do_current = !cid.isEmpty();
        bool do_play = false;
        bool do_next = !nid.isEmpty();
        bool do_clearNext = !do_next;
        QUrl cURL, nURL;
        QString cmeta, nmeta, s_cURI, s_nURI;

        //qDebug() << "2| do_current, do_play, do_next, do_clearNext:" << do_current << do_play << do_next << do_clearNext;
        //qDebug() << "2| cid, nid:" << cid << nid;

        if (do_current) {

            if (!cs->getContentUrl(cid, cURL, cmeta, m_currentURI)) {
                qWarning() << "Id" << cid << "can't be converted to URL";
                emit error(E_InvalidPath);
                return;
            }

            s_cURI = cURL.toString();

            do_current = s_cURI != m_currentURI;
            if (!do_current) {
                qWarning() << "Content URL is the same as currentURI";
                if (m_transportState != Playing)
                    do_play = true;
            }
        }

        if (do_next) {

            if (!cs->getContentUrl(nid, nURL, nmeta, m_nextURI)) {
                qWarning() << "Id" << nid << "can't be converted to URL";
                emit error(E_InvalidPath);
                return;
            }

            s_nURI = nURL.toString();

            do_next = s_nURI != m_nextURI;
            if (!do_next) {
                qWarning() << "Content URL is the same as nextURI";
            } /*else {
                // Don't update nextURI if the same as currentURI
                do_next = s_nURI != m_currentURI;
                if (!do_next)
                    qWarning() << "Next content URL is the same as currentURI";
            }*/
        }

        if (do_next || do_current || do_play || do_clearNext) {
            blockUriChanged(1500);
        } else {
            qWarning() << "Nothing to update";
            return;
        }

        m_updateMutex.lock();

        auto srv = s();
        if (srv == nullptr)
            return;

        if (m_transportState != Playing) {
            if (!handleError(srv->stop())) {
                qWarning() << "Error response for stop()";
                if (srv == nullptr) {
                    m_updateMutex.unlock();
                    return;
                }
            }
            tsleep();
        }

        if (do_next) {
            qDebug() << "Calling setNextAVTransportURI with id:" << nid;
            if (!handleError(srv->setNextAVTransportURI(s_nURI.toStdString(), nmeta.toStdString()))) {
                qWarning() << "Error response for setNextAVTransportURI()";
                if (srv == nullptr) {
                    m_updateMutex.unlock();
                    return;
                }
            }
        }

        if (do_clearNext) {
            qWarning() << "Clearing nextURI";
            if (!handleError(srv->setNextAVTransportURI("", ""))) {
                qWarning() << "Error response for setNextAVTransportURI()";
                if (srv == nullptr) {
                    m_updateMutex.unlock();
                    return;
                }
            }
        }

        if (do_current) {
            // Clearing nextURI on next action on last item in the queue
            if (nid.isEmpty() && m_nextURI == s_cURI) {
                qDebug() << "Clearing nextURI";
                if (!handleError(srv->setNextAVTransportURI("", ""))) {
                    qWarning() << "Error response for setNextAVTransportURI()";
                    if (srv == nullptr) {
                        m_updateMutex.unlock();
                        return;
                    }
                }
            }

            qDebug() << "Calling setAVTransportURI with id:" << cid;
            if (!handleError(srv->setAVTransportURI(s_cURI.toStdString(),cmeta.toStdString()))) {
                qWarning() << "Error response for setAVTransportURI()";
                if (srv == nullptr) {
                    m_updateMutex.unlock();
                    return;
                }
            }
            tsleep();
            do_play = true;
        }

        if (do_play) {
            if (!handleError(srv->play())) {
                qWarning() << "Error response for play()";
                if (srv == nullptr) {
                    m_updateMutex.unlock();
                    return;
                }
            }
        }


        tsleep();

        m_updateMutex.unlock();

        //qDebug() << "--> UPDATE setLocalContent";
        update();
    });
}

void AVTransport::setSpeed(int value)
{
    if (m_speed != value) {
        m_speed = value;
        emit speedChanged();
    }
}

void AVTransport::play()
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this](){
        auto srv = s();
        if (srv == nullptr)
            return;

        m_updateMutex.lock();

        qDebug() << "Calling: play";

        if (!handleError(srv->play(m_speed))) {
            qWarning() << "Error response for play()";
            if (srv == nullptr) {
                m_updateMutex.unlock();
                return;
            }
        }

        tsleep();
        m_updateMutex.unlock();

        //qDebug() << "--> UPDATE play";
        update();
    });
}

void AVTransport::pause()
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this](){
        auto srv = s();
        if (srv == nullptr)
            return;

        m_updateMutex.lock();

        qDebug() << "Calling: pause";

        if (!handleError(srv->pause())) {
            qWarning() << "Error response for pause()";
            if (srv == nullptr) {
                m_updateMutex.unlock();
                return;
            }
        }

        tsleep();
        m_updateMutex.unlock();

        //qDebug() << "--> UPDATE pause";
        update();
    });
}

void AVTransport::stop()
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this](){
        auto srv = s();
        if (srv == nullptr)
            return;

        m_updateMutex.lock();

        qDebug() << "Calling: stop";

        if (!handleError(srv->stop())) {
            qWarning() << "Error response for stop()";
            if (srv == nullptr) {
                m_updateMutex.unlock();
                return;
            }
        }

        tsleep();
        m_updateMutex.unlock();

        //qDebug() << "--> UPDATE stop";
        update();
    });
}

void AVTransport::next()
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this](){
        auto srv = s();
        if (srv == nullptr)
            return;

        m_updateMutex.lock();

        qDebug() << "Calling: next";

        if (!handleError(srv->next())) {
            qWarning() << "Error response for next()";
            if (srv == nullptr) {
                m_updateMutex.unlock();
                return;
            }
        }

        tsleep();
        m_updateMutex.unlock();

        //qDebug() << "--> UPDATE next";
        update();
    });
}

void AVTransport::previous()
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this](){
        auto srv = s();
        if (srv == nullptr)
            return;

        m_updateMutex.lock();

        qDebug() << "Calling: previous";

        if (!handleError(srv->previous())) {
            qWarning() << "Error response for previous()";
            if (srv == nullptr) {
                m_updateMutex.unlock();
                return;
            }
        }

        tsleep();
        m_updateMutex.unlock();

        //qDebug() << "--> UPDATE previous";
        update();
    });
}

void AVTransport::setPlayMode(int value)
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this, value](){
        qDebug() << "setPlayMode:" << value;
        auto srv = s();
        if (srv == nullptr)
            return;

        m_updateMutex.lock();

        qDebug() << "Calling: setPlayMode";

        auto pm = static_cast<UPnPClient::AVTransport::PlayMode>(value);

        if (handleError(srv->setPlayMode(pm))) {
            m_updateMutex.unlock();
        } else {
            qWarning() << "Error response for setPlayMode(" << value << ")";
            m_updateMutex.unlock();
        }
    });
}

void AVTransport::seek(int value)
{
    m_futureSeek = value < 0 ? 0 :
                   value > m_currentTrackDuration ? m_currentTrackDuration : value;
    m_seekTimer.start();
}

void AVTransport::seekTimeout()
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this](){
        qDebug() << "Seek:" << m_futureSeek;
        auto srv = s();
        if (srv == nullptr)
            return;

        m_updateMutex.lock();

        qDebug() << "Calling: seek";

        if (handleError(srv->seek(UPnPClient::AVTransport::SEEK_REL_TIME,
                                  m_futureSeek))) {
            m_relativeTimePosition = m_futureSeek;
            m_oldTimePosition = m_futureSeek - 5;
            tsleep();
            m_updateMutex.unlock();

            //qDebug() << "--> UPDATE seekTimeout";
            update();
        } else {
            qWarning() << "Error response for seek(" << m_futureSeek << ")";
            m_updateMutex.unlock();
        }
    });
}

void AVTransport::fakeUpdateRelativeTimePosition()
{
    if (m_relativeTimePosition < m_currentTrackDuration) {
        if (m_oldTimePosition < m_relativeTimePosition - 5) {
            asyncUpdatePositionInfo();
        } else {
            m_relativeTimePosition++;
            emit relativeTimePositionChanged();
        }
    } else {
        qDebug() << "--> aUPDATE fakeUpdateRelativeTimePosition";
        asyncUpdate(1000);
    }
}

void AVTransport::update(int initDelay, int postDelay)
{
    qDebug() << "Update start";

    auto srv = s();

    if (srv == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    if (!m_updateMutex.tryLock()) {
        qDebug() << "Update is locked";
        return;
    }

    tsleep(initDelay);

    updatePositionInfo();
    updateTransportInfo();
    updateMediaInfo();
    updateCurrentTransportActions();
    //updateTransportSettings();

    //qDebug() << "transportState:" << m_transportState;

    tsleep(postDelay);

    //qDebug() << "getCurrentType():" << getCurrentType();
    //qDebug() << "m_transportState:" << m_transportState;
    if (getCurrentType() == T_Image && m_transportState == Playing) {
        qDebug() << "Calling stop because current track type is image";
        if (!handleError(srv->stop())) {
            qWarning() << "Error response for stop()";
            if (srv == nullptr) {
                m_updateMutex.unlock();
                return;
            }
        }
        tsleep();
    }

    qDebug() << "Update end";

    emit updated();

    needTimerCheck();

    m_updateMutex.unlock();
}

void AVTransport::needTimerCheck()
{
    auto app = static_cast<QGuiApplication*>(QGuiApplication::instance());
    //qDebug() << "applicationState:" << app->applicationState();

    if (m_transportState == Playing &&
            app->applicationState() == Qt::ApplicationActive) {
       emit needTimer(true);
    } else {
       emit needTimer(false);
    }
}

void AVTransport::asyncUpdate(int initDelay, int postDelay)
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this, initDelay, postDelay](){
        update(initDelay, postDelay);
    });
}

void AVTransport::asyncUpdatePositionInfo()
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this](){
        updatePositionInfo();
        updateCurrentTransportActions();
    });
}

void AVTransport::updatePositionInfo()
{
    auto srv = s();
    if (srv == nullptr)
        return;

    UPnPClient::AVTransport::PositionInfo pi;

    if (!handleError(srv->getPositionInfo(pi))) {
        qWarning() << "Unable to get Position Info";
        pi.abscount = 0;
        pi.abstime = 0;
        pi.relcount = 0;
        pi.reltime = 0;
        pi.track = 0;
        pi.trackduration = 0;

    }

    qDebug() << "PositionInfo:";
    qDebug() << "  track:" << pi.track;
    qDebug() << "  trackduration:" << pi.trackduration;
    qDebug() << "  reltime:" << pi.reltime;
    qDebug() << "  abstime:" << pi.abstime;
    qDebug() << "  relcount:" << pi.relcount;
    qDebug() << "  abscount:" << pi.abscount;
    qDebug() << "  trackuri:" << QString::fromStdString(pi.trackuri);
    qDebug() << "  trackmeta id:" << QString::fromStdString(pi.trackmeta.m_id);

    if (m_currentTrack != pi.track) {
        m_currentTrack = pi.track;
        emit currentTrackChanged();
    }

    if (m_absoluteTimePosition != pi.abstime) {
        m_absoluteTimePosition = pi.abstime;
        emit absoluteTimePositionChanged();
    }

    if (m_currentTrackDuration != pi.trackduration) {
        m_currentTrackDuration = pi.trackduration;
        emit currentTrackDurationChanged();
    }

    if (m_relativeTimePosition != pi.reltime) {
        m_relativeTimePosition = pi.reltime;
        m_oldTimePosition = m_relativeTimePosition;
        emit relativeTimePositionChanged();
    }
}

void AVTransport::asyncUpdateTransportInfo()
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this](){
        updateTransportInfo();
    });
}

void AVTransport::updateTransportInfo()
{
    auto srv = s();
    if (srv == nullptr)
        return;

    UPnPClient::AVTransport::TransportInfo ti;
    if (handleError(srv->getTransportInfo(ti))) {

        qDebug() << "TransportInfo:";
        qDebug() << "  tpstate:" << ti.tpstate;
        qDebug() << "  tpstatus:" << ti.tpstatus;

        if (m_transportState != ti.tpstate) {
            m_transportState = ti.tpstate;
            emit transportStateChanged();
            emit transportActionsChanged();
            emit preControlableChanged();
        }

        if (m_transportStatus != ti.tpstatus) {
            m_transportStatus = ti.tpstatus;
            emit transportStatusChanged();
            emit preControlableChanged();
        }

    } else {
        qWarning() << "Unable to get Transport Info";

        if (m_transportState != Unknown) {
            m_transportState = Unknown;
            emit transportStateChanged();
            emit transportActionsChanged();
            emit preControlableChanged();
        }

        if (m_transportStatus != TPS_Unknown) {
            m_transportStatus = TPS_Unknown;
            emit transportStatusChanged();
            emit preControlableChanged();
        }
    }
}

void AVTransport::updateTrackMeta(const UPnPClient::UPnPDirObject &trackmeta)
{
    QString id = QString::fromStdString(trackmeta.m_id);

    m_id = id;
    m_currentTitle = QString::fromStdString(trackmeta.m_title);
    m_currentClass = QString::fromStdString(trackmeta.getprop("upnp:class"));
    m_currentAuthor = QString::fromStdString(trackmeta.getprop("upnp:artist")).split(",").first();
    m_currentDescription  = QString::fromStdString(trackmeta.getprop("upnp:longDescription"));
    m_currentAlbum = QString::fromStdString(trackmeta.getprop("upnp:album"));
    emit currentMetaDataChanged();

    QString new_albumArtURI = QString::fromStdString(trackmeta.getprop("upnp:albumArtURI"));
    if (m_currentAlbumArtURI != new_albumArtURI) {
        m_currentAlbumArtURI = new_albumArtURI;
        emit currentAlbumArtChanged();
    }

    // -- debug --

    qDebug() << "Current meta:";
    qDebug() << "  id:" << QString::fromStdString(trackmeta.m_id);
    qDebug() << "  pid:" << QString::fromStdString(trackmeta.m_pid);
    qDebug() << "  title:" << QString::fromStdString(trackmeta.m_title);
    qDebug() << "  m_type:" << (trackmeta.m_type == UPnPClient::UPnPDirObject::item ? "Item" :
                                trackmeta.m_type == UPnPClient::UPnPDirObject::container ? "Container" :
                                                                                           "Unknown");
    qDebug() << "  m_iclass:" << (trackmeta.m_iclass == UPnPClient::UPnPDirObject::ITC_audioItem ? "AudioItem" :
                                  trackmeta.m_iclass == UPnPClient::UPnPDirObject::ITC_audioItem_musicTrack ? "AudioItem MusicTrack" :
                                  trackmeta.m_iclass == UPnPClient::UPnPDirObject::ITC_audioItem_playlist ? "AudioItem Playlist" :
                                  trackmeta.m_iclass == UPnPClient::UPnPDirObject::ITC_playlist ? "Playlist" :
                                  trackmeta.m_iclass == UPnPClient::UPnPDirObject::ITC_videoItem ? "VideoItem" :
                                  trackmeta.m_iclass == UPnPClient::UPnPDirObject::ITC_unknown ? "Unknown" : "Unknown other");
    qDebug() << "  props:";
    for (auto const& x : trackmeta.m_props) {
        qDebug() << "   " << QString::fromStdString(x.first) << " : " << QString::fromStdString(x.second);
    }
    qDebug() << "  resources:";
    for (auto const& x : trackmeta.m_resources) {
        qDebug() << "   " << QString::fromStdString(x.m_uri);
    }

    // ---
}

void AVTransport::asyncUpdateMediaInfo()
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this](){
        updateMediaInfo();
    });
}

void AVTransport::updateMediaInfo()
{
    auto srv = s();
    if (srv == nullptr)
        return;

    UPnPClient::AVTransport::MediaInfo mi;
    if (handleError(srv->getMediaInfo(mi))) {
        qDebug() << "MediaInfo:";
        qDebug() << "  nrtracks:" << mi.nrtracks;
        qDebug() << "  mduration:" << mi.mduration;
        qDebug() << "  cururi:" << QString::fromStdString(mi.cururi);
        qDebug() << "  nexturi:" << QString::fromStdString(mi.nexturi);
        qDebug() << "  pbstoragemed:" << QString::fromStdString(mi.pbstoragemed);
        qDebug() << "  rcstoragemed:" << QString::fromStdString(mi.rcstoragemed);
        qDebug() << "  ws:" << QString::fromStdString(mi.ws);

        if (m_numberOfTracks != mi.nrtracks) {
            m_numberOfTracks = mi.nrtracks;
            emit numberOfTracksChanged();
        }

        QString cururi = QString::fromStdString(mi.cururi);
        qDebug() << "old currentURI:" << m_currentURI;
        qDebug() << "new currentURI:" << cururi;
        if (m_currentURI != cururi) {
            m_currentURI = cururi;
            emit currentURIChanged();
            emit preControlableChanged();
        }

        QString nexturi = QString::fromStdString(mi.nexturi);
        qDebug() << "old nextURI:" << m_nextURI;
        qDebug() << "new nextURI:" << nexturi;
        if (m_nextURI != nexturi) {
            if (m_nextURI.isEmpty()) {
                m_nextURI = nexturi;
                emit nextURIChanged();
                emit transportActionsChanged();
            } else {
                m_nextURI = nexturi;
                emit nextURIChanged();
            }
        }

        updateTrackMeta(mi.curmeta);

    } else {
        qWarning() << "Unable to get Media Info";

        if (m_numberOfTracks != 0) {
            m_numberOfTracks = 0;
            emit numberOfTracksChanged();
        }

        if (!m_currentURI.isEmpty() || !m_nextURI.isEmpty()) {
            m_currentURI.clear();
            m_nextURI.clear();
            emit currentURIChanged();
            emit nextURIChanged();
            emit preControlableChanged();
            emit transportActionsChanged();
        }

        m_id.clear();
        m_currentTitle.clear();
        m_currentClass.clear();
        m_currentAuthor.clear();
        m_currentDescription.clear();
        m_currentAlbum.clear();
        emit currentMetaDataChanged();

        if (!m_currentAlbumArtURI.isEmpty()) {
            m_currentAlbumArtURI.clear();
            emit currentAlbumArtChanged();
        }
    }
}

void AVTransport::asyncUpdateCurrentTransportActions()
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this](){
        updateCurrentTransportActions();
    });
}

void AVTransport::updateCurrentTransportActions()
{
    auto srv = s();
    if (srv == nullptr)
        return;

    int ac = 0;
    if (handleError(srv->getCurrentTransportActions(ac))) {
        qDebug() << "CurrentTransportActions:";
        qDebug() << "  actions:" << ac;
        qDebug() << "  Next:" << (ac & UPnPClient::AVTransport::TPA_Next);
        qDebug() << "  Pause:" << (ac & UPnPClient::AVTransport::TPA_Pause);
        qDebug() << "  Play:" << (ac & UPnPClient::AVTransport::TPA_Play);
        qDebug() << "  Previous:" << (ac & UPnPClient::AVTransport::TPA_Previous);
        qDebug() << "  Seek:" << (ac & UPnPClient::AVTransport::TPA_Seek);
        qDebug() << "  Stop:" << (ac & UPnPClient::AVTransport::TPA_Stop);
    } else {
        qWarning() << "Unable to get Transport Actions";
    }

    if (m_currentTransportActions != ac) {
        m_currentTransportActions = ac;
        emit transportActionsChanged();
        emit preControlableChanged();
    }
}

void AVTransport::updateTransportSettings()
{
    auto srv = s();
    if (srv == nullptr)
        return;

    UPnPClient::AVTransport::TransportSettings ts;

    if (handleError(srv->getTransportSettings(ts))) {
        qDebug() << "TransportSettings:";
        qDebug() << "  playmode:" << ts.playmode;
        qDebug() << "  recqualitymode:" << QString::fromStdString(ts.recqualitymode);
    } else {
        qWarning() << "Unable to get Transport Settings";
    }

    if (m_playmode != ts.playmode) {
        m_playmode = ts.playmode;
        emit playModeChanged();
    }
}

void AVTransport::asyncUpdateTransportSettings()
{
    if (s() == nullptr) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this](){
        updateTransportSettings();
    });
}

void AVTransport::blockUriChanged(int time)
{
    m_blockEmitUriChanged = true;

    qDebug() << "URIChanged blocked";

    startTask([this, time]() {
        tsleep(time);
        m_blockEmitUriChanged = false;
        qDebug() << "URIChanged unblocked";
    });
}
