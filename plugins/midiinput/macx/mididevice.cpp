/*
  Q Light Controller
  mididevice.cpp

  Copyright (c) Heikki Junnila

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  Version 2 as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details. The license is
  in the file "COPYING".

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <QApplication>
#include <QSettings>
#include <QVariant>
#include <QObject>
#include <QString>
#include <QDebug>

#include "midiinputevent.h"
#include "midiprotocol.h"
#include "mididevice.h"
#include "midiinput.h"

/*****************************************************************************
 * Initialization
 *****************************************************************************/

MIDIDevice::MIDIDevice(MIDIInput* parent, MIDIEntityRef entity)
        : QObject(parent),
        m_entity(entity),
        m_source(0),
        m_destination(0),
        m_inPort(0),
        m_outPort(0),
        m_isOK(false),
        m_mode(ControlChange),
        m_midiChannel(0)
{
}

MIDIDevice::~MIDIDevice()
{
    saveSettings();
    close();
}

bool MIDIDevice::extractUID()
{
    OSStatus s;
    SInt32 uid;

    /* Get the UID property */
    s = MIDIObjectGetIntegerProperty(m_entity, kMIDIPropertyUniqueID, &uid);
    if (s == 0)
    {
        m_uid = uid;
        m_isOK = true;
        return true;
    }
    else
    {
        m_uid = -1;
        qWarning() << "Unable to get UID for MIDI entity:" << s;
        return false;
    }
}

bool MIDIDevice::extractName()
{
    CFStringRef str;
    OSStatus s;

    /* Get the name property */
    s = MIDIObjectGetStringProperty(m_entity, kMIDIPropertyModel, &str);
    if (s != 0)
    {
        qWarning() << "Unable to get manufacturer for MIDI entity:"
        << s;
        m_name = QString("Unknown %1").arg(m_uid);
    }
    else
    {
        /* Convert the name into a QString. */
        CFIndex size = CFStringGetLength(str) + 1;
        char* buf = (char*) malloc(size);
        if (CFStringGetCString(str, buf, size,
                               kCFStringEncodingISOLatin1))
        {
            m_name = QString(buf);
        }
        else
        {
            m_name = QString("Unknown %1").arg(m_uid);
        }

        free(buf);
        CFRelease(str);
    }

    return true;
}

void MIDIDevice::loadSettings()
{
    QSettings settings;
    QVariant value;
    QString key;

    /* Attempt to get a MIDI channel from settings */
    key = QString("/midiinput/%1/midichannel").arg(m_name);
    value = settings.value(key);
    if (value.isValid() == true)
        setMidiChannel(value.toInt());
    else
        setMidiChannel(0);

    /* Attempt to get the mode from settings */
    key = QString("/midiinput/%1/mode").arg(m_name);
    value = settings.value(key);
    if (value.isValid() == true)
        setMode(stringToMode(value.toString()));
    else
        setMode(ControlChange);
}

void MIDIDevice::saveSettings()
{
    QSettings settings;
    QString key;

    /* Store MIDI channel to settings */
    key = QString("/midiinput/%1/midichannel").arg(m_name);
    settings.setValue(key, m_midiChannel);

    /* Store mode to settings */
    key = QString("/midiinput/%1/mode").arg(m_name);
    settings.setValue(key, MIDIDevice::modeToString(m_mode));
}

/*****************************************************************************
 * File operations
 *****************************************************************************/

static void MidiInProc(const MIDIPacketList* pktList, void* readProcRefCon,
                       void* srcConnRefCon)
{
    Q_UNUSED(readProcRefCon);

    MIDIDevice* self = static_cast<MIDIDevice*>(srcConnRefCon);
    Q_ASSERT(self != 0);

    for (quint32 i = 0; i < pktList->numPackets; i++)
    {
        MIDIPacket packet = pktList->packet[i];

        // MIDI System Exclusive messages contain just the one sysex message
        // that we're not interested in.
        if (packet.data[0] == MIDI_SYSEX)
            continue;

        for (quint32 j = 0; j < packet.length; j++)
        {
            uchar cmd = packet.data[j];
            uchar data1 = 0;
            uchar data2 = 0;
            quint32 channel = 0;
            uchar value = 0;

            if ((++j) < packet.length)
                data1 = packet.data[j];
            if ((++j) < packet.length)
                data2 = packet.data[j];

            if (QLCMIDIProtocol::midiToInput(cmd, data1, data2,
                                             (uchar) self->midiChannel(),
                                             &channel, &value) == true)
            {
                MIDIInputEvent* event = new MIDIInputEvent(self, channel, value);
                QApplication::postEvent(self, event);
            }
        }
    }
}

