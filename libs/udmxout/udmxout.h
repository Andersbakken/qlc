/*
  Q Light Controller
  uDMXout.h
  
  Copyright (c)	2008, Lutz Hillebrand (ilLUTZminator)
  
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

#ifndef UDMXOUT_H
#define UDMXOUT_H

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "libusb_dyn.h"
#else
#include <usb.h>
#endif

#include <QStringList>
#include <QtPlugin>
#include <QMutex>
#include <QMap>

#include <QFile>

#include "common/qlcoutplugin.h"
#include "common/qlctypes.h"

enum DebugLevel {DEBUG_ERROR, DEBUG_WARN, DEBUG_INFO, DEBUG_ALL} ;

#define USBDEV_SHARED_VENDOR    0x16C0  /* VOTI */
#define USBDEV_SHARED_PRODUCT   0x05DC  /* Obdev's free shared PID */
/* Use obdev's generic shared VID/PID pair and follow the rules outlined
 * in firmware/usbdrv/USBID-License.txt. */
#define cmd_SetChannelRange 2
#define MAX_UDMX_DEVICES 1

class ConfigureUDmxOut;
class uDMXDevice;
class QString;

/*****************************************************************************
 * USBDMXOut
 *****************************************************************************/

class uDMXOut : public QObject, public QLCOutPlugin
{
	Q_OBJECT
	Q_INTERFACES(QLCOutPlugin)

	friend class ConfigureUDMXOut;

	/*********************************************************************
	 * Initialization
	 *********************************************************************/
public:
	void init();
	void open(t_output output);
	void close(t_output output);

	int usbStringGet(usb_dev_handle *dev, int index, int langid,
			 char *buf, int buflen);

	int debug() const { return m_debug; }
	//bool uDMXOut::debug(int debuglevel, char* format, ...);

protected:
	int  m_debug;
	// QFile *m_hLog;

	/*********************************************************************
	 * Devices
	 *********************************************************************/
public:
	void rescanDevices();
	QStringList outputs();
  
	int firstUniverse() const { return m_firstUniverse; }
	int channels() const { return m_channels; }

protected:
	QString m_deviceName;
	QString m_configDir;
	usb_dev_handle* m_device[MAX_UDMX_DEVICES];

	int m_firstUniverse;
	int m_numOfDevices;
	int m_LastInvalidDevice;

	/*********************************************************************
	 * Name
	 *********************************************************************/
public:
	QString name();

	/*********************************************************************
	 * Configuration
	 *********************************************************************/
public:
	void configure();
	int saveSettings();
	int loadSettings();  

	/*********************************************************************
	 * Plugin status
	 *********************************************************************/
public:
	QString infoText(t_output output = KOutputInvalid);

	/*********************************************************************
	 * Value read/write methods
	 *********************************************************************/
public:
	void writeChannel(t_output output, t_channel channel, t_value value);
	void writeRange(t_output output, t_channel address, t_value* values,
			t_channel num);

	void readChannel(t_output output, t_channel channel, t_value* value);
	void readRange(t_output output, t_channel address, t_value* values,
		       t_channel num);

protected:
	t_value m_values[KChannelMax];
	t_channel m_channels;
};

#endif