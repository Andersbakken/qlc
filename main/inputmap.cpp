/*
  Q Light Controller
  inputmap.cpp
  
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

#include <QPluginLoader>
#include <QApplication>
#include <QMessageBox>
#include <QStringList>
#include <QSettings>
#include <QDebug>
#include <QList>
#include <QtXml>
#include <QDir>

#include "common/qlcinplugin.h"
#include "common/qlctypes.h"

#include "inputmap.h"
#include "app.h"

using namespace std;

extern App* _app;

/*****************************************************************************
 * InputPatch
 *****************************************************************************/

bool InputPatch::saveXML(QDomDocument* doc, QDomElement* map_root, int universe)
{
	QDomElement root;
	QDomElement tag;
	QDomText text;
	QString str;

	Q_ASSERT(doc != NULL);
	Q_ASSERT(this->plugin != NULL);

	/* Patch entry */
	root = doc->createElement(KXMLQLCInputPatch);
	map_root->appendChild(root);

	/* Universe */
	str.setNum(universe);
	root.setAttribute(KXMLQLCInputPatchUniverse, str);

	/* Plugin */
	tag = doc->createElement(KXMLQLCInputPatchPlugin);
	root.appendChild(tag);
	if (this->plugin != NULL)
		text = doc->createTextNode(this->plugin->name());
	else
		text = doc->createTextNode(KXMLQLCInputPatchPluginNone);
	tag.appendChild(text);

	/* Input */
	tag = doc->createElement(KXMLQLCInputPatchInput);
	root.appendChild(tag);
	str.setNum(this->input);
	text = doc->createTextNode(str);
	tag.appendChild(text);

	return true;
}

bool InputPatch::loader(QDomDocument*, QDomElement* root, InputMap* inputMap)
{
	QDomNode node;
	QDomElement tag;
	QString str;
	QString pluginName;
	int input = 0;
	int universe = 0;
	
	Q_ASSERT(root != NULL);
	Q_ASSERT(dmxMap != NULL);

	if (root->tagName() != KXMLQLCInputPatch)
	{
		qWarning() << "Input patch node not found!";
		return false;
	}

	/* QLC input universe that this patch has been made for */
	universe = root->attribute(KXMLQLCInputPatchUniverse).toInt();

	/* Load patch contents */
	node = root->firstChild();
	while (node.isNull() == false)
	{
		tag = node.toElement();
		
		if (tag.tagName() == KXMLQLCInputPatchPlugin)
		{
			/* Plugin name */
			pluginName = tag.text();
		}
		else if (tag.tagName() == KXMLQLCInputPatchInput)
		{
			/* Plugin input */
			input = tag.text().toInt();
		}
		else
		{
			qWarning() << "Unknown Input patch tag: "
				   << tag.tagName();
		}
		
		node = node.nextSibling();
	}

	return inputMap->setPatch(universe, pluginName, input);
}

/*****************************************************************************
 * Initialization
 *****************************************************************************/

InputMap::InputMap(int universes)
{
	m_universes = universes;

	initPatch();
	load();
}

InputMap::~InputMap()
{
	while (m_plugins.isEmpty() == false)
	{
		QLCInPlugin* plugin = m_plugins.takeFirst();
		delete plugin;
	}
}

