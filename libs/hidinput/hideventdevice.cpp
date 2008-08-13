/*
  Q Light Controller
  hideventdevice.cpp

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

#include <linux/input.h>
#include <errno.h>

#include <QApplication>
#include <QObject>
#include <QString>
#include <QDebug>
#include <QFile>

#include "hideventdevice.h"
#include "hidinput.h"

/**
 * This macro is used to tell if "bit" is set in "array".
 * It selects a byte from the array, and does a boolean AND
 * operation with a byte that only has the relevant bit set.
 * eg. to check for the 12th bit, we do (array[1] & 1<<4)
 */
#define test_bit(bit, array)    (array[bit / 8] & (1 << (bit % 8)))

HIDEventDevice::HIDEventDevice(HIDInput* parent, t_input line,
			       const QString& path)
	: HIDDevice(parent, line, path)
{
	init();
}

HIDEventDevice::~HIDEventDevice()
{
	setEnabled(false);

	while (m_channels.isEmpty() == false)
		delete m_channels.takeFirst();
}

void HIDEventDevice::init()
{
	if (open() == false)
		return;

	qDebug() << "*******************************************************";
	qDebug() << "Device file: " << m_file.fileName();

	/* Device name */
	char name[128] = "Unknown";
	if (ioctl(m_file.handle(), EVIOCGNAME(sizeof(name)), name) <= 0)
	{
		m_name = QString(strerror(errno));
		perror("ioctl EVIOCGNAME");
	}
	else
	{
		m_name = QString(name);
		qDebug() << "Device name:" << m_name;
	}

	/* Supported event types */
	uint8_t mask[EV_MAX/8 + 1];
	if (ioctl(m_file.handle(), EVIOCGBIT(0, sizeof(mask)), mask) <= 0)
	{
		qDebug() << "Unable to get device features:"
			 << strerror(errno);
	}
	else
	{
		getCapabilities(mask);
	}

	close();
}

void HIDEventDevice::getCapabilities(uint8_t* mask)
{
	Q_ASSERT(mask != NULL);
	Q_ASSERT(m_file.isOpen() == true);

	qDebug() << "Supported event types:";

	for (int i = 0; i < EV_MAX; i++)
	{
		if (test_bit(i, mask))
		{
			switch (i)
			{
			case EV_SYN:
				break;

			case EV_ABS:
				qDebug() << "\tAbsolute Axes";
				getAbsoluteAxesCapabilities();
				break;

			case EV_REL:
				qDebug() << "\tRelative axes";
				break;

			case EV_KEY:
				qDebug() << "\tKeys or Buttons";
				break;

			case EV_LED:
				qDebug() << "\tLEDs";
				break;

			case EV_REP:
				qDebug() << "\tRepeat";
				break;

			case EV_SW:
				qDebug() << "\tSwitches";
				break;

			case EV_SND:
				qDebug() << "\tSounds";
				break;

			case EV_MSC:
				qDebug() << "\tMiscellaneous";
				break;

			case EV_FF:
				qDebug() << "\tForce feedback";
				break;

			case EV_FF_STATUS:
				qDebug() << "\tForce feedback status";
				break;

			case EV_PWR:
				qDebug() << "\tPower";
				break;
				
			default:
				qDebug() << "\tUnknown event type: " << i;
				break;
			}
		}
	}
}

