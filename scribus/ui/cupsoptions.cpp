/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/
/***************************************************************************
                          cupsoptions.cpp  -  description
                             -------------------
    begin                : Fre Jan 3 2003
    copyright            : (C) 2003 by Franz Schmid
    email                : Franz.Schmid@altmuehlnet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "cupsoptions.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPixmap>
#include <QStringList>
#include <QToolTip>
#include <QSpacerItem>
#include <QPrinterInfo>
#include <QPrinter>

#include "prefsmanager.h"
#include "prefscontext.h"
#include "prefsfile.h"
#include "commonstrings.h"
#include "scconfig.h"
#ifdef HAVE_CUPS
#include <cups/cups.h>
// PPD API is deprecated in CUPS 2.x, using IPP attributes instead
// Old: #include <cups/ppd.h>
#endif
#include "iconmanager.h"

CupsOptions::CupsOptions(QWidget* parent, const QString& device) : QDialog( parent )
{
	setModal(true);
	setWindowTitle( tr( "Printer Options" ) );
	setWindowIcon(IconManager::instance().loadIcon("app-icon"));
	setSizeGripEnabled(true);

	prefs = PrefsManager::instance().prefsFile->getContext("cups_options");

	CupsOptionsLayout = new QVBoxLayout( this );
	CupsOptionsLayout->setSpacing(6);
	CupsOptionsLayout->setContentsMargins(9, 9, 9, 9);
	Table = new QTableWidget(0, 2, this);
	Table->setSortingEnabled(false);
	Table->setSelectionMode(QAbstractItemView::NoSelection);
	Table->verticalHeader()->hide();
	Table->setHorizontalHeaderItem(0, new QTableWidgetItem( tr("Option")));
	Table->setHorizontalHeaderItem(1, new QTableWidgetItem( tr("Value")));
	QHeaderView* headerH = Table->horizontalHeader();
	headerH->setStretchLastSection(true);
	headerH->setSectionsClickable(false );
	headerH->setSectionsMovable( false );
	headerH->setSectionResizeMode(QHeaderView::Fixed);
	Table->setMinimumSize(300, 100);
#ifdef HAVE_CUPS

	/* ========== OLD DEPRECATED PPD-BASED CODE (CUPS 1.x) ==========
	int i;
	cups_dest_t *dests;
	cups_dest_t *dest;
	int num_dests;
	const char *filename;	// PPD filename
	ppd_file_t *ppd = nullptr;				// PPD data
	ppd_group_t *group = nullptr;			// Current group
	num_dests = cupsGetDests(&dests);
	dest = cupsGetDest(device.toLocal8Bit().constData(), nullptr, num_dests, dests);
	if (!(dest == nullptr || (filename = cupsGetPPD(dest->name)) == nullptr || (ppd = ppdOpenFile(filename)) == nullptr))
	{
		ppdMarkDefaults(ppd);
		cupsMarkOptions(ppd, dest->num_options, dest->options);
		QStringList opts;
		QString Marked;
		m_keyToDataMap.clear();
		m_keyToDefault.clear();
		for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, ++group)
		{
			int ix;
			ppd_option_t	*option;	// Current option
			ppd_choice_t	*choice;	// Current choice
			for (ix = group->num_options, option = group->options; ix > 0; ix --, ++option)
			{
				int j;
				Marked = "";
				struct OptionData optionData;
				opts.clear();
				for (j = option->num_choices, choice = option->choices; j > 0; j --, ++choice)
				{
					opts.append(QString(choice->choice));
					if (choice->marked)
						Marked = QString(choice->choice);
				}
				if (!Marked.isEmpty())
				{
					Table->setRowCount(Table->rowCount() + 1);
					Table->setItem(Table->rowCount() - 1, 0, new QTableWidgetItem(QString(option->text)));
					QComboBox *item = new QComboBox( this );
					item->setEditable(false);
					m_optionCombos.append(item);
					optionData.comboIndex = m_optionCombos.count() - 1;
					optionData.keyword = QString(option->keyword);
					m_keyToDataMap[QString(option->text)] = optionData;
					item->addItems(opts);
					int lastSelected = prefs->getInt(QString(option->text), 0);
					if (lastSelected >= static_cast<int>(opts.count()))
						lastSelected = 0;
					item->setCurrentIndex(lastSelected);
					m_keyToDefault[QString(option->text)] = Marked;
					Table->setCellWidget(Table->rowCount() - 1, 1, item);
				}
			}
		}
		ppdClose(ppd);
		cupsFreeDests(num_dests, dests);
	}
	========== END OLD CODE ========== */
	// ========== NEW IPP-BASED CODE (CUPS 2.x) ==========
	cups_dest_t *dests = nullptr;
	cups_dest_t *dest = nullptr;
	int num_dests = 0;

	// cupsGetDests is still valid but cupsGetDests2 is recommended
	num_dests = cupsGetDests2(CUPS_HTTP_DEFAULT, &dests);
	dest = cupsGetDest(device.toLocal8Bit().constData(), nullptr, num_dests, dests);

	if (dest != nullptr)
	{
		// Use cupsCopyDestInfo instead of cupsGetPPD + ppdOpenFile
		cups_dinfo_t *dinfo = cupsCopyDestInfo(CUPS_HTTP_DEFAULT, dest);

		if (dinfo != nullptr)
		{
			QStringList opts;
			QString marked;
			m_keyToDataMap.clear();
			m_keyToDefault.clear();

			// Common IPP attributes to enumerate
			const char *ipp_attributes[] = {
				"media",              // Paper size
				"media-source",       // Paper tray/source
				"sides",              // Duplex/Simplex
				"print-quality",      // Print quality
				"print-color-mode",   // Color/Grayscale/Monochrome
				"output-bin",         // Output tray
				"finishings",         // Stapling, hole-punching, etc
				nullptr
			};

			// Enumerate all supported IPP options
			for (int attr_idx = 0; ipp_attributes[attr_idx] != nullptr; attr_idx++)
			{
				addIPPOption(ipp_attributes[attr_idx], dest, dinfo);
			}

			cupsFreeDestInfo(dinfo);
		}
		cupsFreeDests(num_dests, dests);
	}
	// ========== END NEW CODE ==========


	struct OptionData optionData;

	Table->setRowCount(Table->rowCount() + 1);
	Table->setItem(Table->rowCount() - 1, 0, new QTableWidgetItem(QString( tr("Page Set"))));
	QComboBox *item4 = new QComboBox( this );
	item4->setEditable(false);
	m_optionCombos.append(item4);
	optionData.comboIndex = m_optionCombos.count() - 1;
	optionData.keyword = "page-set";
	m_keyToDataMap["Page Set"] = optionData;
	item4->addItem( tr("All Pages"));
	item4->addItem( tr("Even Pages only"));
	item4->addItem( tr("Odd Pages only"));
	int lastSelected = prefs->getInt( tr("Page Set"), 0);
	if (lastSelected >= 3)
		lastSelected = 0;
	item4->setCurrentIndex(lastSelected);
	m_keyToDefault["Page Set"] = tr("All Pages");
	Table->setCellWidget(Table->rowCount() - 1, 1, item4);
	
	Table->setRowCount(Table->rowCount() + 1);
	Table->setItem(Table->rowCount() - 1, 0, new QTableWidgetItem(QString( tr("Mirror"))));
	QComboBox *item2 = new QComboBox( this );
	item2->setEditable(false);
	m_optionCombos.append(item2);
	optionData.comboIndex = m_optionCombos.count() - 1;
	optionData.keyword = "mirror";
	m_keyToDataMap["Mirror"] = optionData;
	item2->addItem(CommonStrings::trNo);
	item2->addItem(CommonStrings::trYes);
	item2->setCurrentIndex(0);
	lastSelected = prefs->getInt( tr("Mirror"), 0);
	if (lastSelected >= 2)
		lastSelected = 0;
	item2->setCurrentIndex(lastSelected);
	m_keyToDefault["Mirror"] = CommonStrings::trNo;
	Table->setCellWidget(Table->rowCount() - 1, 1, item2);
	
	Table->setRowCount(Table->rowCount() + 1);
	Table->setItem(Table->rowCount() - 1, 0, new QTableWidgetItem(QString( tr("Orientation"))));
	QComboBox *item5 = new QComboBox( this );
	item5->setEditable(false);
	m_optionCombos.append(item5);
	optionData.comboIndex = m_optionCombos.count() - 1;
	optionData.keyword = "orientation";
	m_keyToDataMap["Orientation"] = optionData;
	item5->addItem( tr("Portrait"));
	item5->addItem( tr("Landscape"));
	item5->setCurrentIndex(0);
	lastSelected = prefs->getInt( tr("Orientation"), 0);
	if (lastSelected >= 2)
		lastSelected = 0;
	item5->setCurrentIndex(lastSelected);
	m_keyToDefault["Orientation"] = tr("Portrait");
	Table->setCellWidget(Table->rowCount() - 1, 1, item5);
	
	Table->setRowCount(Table->rowCount() + 1);
	Table->setItem(Table->rowCount() - 1, 0, new QTableWidgetItem(QString( tr("N-Up Printing"))));
	QComboBox *item3 = new QComboBox( this );
	item3->setEditable(false);
	m_optionCombos.append(item3);
	optionData.comboIndex = m_optionCombos.count() - 1;
	optionData.keyword = "number-up";
	m_keyToDataMap["N-Up Printing"] = optionData;
	item3->addItem("1 " + tr("Page per Sheet"));
	item3->addItem("2 " + tr("Pages per Sheet"));
	item3->addItem("4 " + tr("Pages per Sheet"));
	item3->addItem("6 " + tr("Pages per Sheet"));
	item3->addItem("9 " + tr("Pages per Sheet"));
	item3->addItem("16 "+ tr("Pages per Sheet"));
	lastSelected = prefs->getInt( tr("N-Up Printing"), 0);
	if (lastSelected >= 6)
		lastSelected = 0;
	item3->setCurrentIndex(lastSelected);
	m_keyToDefault["N-Up Printing"] = "1 "+ tr("Page per Sheet");
	Table->setCellWidget(Table->rowCount() - 1, 1, item3);