void InputMap::load()
{
	QSettings s;
	
	QString pluginPath = s.value("directories/plugins").toString();
	if (pluginPath.isEmpty() == true)
	{
#ifdef WIN32
		pluginPath = "%%SystemRoot%%\\QLC\\Plugins";
#else
		pluginPath = "/usr/lib/qlc";
#endif
		s.setValue("directories/plugins", pluginPath);
	}

#ifdef WIN32
	QDir dir(pluginPath + "\\Input", "*.dll", QDir::Name, QDir::Files);
#else
	QDir dir(pluginPath + "/input", "*.so", QDir::Name, QDir::Files);
#endif

	/* Check that we can access the directory */
	if (dir.exists() == false)
	{
		qWarning() << "Input plugin path" << dir.absolutePath()
			   << "doesn't exist.";
		return;
	}
	else if (dir.isReadable() == false)
	{
		qWarning() << "Input plugin path" << dir.absolutePath()
			   << "is not accessible";
		return;
	}

	/* Loop thru all files in the directory */
	QStringList dirlist(dir.entryList());
	QStringList::Iterator it;
	for (it = dirlist.begin(); it != dirlist.end(); ++it)
	{
		QPluginLoader loader(dir.absoluteFilePath(*it), this);
		QLCInPlugin* plugin;

		plugin = qobject_cast<QLCInPlugin*> (loader.instance());
		if (plugin == NULL)
		{
			qWarning() << "Unable to find an input plugin in"
				   << dir.absoluteFilePath(*it);
		}
		else
		{
			plugin->init();
			appendPlugin(plugin);
			plugin->connectInputData(this);
		}
	}
}

/*****************************************************************************
 * Input data
 *****************************************************************************/

void InputMap::slotValueChanged(QLCInPlugin* plugin, t_input input,
				t_input_channel channel, t_input_value value)
{
	qDebug() << plugin->name() << input << channel << value;
}

/*****************************************************************************
 * Patch
 *****************************************************************************/

void InputMap::initPatch()
{
	InputPatch* inputPatch = NULL;
	int i = 0;

	for (i = 0; i < m_universes; i++)
	{
		inputPatch = new InputPatch(NULL, -1);
		m_patch.insert(i, inputPatch);
	}
}

bool InputMap::setPatch(unsigned int universe, const QString& pluginName,
			unsigned int input)
{
	if (int(universe) >= m_patch.size())
	{
		qWarning() << "Universe number out of bounds:" << universe
			   << "Unable to set patch.";
		return false;
	}

	QLCInPlugin* inputPlugin = plugin(pluginName);
	//m_patch[universe]->plugin->close();
	m_patch[universe]->plugin = inputPlugin;
	m_patch[universe]->input = input;
	//m_patch[universe]->plugin->open();

	return true;
}

InputPatch* InputMap::patch(int universe)
{
	Q_ASSERT(universe >= 0 && universe < KInputUniverseCount);
	return m_patch[universe];
}

/*****************************************************************************
 * Plugins
 *****************************************************************************/

QStringList InputMap::pluginNames()
{
	QListIterator <QLCInPlugin*> it(m_plugins);
	QStringList list;

	while (it.hasNext() == true)
		list.append(it.next()->name());
	
	return list;
}

int InputMap::pluginInputs(const QString& pluginName)
{
	QLCInPlugin* ip = NULL;

	ip = plugin(pluginName);
	if (ip == NULL)
		return 0;
	else
		return ip->inputs();
}

void InputMap::configurePlugin(const QString& pluginName)
{
	QLCInPlugin* inputPlugin = plugin(pluginName);
	if (inputPlugin == NULL)
		QMessageBox::warning(_app,
				     "Unable to configure plugin",
				     pluginName + QString(" not found!"));
	else
		inputPlugin->configure();
}