bool MIDIDevice::open()
{
    MIDIInput* plugin;
    OSStatus s;

    plugin = qobject_cast<MIDIInput*> (parent());
    Q_ASSERT(plugin != 0);

    /* Don't open twice */
    if (m_inPort != 0)
        return true;

    /* Make an input port */
    s = MIDIInputPortCreate(plugin->client(), CFSTR("QLC Input Port"),
                            MidiInProc, 0, &m_inPort);
    if (s != 0)
    {
        qWarning() << "Unable to make an input port for" << name()
        << ":" << s;
        m_inPort = 0;
        m_isOK = false;
        return false;
    }

    /* Connect the input port to the first source */
    m_source = MIDIEntityGetSource(m_entity, 0);
    s = MIDIPortConnectSource(m_inPort, m_source, this);
    if (s != 0)
    {
        qWarning() << "Unable to connect input port to source for"
        << name() << ":" << s;

        s = MIDIPortDispose(m_inPort);
        if (s != 0)
            qWarning() << "Unable to dispose of port for" << name();

        m_inPort = 0;
        m_source = 0;
        m_isOK = false;

        return false;
    }

    /* If the entity has destinations, use one of them for feedback */
    if (MIDIEntityGetNumberOfDestinations(m_entity) > 0 && m_outPort == 0)
    {
        /* Make an output port */
        s = MIDIOutputPortCreate(plugin->client(),
                                 CFSTR("QLC Input FB Port"),
                                 &m_outPort);
        if (s != 0)
        {
            qWarning() << "Unable to make an output port for"
            << name() << ":" << s;
            m_outPort = 0;
            m_destination = 0;
        }
        else
        {
            /* Use the first destination */
            m_destination = MIDIEntityGetDestination(m_entity, 0);
        }
    }

    m_isOK = true;
    return true;
}

void MIDIDevice::close()
{
    OSStatus s;

    if (m_inPort != 0 && m_source != 0)
    {
        s = MIDIPortDisconnectSource(m_inPort, m_source);
        if (s != 0)
        {
            qWarning() << "Unable to disconnect input port for"
            << name();
        }

        s = MIDIPortDispose(m_inPort);
        if (s != 0)
        {
            qWarning() << "Unable to dispose of input port for"
            << name();
        }
        else
        {
            m_inPort = 0;
            m_source = 0;
        }
    }

    if (m_outPort != 0 && m_destination != 0)
    {
        s = MIDIPortDispose(m_outPort);
        if (s != 0)
        {
            qWarning() << "Unable to dispose of output port for"
            << name();
        }
        else
        {
            m_outPort = 0;
            m_destination = 0;
        }
    }
}

/*****************************************************************************
 * Device info
 *****************************************************************************/

QString MIDIDevice::infoText() const
{
    QString info;

    if (m_isOK == true)
    {
        info += QString("<B>%1</B>").arg(name());
        info += QString("<P>");
        info += QString("Device is working correctly.");
        info += QString("</P>");
        info += QString("<P>");
        info += QString("<B>MIDI Channel: </B>%1<BR>")
                .arg(midiChannel() + 1);
        info += QString("<B>Mode: </B>%1").arg(modeToString(mode()));
        info += QString("</P>");
    }
    else
    {
        info += QString("<B>Unknown device</B>");
        info += QString("<P>");
        info += QString("MIDI interface is not available.");
        info += QString("</P>");
    }

    return info;
}

/*****************************************************************************
 * Operational mode
 *****************************************************************************/

QString MIDIDevice::modeToString(Mode mode)
{
    switch (mode)
    {
    default:
    case ControlChange:
        return QString("Control Change");
        break;
    case Note:
        return QString("Note Velocity");
        break;
    }
}

MIDIDevice::Mode MIDIDevice::stringToMode(const QString& mode)
{
    if (mode == QString("Note Velocity"))
        return Note;
    else
        return ControlChange;
}

/*****************************************************************************
 * Input data
 *****************************************************************************/

void MIDIDevice::customEvent(QEvent* event)
{
    if (event->type() == MIDIInputEvent::eventType)
    {
        MIDIInputEvent* e = static_cast<MIDIInputEvent*> (event);
        emit valueChanged(e->m_channel, e->m_value);
        event->accept();
    }
}

/*****************************************************************************
 * Feedback
 *****************************************************************************/

void MIDIDevice::feedBack(quint32 channel, uchar value)
{
    /* If there's no output port or a destination, the endpoint probably
       doesn't have a MIDI IN port -> no feedback. */
    if (m_outPort == 0 || m_destination == 0)
        return;

    Byte cmd[3];
    bool d2v = false;
    if (QLCMIDIProtocol::feedbackToMidi(channel, value, midiChannel(),
                                        cmd, cmd + 1, cmd + 2, &d2v) == true)
    {
        /* Construct a MIDI packet list containing one packet */
        Byte buffer[32]; // Should be enough
        MIDIPacketList* list = (MIDIPacketList*) buffer;
        MIDIPacket* packet = MIDIPacketListInit(list);
        if (d2v == true)
            packet = MIDIPacketListAdd(list, sizeof(buffer), packet, 0, 3, cmd);
        else
            packet = MIDIPacketListAdd(list, sizeof(buffer), packet, 0, 2, cmd);
        if (packet == 0)
        {
            qWarning() << "MIDI buffer overflow";
        }
        else
        {
            /* Send the MIDI packet */
            OSStatus s = MIDISend(m_outPort, m_destination, list);
            if (s != 0)
                qWarning() << "Unable to send feedback to" << name();
        }
    }
}