#endif
	Table->resizeColumnsToContents();
	CupsOptionsLayout->addWidget( Table );

	Layout2 = new QHBoxLayout;
	Layout2->setSpacing(6);
	Layout2->setContentsMargins(0, 0, 0, 0);
	QSpacerItem* spacer = new QSpacerItem( 2, 2, QSizePolicy::Expanding, QSizePolicy::Minimum );
	Layout2->addItem( spacer );
	PushButton1 = new QPushButton( CommonStrings::tr_OK, this );
	PushButton1->setDefault( true );
	Layout2->addWidget( PushButton1 );
	PushButton2 = new QPushButton( CommonStrings::tr_Cancel, this );
	PushButton2->setDefault( false );
	PushButton1->setFocus();
	Layout2->addWidget( PushButton2 );
	CupsOptionsLayout->addLayout( Layout2 );
	setMinimumSize( sizeHint() );
	resize(minimumSizeHint().expandedTo(QSize(300, 100)));

//tooltips
	Table->setToolTip( "<qt>" + tr( "This panel displays various CUPS options when printing. The exact parameters available will depend on your printer driver. You can confirm CUPS support by selecting Help > About. Look for the listings: C-C-T These equate to C=CUPS C=littlecms T=TIFF support. Missing library support is indicated by a *." ) + "</qt>" );

    // signals and slots connections
	connect( PushButton2, SIGNAL( clicked() ), this, SLOT( reject() ) );
	connect( PushButton1, SIGNAL( clicked() ), this, SLOT( accept() ) );
}

