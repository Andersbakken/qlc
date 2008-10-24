/*
  Q Light Controller
  ftdidmxdevice.cpp
  
  Copyright (c) Christopher Staite
  
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

#include <sys/ioctl.h>
#include <QDebug>
#include <QThread>

#include "ftdidmxdevice.h"

/****************************************************************************
 * Initialization
 ****************************************************************************/

FTDIDMXDevice::FTDIDMXDevice(QObject* parent, char *description, int input_id,
			   t_output output) : QThread(parent)
{
	Q_ASSERT(path.isEmpty() == false);
	Q_ASSERT(output != KOutputInvalid);
	
	m_output = output;
	m_input_id = input_id;
	m_path.setNum(m_input_id);

	// Ensure we set everything to 0
	for (t_channel i = 0; i < sizeof(m_values); i++)
		m_values[i] = 0;
	m_dataChanged = true;

	m_name = QString("FTDI DMX Device: ") + QString(description);
}

FTDIDMXDevice::~FTDIDMXDevice()
{
	close();
}

/****************************************************************************
 * Properties
 ****************************************************************************/

QString FTDIDMXDevice::name() const
{
	return m_name;
}

QString FTDIDMXDevice::path() const
{
	return m_path;
}

t_output FTDIDMXDevice::output() const
{
	return m_output;
}

/****************************************************************************
 * Threading class
 ****************************************************************************/

void FTDIDMXDevice::run() {
	// Write the data to the device
	ULONG bytesWritten;
	unsigned char startCode = 0;

	// Wait for device to clear
	sleep(1000);

	while (m_threadRunning) {
		// Write data
		FT_SetBreakOn(m_handle);
		FT_SetBreakOff(m_handle);
		FT_Write(m_handle, &startCode, 1, &bytesWritten);
		FT_Write(m_handle, m_values, 512, &bytesWritten);
		sleep(30);
	}
}

/****************************************************************************
 * Open & close
 ****************************************************************************/

bool FTDIDMXDevice::open()
{
	if (FT_Open(m_input_id, &m_handle) == FT_OK)
	{
		if (!FT_SUCCESS(FT_ResetDevice(m_handle)))
			return false;

		// Set the baud rate 12 will give us 250Kbits
		if (!FT_SUCCESS(FT_SetDivisor(m_handle,12))) 
			return false;

		// Set the data characteristics
		if (!FT_SUCCESS(FT_SetDataCharacteristics(m_handle,FT_BITS_8,FT_STOP_BITS_2,FT_PARITY_NONE))) 
			return false;

		// Set flow control
	 	if (!FT_SUCCESS(FT_SetFlowControl(m_handle, FT_FLOW_NONE, NULL, NULL )))
			return false;

		// set RS485 for sendin
		FT_ClrRts(m_handle);

		// Clear TX RX buffers
		FT_Purge(m_handle,FT_PURGE_TX | FT_PURGE_RX);

		m_threadRunning = true;
		start(QThread::TimeCriticalPriority);

		return true;
	}
	else
	{
		qWarning() << QString("Unable to open FTDIDMX %1: %2")
			.arg(m_output).arg("Because the world is stupid");
		return false;
	}
}

bool FTDIDMXDevice::close()
{
	// Kill thread
	m_threadRunning = false;
	wait();

	if (FT_Close(m_handle) != FT_OK)
	{
		qWarning() << QString("Unable to close FTDIDMX %1: %2")
			.arg(m_output).arg("Because it was never open");
		return false;
	}
	else
	{
		return true;
	}
}

/****************************************************************************
 * Read & write
 ****************************************************************************/

void FTDIDMXDevice::write(t_channel channel, t_value value)
{
	m_mutex.lock();
	m_values[channel + 1] = value;
	m_dataChanged = true;
	m_mutex.unlock();
}

void FTDIDMXDevice::writeRange(t_channel address, t_value* values, t_channel num)
{
	Q_ASSERT(address + num <= 512);

	m_mutex.lock();
	memcpy(m_values + address + 1, values, num);
	m_dataChanged = true;
	m_mutex.unlock();
}

void FTDIDMXDevice::read(t_channel channel, t_value* value)
{
	m_mutex.lock();
	*value = m_values[channel + 1];
	m_dataChanged = true;
	m_mutex.unlock();
}

void FTDIDMXDevice::readRange(t_channel address, t_value* values, t_channel num)
{
	Q_ASSERT(address + num <= 512);

	m_mutex.lock();
	memcpy(values, m_values + address + 1, num);
	m_dataChanged = true;
	m_mutex.unlock();
}