QString InputMap::pluginStatus(const QString& pluginName)
{
	QLCInPlugin* inputPlugin;
	QString info;

	if (pluginName != QString::null)
		inputPlugin = plugin(pluginName);
	else
		inputPlugin = NULL;
	
	if (inputPlugin == NULL)
	{
		InputPatch* inputPatch = NULL;
		int i = 0;

		// HTML header
		info += QString("<HTML>");
		info += QString("<HEAD>");
		info += QString("<TITLE>Input mapping status</TITLE>");
		info += QString("</HEAD>");
		info += QString("<BODY>");

		// Mapping status title
		info += QString("<TABLE COLS=\"1\" WIDTH=\"100%\">");
		info += QString("<TR>");
		info += QString("<TD BGCOLOR=\"");
		info += QApplication::palette()
			.color(QPalette::Highlight).name();
		info += QString("\">");
		info += QString("<FONT COLOR=\"");
		info += QApplication::palette()
			.color(QPalette::HighlightedText).name();
		info += QString("\" SIZE=\"5\">");
		info += QString("Input mapping status");
		info += QString("</FONT>");
		info += QString("</TD>");
		info += QString("</TR>");
		info += QString("</TABLE>");

		// Universe mappings
		info += QString("<TABLE COLS=\"2\" WIDTH=\"100%\">");

		for (i = 0; i < KInputUniverseCount; i++)
		{
			inputPatch = patch(i);
			Q_ASSERT(inputPatch != NULL);

			if (i % 2 == 0)
				info += QString("<TR>");
			else
			{
				info += QString("<TR BGCOLOR=\"");
				info += QApplication::palette()
					.color(QPalette::Midlight).name();
				info += QString("\">");
			}

			info += QString("<TD>");
			info += QString("<B>Universe %1</B>").arg(i + 1);
			info += QString("</TD>");

			/* Plugin name */
			info += QString("<TD>");
			if (inputPatch->plugin != NULL)
				info += inputPatch->plugin->name();
			else
				info += KInputNone;
			info += QString("</TD>");

			/* Input */
			info += QString("<TD>");
			if (inputPatch->input >= 0)
				info += QString("Input %1")
					.arg(inputPatch->input + 1);
			else
				info += KInputNone;
			info += QString("</TD>");

			info += QString("</TR>");
		}

		info += QString("</TABLE>");
	}
	else
	{
		/* Plugin-specific info */
		info = inputPlugin->infoText();
	}

	return info;
}

bool InputMap::appendPlugin(QLCInPlugin* inputPlugin)
{
	Q_ASSERT(inputPlugin != NULL);

	if (plugin(inputPlugin->name()) == NULL)
	{
		qDebug() << "Found an input plugin:" << inputPlugin->name();
		m_plugins.append(inputPlugin);
		return true;
	}
	else
	{
		qWarning() << "Input plugin" << inputPlugin->name()
			   << "is already loaded. Skipping.";
		return false;
	}
}

QLCInPlugin* InputMap::plugin(const QString& name)
{
	QListIterator <QLCInPlugin*> it(m_plugins);

	while (it.hasNext() == true)
	{
		QLCInPlugin* plugin = it.next();
		if (plugin->name() == name)
			return plugin;
	}
	
	return NULL;
}

/*****************************************************************************
 * Save & Load
 *****************************************************************************/

bool InputMap::saveXML(QDomDocument* doc, QDomElement* wksp_root)
{
	QDomElement root;
	int i = 0;

	Q_ASSERT(doc != NULL);
	Q_ASSERT(wksp_root != NULL);

	/* InputMap entry */
	root = doc->createElement(KXMLQLCInputMap);
	wksp_root->appendChild(root);

	/* Patches */
	for (i = 0; i < m_patch.size(); i++)
		m_patch[i]->saveXML(doc, &root, i);

	return true;
}

bool InputMap::loadXML(QDomDocument* doc, QDomElement* root)
{
	QDomNode node;
	QDomElement tag;
	
	Q_ASSERT(doc != NULL);
	Q_ASSERT(root != NULL);

	if (root->tagName() != KXMLQLCInputMap)
	{
		qWarning() << "InputMap node not found!";
		return false;
	}

	/* Patches */
	node = root->firstChild();
	while (node.isNull() == false)
	{
		tag = node.toElement();
		
		if (tag.tagName() == KXMLQLCInputPatch)
		{
			InputPatch::loader(doc, &tag, this);
		}
		else
		{
			qWarning() << "Unknown InputMap tag:" << tag.tagName();
		}
		
		node = node.nextSibling();
	}

	return true;
}

/*****************************************************************************
 * Defaults
 *****************************************************************************/

void InputMap::loadDefaults()
{
}

void InputMap::saveDefaults()
{
}