#ifdef HAVE_CUPS
// New helper method to add IPP options to the table
void CupsOptions::addIPPOption(const char* ipp_name, cups_dest_t* dest, cups_dinfo_t* dinfo)
{
	// Find if this attribute is supported
	ipp_attribute_t *attr = cupsFindDestSupported(CUPS_HTTP_DEFAULT, dest, dinfo, ipp_name);
	if (!attr)
		return; // Not supported by this printer

	int count = ippGetCount(attr);
	if (count <= 0)
		return; // No choices available

	QStringList opts;
	QString marked;
	struct OptionData optionData;

	// Get human-readable name for the option
	QString optionName = getIPPOptionDisplayName(ipp_name);

	// Get all available choices
	for (int i = 0; i < count; i++)
	{
		const char *choice = nullptr;

		// Different value types need different getters
		ipp_tag_t value_tag = ippGetValueTag(attr);
		switch (value_tag)
		{
			case IPP_TAG_KEYWORD:
			case IPP_TAG_NAME:
			case IPP_TAG_TEXT:
			case IPP_TAG_URI:
				choice = ippGetString(attr, i, nullptr);
				break;
			case IPP_TAG_INTEGER:
			case IPP_TAG_ENUM:
				{
					int int_val = ippGetInteger(attr, i);
					choice = QString::number(int_val).toUtf8().constData();
				}
				break;
			default:
				continue; // Skip unsupported types
		}

		if (choice)
			opts.append(QString::fromUtf8(choice));
	}

	if (opts.isEmpty())
		return;

	// Get current/default value
	// First check current options set on destination
	const char *current_value = cupsGetOption(ipp_name, dest->num_options, dest->options);

	// If not set, try to get the default
	if (!current_value)
	{
		QString default_attr_name = QString("%1-default").arg(ipp_name);
		ipp_attribute_t *def_attr = cupsFindDestDefault(CUPS_HTTP_DEFAULT, dest, dinfo, default_attr_name.toUtf8().constData());
		if (def_attr)
			current_value = ippGetString(def_attr, 0, nullptr);
	}

	if (current_value)
		marked = QString::fromUtf8(current_value);
	else if (!opts.isEmpty())
		marked = opts.first(); // Use first option as default

	// Add to table
	Table->setRowCount(Table->rowCount() + 1);
	Table->setItem(Table->rowCount() - 1, 0, new QTableWidgetItem(optionName));

	QComboBox *item = new QComboBox(this);
	item->setEditable(false);
	m_optionCombos.append(item);
	optionData.comboIndex = m_optionCombos.count() - 1;
	optionData.keyword = QString::fromUtf8(ipp_name);
	m_keyToDataMap[optionName] = optionData;

	item->addItems(opts);

	// Restore previously saved selection
	int lastSelected = prefs->getInt(optionName, 0);
	if (lastSelected >= opts.count())
		lastSelected = 0;

	// Try to select the marked/default value
	if (!marked.isEmpty())
	{
		int markedIdx = opts.indexOf(marked);
		if (markedIdx >= 0)
			lastSelected = markedIdx;
	}

	item->setCurrentIndex(lastSelected);
	m_keyToDefault[optionName] = marked;
	Table->setCellWidget(Table->rowCount() - 1, 1, item);
}

