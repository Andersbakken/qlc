/*
  Q Light Controller
  unix-mididevice.h

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

#ifndef MIDIDEVICE_H
#define MIDIDEVICE_H

#include <QObject>
#include <QFile>

#include <alsa/asoundlib.h>

#include "common/qlctypes.h"

class MIDIDevice;
class MIDIInput;
class QString;

/*****************************************************************************
 * MIDIDevice
 *****************************************************************************/

class MIDIDevice : public QObject
{
	Q_OBJECT

public:
	MIDIDevice(MIDIInput* parent);
	virtual ~MIDIDevice();

	/*********************************************************************
 	 * ALSA address
	 *********************************************************************/
public:
	/** Get the device's ALSA client:port address */
	const snd_seq_addr_t* address() const;

	/** Set the device's ALSA client:port address */
	void setAddress(const snd_seq_addr_t* address);

protected:
	snd_seq_addr_t* m_address;

	/*********************************************************************
	 * Device info
	 *********************************************************************/
public:
	/** Get device information string to be used in plugin manager */
	QString infoText();

	/** Get the device's name */
	QString name() const;

	/**
	 * Extract the name of this device from ALSA.
	 *
	 * @return true if successful, otherwise false.
	 */
	bool extractName();

protected:
	/** The name of this ALSA MIDI device */
	QString m_name;

	/*********************************************************************
	 * Input data
	 *********************************************************************/
signals:
	/**
	 * Signal that is emitted when an input channel's value is changed
	 *
	 * @param device The eventing HIDDevice
	 * @param channel The channel whose value has changed
	 * @param value The changed value
	 */
	void valueChanged(MIDIDevice* device, t_input_channel channel,
			  t_input_value value);

public:
	/**
	 * Send an input value back the device to move motorized sliders
	 * and such.
	 */
	void feedBack(t_input_channel channel, t_input_value value);
};

#endif