void HIDEventDevice::getAbsoluteAxesCapabilities()
{
	uint8_t mask[ABS_MAX/8 + 1];
	struct input_absinfo feats;
	int r;

	Q_ASSERT(m_file.isOpen() == true);

	memset(mask, 0, sizeof(mask));
	r = ioctl(m_file.handle(), EVIOCGBIT(EV_ABS, sizeof(mask)), mask);
	if (r < 0)
	{
		qWarning() << "\t\tUnable to get axes:" << strerror(errno);
		return;
	}

	for (int i = 0; i < ABS_MAX; i++)
	{
		if (test_bit(i, mask) == 0)
			continue;

		r = ioctl(m_file.handle(), EVIOCGABS(i), &feats);
		if (r != 0)
		{
			qWarning() << "\t\tUnable to get axes' features:"
				   << strerror(errno);
		}
		else
		{
			if (feats.maximum <= 0)
				continue;
			
			HIDEventDeviceChannel* channel;
			channel = new HIDEventDeviceChannel(i, EV_ABS,
							    feats.minimum,
							    feats.maximum);
			m_channels.append(channel);
			
			qDebug() << "\t\tChannel:" << i
				 << "min:" << feats.minimum
				 << "max:" << feats.maximum
				 << "flatness:" << feats.flat
				 << "fuzz:" << feats.fuzz;
		}
	}
}

/*****************************************************************************
 * File operations
 *****************************************************************************/

bool HIDEventDevice::open()
{
	bool result = false;

	result = m_file.open(QIODevice::Unbuffered | QIODevice::ReadWrite);
	if (result == false)
	{
		result = m_file.open(QIODevice::Unbuffered | QIODevice::ReadOnly);
		if (result == false)
		{
			qWarning() << "Unable to open" << m_file.fileName()
				   << m_file.errorString();
		}
		else
		{
			qDebug() << "Opened" << m_file.fileName()
				 << "in read only mode";
		}
	}

	return result;
}

void HIDEventDevice::close()
{
	m_file.close();
}

QString HIDEventDevice::path() const
{
	return m_file.fileName();
}

t_input_channel HIDEventDevice::channels()
{
	return m_channels.count();
}

bool HIDEventDevice::readEvent()
{
	struct input_event ev;
	HIDInputEvent* e;
	int r;

	r = read(m_file.handle(), &ev, sizeof(struct input_event));
	if (r > 0)
	{
		if (ev.type != 0 && ev.code < m_channels.count())
		{
			HIDEventDeviceChannel* ch;
			t_input_value val;

			ch = m_channels.at(ev.code);
			Q_ASSERT(ch != NULL);

			/* Scale the device's native value range to
			   0 - KInputValueMax:
			   y = (x - from_min) * (to_max / from_range) */
			val = (ev.value - ch->m_min) * 
				(KInputValueMax / (ch->m_max - ch->m_min));

			/* Post the event to the global event loop so that
			   we can switch context away from the poller thread
			   and into the main application thread. This is
			   caught in HIDInput::customEvent(). */
			e = new HIDInputEvent(this, m_line, ch->m_channel,
					      val, true);
			QApplication::postEvent(parent(), e);
		}

		return true;
	}
	else
	{
		e = new HIDInputEvent(this, 0, 0, 0, false);
		QApplication::postEvent(parent(), e);

		return false;
	}
}

/*****************************************************************************
 * Enabled status
 *****************************************************************************/

bool HIDEventDevice::isEnabled()
{
	return m_file.isOpen();
}

void HIDEventDevice::setEnabled(bool state)
{
	Q_ASSERT(parent() != NULL);

	if (isEnabled() == state)
		return;

	if (state == true)
		qobject_cast <HIDInput*> (parent())->addPollDevice(this);
	else
		qobject_cast <HIDInput*> (parent())->removePollDevice(this);
}

/*****************************************************************************
 * Device info
 *****************************************************************************/

QString HIDEventDevice::infoText()
{
	QString info;
	QString str;

	info += QString("<TR>");

	/* File name */
	info += QString("<TD>");
	info += m_file.fileName();
	info += QString("</TD>");

	/* Name */
	info += QString("<TD>");
	info += m_name;
	info += QString("</TD>");

	/* Mode */
	info += QString("<TD ALIGN=\"CENTER\">");
	info += QString("%1").arg(channels());
	info += QString("</TD>");

	info += QString("</TR>");

	return info;
}

/*****************************************************************************
 * Input data
 *****************************************************************************/

void HIDEventDevice::feedBack(t_input_channel /*channel*/,
			      t_input_value /*value*/)
{
}