// Helper to convert IPP attribute names to human-readable names
QString CupsOptions::getIPPOptionDisplayName(const char* ipp_name) const
{
	QString name(ipp_name);

	// Map common IPP names to friendly display names
	if (name == "media")
		return tr("Paper Size");
	else if (name == "media-source")
		return tr("Paper Source");
	else if (name == "sides")
		return tr("Duplex");
	else if (name == "print-quality")
		return tr("Print Quality");
	else if (name == "print-color-mode")
		return tr("Color Mode");
	else if (name == "output-bin")
		return tr("Output Tray");
	else if (name == "finishings")
		return tr("Finishing");
	else
	{
		// Convert "some-attribute-name" to "Some Attribute Name"
		name.replace('-', ' ');
		if (!name.isEmpty())
		{
			name[0] = name[0].toUpper();
			for (int i = 1; i < name.length(); i++)
			{
				if (name[i-1] == ' ')
					name[i] = name[i].toUpper();
			}
		}
		return name;
	}
}
#endif

CupsOptions::~CupsOptions()
{
	for (int i = 0; i < Table->rowCount(); ++i)
	{
		QComboBox* combo = dynamic_cast<QComboBox*>(Table->cellWidget(i, 1));
		if (combo)
			prefs->set(Table->item(i, 0)->text(), combo->currentIndex());
	}
}

QString CupsOptions::defaultOptionValue(const QString& optionKey) const
{
	QString defValue = m_keyToDefault.value(optionKey, QString());
	return defValue;
}

bool CupsOptions::useDefaultValue(const QString& optionKey) const
{
	QString defValue = m_keyToDefault.value(optionKey, QString());
	QString optValue = optionText(optionKey);
	return (optValue == defValue);
}

int CupsOptions::optionIndex(const QString& optionKey) const
{
	if (!m_keyToDataMap.contains(optionKey))
		return -1;
	const OptionData& optionData = m_keyToDataMap[optionKey];

	int comboIndex = optionData.comboIndex;
	if (comboIndex < 0 || comboIndex >= m_optionCombos.count())
		return -1;

	QComboBox* optionCombo = m_optionCombos.at(comboIndex);
	return optionCombo->currentIndex();
}

QString CupsOptions::optionText(const QString& optionKey) const
{
	if (!m_keyToDataMap.contains(optionKey))
		return QString();
	const OptionData& optionData = m_keyToDataMap[optionKey];

	int comboIndex = optionData.comboIndex;
	if (comboIndex < 0 || comboIndex >= m_optionCombos.count())
		return QString();

	QComboBox* optionCombo = m_optionCombos.at(comboIndex);
	return optionCombo->currentText();
}
