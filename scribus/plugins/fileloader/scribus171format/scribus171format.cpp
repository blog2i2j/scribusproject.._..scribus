/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/
#include "scribus171format.h"
#include "scribus171formatimpl.h"

#include <algorithm>

#include <QApplication>
#include <QByteArray>
#include <QCursor>
// #include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QStringView>

#include "../../formatidlist.h"
#include "commonstrings.h"
#include "hyphenator.h"
#include "langmgr.h"
#include "notesstyles.h"
#include "pageitem_arc.h"
#include "pageitem_latexframe.h"
#include "pageitem_noteframe.h"
#ifdef HAVE_OSG
	#include "pageitem_osgframe.h"
#endif
#include "pageitem_regularpolygon.h"
#include "pageitem_spiral.h"
#include "pageitem_table.h"
#include "pagestructs.h"
#include "pageitem_line.h"
#include "pagesize.h"
#include "prefsmanager.h"
#include "qtiocompressor.h"
#include "scclocale.h"
#include "scconfig.h"
#include "sccolorengine.h"
#include "scpattern.h"
#include "scribuscore.h"
#include "scribusdoc.h"
#include "sctextstream.h"
#include "scxmlstreamreader.h"
#include "textnote.h"
#include "undomanager.h"
#include "ui/missing.h"
#include "units.h"
#include "util.h"
#include "util_color.h"
#include "util_math.h"
#include "util_printer.h"
#include "util_text.h"

// See scplugin.h and pluginmanager.{cpp,h} for detail on what these methods
// do. That documentatation is not duplicated here.
// Please don't implement the functionality of your plugin here; do that
// in scribus171formatimpl.h and scribus171formatimpl.cpp .

Scribus171Format::Scribus171Format()
{
	// Set action info in languageChange, so we only have to do
	// it in one place. This includes registering file formats.
	registerFormats();
	languageChange();
}

Scribus171Format::~Scribus171Format()
{
	unregisterAll();
}

void Scribus171Format::languageChange()
{
	FileFormat* fmt = getFormatByID(FORMATID_SLA171IMPORT);
	fmt->trName = tr("Scribus 1.7.1+ Document");
	fmt->filter = fmt->trName + " (*.sla *.SLA *.sla.gz *.SLA.GZ *.scd *.SCD *.scd.gz *.SCD.GZ)";
}

QString Scribus171Format::fullTrName() const
{
	return QObject::tr("Scribus 1.7.1+ Support");
}

const ScActionPlugin::AboutData* Scribus171Format::getAboutData() const
{
	AboutData* about = new AboutData;
	Q_CHECK_PTR(about);
	about->authors = QString::fromUtf8(
			"Franz Schmid <franz@scribus.info>, "
			"The Scribus Team");
	about->shortDescription = tr("Scribus 1.7.1+ File Format Support");
	about->description = tr("Allows Scribus to read Scribus 1.7.1 and higher formatted files.");
	// about->version
	// about->releaseDate
	// about->copyright
	about->license = "GPL";
	return about;
}

void Scribus171Format::deleteAboutData(const AboutData* about) const
{
	Q_ASSERT(about);
	delete about;
}

void Scribus171Format::registerFormats()
{
	FileFormat fmt(this);
	fmt.trName = tr("Scribus 1.7.1+ Document");
	fmt.formatId = FORMATID_SLA171IMPORT;
	fmt.load = true;
	fmt.save = true;
	fmt.colorReading = true;
	fmt.filter = fmt.trName + " (*.sla *.SLA *.sla.gz *.SLA.GZ *.scd *.SCD *.scd.gz *.SCD.GZ)";
	fmt.mimeTypes = QStringList();
	fmt.mimeTypes.append("application/x-scribus");
	fmt.fileExtensions = QStringList() << "sla" << "sla.gz" << "scd" << "scd.gz";
	fmt.priority = 64;
	fmt.nativeScribus = true;
	registerFormat(fmt);
}

bool Scribus171Format::fileSupported(QIODevice* /* file */, const QString & fileName) const
{
	QByteArray docBytes;
	if (fileName.right(2) == "gz")
	{
		QFile file(fileName);
		QtIOCompressor compressor(&file);
		compressor.setStreamFormat(QtIOCompressor::GzipFormat);
		compressor.open(QIODevice::ReadOnly);
		docBytes = compressor.read(1024);
		compressor.close();
		if (docBytes.isEmpty())
			return false;
	}
	else
	{
		// Not gzip encoded, just load it
		loadRawBytes(fileName, docBytes, 1024);
	}

	int startElemPos = docBytes.left(512).indexOf("<SCRIBUSUTF8NEW ");
	if (startElemPos < 0)
		return false;
	QRegularExpression regExp171("Version=\"1.7.[1-9]");
	QRegularExpressionMatch match171 = regExp171.match(docBytes.mid(startElemPos, 64));
	return match171.hasMatch();
}

bool Scribus171Format::paletteSupported(QIODevice* /* file */, const QString & fileName) const
{
	QByteArray docBytes;
	if (fileName.right(2) == "gz")
	{
		QFile file(fileName);
		QtIOCompressor compressor(&file);
		compressor.setStreamFormat(QtIOCompressor::GzipFormat);
		compressor.open(QIODevice::ReadOnly);
		docBytes = compressor.read(1024);
		compressor.close();
		if (docBytes.isEmpty())
			return false;
	}
	else
	{
		// Not gzip encoded, just load it
		loadRawBytes(fileName, docBytes, 1024);
	}

	int startElemPos = docBytes.indexOf("<SCRIBUSCOLORS");
	return (startElemPos >= 0);
}

bool Scribus171Format::storySupported(const QByteArray& storyData) const
{
	int startElemPos = storyData.left(512).indexOf("<ScribusStory ");
	if (startElemPos < 0)
		return false;
	QRegularExpression regExp171("Version=\"1.7.[1-9]");
	QRegularExpressionMatch match171 = regExp171.match(storyData.mid(startElemPos, 64));
	return match171.hasMatch();
}

QIODevice* Scribus171Format::slaReader(const QString & fileName)
{
	if (!fileSupported(nullptr, fileName))
		return nullptr;

	QIODevice* ioDevice = nullptr;
	if (fileName.right(2) == "gz")
	{
		aFile.setFileName(fileName);
		QtIOCompressor *compressor = new QtIOCompressor(&aFile);
		compressor->setStreamFormat(QtIOCompressor::GzipFormat);
		if (!compressor->open(QIODevice::ReadOnly))
		{
			delete compressor;
			return nullptr;
		}
		ioDevice = compressor;
	}
	else
	{
		ioDevice = new QFile(fileName);
		if (!ioDevice->open(QIODevice::ReadOnly))
		{
			delete ioDevice;
			return nullptr;
		}
	}
	return ioDevice;
}

QIODevice* Scribus171Format::paletteReader(const QString & fileName)
{
	if (!paletteSupported(nullptr, fileName))
		return nullptr;

	QIODevice* ioDevice = nullptr;
	if (fileName.right(2) == "gz")
	{
		aFile.setFileName(fileName);
		QtIOCompressor *compressor = new QtIOCompressor(&aFile);
		compressor->setStreamFormat(QtIOCompressor::GzipFormat);
		if (!compressor->open(QIODevice::ReadOnly))
		{
			delete compressor;
			return nullptr;
		}
		ioDevice = compressor;
	}
	else
	{
		ioDevice = new QFile(fileName);
		if (!ioDevice->open(QIODevice::ReadOnly))
		{
			delete ioDevice;
			return nullptr;
		}
	}
	return ioDevice;
}

void Scribus171Format::getReplacedFontData(bool & getNewReplacement, QMap<QString,QString> &getReplacedFonts, QList<ScFace> &getDummyScFaces)
{
	getNewReplacement = false;
	getReplacedFonts.clear();
}

bool Scribus171Format::loadElements(const QString& data, const QString& fileDir, int toLayer, double Xp_in, double Yp_in, bool loc)
{
	isNewFormat = false;
	LayerToPaste = toLayer;
	Xp = Xp_in;
	Yp = Yp_in;
	GrX = 0.0;
	GrY = 0.0;

	QList<PageItem*> TableItems;
	QList<PageItem*> TableItemsF;
	QList<PageItem*> TableItemsM;
	QList<PageItem*> WeldItems;
	QMap<int, PageItem*> TableID;
	QMap<int, PageItem*> TableIDF;
	QMap<int, PageItem*> TableIDM;
	QMap<int, PageItem*> WeldID;
	QStack< QList<PageItem*> > groupStackFI;
	QStack< QList<PageItem*> > groupStackMI;
	QStack< QList<PageItem*> > groupStackPI;
	QStack< QList<PageItem*> > groupStackF;
	QStack< QList<PageItem*> > groupStackM;
	QStack< QList<PageItem*> > groupStackP;
	QStack<int> groupStackFI2;
	QStack<int> groupStackMI2;
	QStack<int> groupStackPI2;

	parStyleMap.clear();
	charStyleMap.clear();
	itemRemap.clear();
	itemNext.clear();
	itemCount = 0;
	itemRemapM.clear();
	itemNextM.clear();
	itemCountM = 0;
	itemRemapF.clear();
	itemNextF.clear();
	FrameItems.clear();
	LinkID.clear();

	markeredItemsMap.clear();
	markeredMarksMap.clear();
	nsetRangeItemNamesMap.clear();
	notesFramesData.clear();
	notesMasterMarks.clear();
	notesNSets.clear();

	bool firstElement = true;
	bool success = true;

	ReadObjectParams readObjectParams;
	readObjectParams.baseDir = fileDir;
	readObjectParams.itemKind = PageItem::StandardItem;
	readObjectParams.loadingPage = true;

	ScXmlStreamReader reader(data);
	ScXmlStreamAttributes attrs;
	while (!reader.atEnd() && !reader.hasError())
	{
		QXmlStreamReader::TokenType tType = reader.readNext();
		if (tType != QXmlStreamReader::StartElement)
			continue;
		QString tagName(reader.nameAsString());
		attrs = reader.scAttributes();

		if (firstElement)
		{
			if (tagName == QLatin1String("SCRIBUSELEM") || tagName == QLatin1String("SCRIBUSELEMUTF8"))
			{
				if (!loc)
				{
					GrX = attrs.valueAsDouble("XP");
					GrY = attrs.valueAsDouble("YP");
				}
			}
			else
			if (tagName == QLatin1String("ScribusElementUTF8"))
			{
				if (!loc)
				{
					GrX = attrs.valueAsDouble("XPosition");
					GrY = attrs.valueAsDouble("YPosition");
				}
			}
			else
			{
				success = false;
				break;
			}
			firstElement = false;
			continue;
		}

		// 10/25/2004 pv - None is "reserved" color. cannot be defined in any file...
		if (tagName == QLatin1String("COLOR") && attrs.valueAsString("NAME") != CommonStrings::None)
		{
			QString colorName = attrs.valueAsString("NAME");
			if (m_Doc->PageColors.contains(colorName))
				continue;
			readColor(m_Doc->PageColors, attrs);
		}
		else if (tagName == QLatin1String("Color") && attrs.valueAsString("Name") != CommonStrings::None)
		{
			QString colorName = attrs.valueAsString("Name");
			if (m_Doc->PageColors.contains(colorName))
				continue;
			readColor(m_Doc->PageColors, attrs);
		}
		else if (tagName == QLatin1String("Gradient"))
		{
			VGradient gra;
			QString grName = attrs.valueAsString("Name");
			success = readGradient(m_Doc, gra, reader);
			if (!success)
				break;
			gra.setRepeatMethod((VGradient::VGradientRepeatMethod)(attrs.valueAsInt("Ext", VGradient::pad)));
			if (!grName.isEmpty() && !m_Doc->docGradients.contains(grName))
				m_Doc->docGradients.insert(grName, gra);
		}
		//Remove uppercase in 1.8 format
		else if (tagName == QLatin1String("STYLE") || tagName == QLatin1String("ParagraphStyle"))
		{
			ParagraphStyle pstyle;
			getStyle(pstyle, reader, nullptr, m_Doc, true);
		}
		//Remove uppercase in 1.8 format
		else if (tagName == QLatin1String("CHARSTYLE") || tagName == QLatin1String("CharacterStyle"))
		{
			CharStyle cstyle;
			getStyle(cstyle, reader, nullptr, m_Doc, true);
		}
		else if (tagName == QLatin1String("TableStyle"))
		{
			TableStyle tstyle;
			readTableStyle(m_Doc, reader, tstyle);
			// FIXME: import style under new name if existing
			// Do not break current doc for now
			if (m_Doc->tableStyles().contains(tstyle.name()))
				continue;
			StyleSet<TableStyle> temp;
			temp.create(tstyle);
			m_Doc->redefineTableStyles(temp, false);
		}
		else if (tagName == QLatin1String("CellStyle"))
		{
			CellStyle tstyle;
			readCellStyle(m_Doc, reader, tstyle);
			// FIXME: import style under new name if existing
			// Do not break current doc for now
			if (m_Doc->cellStyles().contains(tstyle.name()))
				continue;
			StyleSet<CellStyle> temp;
			temp.create(tstyle);
			m_Doc->redefineCellStyles(temp, false);
		}
		else if (tagName == QLatin1String("Arrows"))
		{
			success = readArrows(m_Doc, attrs);
			if (!success) break;
		}
		else if (tagName == QLatin1String("MultiLine"))
		{
			MultiLine ml;
			QString mlName = attrs.valueAsString("Name");
			success = readMultiline(ml, reader);
			if (!success)
				break;
			if (!mlName.isEmpty() && !m_Doc->docLineStyles.contains(mlName))
				m_Doc->docLineStyles.insert(mlName, ml);
		}
		else if ((tagName == QLatin1String("ITEM") || tagName == QLatin1String("Item")) || (tagName == QLatin1String("PAGEOBJECT")) || (tagName == QLatin1String("FRAMEOBJECT")) || (tagName == QLatin1String("PageObject")) || (tagName == QLatin1String("FrameObject")))
		{
			ItemInfo itemInfo;
			success = readObject(m_Doc, reader, readObjectParams, itemInfo);
			if (!success)
				break;
			itemInfo.item->m_layerID = LayerToPaste;
			if (isNewFormat)
			{
				if (itemInfo.nextItem != -1)
					itemNext[itemInfo.itemID] = itemInfo.nextItem;
				if (itemInfo.item->isTableItem)
					TableItems.append(itemInfo.item);
				if (itemInfo.isWeldFlag)
					WeldItems.append(itemInfo.item);
			}
			else
			{
			// first of linked chain?
				if (tagName == QLatin1String("ITEM") || tagName == QLatin1String("Item"))
				{
					if (itemInfo.nextItem != -1)
						itemNext[itemInfo.ownNr] = itemInfo.nextItem;
				}
				if (itemInfo.item->isTableItem)
				{
					if (tagName == QLatin1String("ITEM") || tagName == QLatin1String("Item"))
					{
						TableItems.append(itemInfo.item);
						TableID.insert(itemInfo.ownLink, itemInfo.item);
					}
				}
				if (itemInfo.isWeldFlag)
				{
					if (tagName == QLatin1String("ITEM") || tagName == QLatin1String("Item"))
					{
						WeldItems.append(itemInfo.item);
						WeldID.insert(itemInfo.ownWeld, itemInfo.item);
					}
				}
			}

			if ((tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject")) && (groupStackPI.count() > 0))
			{
				groupStackPI.top().append(itemInfo.item);
				while (itemInfo.ownNr == groupStackPI2.top())
				{
					groupStackP.push(groupStackPI.pop());
					groupStackPI2.pop();
					if (groupStackPI2.count() == 0)
						break;
				}
			}
			else if ((tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject")) && (groupStackFI.count() > 0))
			{
				groupStackFI.top().append(itemInfo.item);
				while (itemInfo.ownNr == groupStackFI2.top())
				{
					groupStackF.push(groupStackFI.pop());
					groupStackFI2.pop();
					if (groupStackFI2.count() == 0)
						break;
				}
			}
			else if ((tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("MasterObject")) && (groupStackMI.count() > 0))
			{
				groupStackMI.top().append(itemInfo.item);
				while (itemInfo.ownNr == groupStackMI2.top())
				{
					groupStackM.push(groupStackMI.pop());
					groupStackMI2.pop();
					if (groupStackMI2.count() == 0)
						break;
				}
			}

			if (itemInfo.isGroupFlag)
			{
				QList<PageItem*> groupItems;
				groupItems.append(itemInfo.item);
				if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject"))
				{
					groupStackPI.push(groupItems);
					groupStackPI2.push(itemInfo.groupLastItem + itemInfo.ownNr);
				}
				else if (tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject"))
				{
					groupStackFI.push(groupItems);
					groupStackFI2.push(itemInfo.groupLastItem + itemInfo.ownNr);
				}
				else
				{
					groupStackMI.push(groupItems);
					groupStackMI2.push(itemInfo.groupLastItem + itemInfo.ownNr);
				}
			}
		}
		else if (tagName == QLatin1String("Pattern"))
		{
			success = readPattern(m_Doc, reader, fileDir);
			if (!success) break;
		}
		else if (tagName == QLatin1String("NotesStyles"))
		{
			success = readNotesStyles(m_Doc, reader);
			if (!success) break;
		}
		else if (tagName == QLatin1String("NotesFrames"))
		{
			success = readNotesFrames(reader);
			if (!success) break;
		}
		else if (tagName == QLatin1String("Notes"))
		{
			success = readNotes(m_Doc, reader);
			if (!success) break;
		}
		else if (tagName == QLatin1String("Marks"))
		{
			success = readMarks(m_Doc, reader);
			if (!success) break;
		}
		else
		{
			reader.skipCurrentElement();
		}
	}
	if (reader.hasError())
	{
		setDomParsingError(reader.errorString(), reader.lineNumber(), reader.columnNumber());
		return false;
	}
	if (isNewFormat)
	{
		if (TableItems.count() != 0)
		{
			for (int ttc = 0; ttc < TableItems.count(); ++ttc)
			{
				PageItem* ta = TableItems.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = LinkID[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = LinkID[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = LinkID[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = LinkID[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (WeldItems.count() != 0)
		{
			for (int ttc = 0; ttc < WeldItems.count(); ++ttc)
			{
				PageItem* ta = WeldItems.at(ttc);
				for (int i = 0; i < ta->weldList.count(); ++i)
				{
					PageItem::WeldingInfo wInf = ta->weldList.at(i);
					ta->weldList[i].weldItem = LinkID.value(wInf.weldID, 0);
					if (ta->weldList[i].weldItem == nullptr)
						ta->weldList.removeAt(i--);
				}
			}
		}

		//update names to pointers
		updateNames2Ptr();

		if (itemNext.count() != 0)
		{
			for (auto lc = itemNext.begin(); lc != itemNext.end(); ++lc)
			{
				if (lc.value() >= 0)
				{
					PageItem * Its = LinkID[lc.key()];
					PageItem * Itn = LinkID[lc.value()];
					if (!Its->canBeLinkedTo(Itn))
					{
						qDebug() << "scribus171format: corruption in linked textframes detected";
						continue;
					}
					Its->link(Itn);
				}
			}
		}
	}
	else
	{
		if (TableItemsF.count() != 0)
		{
			for (int ttc = 0; ttc < TableItemsF.count(); ++ttc)
			{
				PageItem* ta = TableItemsF.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = TableIDF[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = TableIDF[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = TableIDF[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = TableIDF[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (TableItemsM.count() != 0)
		{
			for (int ttc = 0; ttc < TableItemsM.count(); ++ttc)
			{
				PageItem* ta = TableItemsM.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = TableIDM[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = TableIDM[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = TableIDM[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = TableIDM[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (TableItems.count() != 0)
		{
			for (int ttc = 0; ttc < TableItems.count(); ++ttc)
			{
				PageItem* ta = TableItems.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = TableID[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = TableID[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = TableID[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = TableID[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (WeldItems.count() != 0)
		{
			for (int ttc = 0; ttc < WeldItems.count(); ++ttc)
			{
				PageItem* ta = WeldItems.at(ttc);
				for (int i = 0 ; i < ta->weldList.count(); ++i)
				{
					PageItem::WeldingInfo wInf = ta->weldList.at(i);
					ta->weldList[i].weldItem = WeldID.value(wInf.weldID, 0);
					if (ta->weldList[i].weldItem == nullptr)
						ta->weldList.removeAt(i--);
				}
			}
		}

		//update names to pointers
		updateNames2Ptr();

		// reestablish textframe links
		if (itemNext.count() != 0)
		{
			for (auto lc = itemNext.begin(); lc != itemNext.end(); ++lc)
			{
				if (lc.value() >= 0)
				{
					PageItem *Its(nullptr), *Itn(nullptr);
					if (lc.key() < m_Doc->DocItems.count())
						Its = m_Doc->DocItems.at(lc.key());
					if (lc.value() < m_Doc->DocItems.count())
						Itn = m_Doc->DocItems.at(lc.value());
					if (!Its || !Itn || !Its->canBeLinkedTo(Itn))
					{
						qDebug() << "scribus171format: corruption in linked textframes detected";
						continue;
					}
					Its->link(Itn);
				}
			}
		}
	}

	while (groupStackP.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackP.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->DocItems.removeOne(cItem);
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackP, &m_Doc->DocItems);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	while (groupStackF.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackF.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->FrameItems.remove(m_Doc->FrameItems.key(cItem));
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackF, nullptr);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	while (groupStackM.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackM.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->MasterItems.removeOne(cItem);
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackM, &m_Doc->MasterItems);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	return true;
}

bool Scribus171Format::loadStory(const QByteArray& data, StoryText& story, PageItem* item)
{
	isNewFormat = false;

	parStyleMap.clear();
	charStyleMap.clear();
	itemRemap.clear();
	itemNext.clear();
	itemCount = 0;
	itemRemapM.clear();
	itemNextM.clear();
	itemCountM = 0;
	itemRemapF.clear();
	itemNextF.clear();
	FrameItems.clear();
	LinkID.clear();

	markeredItemsMap.clear();
	markeredMarksMap.clear();
	nsetRangeItemNamesMap.clear();
	notesFramesData.clear();
	notesMasterMarks.clear();
	notesNSets.clear();

	bool firstElement = true;
	bool success = true;

	ReadObjectParams readObjectParams;
	readObjectParams.baseDir = QString() /*fileDir*/; //FIXME
	readObjectParams.itemKind = PageItem::InlineItem;
	readObjectParams.loadingPage = true;

	ScXmlStreamReader reader(data);
	ScXmlStreamAttributes attrs;
	while (!reader.atEnd() && !reader.hasError())
	{
		QXmlStreamReader::TokenType tType = reader.readNext();
		if (tType != QXmlStreamReader::StartElement)
			continue;
		QString tagName(reader.nameAsString());
		attrs = reader.scAttributes();

		if (firstElement)
		{
			if (tagName != QLatin1String("ScribusStory"))
			{
				success = false;
				break;
			}
			firstElement = false;
		}

		// 10/25/2004 pv - None is "reserved" color. cannot be defined in any file...
		if (tagName == QLatin1String("COLOR") && attrs.valueAsString("NAME") != CommonStrings::None)
		{
			QString colorName = attrs.valueAsString("NAME");
			if (m_Doc->PageColors.contains(colorName))
				continue;
			readColor(m_Doc->PageColors, attrs);
		}
		if (tagName == QLatin1String("Color") && attrs.valueAsString("Name") != CommonStrings::None)
		{
			QString colorName = attrs.valueAsString("Name");
			if (m_Doc->PageColors.contains(colorName))
				continue;
			readColor(m_Doc->PageColors, attrs);
		}
		if (tagName == QLatin1String("Gradient"))
		{
			VGradient gra;
			QString grName = attrs.valueAsString("Name");
			success = readGradient(m_Doc, gra, reader);
			if (!success)
				break;
			gra.setRepeatMethod((VGradient::VGradientRepeatMethod)(attrs.valueAsInt("Ext", VGradient::pad)));
			if (!grName.isEmpty() && !m_Doc->docGradients.contains(grName))
				m_Doc->docGradients.insert(grName, gra);
		}
		//Remove uppercase in 1.8 format
		if ((tagName == QLatin1String("ParagraphStyle")) || (tagName == QLatin1String("STYLE")))
		{
			ParagraphStyle pstyle;
			readParagraphStyle(m_Doc, reader, pstyle);
			// FIXME: import style under new name if existing
			// Do not break current doc for now
			if (m_Doc->paragraphStyles().contains(pstyle.name()))
				continue;
			StyleSet<ParagraphStyle> tmp;
			tmp.create(pstyle);
			m_Doc->redefineStyles(tmp, false);
		}
		//Remove uppercase in 1.8 format
		if (tagName == QLatin1String("CharacterStyle") || tagName == QLatin1String("CHARSTYLE"))
		{
			CharStyle cstyle;
			ScXmlStreamAttributes attrs = reader.scAttributes();
			readNamedCharacterStyleAttrs(m_Doc, attrs, cstyle);
			// FIXME: import style under new name if existing
			// Do not break current doc for now
			if (m_Doc->charStyles().contains(cstyle.name()))
				continue;
			StyleSet<CharStyle> temp;
			temp.create(cstyle);
			m_Doc->redefineCharStyles(temp, false);
		}
		if (tagName == QLatin1String("TableStyle"))
		{
			TableStyle tstyle;
			readTableStyle(m_Doc, reader, tstyle);
			// FIXME: import style under new name if existing
			// Do not break current doc for now
			if (m_Doc->tableStyles().contains(tstyle.name()))
				continue;
			StyleSet<TableStyle> temp;
			temp.create(tstyle);
			m_Doc->redefineTableStyles(temp, false);
		}
		if (tagName == QLatin1String("CellStyle"))
		{
			CellStyle tstyle;
			readCellStyle(m_Doc, reader, tstyle);
			// FIXME: import style under new name if existing
			// Do not break current doc for now
			if (m_Doc->cellStyles().contains(tstyle.name()))
				continue;
			StyleSet<CellStyle> temp;
			temp.create(tstyle);
			m_Doc->redefineCellStyles(temp, false);
		}
		if (tagName == QLatin1String("Arrows"))
		{
			success = readArrows(m_Doc, attrs);
			if (!success) break;
		}
		if (tagName == QLatin1String("MultiLine"))
		{
			MultiLine ml;
			QString mlName = attrs.valueAsString("Name");
			success = readMultiline(ml, reader);
			if (!success)
				break;
			if (!mlName.isEmpty() && !m_Doc->docLineStyles.contains(mlName))
				m_Doc->docLineStyles.insert(mlName, ml);
		}
		if (tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject"))
		{
			ItemInfo itemInfo;
			success = readObject(m_Doc, reader, readObjectParams, itemInfo);
			if (!success)
				break;
			itemInfo.item->m_layerID = LayerToPaste;
		}
		if (tagName == QLatin1String("StoryText"))
		{
			readStoryText(m_Doc, reader, story, item);
		}
		if (tagName == QLatin1String("Pattern")) // FIXME
		{
			/*success = readPattern(m_Doc, reader, fileDir);
			if (!success) break;*/
		}
		if (tagName == QLatin1String("NotesStyles"))
		{
			success = readNotesStyles(m_Doc, reader);
			if (!success) break;
		}
		if (tagName == QLatin1String("Notes"))
		{
			success = readNotes(m_Doc, reader);
			if (!success) break;
		}
		if (tagName == QLatin1String("Marks"))
		{
			success = readMarks(m_Doc, reader);
			if (!success) break;
		}
	}

	if (reader.hasError())
	{
		setDomParsingError(reader.errorString(), reader.lineNumber(), reader.columnNumber());
		return false;
	}

	//update names to pointers
	updateNames2Ptr();

	return true;
}

bool Scribus171Format::loadPalette(const QString & fileName)
{
	if (m_Doc == nullptr || m_AvailableFonts == nullptr)
	{
		Q_ASSERT(m_Doc == nullptr || m_AvailableFonts == nullptr);
		return false;
	}

	Xp = 0.0;
	Yp = 0.0;
	GrX = 0.0;
	GrY = 0.0;
	isNewFormat = false;

	QMap<int, PageItem*> TableID;
	QMap<int, PageItem*> TableIDM;
	QMap<int, PageItem*> TableIDF;
	QList<PageItem*> TableItems;
	QList<PageItem*> TableItemsM;
	QList<PageItem*> TableItemsF;
	QMap<int, PageItem*> WeldID;
	QList<PageItem*> WeldItems;
	QStack< QList<PageItem*> > groupStackFI;
	QStack< QList<PageItem*> > groupStackMI;
	QStack< QList<PageItem*> > groupStackPI;
	QStack< QList<PageItem*> > groupStackF;
	QStack< QList<PageItem*> > groupStackM;
	QStack< QList<PageItem*> > groupStackP;
	QStack<int> groupStackFI2;
	QStack<int> groupStackMI2;
	QStack<int> groupStackPI2;

	QScopedPointer<QIODevice> ioDevice(paletteReader(fileName));
	if (ioDevice.isNull())
	{
		setFileReadError();
		return false;
	}
	QString fileDir = QFileInfo(fileName).absolutePath();

	if (m_mwProgressBar != nullptr)
	{
		m_mwProgressBar->setMaximum(ioDevice->size());
		m_mwProgressBar->setValue(0);
	}

	itemRemap.clear();
	itemNext.clear();
	itemCount = 0;
	itemRemapM.clear();
	itemNextM.clear();
	itemCountM = 0;
	itemRemapF.clear();
	itemNextF.clear();

	TableItems.clear();
	TableID.clear();
	TableItemsM.clear();
	TableIDM.clear();
	TableItemsF.clear();
	TableIDF.clear();
	WeldItems.clear();

	m_Doc->GroupCounter = 1;
	m_Doc->LastAuto = nullptr;
//	m_Doc->PageColors.clear();
//	m_Doc->Layers.clear();

	bool firstElement = true;
	bool success = true;
	int  progress = 0;

	ReadObjectParams readObjectParams;
	readObjectParams.baseDir = fileDir;
	readObjectParams.itemKind = PageItem::StandardItem;
	readObjectParams.loadingPage = false;

	ScXmlStreamReader reader(ioDevice.data());
	ScXmlStreamAttributes attrs;
	while (!reader.atEnd() && !reader.hasError())
	{
		QXmlStreamReader::TokenType tType = reader.readNext();
		if (tType != QXmlStreamReader::StartElement)
			continue;
		QString tagName(reader.nameAsString());
		attrs = reader.scAttributes();

		if (m_mwProgressBar != nullptr)
		{
			int newProgress = qRound(ioDevice->pos() / (double) ioDevice->size() * 100);
			if (newProgress != progress)
			{
				m_mwProgressBar->setValue(reader.characterOffset());
				progress = newProgress;
			}
		}

		if (firstElement)
		{
			if (tagName != QLatin1String("SCRIBUSCOLORS") && tagName != QLatin1String("ScribusColors"))
			{
				success = false;
				break;
			}
			firstElement = false;
		}
		// 10/25/2004 pv - None is "reserved" color. cannot be defined in any file...
		//Remove uppercase in 1.8
		if (tagName == QLatin1String("COLOR") && attrs.valueAsString("NAME") != CommonStrings::None)
			readColor(m_Doc->PageColors, attrs);
		if (tagName == QLatin1String("Color") && attrs.valueAsString("Name") != CommonStrings::None)
			readColor(m_Doc->PageColors, attrs);
		if (tagName == QLatin1String("Gradient"))
		{
			VGradient gra;
			QString grName = attrs.valueAsString("Name");
			success = readGradient(m_Doc, gra, reader);
			if (!success)
				break;
			gra.setRepeatMethod((VGradient::VGradientRepeatMethod)(attrs.valueAsInt("Ext", VGradient::pad)));
			if (!grName.isEmpty())
			{
				m_Doc->docGradients.insert(grName, gra);
			}
		}
		if (tagName == QLatin1String("Arrows"))
		{
			success = readArrows(m_Doc, attrs);
			if (!success) break;
		}
		if (tagName == QLatin1String("MultiLine"))
		{
			MultiLine ml;
			QString mlName = attrs.valueAsString("Name");
			success = readMultiline(ml, reader);
			if (!success) break;
			if (!mlName.isEmpty())
			{
				m_Doc->docLineStyles.insert(mlName, ml);
			}
		}
		if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("FRAMEOBJECT") ||
				tagName == QLatin1String("PageObject") || tagName == QLatin1String("MasterObject") || tagName == QLatin1String("FrameObject"))
		{
			ItemInfo itemInfo;
			success = readObject(m_Doc, reader, readObjectParams, itemInfo);
			if (!success) break;

			if (isNewFormat)
			{
				if (itemInfo.nextItem != -1)
					itemNext[itemInfo.itemID] = itemInfo.nextItem;
				if (itemInfo.item->isTableItem)
					TableItems.append(itemInfo.item);
				if (itemInfo.isWeldFlag)
					WeldItems.append(itemInfo.item);
			}
			else
			{
				// first of linked chain?
				if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject"))
				{
					if (itemInfo.nextItem != -1)
						itemNext[itemInfo.ownNr] = itemInfo.nextItem;
				}
				else if (tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("MasterObject"))
				{
					if (itemInfo.nextItem != -1)
						itemNextM[itemInfo.ownNr] = itemInfo.nextItem;
				}
				if (itemInfo.item->isTableItem)
				{
					if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject"))
					{
						TableItems.append(itemInfo.item);
						TableID.insert(itemInfo.ownLink, itemInfo.item);
					}
					else if (tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject"))
					{
						TableItemsF.append(itemInfo.item);
						TableIDF.insert(itemInfo.ownLink, itemInfo.item);
					}
					else
					{
						TableItemsM.append(itemInfo.item);
						TableIDM.insert(itemInfo.ownLink, itemInfo.item);
					}
				}
				if (itemInfo.isWeldFlag)
				{
					WeldItems.append(itemInfo.item);
					WeldID.insert(itemInfo.ownWeld, itemInfo.item);
				}
			}

			if ((tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject")) && (groupStackPI.count() > 0))
			{
				groupStackPI.top().append(itemInfo.item);
				while (itemInfo.ownNr == groupStackPI2.top())
				{
					groupStackP.push(groupStackPI.pop());
					groupStackPI2.pop();
					if (groupStackPI2.count() == 0)
						break;
				}
			}
			else if ((tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject")) && (groupStackFI.count() > 0))
			{
				groupStackFI.top().append(itemInfo.item);
				while (itemInfo.ownNr == groupStackFI2.top())
				{
					groupStackF.push(groupStackFI.pop());
					groupStackFI2.pop();
					if (groupStackFI2.count() == 0)
						break;
				}
			}
			else if ((tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("MasterObject")) && (groupStackMI.count() > 0))
			{
				groupStackMI.top().append(itemInfo.item);
				while (itemInfo.ownNr == groupStackMI2.top())
				{
					groupStackM.push(groupStackMI.pop());
					groupStackMI2.pop();
					if (groupStackMI2.count() == 0)
						break;
				}
			}

			if (itemInfo.isGroupFlag)
			{
				QList<PageItem*> groupItems;
				groupItems.append(itemInfo.item);
				if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject"))
				{
					groupStackPI.push(groupItems);
					groupStackPI2.push(itemInfo.groupLastItem + itemInfo.ownNr);
				}
				else if (tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject"))
				{
					groupStackFI.push(groupItems);
					groupStackFI2.push(itemInfo.groupLastItem + itemInfo.ownNr);
				}
				else
				{
					groupStackMI.push(groupItems);
					groupStackMI2.push(itemInfo.groupLastItem + itemInfo.ownNr);
				}
			}
		}
		if (tagName == QLatin1String("Pattern"))
		{
			success = readPattern(m_Doc, reader, fileDir);
			if (!success) break;
		}
		if (tagName == QLatin1String("NotesStyles"))
		{
			success = readNotesStyles(m_Doc, reader);
			if (!success) break;
		}
		if (tagName == QLatin1String("NotesFrames"))
		{
			success = readNotesFrames(reader);
			if (!success) break;
		}
		if (tagName == QLatin1String("Notes"))
		{
			success = readNotes(m_Doc, reader);
			if (!success) break;
		}
		if (tagName == QLatin1String("Marks"))
		{
			success = readMarks(m_Doc, reader);
			if (!success) break;
		}
	}

	if (reader.hasError())
	{
		setDomParsingError(reader.errorString(), reader.lineNumber(), reader.columnNumber());
		return false;
	}
	if (isNewFormat)
	{
		if (TableItems.count() != 0)
		{
			for (int ttc = 0; ttc < TableItems.count(); ++ttc)
			{
				PageItem* ta = TableItems.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = LinkID[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = LinkID[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = LinkID[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = LinkID[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (WeldItems.count() != 0)
		{
			for (int ttc = 0; ttc < WeldItems.count(); ++ttc)
			{
				PageItem* ta = WeldItems.at(ttc);
				for (int i = 0 ; i < ta->weldList.count(); ++i)
				{
					PageItem::WeldingInfo wInf = ta->weldList.at(i);
					ta->weldList[i].weldItem = LinkID.value(wInf.weldID, 0);
					if (ta->weldList[i].weldItem == nullptr)
						ta->weldList.removeAt(i--);
				}
			}
		}
		if (itemNext.count() != 0)
		{
			for (auto lc = itemNext.begin(); lc != itemNext.end(); ++lc)
			{
				if (lc.value() >= 0)
				{
					PageItem * Its = LinkID[lc.key()];
					PageItem * Itn = LinkID[lc.value()];
					if (!Its->canBeLinkedTo(Itn))
					{
						qDebug() << "scribus171format: corruption in linked textframes detected";
						continue;
					}
					Its->link(Itn);
				}
			}
		}
	}
	else
	{
		if (TableItemsF.count() != 0)
		{
			for (int ttc = 0; ttc < TableItemsF.count(); ++ttc)
			{
				PageItem* ta = TableItemsF.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = TableIDF[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = TableIDF[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = TableIDF[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = TableIDF[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (TableItemsM.count() != 0)
		{
			for (int ttc = 0; ttc < TableItemsM.count(); ++ttc)
			{
				PageItem* ta = TableItemsM.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = TableIDM[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = TableIDM[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = TableIDM[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = TableIDM[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (TableItems.count() != 0)
		{
			for (int ttc = 0; ttc < TableItems.count(); ++ttc)
			{
				PageItem* ta = TableItems.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = TableID[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = TableID[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = TableID[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = TableID[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (WeldItems.count() != 0)
		{
			for (int ttc = 0; ttc < WeldItems.count(); ++ttc)
			{
				PageItem* ta = WeldItems.at(ttc);
				for (int i = 0 ; i < ta->weldList.count(); ++i)
				{
					PageItem::WeldingInfo wInf = ta->weldList.at(i);
					ta->weldList[i].weldItem = WeldID.value(wInf.weldID, 0);
					if (ta->weldList[i].weldItem == nullptr)
						ta->weldList.removeAt(i--);
				}
			}
		}
		// reestablish textframe links
		if (itemNext.count() != 0)
		{
			for (auto lc = itemNext.begin(); lc != itemNext.end(); ++lc)
			{
				if (lc.value() >= 0)
				{
					PageItem *Its(nullptr), *Itn(nullptr);
					if (lc.key() < m_Doc->DocItems.count())
						Its = m_Doc->DocItems.at(lc.key());
					if (lc.value() < m_Doc->DocItems.count())
						Itn = m_Doc->DocItems.at(lc.value());
					if (!Its || !Itn || !Its->canBeLinkedTo(Itn))
					{
						qDebug() << "scribus171format: corruption in linked textframes detected";
						continue;
					}
					Its->link(Itn);
				}
			}
		}
		if (itemNextM.count() != 0)
		{
			for (auto lc = itemNextM.begin(); lc != itemNextM.end(); ++lc)
			{
				if (lc.value() >= 0)
				{
					PageItem *Its(nullptr), *Itn(nullptr);
					if (lc.key() < m_Doc->MasterItems.count())
						Its = m_Doc->MasterItems.at(lc.key());
					if (lc.value() < m_Doc->MasterItems.count())
						Itn = m_Doc->MasterItems.at(lc.value());
					if (!Its || !Itn || !Its->canBeLinkedTo(Itn))
					{
						qDebug() << "scribus171format: corruption in linked textframes detected";
						continue;
					}
					Its->link(Itn);
				}
			}
		}
	}

	while (groupStackP.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackP.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->DocItems.removeOne(cItem);
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackP, &m_Doc->DocItems);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	while (groupStackF.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackF.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->FrameItems.remove(m_Doc->FrameItems.key(cItem));
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackF, nullptr);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	while (groupStackM.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackM.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->MasterItems.removeOne(cItem);
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackM, &m_Doc->MasterItems);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	if (m_Doc->Layers.count() == 0)
		m_Doc->Layers.newLayer( QObject::tr("Background") );
	if (m_mwProgressBar != nullptr)
	{
		m_mwProgressBar->setValue(reader.characterOffset());
		m_mwProgressBar->reset();
	}
	return true;
}

bool Scribus171Format::loadFile(const QString & fileName, const FileFormat & /* fmt */, int /* flags */, int /* index */)
{
	if (m_Doc == nullptr || m_AvailableFonts == nullptr)
	{
		Q_ASSERT(m_Doc == nullptr || m_AvailableFonts == nullptr);
		return false;
	}

	Xp = 0.0;
	Yp = 0.0;
	GrX = 0.0;
	GrY = 0.0;
	struct ScribusDoc::BookMa bok;
	QMap<int, ScribusDoc::BookMa> bookmarks;

	isNewFormat = false;

	QMap<int, PageItem*> TableID;
	QMap<int, PageItem*> TableIDM;
	QMap<int, PageItem*> TableIDF;
	QList<PageItem*> TableItems;
	QList<PageItem*> TableItemsM;
	QList<PageItem*> TableItemsF;
	QMap<int, PageItem*> WeldID;
	QList<PageItem*> WeldItems;
	QStack< QList<PageItem*> > groupStackFI;
	QStack< QList<PageItem*> > groupStackMI;
	QStack< QList<PageItem*> > groupStackPI;
	QStack< QList<PageItem*> > groupStackF;
	QStack< QList<PageItem*> > groupStackM;
	QStack< QList<PageItem*> > groupStackP;
	QStack<int> groupStackFI2;
	QStack<int> groupStackMI2;
	QStack<int> groupStackPI2;
	
	markeredItemsMap.clear();
	markeredMarksMap.clear();
	nsetRangeItemNamesMap.clear();
	notesFramesData.clear();
	notesMasterMarks.clear();
	notesNSets.clear();

	QScopedPointer<QIODevice> ioDevice(slaReader(fileName));
	if (ioDevice.isNull())
	{
		setFileReadError();
		return false;
	}
	QString fileDir = QFileInfo(fileName).absolutePath();
	int firstPage = 0;
	int layerToSetActive = 0;
	
	if (m_mwProgressBar != nullptr)
	{
		m_mwProgressBar->setMaximum(ioDevice->size());
		m_mwProgressBar->setValue(0);
	}
	// Stop autosave timer,it will be restarted only if doc has autosave feature is enabled
	if (m_Doc->autoSaveTimer->isActive())
		m_Doc->autoSaveTimer->stop();

	parStyleMap.clear();
	charStyleMap.clear();
	itemRemap.clear();
	itemNext.clear();
	itemCount = 0;
	itemRemapM.clear();
	itemNextM.clear();
	itemCountM = 0;
	itemRemapF.clear();
	itemNextF.clear();

	FrameItems.clear();
	LinkID.clear();

	m_Doc->GroupCounter = 1;
	m_Doc->LastAuto = nullptr;
	m_Doc->PageColors.clear();
	m_Doc->Layers.clear();

	ReadObjectParams readObjectParams;
	readObjectParams.baseDir = fileDir;
	readObjectParams.itemKind = PageItem::StandardItem;
	readObjectParams.loadingPage = false;

	bool firstElement = true;
	bool success = true;
	bool hasPageSets = false;
	int  progress = 0;

	ScXmlStreamReader reader(ioDevice.data());
	ScXmlStreamAttributes attrs;
	while (!reader.atEnd() && !reader.hasError())
	{
		QXmlStreamReader::TokenType tType = reader.readNext();
		if (tType != QXmlStreamReader::StartElement)
			continue;
		QString tagName(reader.nameAsString());
		attrs = reader.scAttributes();

		if (m_mwProgressBar != nullptr)
		{
			int newProgress = qRound(ioDevice->pos() / (double) ioDevice->size() * 100);
			if (newProgress != progress)
			{
				m_mwProgressBar->setValue(reader.characterOffset());
				progress = newProgress;
			}
		}

		if (firstElement)
		{
			if (tagName != QLatin1String("SCRIBUSUTF8NEW"))
			{
				success = false;
				break;
			}
			firstElement = false;
			continue;
		}
		//Remove uppercase in 1.8
		if (tagName == QLatin1String("DOCUMENT") || tagName == QLatin1String("Document"))
		{
			readDocAttributes(m_Doc, attrs);
			layerToSetActive = attrs.valueAsInt("ActiveLayer", 0);
			if (m_Doc->pagePositioning() == 0)
				firstPage = 0;
			else
			{
				if (attrs.valueAsInt("FIRSTLEFT", 0) == 1)
					firstPage = 0;
				else
					firstPage = 1;
			}
			//Remove lower case in 1.8
			if (attrs.hasAttribute("currentProfile"))
			{
				m_Doc->clearCheckerProfiles();
				m_Doc->setCurCheckProfile(attrs.valueAsString("currentProfile"));
			}
			else if (attrs.hasAttribute("CurrentProfile"))
			{
				m_Doc->clearCheckerProfiles();
				m_Doc->setCurCheckProfile(attrs.valueAsString("CurrentProfile"));
			}
		}
		else if (tagName == QLatin1String("CheckProfile"))
		{
			success = readCheckProfile(m_Doc, attrs);
			if (!success) break;
		}
		else if (tagName == QLatin1String("PageSets"))
		{
			success = readPageSets(m_Doc, reader);
			if (!success) break;
			hasPageSets = true;
		}
		// 10/25/2004 pv - None is "reserved" color. cannot be defined in any file...
		else if (tagName == QLatin1String("COLOR") && attrs.valueAsString("NAME") != CommonStrings::None)
			readColor(m_Doc->PageColors, attrs);
		else if (tagName == QLatin1String("Color") && attrs.valueAsString("Name") != CommonStrings::None)
			readColor(m_Doc->PageColors, attrs);
		else if (tagName == QLatin1String("Gradient"))
		{
			VGradient gra;
			QString grName = attrs.valueAsString("Name");
			success = readGradient(m_Doc, gra, reader);
			if (!success)
				break;
			gra.setRepeatMethod((VGradient::VGradientRepeatMethod)(attrs.valueAsInt("Ext", VGradient::pad)));
			if (!grName.isEmpty())
				m_Doc->docGradients.insert(grName, gra);
		}
		//Remove uppercase in 1.8 format
		else if ((tagName == QLatin1String("ParagraphStyle")) || (tagName == QLatin1String("STYLE")))
		{
			ParagraphStyle pstyle;
			readParagraphStyle(m_Doc, reader, pstyle);
			StyleSet<ParagraphStyle>tmp;
			tmp.create(pstyle);
			m_Doc->redefineStyles(tmp, false);
		}
		//Remove uppercase in 1.8 format
		else if (tagName == QLatin1String("CharacterStyle") || tagName == QLatin1String("CHARSTYLE"))
		{
			CharStyle cstyle;
			ScXmlStreamAttributes attrs = reader.scAttributes();
			readNamedCharacterStyleAttrs(m_Doc, attrs, cstyle);
			StyleSet<CharStyle> temp;
			temp.create(cstyle);
			m_Doc->redefineCharStyles(temp, false);
		}
		else if (tagName == QLatin1String("TableStyle"))
		{
			TableStyle tstyle;
			readTableStyle(m_Doc, reader, tstyle);
			StyleSet<TableStyle> temp;
			temp.create(tstyle);
			m_Doc->redefineTableStyles(temp, false);
		}
		else if (tagName == QLatin1String("CellStyle"))
		{
			CellStyle tstyle;
			readCellStyle(m_Doc, reader, tstyle);
			StyleSet<CellStyle> temp;
			temp.create(tstyle);
			m_Doc->redefineCellStyles(temp, false);
		}
		//Remove uppercase in 1.8 format
		else if (tagName == QLatin1String("JAVA"))
		{
			QString name = attrs.valueAsString("NAME");
			if (!name.isEmpty())
				m_Doc->JavaScripts[name] = attrs.valueAsString("SCRIPT");
		}
		else if (tagName == QLatin1String("Java"))
		{
			QString name = attrs.valueAsString("Name");
			if (!name.isEmpty())
				m_Doc->JavaScripts[name] = attrs.valueAsString("Script");
		}
		//Remove uppercase in 1.8 format
		else if ((tagName == QLatin1String("LAYERS")) || (tagName == QLatin1String("Layers")))
		{
			ScLayer newLayer;
			readLayers(newLayer, attrs);
			m_Doc->Layers.append(newLayer);
		}
		else if (tagName == QLatin1String("Arrows"))
		{
			success = readArrows(m_Doc, attrs);
			if (!success) break;
		}
		else if (tagName == QLatin1String("MultiLine"))
		{
			MultiLine ml;
			QString mlName = attrs.valueAsString("Name");
			success = readMultiline(ml, reader);
			if (!success) break;
			if (!mlName.isEmpty())
			{
				m_Doc->docLineStyles.insert(mlName, ml);
			}
		}
		else if (tagName == QLatin1String("Bookmark"))
		{
			int bmElem = 0;
			struct ScribusDoc::BookMa bookmark;
			success = readBookMark(bookmark, bmElem, attrs);
			if (!success) break;
			bookmarks.insert(bmElem, bookmark);
		}
		else if (tagName == QLatin1String("PDF"))
		{
			success = readPDFOptions(m_Doc, reader);
			if (!success) break;
		}
		else if (tagName == QLatin1String("Printer"))
		{
			success = readPrinterOptions(m_Doc, reader);
			if (!success) break;
		}
		else if (tagName == QLatin1String("DocItemAttributes"))
		{
			success = readDocItemAttributes(m_Doc, reader);
			if (!success) break;
		}
		else if (tagName == QLatin1String("Indexes"))
		{
			success = readIndexes(m_Doc, reader);
			if (!success) break;
		}
		else if (tagName == QLatin1String("TablesOfContents"))
		{
			success = readTableOfContents(m_Doc, reader);
			if (!success) break;
		}
		else if (tagName == QLatin1String("Sections"))
		{
			success = readSections(m_Doc, reader);
			if (!success) break;
		}
		//Remove uppercase in 1.8 format
		else if (tagName == QLatin1String("Hyphenator") || tagName == QLatin1String("HYPHEN"))
		{
			success = readHyphen(m_Doc, reader);
			if (!success) break;
		}
		//Remove uppercase in 1.8 format
		else if (tagName == QLatin1String("PAGE") || tagName == QLatin1String("MASTERPAGE"))
		{
			success = readPage(m_Doc, reader);
			if (!success)
				break;
		}
		else if (tagName == QLatin1String("Page") || tagName == QLatin1String("MasterPage"))
		{
			success = readPage(m_Doc, reader);
			if (!success)
				break;
		}
		else if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("FRAMEOBJECT") ||
				 tagName == QLatin1String("PageObject") || tagName == QLatin1String("MasterObject") || tagName == QLatin1String("FrameObject"))
		{
			ItemInfo itemInfo;
			success = readObject(m_Doc, reader, readObjectParams, itemInfo);
			if (!success)
				break;

			// first of linked chain?
			if (isNewFormat)
			{
				if (itemInfo.nextItem != -1)
					itemNext[itemInfo.itemID] = itemInfo.nextItem;
				if (itemInfo.item->isTableItem)
					TableItems.append(itemInfo.item);
				if (itemInfo.isWeldFlag)
					WeldItems.append(itemInfo.item);
			}
			else
			{
				if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject"))
				{
					if (itemInfo.nextItem != -1)
						itemNext[itemInfo.ownNr] = itemInfo.nextItem;
				}
				else if (tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("MasterObject"))
				{
					if (itemInfo.nextItem != -1)
						itemNextM[itemInfo.ownNr] = itemInfo.nextItem;
				}
				if (itemInfo.item->isTableItem)
				{
					if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject"))
					{
						TableItems.append(itemInfo.item);
						TableID.insert(itemInfo.ownLink, itemInfo.item);
					}
					else if (tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject"))
					{
						TableItemsF.append(itemInfo.item);
						TableIDF.insert(itemInfo.ownLink, itemInfo.item);
					}
					else
					{
						TableItemsM.append(itemInfo.item);
						TableIDM.insert(itemInfo.ownLink, itemInfo.item);
					}
				}

				if (itemInfo.isWeldFlag)
				{
					WeldItems.append(itemInfo.item);
					WeldID.insert(itemInfo.ownWeld, itemInfo.item);
				}
			}

			if ((tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject")) && (groupStackPI.count() > 0))
			{
				groupStackPI.top().append(itemInfo.item);
				while (itemInfo.ownNr == groupStackPI2.top())
				{
					groupStackP.push(groupStackPI.pop());
					groupStackPI2.pop();
					if (groupStackPI2.count() == 0)
						break;
				}
			}
			else if ((tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject")) && (groupStackFI.count() > 0))
			{
				groupStackFI.top().append(itemInfo.item);
				while (itemInfo.ownNr == groupStackFI2.top())
				{
					groupStackF.push(groupStackFI.pop());
					groupStackFI2.pop();
					if (groupStackFI2.count() == 0)
						break;
				}
			}
			else if ((tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("MasterObject")) && (groupStackMI.count() > 0))
			{
				groupStackMI.top().append(itemInfo.item);
				while (itemInfo.ownNr == groupStackMI2.top())
				{
					groupStackM.push(groupStackMI.pop());
					groupStackMI2.pop();
					if (groupStackMI2.count() == 0)
						break;
				}
			}

			if (itemInfo.isGroupFlag)
			{
				QList<PageItem*> groupItems;
				groupItems.append(itemInfo.item);
				if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject"))
				{
					groupStackPI.push(groupItems);
					groupStackPI2.push(itemInfo.groupLastItem + itemInfo.ownNr);
				}
				else if (tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject"))
				{
					groupStackFI.push(groupItems);
					groupStackFI2.push(itemInfo.groupLastItem + itemInfo.ownNr);
				}
				else
				{
					groupStackMI.push(groupItems);
					groupStackMI2.push(itemInfo.groupLastItem + itemInfo.ownNr);
				}
			}
		}
		else if (tagName == QLatin1String("Pattern"))
		{
			success = readPattern(m_Doc, reader, fileDir);
			if (!success)
				break;
		}
		else if (tagName == QLatin1String("NotesStyles"))
		{
			success = readNotesStyles(m_Doc, reader);
			if (!success) break;
			m_Doc->setUsesMarksAndNotes(true);
		}
		else if (tagName == QLatin1String("NotesFrames"))
		{
			success = readNotesFrames(reader);
			if (!success) break;
			m_Doc->setUsesMarksAndNotes(true);
		}
		else if (tagName == QLatin1String("Notes"))
		{
			success = readNotes(m_Doc, reader);
			if (!success) break;
			m_Doc->setUsesMarksAndNotes(true);
		}
		else if (tagName == QLatin1String("Marks"))
		{
			success = readMarks(m_Doc, reader);
			if (!success) break;
			m_Doc->setUsesMarksAndNotes(true);
		}
		else if (tagName == QLatin1String("OpticalMarginSets"))
		{
			success = readOpticalMarginSets(m_Doc, reader);
			if (!success) break;
		}
		else
		{
			reader.skipCurrentElement();
		}
	}

	if (reader.hasError())
	{
		setDomParsingError(reader.errorString(), reader.lineNumber(), reader.columnNumber());
		return false;
	}

	for (auto it = bookmarks.begin(); it != bookmarks.end(); ++it)
	{
		int elem = it.key();
		PageItem* item = LinkID.value(elem, (PageItem*) nullptr);
		if (!item)
			continue;
		ScribusDoc::BookMa bookmark = it.value();
		bookmark.PageObject = item;
		m_Doc->BookMarks.append( bookmark );
	}
	std::stable_sort(m_Doc->BookMarks.begin(), m_Doc->BookMarks.end());

	if (isNewFormat)
	{
		if (TableItems.count() != 0)
		{
			for (int ttc = 0; ttc < TableItems.count(); ++ttc)
			{
				PageItem* ta = TableItems.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = LinkID[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = LinkID[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = LinkID[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = LinkID[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (WeldItems.count() != 0)
		{
			for (int ttc = 0; ttc < WeldItems.count(); ++ttc)
			{
				PageItem* ta = WeldItems.at(ttc);
				for (int i = 0 ; i < ta->weldList.count(); ++i)
				{
					PageItem::WeldingInfo wInf = ta->weldList.at(i);
					ta->weldList[i].weldItem = LinkID.value(wInf.weldID, 0);
					if (ta->weldList[i].weldItem == nullptr)
						ta->weldList.removeAt(i--);
				}
			}
		}
		if (itemNext.count() != 0)
		{
			for (auto lc = itemNext.begin(); lc != itemNext.end(); ++lc)
			{
				if (lc.value() >= 0)
				{
					PageItem * Its = LinkID[lc.key()];
					PageItem * Itn = LinkID[lc.value()];
					if (!Its->canBeLinkedTo(Itn))
					{
						qDebug() << "scribus171format: corruption in linked textframes detected";
						continue;
					}
					Its->link(Itn);
				}
			}
		}
	}
	else
	{
		if (TableItemsF.count() != 0)
		{
			for (int ttc = 0; ttc < TableItemsF.count(); ++ttc)
			{
				PageItem* ta = TableItemsF.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = TableIDF[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = TableIDF[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = TableIDF[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = TableIDF[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (TableItemsM.count() != 0)
		{
			for (int ttc = 0; ttc < TableItemsM.count(); ++ttc)
			{
				PageItem* ta = TableItemsM.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = TableIDM[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = TableIDM[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = TableIDM[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = TableIDM[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (TableItems.count() != 0)
		{
			for (int ttc = 0; ttc < TableItems.count(); ++ttc)
			{
				PageItem* ta = TableItems.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = TableID[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = TableID[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = TableID[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = TableID[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (WeldItems.count() != 0)
		{
			for (int ttc = 0; ttc < WeldItems.count(); ++ttc)
			{
				PageItem* ta = WeldItems.at(ttc);
				for (int i = 0 ; i < ta->weldList.count(); ++i)
				{
					PageItem::WeldingInfo wInf = ta->weldList.at(i);
					ta->weldList[i].weldItem = WeldID.value(wInf.weldID, 0);
					if (ta->weldList[i].weldItem == nullptr)
						ta->weldList.removeAt(i--);
				}
			}
		}
		if (itemNext.count() != 0)
		{
			for (auto lc = itemNext.begin(); lc != itemNext.end(); ++lc)
			{
				if (lc.value() >= 0)
				{
					PageItem *Its(nullptr), *Itn(nullptr);
					if (lc.key() < m_Doc->DocItems.count())
						Its = m_Doc->DocItems.at(lc.key());
					if (lc.value() < m_Doc->DocItems.count())
						Itn = m_Doc->DocItems.at(lc.value());
					if (!Its || !Itn || !Its->canBeLinkedTo(Itn))
					{
						qDebug() << "scribus171format: corruption in linked textframes detected";
						continue;
					}
					Its->link(Itn);
				}
			}
		}
		if (itemNextM.count() != 0)
		{
			for (auto lc = itemNextM.begin(); lc != itemNextM.end(); ++lc)
			{
				if (lc.value() >= 0)
				{
					PageItem *Its(nullptr), *Itn(nullptr);
					if (lc.key() < m_Doc->MasterItems.count())
						Its = m_Doc->MasterItems.at(lc.key());
					if (lc.value() < m_Doc->MasterItems.count())
						Itn = m_Doc->MasterItems.at(lc.value());
					if (!Its || !Itn || !Its->canBeLinkedTo(Itn))
					{
						qDebug() << "scribus171format: corruption in linked textframes detected";
						continue;
					}
					Its->link(Itn);
				}
			}
		}
	}
	//CB Add this in to set this in the file in memory. Its saved, why not load it.
	//Will of course be replaced by per page settings although we still probably need a document default
	if (!hasPageSets)
	{
		m_Doc->setPageSetFirstPage(m_Doc->pagePositioning(), firstPage);
//->Prefs		m_Doc->pageSets[m_Doc->currentPageLayout].FirstPage = firstPage;
//		m_Doc->pageSets[m_Doc->currentPageLayout].GapHorizontal = dc.attribute("GapHorizontal", "0").toDouble();
//		m_Doc->pageSets[m_Doc->currentPageLayout].GapVertical = 0.0;
//		m_Doc->pageSets[m_Doc->currentPageLayout].GapBelow = dc.attribute("GapVertical", "40").toDouble();
	}
	m_Doc->setActiveLayer(layerToSetActive);
	m_Doc->setMasterPageMode(false);
	m_Doc->reformPages();
	m_Doc->refreshGuides();

	// #12282 : some docs have language dependent style names specified in style properties
	// #14129 : some others reference deleted character styles
	m_Doc->fixCharacterStyles();
	m_Doc->fixParagraphStyles();
	m_Doc->fixNotesStyles();

	// #14603 : it seems we need this also for some 1.5.x docs
	m_Doc->fixItemPageOwner();

	if (m_Doc->Layers.count() == 0)
		m_Doc->Layers.newLayer( QObject::tr("Background") );
	if (!pdfPresEffects.isEmpty())
	{
		for (int pdoE = 0; pdoE < pdfPresEffects.count(); ++pdoE)
		{
			if (pdoE < m_Doc->Pages->count())
				m_Doc->Pages->at(pdoE)->PresentVals = pdfPresEffects[pdoE];
		}
	}

	while (groupStackP.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackP.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->DocItems.removeOne(cItem);
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackP, &m_Doc->DocItems);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	while (groupStackF.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackF.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->FrameItems.remove(m_Doc->FrameItems.key(cItem));
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackF, nullptr);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	while (groupStackM.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackM.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->MasterItems.removeOne(cItem);
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackM, &m_Doc->MasterItems);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	//update names to pointers
	updateNames2Ptr();

	// reestablish first/lastAuto
	m_Doc->FirstAuto = m_Doc->LastAuto;
	if (m_Doc->LastAuto)
	{
		while (m_Doc->LastAuto->nextInChain())
			m_Doc->LastAuto = m_Doc->LastAuto->nextInChain();
		while (m_Doc->FirstAuto->prevInChain())
			m_Doc->FirstAuto = m_Doc->FirstAuto->prevInChain();
	}

	// start auto save timer if needed
	if (m_Doc->autoSave() && ScCore->usingGUI())
		m_Doc->restartAutoSaveTimer();
//	m_Doc->autoSaveTimer->start(m_Doc->autoSaveTime());

	if (m_mwProgressBar != nullptr)
		m_mwProgressBar->setValue(reader.characterOffset());
	return true;
}

// Low level plugin API
int scribus171format_getPluginAPIVersion()
{
	return PLUGIN_API_VERSION;
}

ScPlugin* scribus171format_getPlugin()
{
	Scribus171Format* plug = new Scribus171Format();
	Q_CHECK_PTR(plug);
	return plug;
}

void scribus171format_freePlugin(ScPlugin* plugin)
{
	Scribus171Format* plug = qobject_cast<Scribus171Format*>(plugin);
	Q_ASSERT(plug);
	delete plug;
}


namespace {
	const int NOVALUE = -16000;

	void fixLegacyCharStyle(CharStyle& cstyle) 
	{
		if (! cstyle.font().usable())
			cstyle.resetFont();
		if (cstyle.fontSize() <= NOVALUE / 10)
			cstyle.resetFontSize();
//		if (cstyle.effects() == 65535)
//			cstyle.resetEffects();
		if (cstyle.fillColor().isEmpty())
			cstyle.resetFillColor();
		if (cstyle.fillShade() <= NOVALUE)
			cstyle.resetFillShade();
		if (cstyle.strokeColor().isEmpty())
			cstyle.resetStrokeColor();
		if (cstyle.strokeShade() <= NOVALUE)
			cstyle.resetStrokeShade();
		if (cstyle.shadowXOffset() <= NOVALUE / 10)
			cstyle.resetShadowXOffset();
		if (cstyle.shadowYOffset() <= NOVALUE / 10)
			cstyle.resetShadowYOffset();
		if (cstyle.outlineWidth() <= NOVALUE / 10)
			cstyle.resetOutlineWidth();
		if (cstyle.underlineOffset() <= NOVALUE / 10)
			cstyle.resetUnderlineOffset();
		if (cstyle.underlineWidth() <= NOVALUE / 10)
			cstyle.resetUnderlineWidth();
		if (cstyle.strikethruOffset() <= NOVALUE / 10)
			cstyle.resetStrikethruOffset();
		if (cstyle.strikethruWidth() <= NOVALUE / 10)
			cstyle.resetStrikethruWidth();
		if (cstyle.scaleH() <= NOVALUE / 10)
			cstyle.resetScaleH();
		if (cstyle.scaleV() <= NOVALUE / 10)
			cstyle.resetScaleV();
		if (cstyle.baselineOffset() <= NOVALUE / 10)
			cstyle.resetBaselineOffset();
		if (cstyle.tracking() <= NOVALUE / 10)
			cstyle.resetTracking();
	}

	void fixLegacyParStyle(ParagraphStyle& pstyle) 
	{
		if (pstyle.lineSpacing() <= NOVALUE)
			pstyle.resetLineSpacing();
		if (pstyle.leftMargin() <= NOVALUE)
			pstyle.resetLeftMargin();
		if (pstyle.rightMargin() <= NOVALUE)
			pstyle.resetRightMargin();
		if (pstyle.firstIndent() <= NOVALUE)
			pstyle.resetFirstIndent();
		if (pstyle.alignment() < 0)
			pstyle.resetAlignment();
		if (pstyle.gapBefore() <= NOVALUE)
			pstyle.resetGapBefore();
		if (pstyle.gapAfter() <= NOVALUE)
			pstyle.resetGapAfter();
		if (pstyle.dropCapLines() < 0)
			pstyle.resetDropCapLines();
		if (pstyle.parEffectOffset() <= NOVALUE)
			pstyle.resetParEffectOffset();
		fixLegacyCharStyle(pstyle.charStyle());
	}

}// namespace

void Scribus171Format::readDocAttributes(ScribusDoc* doc, const ScXmlStreamAttributes& attrs) const
{
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("PAGESIZE"))
	{
		m_Doc->setPageSize(attrs.valueAsString("PAGESIZE"));
		m_Doc->setPageOrientation(attrs.valueAsInt("ORIENTATION", 0));
		m_Doc->FirstPnum = attrs.valueAsInt("FIRSTNUM", 1);
		m_Doc->setPagePositioning(attrs.valueAsInt("BOOK", 0));

		m_Doc->setUsesAutomaticTextFrames( attrs.valueAsInt("AUTOTEXT") );
		m_Doc->PageSp = attrs.valueAsInt("AUTOSPALTEN");
		m_Doc->PageSpa = attrs.valueAsDouble("ABSTSPALTEN");
		m_Doc->setUnitIndex( attrs.valueAsInt("UNITS", 0) );

		static const QString LANGUAGE("LANGUAGE");
		if (attrs.hasAttribute(LANGUAGE))
		{
			QString l(attrs.valueAsString(LANGUAGE));
			if (LanguageManager::instance()->langTableIndex(l) != -1)
				m_Doc->setLanguage(l); //new style storage
			else
			{ //old style storage
				QString lnew = LanguageManager::instance()->getAbbrevFromLang(l, false);
				if (lnew.isEmpty())
					lnew = LanguageManager::instance()->getAbbrevFromLang(l, false);
				m_Doc->setLanguage(lnew);
			}
		}

		m_Doc->setPageWidth(attrs.valueAsDouble("PAGEWIDTH"));
		m_Doc->setPageHeight(attrs.valueAsDouble("PAGEHEIGHT"));
		m_Doc->margins()->setLeft(qMax(0.0, attrs.valueAsDouble("BORDERLEFT")));
		m_Doc->margins()->setRight(qMax(0.0, attrs.valueAsDouble("BORDERRIGHT")));
		m_Doc->margins()->setTop(qMax(0.0, attrs.valueAsDouble("BORDERTOP")));
		m_Doc->margins()->setBottom(qMax(0.0, attrs.valueAsDouble("BORDERBOTTOM")));
		m_Doc->setMarginPreset(attrs.valueAsInt("PRESET", 0));
		m_Doc->bleeds()->setTop(attrs.valueAsDouble("BleedTop", 0.0));
		m_Doc->bleeds()->setLeft(attrs.valueAsDouble("BleedLeft", 0.0));
		m_Doc->bleeds()->setRight(attrs.valueAsDouble("BleedRight", 0.0));
		m_Doc->bleeds()->setBottom(attrs.valueAsDouble("BleedBottom", 0.0));
		m_Doc->setHyphAutomatic(attrs.valueAsBool("AUTOMATIC", true));
		m_Doc->setHyphAutoCheck(attrs.valueAsBool("AUTOCHECK", false));
		m_Doc->GuideLock = attrs.valueAsBool("GUIDELOCK", false);

		m_Doc->rulerXoffset = attrs.valueAsDouble("rulerXoffset", 0.0);
		m_Doc->rulerYoffset = attrs.valueAsDouble("rulerYoffset", 0.0);
		m_Doc->SnapGuides = attrs.valueAsBool("SnapToGuides", false);
		m_Doc->SnapElement = attrs.valueAsBool("SnapToElement", false);
		m_Doc->SnapGrid = attrs.valueAsBool("SnapToGrid", false);

		m_Doc->setAutoSave(attrs.valueAsBool("AutoSave", false));
		m_Doc->setAutoSaveTime(attrs.valueAsInt("AutoSaveTime", 600000));
		m_Doc->setAutoSaveKeep(attrs.valueAsBool("AutoSaveKeep", false));
		m_Doc->setAutoSaveCount(attrs.valueAsInt("AutoSaveCount", 1));
		m_Doc->setAutoSaveInDocDir(attrs.valueAsBool("AUtoSaveInDocDir", true));
		m_Doc->setAutoSaveDir(attrs.valueAsString("AutoSaveDir", ""));
		double leftScratch = attrs.valueAsDouble("ScratchLeft", 100.0);
		m_Doc->scratch()->set(attrs.valueAsDouble("ScratchTop", 20.0), leftScratch,
							  attrs.valueAsDouble("ScratchBottom", 20.0),attrs.valueAsDouble("ScratchRight", 100.0));
		m_Doc->setPageGapHorizontal(attrs.valueAsDouble("GapHorizontal", -1.0));
		m_Doc->setPageGapVertical(attrs.valueAsDouble("GapVertical", -1.0));

		if (attrs.hasAttribute("PAGEC"))
			m_Doc->setPaperColor(QColor(attrs.valueAsString("PAGEC")));
		//->Prefs m_Doc->papColor = QColor(attrs.valueAsString("PAGEC"));

		m_Doc->setMarginColored(attrs.valueAsBool("RANDF", false));
	}
	else
	{
		m_Doc->setPageSize(attrs.valueAsString("PageSize"));
		m_Doc->setPageOrientation(attrs.valueAsInt("PageOrientation", 0));
		m_Doc->FirstPnum = attrs.valueAsInt("FirstPageNumber", 1);
		m_Doc->setPagePositioning(attrs.valueAsInt("PagePositioning", 0));

		m_Doc->setUsesAutomaticTextFrames( attrs.valueAsInt("AutomaticTextFrames") );
		m_Doc->PageSp = attrs.valueAsInt("AutomaticTextFrameColumnCount");
		m_Doc->PageSpa = attrs.valueAsDouble("AutomaticTextFrameColumnGap");
		m_Doc->setUnitIndex( attrs.valueAsInt("UnitIndex", 0) );

		static const QString LANGUAGE("Language");
		if (attrs.hasAttribute(LANGUAGE))
		{
			QString l(attrs.valueAsString(LANGUAGE));
			if (LanguageManager::instance()->langTableIndex(l) != -1)
				m_Doc->setLanguage(l); //new style storage
			else
			{ //old style storage
				QString lnew = LanguageManager::instance()->getAbbrevFromLang(l, false);
				if (lnew.isEmpty())
					lnew = LanguageManager::instance()->getAbbrevFromLang(l, false);
				m_Doc->setLanguage(lnew);
			}
		}

		m_Doc->setPageWidth(attrs.valueAsDouble("PageWidth"));
		m_Doc->setPageHeight(attrs.valueAsDouble("PageHeight"));
		m_Doc->margins()->setLeft(qMax(0.0, attrs.valueAsDouble("BorderLeft")));
		m_Doc->margins()->setRight(qMax(0.0, attrs.valueAsDouble("BorderRight")));
		m_Doc->margins()->setTop(qMax(0.0, attrs.valueAsDouble("BorderTop")));
		m_Doc->margins()->setBottom(qMax(0.0, attrs.valueAsDouble("BorderBottom")));
		m_Doc->setMarginPreset(attrs.valueAsInt("MarginPreset", 0));
		m_Doc->bleeds()->setTop(attrs.valueAsDouble("BleedTop", 0.0));
		m_Doc->bleeds()->setLeft(attrs.valueAsDouble("BleedLeft", 0.0));
		m_Doc->bleeds()->setRight(attrs.valueAsDouble("BleedRight", 0.0));
		m_Doc->bleeds()->setBottom(attrs.valueAsDouble("BleedBottom", 0.0));
		m_Doc->setHyphAutomatic(attrs.valueAsBool("AutomaticHyphenation", true));
		m_Doc->setHyphAutoCheck(attrs.valueAsBool("AutomaticHyphenationCheck", false));
		m_Doc->GuideLock = attrs.valueAsBool("GuideLock", false);

		m_Doc->rulerXoffset = attrs.valueAsDouble("RulerXOffset", 0.0);
		m_Doc->rulerYoffset = attrs.valueAsDouble("RulerYOffset", 0.0);
		m_Doc->SnapGuides = attrs.valueAsBool("SnapToGuides", false);
		m_Doc->SnapElement = attrs.valueAsBool("SnapToElement", false);
		m_Doc->SnapGrid = attrs.valueAsBool("SnapToGrid", false);

		m_Doc->setAutoSave(attrs.valueAsBool("AutoSave", false));
		m_Doc->setAutoSaveTime(attrs.valueAsInt("AutoSaveTime", 600000));
		m_Doc->setAutoSaveKeep(attrs.valueAsBool("AutoSaveKeep", false));
		m_Doc->setAutoSaveCount(attrs.valueAsInt("AutoSaveCount", 1));
		m_Doc->setAutoSaveInDocDir(attrs.valueAsBool("AutoSaveInDocumentDirectory", true));
		m_Doc->setAutoSaveDir(attrs.valueAsString("AutoSaveDirectory", ""));
		double leftScratch = attrs.valueAsDouble("ScratchLeft", 100.0);
		m_Doc->scratch()->set(attrs.valueAsDouble("ScratchTop", 20.0), leftScratch,
							  attrs.valueAsDouble("ScratchBottom", 20.0),attrs.valueAsDouble("ScratchRight", 100.0));
		m_Doc->setPageGapHorizontal(attrs.valueAsDouble("GapHorizontal", -1.0));
		m_Doc->setPageGapVertical(attrs.valueAsDouble("GapVertical", -1.0));

		if (attrs.hasAttribute("PageColor"))
			m_Doc->setPaperColor(QColor(attrs.valueAsString("PageColor")));

		m_Doc->setMarginColored(attrs.valueAsBool("ShowMarginsFilled", false));
	}
	readCMSSettings(doc, attrs);
	readDocumentInfo(doc, attrs);
	readGuideSettings(doc, attrs);
	readToolSettings(doc, attrs);
	readTypographicSettings(doc, attrs);
}

void Scribus171Format::readCMSSettings(ScribusDoc* doc, const ScXmlStreamAttributes& attrs) const
{
	//Remove old version in 1.8
	if (attrs.hasAttribute("DPSo"))
	{
		doc->cmsSettings().SoftProofOn = attrs.valueAsBool("DPSo", false);
		doc->cmsSettings().SoftProofFullOn = attrs.valueAsBool("DPSFo", false);
		doc->cmsSettings().CMSinUse = attrs.valueAsBool("DPuse", false);
		doc->cmsSettings().GamutCheck = attrs.valueAsBool("DPgam", false);
		doc->cmsSettings().BlackPoint = attrs.valueAsBool("DPbla", true);
		doc->cmsSettings().DefaultMonitorProfile = PrefsManager::instance().appPrefs.colorPrefs.DCMSset.DefaultMonitorProfile;
		doc->cmsSettings().DefaultPrinterProfile = attrs.valueAsString("DPPr","");
		doc->cmsSettings().DefaultImageRGBProfile = attrs.valueAsString("DPIn","");
		doc->cmsSettings().DefaultImageCMYKProfile = attrs.valueAsString("DPInCMYK","");
		doc->cmsSettings().DefaultSolidColorRGBProfile = attrs.valueAsString("DPIn2","");
		if (attrs.hasAttribute("DPIn3"))
			doc->cmsSettings().DefaultSolidColorCMYKProfile = attrs.valueAsString("DPIn3","");
		else
			doc->cmsSettings().DefaultSolidColorCMYKProfile = attrs.valueAsString("DPPr","");
		doc->cmsSettings().DefaultIntentColors = (eRenderIntent) attrs.valueAsInt("DISc", 1);
		doc->cmsSettings().DefaultIntentImages = (eRenderIntent) attrs.valueAsInt("DIIm", 0);
	}
	else
	{
		doc->cmsSettings().SoftProofOn = attrs.valueAsBool("SoftProofOn", false);
		doc->cmsSettings().SoftProofFullOn = attrs.valueAsBool("SoftProofFullOn", false);
		doc->cmsSettings().CMSinUse = attrs.valueAsBool("ColorManagementActive", false);
		doc->cmsSettings().GamutCheck = attrs.valueAsBool("GamutCheck", false);
		doc->cmsSettings().BlackPoint = attrs.valueAsBool("BlackPoint", true);
		doc->cmsSettings().DefaultMonitorProfile = PrefsManager::instance().appPrefs.colorPrefs.DCMSset.DefaultMonitorProfile;
		doc->cmsSettings().DefaultPrinterProfile = attrs.valueAsString("DefaultPrinterProfile","");
		doc->cmsSettings().DefaultImageRGBProfile = attrs.valueAsString("DefaultImageRGBProfile","");
		doc->cmsSettings().DefaultImageCMYKProfile = attrs.valueAsString("DefaultImageCMYKProfile","");
		doc->cmsSettings().DefaultSolidColorRGBProfile = attrs.valueAsString("DefaultSolidColorRGBProfile","");
		doc->cmsSettings().DefaultSolidColorCMYKProfile = attrs.valueAsString("DefaultSolidColorCMYKProfile","");
		doc->cmsSettings().DefaultIntentColors = (eRenderIntent) attrs.valueAsInt("DefaultIntentColors", 1);
		doc->cmsSettings().DefaultIntentImages = (eRenderIntent) attrs.valueAsInt("DefaultIntentImages", 0);
	}
}

void Scribus171Format::readDocumentInfo(ScribusDoc* doc, const ScXmlStreamAttributes& attrs) const
{
	DocumentInformation di;
	//Remove old version in 1.8
	if (attrs.hasAttribute("AUTHOR"))
	{
		di.setAuthor(attrs.valueAsString("AUTHOR"));
		di.setComments(attrs.valueAsString("COMMENTS"));
		di.setKeywords(attrs.valueAsString("KEYWORDS",""));
		di.setTitle(attrs.valueAsString("TITLE"));
		di.setSubject(attrs.valueAsString("SUBJECT"));
		di.setPublisher(attrs.valueAsString("PUBLISHER", ""));
		di.setDate(attrs.valueAsString("DOCDATE", ""));
		di.setType(attrs.valueAsString("DOCTYPE", ""));
		di.setFormat(attrs.valueAsString("DOCFORMAT", ""));
		di.setIdent(attrs.valueAsString("DOCIDENT", ""));
		di.setSource(attrs.valueAsString("DOCSOURCE", ""));
		di.setLangInfo(attrs.valueAsString("DOCLANGINFO", ""));
		di.setRelation(attrs.valueAsString("DOCRELATION", ""));
		di.setCover(attrs.valueAsString("DOCCOVER", ""));
		di.setRights(attrs.valueAsString("DOCRIGHTS", ""));
		di.setContrib(attrs.valueAsString("DOCCONTRIB", ""));
	}
	else
	{
		di.setAuthor(attrs.valueAsString("DocumentInfoAuthor"));
		di.setComments(attrs.valueAsString("DocumentInfoComments"));
		di.setKeywords(attrs.valueAsString("DocumentInfoKeywords",""));
		di.setTitle(attrs.valueAsString("DocumentInfoTitle"));
		di.setSubject(attrs.valueAsString("DocumentInfoSubject"));
		di.setPublisher(attrs.valueAsString("DocumentInfoPublisher", ""));
		di.setDate(attrs.valueAsString("DocumentInfoDate", ""));
		di.setType(attrs.valueAsString("DocumentInfoType", ""));
		di.setFormat(attrs.valueAsString("DocumentInfoFormat", ""));
		di.setIdent(attrs.valueAsString("DocumentInfoIdentifier", ""));
		di.setSource(attrs.valueAsString("DocumentInfoSource", ""));
		di.setLangInfo(attrs.valueAsString("DocumentInfoLanguage", ""));
		di.setRelation(attrs.valueAsString("DocumentInfoRelation", ""));
		di.setCover(attrs.valueAsString("DocumentInfoCoverage", ""));
		di.setRights(attrs.valueAsString("DocumentInfoRights", ""));
		di.setContrib(attrs.valueAsString("DocumentInfoContributors", ""));
	}
	doc->setDocumentInfo(di);
}

void Scribus171Format::readGuideSettings(ScribusDoc* doc, const ScXmlStreamAttributes& attrs) const
{
	PrefsManager& prefsManager = PrefsManager::instance();
	if (attrs.hasAttribute("MINGRID"))
	{
		doc->guidesPrefs().minorGridSpacing = attrs.valueAsDouble("MINGRID", prefsManager.appPrefs.guidesPrefs.minorGridSpacing);
		doc->guidesPrefs().majorGridSpacing = attrs.valueAsDouble("MAJGRID", prefsManager.appPrefs.guidesPrefs.majorGridSpacing);
		doc->guidesPrefs().gridShown = attrs.valueAsBool("SHOWGRID", false);
		doc->guidesPrefs().guidesShown = attrs.valueAsBool("SHOWGUIDES", true);
		doc->guidesPrefs().colBordersShown = attrs.valueAsBool("showcolborders", false);
		doc->guidesPrefs().framesShown = attrs.valueAsBool("SHOWFRAME", true);
		doc->guidesPrefs().layerMarkersShown = attrs.valueAsBool("SHOWLAYERM", false);
		doc->guidesPrefs().marginsShown = attrs.valueAsBool("SHOWMARGIN", true);
		doc->guidesPrefs().baselineGridShown = attrs.valueAsBool("SHOWBASE", false);
		doc->guidesPrefs().showPic = attrs.valueAsBool("SHOWPICT", true);
		doc->guidesPrefs().linkShown = attrs.valueAsBool("SHOWLINK", false);
		doc->guidesPrefs().showControls = attrs.valueAsBool("SHOWControl", false);
		doc->guidesPrefs().rulerMode = attrs.valueAsBool("rulerMode", true);
		doc->guidesPrefs().rulersShown = attrs.valueAsBool("showrulers", true);
		doc->guidesPrefs().showBleed = attrs.valueAsBool("showBleed", true);
		m_Doc->drawAsPreview		 = false /*attrs.valueAsBool("previewMode", false)*/;
		if (attrs.hasAttribute("MARGC"))
			doc->guidesPrefs().marginColor = QColor(attrs.valueAsString("MARGC"));
		if (attrs.hasAttribute("MINORC"))
			doc->guidesPrefs().minorGridColor = QColor(attrs.valueAsString("MINORC"));
		if (attrs.hasAttribute("MAJORC"))
			doc->guidesPrefs().majorGridColor = QColor(attrs.valueAsString("MAJORC"));
		if (attrs.hasAttribute("GuideC")) //legacy < 1.7.x
			doc->guidesPrefs().guideColor = QColor(attrs.valueAsString("GuideC"));
		if (attrs.hasAttribute("GuidesColor"))
			doc->guidesPrefs().guideColor = QColor(attrs.valueAsString("GuidesColor"));
		if (attrs.hasAttribute("BaseC")) //legacy < 1.7.x
			doc->guidesPrefs().baselineGridColor = QColor(attrs.valueAsString("BaseC"));
		if (attrs.hasAttribute("BaselineGridColor"))
			doc->guidesPrefs().baselineGridColor = QColor(attrs.valueAsString("BaselineGridColor"));
		if (attrs.hasAttribute("BACKG"))
		{
			doc->guidesPrefs().renderStackOrder.clear();
			if (attrs.valueAsBool("BACKG", true))
				doc->guidesPrefs().renderStackOrder << 0 << 1 << 2 << 3 << 4;
			else
				doc->guidesPrefs().renderStackOrder << 4 << 0 << 1 << 2 << 3;
		}
		if (attrs.hasAttribute("renderStack"))
		{
			doc->guidesPrefs().renderStackOrder.clear();
			QString renderStack = attrs.valueAsString("renderStack", "0 1 2 3 4");
			ScTextStream fp(&renderStack, QIODevice::ReadOnly);
			QString val;
			while (!fp.atEnd())
			{
				fp >> val;
				doc->guidesPrefs().renderStackOrder << val.toInt();
			}
		}
		doc->guidesPrefs().gridType = attrs.valueAsInt("GridType", 0);
		doc->guidesPrefs().guideRad = attrs.valueAsDouble("GuideRad", 10.0);
		doc->guidesPrefs().grabRadius = attrs.valueAsInt("GRAB", 4);
	}
	else
	{
		doc->guidesPrefs().minorGridSpacing = attrs.valueAsDouble("MinorGridSpacing", prefsManager.appPrefs.guidesPrefs.minorGridSpacing);
		doc->guidesPrefs().majorGridSpacing = attrs.valueAsDouble("MajorGridSpacing", prefsManager.appPrefs.guidesPrefs.majorGridSpacing);
		doc->guidesPrefs().gridShown = attrs.valueAsBool("ShowGrid", false);
		doc->guidesPrefs().guidesShown = attrs.valueAsBool("ShowGuides", true);
		doc->guidesPrefs().colBordersShown = attrs.valueAsBool("ShowColumnBorders", false);
		doc->guidesPrefs().framesShown = attrs.valueAsBool("ShowFrames", true);
		doc->guidesPrefs().layerMarkersShown = attrs.valueAsBool("ShowLayerMarkers", false);
		doc->guidesPrefs().marginsShown = attrs.valueAsBool("ShowMargins", true);
		doc->guidesPrefs().baselineGridShown = attrs.valueAsBool("ShowBaselineGrid", false);
		doc->guidesPrefs().showPic = attrs.valueAsBool("ShowImages", true);
		doc->guidesPrefs().linkShown = attrs.valueAsBool("ShowLinks", false);
		doc->guidesPrefs().showControls = attrs.valueAsBool("ShowControls", false);
		doc->guidesPrefs().rulerMode = attrs.valueAsBool("RulerMode", true);
		doc->guidesPrefs().rulersShown = attrs.valueAsBool("ShowRulers", true);
		doc->guidesPrefs().showBleed = attrs.valueAsBool("ShowBleed", true);
		m_Doc->drawAsPreview		 = false /*attrs.valueAsBool("previewMode", false)*/;
		if (attrs.hasAttribute("MarginColor"))
			doc->guidesPrefs().marginColor = QColor(attrs.valueAsString("MarginColor"));
		if (attrs.hasAttribute("MinorGridColor"))
			doc->guidesPrefs().minorGridColor = QColor(attrs.valueAsString("MinorGridColor"));
		if (attrs.hasAttribute("MajorGridColor"))
			doc->guidesPrefs().majorGridColor = QColor(attrs.valueAsString("MajorGridColor"));
		if (attrs.hasAttribute("GuidesColor"))
			doc->guidesPrefs().guideColor = QColor(attrs.valueAsString("GuidesColor"));
		if (attrs.hasAttribute("BaselineGridColor"))
			doc->guidesPrefs().baselineGridColor = QColor(attrs.valueAsString("BaselineGridColor"));
		if (attrs.hasAttribute("BACKG"))
		{
			doc->guidesPrefs().renderStackOrder.clear();
			if (attrs.valueAsBool("BACKG", true))
				doc->guidesPrefs().renderStackOrder << 0 << 1 << 2 << 3 << 4;
			else
				doc->guidesPrefs().renderStackOrder << 4 << 0 << 1 << 2 << 3;
		}
		if (attrs.hasAttribute("RenderStack"))
		{
			doc->guidesPrefs().renderStackOrder.clear();
			QString renderStack = attrs.valueAsString("renderStack", "0 1 2 3 4");
			ScTextStream fp(&renderStack, QIODevice::ReadOnly);
			QString val;
			while (!fp.atEnd())
			{
				fp >> val;
				doc->guidesPrefs().renderStackOrder << val.toInt();
			}
		}
		doc->guidesPrefs().gridType = attrs.valueAsInt("GridType", 0);
		doc->guidesPrefs().guideRad = attrs.valueAsDouble("GuideRadius", 10.0);
		doc->guidesPrefs().grabRadius = attrs.valueAsInt("GrabRadius", 4);
	}
}

void Scribus171Format::readToolSettings(ScribusDoc* doc, const ScXmlStreamAttributes& attrs) const
{
	const ItemToolPrefs& defToolPrefs = PrefsManager::instance().appPrefs.itemToolPrefs;

	if (attrs.hasAttribute("DFONT"))
	{
		QString textFont = attrs.valueAsString("DFONT");
		m_AvailableFonts->findFont(textFont, doc);

		doc->itemToolPrefs().textFont = textFont;
		doc->itemToolPrefs().textSize = qRound(attrs.valueAsDouble("DSIZE", 12.0) * 10);
		doc->itemToolPrefs().textColumns = attrs.valueAsInt("DCOL", 1);
		doc->itemToolPrefs().textColumnGap = attrs.valueAsDouble("DGAP", 0.0);

		const MarginStruct& defDistances = defToolPrefs.textDistances;
		doc->itemToolPrefs().textDistances.setLeft(attrs.valueAsDouble("TextDistLeft", defDistances.left()));
		doc->itemToolPrefs().textDistances.setRight(attrs.valueAsDouble("TextDistRight", defDistances.right()));
		doc->itemToolPrefs().textDistances.setBottom(attrs.valueAsDouble("TextDistBottom", defDistances.bottom()));
		doc->itemToolPrefs().textDistances.setTop(attrs.valueAsDouble("TextDistTop", defDistances.top()));

		doc->itemToolPrefs().polyCorners = attrs.valueAsInt("POLYC", 4);
		doc->itemToolPrefs().polyFactor = attrs.valueAsDouble("POLYF", 0.5);
		doc->itemToolPrefs().polyRotation = attrs.valueAsDouble("POLYR", 0.0);
		doc->itemToolPrefs().polyInnerRot = attrs.valueAsDouble("POLYIR", 0.0);
		doc->itemToolPrefs().polyCurvature = attrs.valueAsDouble("POLYCUR", 0.0);
		doc->itemToolPrefs().polyOuterCurvature = attrs.valueAsDouble("POLYOCUR", 0.0);
		doc->itemToolPrefs().polyUseFactor = attrs.valueAsBool("POLYS", false);

		doc->itemToolPrefs().arcStartAngle = attrs.valueAsDouble("arcStartAngle", 30.0);
		doc->itemToolPrefs().arcSweepAngle = attrs.valueAsDouble("arcSweepAngle", 300.0);
		doc->itemToolPrefs().spiralStartAngle = attrs.valueAsDouble("spiralStartAngle", 0.0);
		doc->itemToolPrefs().spiralEndAngle = attrs.valueAsDouble("spiralEndAngle", 1080.0);
		doc->itemToolPrefs().spiralFactor = attrs.valueAsDouble("spiralFactor", 1.2);

		doc->itemToolPrefs().lineStartArrow = attrs.valueAsInt("StartArrow", 0);
		doc->itemToolPrefs().lineEndArrow = attrs.valueAsInt("EndArrow", 0);
		doc->itemToolPrefs().imageScaleX = attrs.valueAsDouble("PICTSCX", 1.0);
		doc->itemToolPrefs().imageScaleY = attrs.valueAsDouble("PICTSCY", 1.0);
		doc->itemToolPrefs().imageScaleType = attrs.valueAsBool("PSCALE", true);
		doc->itemToolPrefs().imageAspectRatio = attrs.valueAsBool("PASPECT", false);
		doc->itemToolPrefs().imageLowResType = attrs.valueAsInt("HalfRes", 1);
		doc->itemToolPrefs().imageUseEmbeddedPath = attrs.valueAsBool("EmbeddedPath", false);
		if (attrs.hasAttribute("PEN"))
			doc->itemToolPrefs().shapeLineColor = attrs.valueAsString("PEN");
		if (attrs.hasAttribute("BRUSH"))
			doc->itemToolPrefs().shapeFillColor = attrs.valueAsString("BRUSH");
		if (attrs.hasAttribute("PENLINE"))
			doc->itemToolPrefs().lineColor = attrs.valueAsString("PENLINE");
		if (attrs.hasAttribute("PENTEXT"))
			doc->itemToolPrefs().textColor = attrs.valueAsString("PENTEXT");
		if (attrs.hasAttribute("StrokeText"))
			doc->itemToolPrefs().textStrokeColor = attrs.valueAsString("StrokeText");
		doc->itemToolPrefs().textFillColor = attrs.valueAsString("TextBackGround", CommonStrings::None);
		doc->itemToolPrefs().textLineColor = attrs.valueAsString("TextLineColor", CommonStrings::None);
		doc->itemToolPrefs().textFillColorShade = attrs.valueAsInt("TextBackGroundShade", 100);
		doc->itemToolPrefs().textLineColorShade = attrs.valueAsInt("TextLineShade", 100);
		doc->itemToolPrefs().textShade = attrs.valueAsInt("TextPenShade", 100);
		doc->itemToolPrefs().textStrokeShade = attrs.valueAsInt("TextStrokeShade", 100);
		doc->itemToolPrefs().shapeLineStyle = static_cast<Qt::PenStyle>(attrs.valueAsInt("STIL"));
		doc->itemToolPrefs().lineStyle = static_cast<Qt::PenStyle>(attrs.valueAsInt("STILLINE"));
		doc->itemToolPrefs().shapeLineWidth = attrs.valueAsDouble("WIDTH", 0.0);
		doc->itemToolPrefs().lineWidth = attrs.valueAsDouble("WIDTHLINE", 1.0);
		doc->itemToolPrefs().shapeLineColorShade = attrs.valueAsInt("PENSHADE", 100);
		doc->itemToolPrefs().lineColorShade = attrs.valueAsInt("LINESHADE", 100);
		doc->itemToolPrefs().shapeFillColorShade = attrs.valueAsInt("BRUSHSHADE", 100);
		doc->itemToolPrefs().calligraphicPenFillColor = attrs.valueAsString("calligraphicPenFillColor", "Black");
		doc->itemToolPrefs().calligraphicPenLineColor = attrs.valueAsString("calligraphicPenLineColor", "Black");
		doc->itemToolPrefs().calligraphicPenFillColorShade = attrs.valueAsInt("calligraphicPenFillColorShade", 100);
		doc->itemToolPrefs().calligraphicPenLineColorShade = attrs.valueAsInt("calligraphicPenLineColorShade", 100);
		doc->itemToolPrefs().calligraphicPenLineWidth = attrs.valueAsDouble("calligraphicPenLineWidth", 1.0);
		doc->itemToolPrefs().calligraphicPenAngle = attrs.valueAsDouble("calligraphicPenAngle", 0.0);
		doc->itemToolPrefs().calligraphicPenWidth = attrs.valueAsDouble("calligraphicPenWidth", 10.0);
		doc->itemToolPrefs().calligraphicPenStyle = static_cast<Qt::PenStyle>(attrs.valueAsInt("calligraphicPenStyle"));
		doc->opToolPrefs().dispX = attrs.valueAsDouble("dispX", 10.0);
		doc->opToolPrefs().dispY = attrs.valueAsDouble("dispY", 10.0);
		doc->opToolPrefs().constrain = attrs.valueAsDouble("constrain", 15.0);
		doc->itemToolPrefs().textTabFillChar = attrs.valueAsString("TabFill","");
		doc->itemToolPrefs().textTabWidth = attrs.valueAsDouble("TabWidth", 36.0);
		doc->itemToolPrefs().firstLineOffset = (FirstLineOffsetPolicy) attrs.valueAsInt("FirstLineOffset", (int) FLOPRealGlyphHeight); // Default to FLOPRealGlyphHeight for legacy docs
		doc->itemToolPrefs().firstLineOffset = qMax(FLOPRealGlyphHeight, qMin(doc->itemToolPrefs().firstLineOffset, FLOPBaselineGrid));
		if (attrs.hasAttribute("CPICT"))
			doc->itemToolPrefs().imageFillColor = attrs.valueAsString("CPICT");
		doc->itemToolPrefs().imageFillColorShade = attrs.valueAsInt("PICTSHADE", 100);
		if (attrs.hasAttribute("CSPICT"))
			doc->itemToolPrefs().imageStrokeColor = attrs.valueAsString("CSPICT");
		doc->itemToolPrefs().imageStrokeColorShade = attrs.valueAsInt("PICTSSHADE", 100);
	}
	else
	{
		QString textFont = attrs.valueAsString("FontFace");
		m_AvailableFonts->findFont(textFont, doc);

		doc->itemToolPrefs().textFont = textFont;
		doc->itemToolPrefs().textSize = qRound(attrs.valueAsDouble("FontSize", 12.0) * 10);
		doc->itemToolPrefs().textColumns = attrs.valueAsInt("TextColumnCount", 1);
		doc->itemToolPrefs().textColumnGap = attrs.valueAsDouble("TextColumnGap", 0.0);

		const MarginStruct& defDistances = defToolPrefs.textDistances;
		doc->itemToolPrefs().textDistances.setLeft(attrs.valueAsDouble("TextDistanceLeft", defDistances.left()));
		doc->itemToolPrefs().textDistances.setRight(attrs.valueAsDouble("TextDistanceRight", defDistances.right()));
		doc->itemToolPrefs().textDistances.setBottom(attrs.valueAsDouble("TextDistanceBottom", defDistances.bottom()));
		doc->itemToolPrefs().textDistances.setTop(attrs.valueAsDouble("TextDistanceTop", defDistances.top()));

		doc->itemToolPrefs().polyCorners = attrs.valueAsInt("PolygonCorners", 4);
		doc->itemToolPrefs().polyFactor = attrs.valueAsDouble("PolygonFactor", 0.5);
		doc->itemToolPrefs().polyRotation = attrs.valueAsDouble("PolygonRotation", 0.0);
		doc->itemToolPrefs().polyInnerRot = attrs.valueAsDouble("PolygonInnerRotation", 0.0);
		doc->itemToolPrefs().polyCurvature = attrs.valueAsDouble("PolygonCurvature", 0.0);
		doc->itemToolPrefs().polyOuterCurvature = attrs.valueAsDouble("PolygonOuterCurvature", 0.0);
		doc->itemToolPrefs().polyUseFactor = attrs.valueAsBool("PolygonUseFactor", false);

		doc->itemToolPrefs().arcStartAngle = attrs.valueAsDouble("ArcStartAngle", 30.0);
		doc->itemToolPrefs().arcSweepAngle = attrs.valueAsDouble("ArcSweepAngle", 300.0);
		doc->itemToolPrefs().spiralStartAngle = attrs.valueAsDouble("SpiralStartAngle", 0.0);
		doc->itemToolPrefs().spiralEndAngle = attrs.valueAsDouble("SpiralEndAngle", 1080.0);
		doc->itemToolPrefs().spiralFactor = attrs.valueAsDouble("SpiralFactor", 1.2);

		doc->itemToolPrefs().lineStartArrow = attrs.valueAsInt("LineStartArrow", 0);
		doc->itemToolPrefs().lineEndArrow = attrs.valueAsInt("LineEndArrow", 0);
		doc->itemToolPrefs().imageScaleX = attrs.valueAsDouble("ImageScaleX", 1.0);
		doc->itemToolPrefs().imageScaleY = attrs.valueAsDouble("ImageScaleY", 1.0);
		doc->itemToolPrefs().imageScaleType = attrs.valueAsBool("ImageScaleType", true);
		doc->itemToolPrefs().imageAspectRatio = attrs.valueAsBool("ImageAspectRatio", false);
		doc->itemToolPrefs().imageLowResType = attrs.valueAsInt("ImageLowResType", 1);
		doc->itemToolPrefs().imageUseEmbeddedPath = attrs.valueAsBool("ImageUseEmbeddedPath", false);
		doc->itemToolPrefs().shapeLineColor = attrs.valueAsString("ShapeLineColor");
		doc->itemToolPrefs().shapeFillColor = attrs.valueAsString("ShapeFillColor");
		doc->itemToolPrefs().lineColor = attrs.valueAsString("LineColor");
		doc->itemToolPrefs().textColor = attrs.valueAsString("TextColor");
		doc->itemToolPrefs().textStrokeColor = attrs.valueAsString("TextStrokeColor");
		doc->itemToolPrefs().textFillColor = attrs.valueAsString("TextBackgroundColor", CommonStrings::None);
		doc->itemToolPrefs().textLineColor = attrs.valueAsString("TextLineColor", CommonStrings::None);
		doc->itemToolPrefs().textFillColorShade = attrs.valueAsInt("TextBackgroundColorShade", 100);
		doc->itemToolPrefs().textLineColorShade = attrs.valueAsInt("TextLineColorShade", 100);
		doc->itemToolPrefs().textShade = attrs.valueAsInt("TextColorShade", 100);
		doc->itemToolPrefs().textStrokeShade = attrs.valueAsInt("TextStrokeColorShade", 100);
		doc->itemToolPrefs().shapeLineStyle = static_cast<Qt::PenStyle>(attrs.valueAsInt("ShapeLineStyle"));
		doc->itemToolPrefs().lineStyle = static_cast<Qt::PenStyle>(attrs.valueAsInt("LineStyle"));
		doc->itemToolPrefs().shapeLineWidth = attrs.valueAsDouble("ShapeLineWidth", 0.0);
		doc->itemToolPrefs().lineWidth = attrs.valueAsDouble("LineWidth", 1.0);
		doc->itemToolPrefs().shapeLineColorShade = attrs.valueAsInt("ShapeLineColorShade", 100);
		doc->itemToolPrefs().lineColorShade = attrs.valueAsInt("LineColorShade", 100);
		doc->itemToolPrefs().shapeFillColorShade = attrs.valueAsInt("ShapeFillColorShade", 100);
		doc->itemToolPrefs().calligraphicPenFillColor = attrs.valueAsString("CalligraphicPenFillColor", "Black");
		doc->itemToolPrefs().calligraphicPenLineColor = attrs.valueAsString("CalligraphicPenLineColor", "Black");
		doc->itemToolPrefs().calligraphicPenFillColorShade = attrs.valueAsInt("CalligraphicPenFillColorShade", 100);
		doc->itemToolPrefs().calligraphicPenLineColorShade = attrs.valueAsInt("CalligraphicPenLineColorShade", 100);
		doc->itemToolPrefs().calligraphicPenLineWidth = attrs.valueAsDouble("CalligraphicPenLineWidth", 1.0);
		doc->itemToolPrefs().calligraphicPenAngle = attrs.valueAsDouble("CalligraphicPenAngle", 0.0);
		doc->itemToolPrefs().calligraphicPenWidth = attrs.valueAsDouble("CalligraphicPenWidth", 10.0);
		doc->itemToolPrefs().calligraphicPenStyle = static_cast<Qt::PenStyle>(attrs.valueAsInt("CalligraphicPenStyle"));
		doc->opToolPrefs().dispX = attrs.valueAsDouble("DisplayOffsetX", 10.0);
		doc->opToolPrefs().dispY = attrs.valueAsDouble("DisplayOffsetY", 10.0);
		doc->opToolPrefs().constrain = attrs.valueAsDouble("RotationConstrainAngle", 15.0);
		doc->itemToolPrefs().textTabFillChar = attrs.valueAsString("TabFillCharacter","");
		doc->itemToolPrefs().textTabWidth = attrs.valueAsDouble("TabWidth", 36.0);
		doc->itemToolPrefs().firstLineOffset = (FirstLineOffsetPolicy) attrs.valueAsInt("FirstLineOffset", (int) FLOPRealGlyphHeight); // Default to FLOPRealGlyphHeight for legacy docs
		doc->itemToolPrefs().firstLineOffset = qMax(FLOPRealGlyphHeight, qMin(doc->itemToolPrefs().firstLineOffset, FLOPBaselineGrid));
		doc->itemToolPrefs().imageFillColor = attrs.valueAsString("ImageFillColor");
		doc->itemToolPrefs().imageFillColorShade = attrs.valueAsInt("ImageFillColorShade", 100);
		doc->itemToolPrefs().imageStrokeColor = attrs.valueAsString("ImageStrokeColor");
		doc->itemToolPrefs().imageStrokeColorShade = attrs.valueAsInt("ImageStrokeColorShade", 100);
	}
}

void Scribus171Format::readTypographicSettings(ScribusDoc* doc, const ScXmlStreamAttributes& attrs) const
{
	if (attrs.hasAttribute("VHOCH"))
	{
		doc->typographicPrefs().valueSuperScript = attrs.valueAsInt("VHOCH");
		doc->typographicPrefs().scalingSuperScript = attrs.valueAsInt("VHOCHSC");
		doc->typographicPrefs().valueSubScript = attrs.valueAsInt("VTIEF");
		doc->typographicPrefs().scalingSubScript = attrs.valueAsInt("VTIEFSC");
		doc->typographicPrefs().valueSmallCaps = attrs.valueAsInt("VKAPIT");
		doc->guidesPrefs().valueBaselineGrid = attrs.valueAsDouble("BASEGRID", 12.0);
		doc->guidesPrefs().offsetBaselineGrid = attrs.valueAsDouble("BASEO", 0.0);
		// #9621 : autolinespacing is now expressed as a percentage of the font height
		doc->typographicPrefs().autoLineSpacing = attrs.valueAsInt("AUTOL", 1, 500, 100);
		doc->typographicPrefs().valueUnderlinePos = attrs.valueAsInt("UnderlinePos", -1);
		doc->typographicPrefs().valueUnderlineWidth = attrs.valueAsInt("UnderlineWidth", -1);
		doc->typographicPrefs().valueStrikeThruPos = attrs.valueAsInt("StrikeThruPos", -1);
		doc->typographicPrefs().valueStrikeThruWidth = attrs.valueAsInt("StrikeThruWidth", -1);
	}
	else
	{
		doc->typographicPrefs().valueSuperScript = attrs.valueAsInt("SuperScriptDisplacement");
		doc->typographicPrefs().scalingSuperScript = attrs.valueAsInt("SuperScriptScaling");
		doc->typographicPrefs().valueSubScript = attrs.valueAsInt("SubScriptDisplacement");
		doc->typographicPrefs().scalingSubScript = attrs.valueAsInt("SubScriptScaling");
		doc->typographicPrefs().valueSmallCaps = attrs.valueAsInt("SmallCapsScaling");
		doc->guidesPrefs().valueBaselineGrid = attrs.valueAsDouble("BaselineGridSpacing", 12.0);
		doc->guidesPrefs().offsetBaselineGrid = attrs.valueAsDouble("BaselineGridOffset", 0.0);
		// #9621 : autolinespacing is now expressed as a percentage of the font height
		doc->typographicPrefs().autoLineSpacing = attrs.valueAsInt("AutoLineSpacingPct", 1, 500, 100);
		doc->typographicPrefs().valueUnderlinePos = attrs.valueAsInt("UnderlinePosition", -1);
		doc->typographicPrefs().valueUnderlineWidth = attrs.valueAsInt("UnderlineWidth", -1);
		doc->typographicPrefs().valueStrikeThruPos = attrs.valueAsInt("StrikeThruPosition", -1);
		doc->typographicPrefs().valueStrikeThruWidth = attrs.valueAsInt("StrikeThruWidth", -1);
	}
}

bool Scribus171Format::readPageSets(ScribusDoc* doc, ScXmlStreamReader& reader) const
{
	struct PageSet pageS;
	ScXmlStreamAttributes attrs;
	doc->clearPageSets();
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		QString tagName(reader.nameAsString());
		if (reader.isStartElement())
			attrs = reader.attributes();
		if (reader.isEndElement() && tagName == QLatin1String("PageSets"))
			break;
		if (reader.isStartElement() && tagName == QLatin1String("Set"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			pageS.Name = CommonStrings::untranslatePageSetString(attrs.valueAsString("Name"));
			pageS.FirstPage = attrs.valueAsInt("FirstPage", 0);
			pageS.Rows = attrs.valueAsInt("Rows", 1);
			pageS.Columns = attrs.valueAsInt("Columns", 1);
			pageS.pageNames.clear();
		}
		if (reader.isEndElement() && tagName == QLatin1String("Set"))
		{
			doc->appendToPageSets(pageS);
			if ((doc->pageSets().count()-1 == doc->pagePositioning()) && ((doc->pageGapHorizontal() < 0) && (doc->pageGapVertical() < 0)))
			{
				doc->setPageGapHorizontal(attrs.valueAsDouble("GapHorizontal", 0.0));
				doc->setPageGapVertical(attrs.valueAsDouble("GapBelow", 0.0));
			}
		}
		if (reader.isStartElement() && tagName == QLatin1String("PageNames"))
			pageS.pageNames.append(CommonStrings::untranslatePageSetLocString(attrs.valueAsString("Name")));
	}
	return !reader.hasError();
}

bool Scribus171Format::readCheckProfile(ScribusDoc* doc, const ScXmlStreamAttributes& attrs) const
{
	struct CheckerPrefs checkerSettings;

	QString profileName = attrs.valueAsString("Name");
	if (profileName.isEmpty())
		return true;
	//Remove old in 1.8
	if (attrs.hasAttribute("ignoreErrors"))
	{
		checkerSettings.ignoreErrors = attrs.valueAsBool("ignoreErrors", false);
		checkerSettings.autoCheck = attrs.valueAsBool("autoCheck", true);
		checkerSettings.checkGlyphs = attrs.valueAsBool("checkGlyphs", true);
		checkerSettings.checkOrphans = attrs.valueAsBool("checkOrphans", true);
		checkerSettings.checkOverflow = attrs.valueAsBool("checkOverflow", true);
		checkerSettings.checkPictures = attrs.valueAsBool("checkPictures", true);
		checkerSettings.checkPartFilledImageFrames = attrs.valueAsBool("checkPartFilledImageFrames", false);
		checkerSettings.checkResolution = attrs.valueAsBool("checkResolution", true);
		checkerSettings.checkTransparency = attrs.valueAsBool("checkTransparency", true);
		checkerSettings.minResolution = attrs.valueAsDouble("minResolution", 72.0);
		checkerSettings.maxResolution = attrs.valueAsDouble("maxResolution", 4800.0);
		checkerSettings.checkAnnotations = attrs.valueAsBool("checkAnnotations", false);
		checkerSettings.checkRasterPDF = attrs.valueAsBool("checkRasterPDF", true);
		checkerSettings.checkForGIF = attrs.valueAsBool("checkForGIF", true);
		checkerSettings.ignoreOffLayers = attrs.valueAsBool("ignoreOffLayers", false);
		checkerSettings.checkNotCMYKOrSpot = attrs.valueAsBool("checkNotCMYKOrSpot", false);
		checkerSettings.checkDeviceColorsAndOutputIntent = attrs.valueAsBool("checkDeviceColorsAndOutputIntent", false);
		checkerSettings.checkFontNotEmbedded = attrs.valueAsBool("checkFontNotEmbedded", false);
		checkerSettings.checkFontIsOpenType = attrs.valueAsBool("checkFontIsOpenType", false);
		checkerSettings.checkAppliedMasterDifferentSide = attrs.valueAsBool("checkAppliedMasterDifferentSide", true);
		checkerSettings.checkEmptyTextFrames = attrs.valueAsBool("checkEmptyTextFrames", true);
		checkerSettings.checkImageHasProgressiveEncoding = attrs.valueAsBool("checkImageHasProgressiveEncoding", true);
	}
	else
	{
		checkerSettings.ignoreErrors = attrs.valueAsBool("IgnoreErrors", false);
		checkerSettings.autoCheck = attrs.valueAsBool("AutoCheck", true);
		checkerSettings.checkGlyphs = attrs.valueAsBool("CheckGlyphs", true);
		checkerSettings.checkOrphans = attrs.valueAsBool("CheckOrphans", true);
		checkerSettings.checkOverflow = attrs.valueAsBool("CheckOverflow", true);
		checkerSettings.checkPictures = attrs.valueAsBool("CheckPictures", true);
		checkerSettings.checkPartFilledImageFrames = attrs.valueAsBool("CheckPartFilledImageFrames", false);
		checkerSettings.checkResolution = attrs.valueAsBool("CheckResolution", true);
		checkerSettings.checkTransparency = attrs.valueAsBool("CheckTransparency", true);
		checkerSettings.minResolution = attrs.valueAsDouble("MinimumResolution", 72.0);
		checkerSettings.maxResolution = attrs.valueAsDouble("MaximumResolution", 4800.0);
		checkerSettings.checkAnnotations = attrs.valueAsBool("CheckAnnotations", false);
		checkerSettings.checkRasterPDF = attrs.valueAsBool("CheckRasterPDF", true);
		checkerSettings.checkForGIF = attrs.valueAsBool("CheckForGIF", true);
		checkerSettings.ignoreOffLayers = attrs.valueAsBool("IgnoreOffLayers", false);
		checkerSettings.checkNotCMYKOrSpot = attrs.valueAsBool("CheckNotCMYKOrSpot", false);
		checkerSettings.checkDeviceColorsAndOutputIntent = attrs.valueAsBool("CheckDeviceColorsAndOutputIntent", false);
		checkerSettings.checkFontNotEmbedded = attrs.valueAsBool("CheckFontNotEmbedded", false);
		checkerSettings.checkFontIsOpenType = attrs.valueAsBool("CheckFontIsOpenType", false);
		checkerSettings.checkAppliedMasterDifferentSide = attrs.valueAsBool("CheckAppliedMasterDifferentSide", true);
		checkerSettings.checkEmptyTextFrames = attrs.valueAsBool("CheckEmptyTextFrames", true);
		checkerSettings.checkImageHasProgressiveEncoding = attrs.valueAsBool("CheckImageHasProgressiveEncoding", true);
	}
	doc->set1CheckerProfile(profileName, checkerSettings);
	return true;
}

void Scribus171Format::readColor(ColorList& colors, const ScXmlStreamAttributes& attrs) const
{
	ScColor color;
	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("SPACE") || attrs.hasAttribute("Space"))
	{
		//Remove uppercase in 1.8 format
		QString space;
		if (attrs.hasAttribute("SPACE"))
			space = attrs.valueAsString("SPACE");
		else
			space = attrs.valueAsString("Space");
		if (space == "CMYK")
		{
			double c = attrs.valueAsDouble("C", 0) / 100.0;
			double m = attrs.valueAsDouble("M", 0) / 100.0;
			double y = attrs.valueAsDouble("Y", 0) / 100.0;
			double k = attrs.valueAsDouble("K", 0) / 100.0;
			color.setCmykColorF(c, m, y, k);
		}
		else if (space == "RGB")
		{
			double r = attrs.valueAsDouble("R", 0) / 255.0;
			double g = attrs.valueAsDouble("G", 0) / 255.0;
			double b = attrs.valueAsDouble("B", 0) / 255.0;
			color.setRgbColorF(r, g, b);
		}
		else if (space == "Lab")
		{
			double L = attrs.valueAsDouble("L", 0);
			double a = attrs.valueAsDouble("A", 0);
			double b = attrs.valueAsDouble("B", 0);
			color.setLabColor(L, a, b);
		}
	}
	else if (attrs.hasAttribute("CMYK"))
		color.setNamedColor(attrs.valueAsString("CMYK"));
	else if (attrs.hasAttribute("RGB"))
		color.fromQColor(QColor(attrs.valueAsString("RGB")));
	else
	{
		double L = attrs.valueAsDouble("L", 0);
		double a = attrs.valueAsDouble("A", 0);
		double b = attrs.valueAsDouble("B", 0);
		color.setLabColor(L, a, b);
	}
	color.setSpotColor( attrs.valueAsBool("Spot", false) );
	color.setRegistrationColor( attrs.valueAsBool("Register", false) );
	QString name;
	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("NAME"))
		name = attrs.valueAsString("NAME", color.name());
	else
		name = attrs.valueAsString("Name", color.name());
	if (name == "All")
	{
		color.setSpotColor(true);
		color.setRegistrationColor(true);
		color.setCmykColorF(1.0, 1.0, 1.0, 1.0);
	}
	// #10323 : break loading of doc which contain colors with different names
	// and same definition
	// colors.tryAddColor(name, color);
	if (name.length() > 0 && !colors.contains(name))
		colors.insert(name, color);
}

bool Scribus171Format::readGradient(ScribusDoc *doc, VGradient &gra, ScXmlStreamReader& reader) const
{
	gra = VGradient(VGradient::linear);
	gra.clearStops();
	ScXmlStreamAttributes rattrs = reader.scAttributes();
	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		ScXmlStreamReader::TokenType tType = reader.readNext();
		if (tType == ScXmlStreamReader::EndElement && reader.name() == tagName)
			break;
		//Remove uppercase in 1.8 format
		if (tType == ScXmlStreamReader::StartElement && reader.name() == QLatin1String("CSTOP"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			QString name = attrs.valueAsString("NAME");
			double ramp = attrs.valueAsDouble("RAMP", 0.0);
			int shade = attrs.valueAsInt("SHADE", 100);
			double opa = attrs.valueAsDouble("TRANS", 1.0);
			gra.addStop(SetColor(doc, name, shade), ramp, 0.5, opa, name, shade);
		}
		else
			if (tType == ScXmlStreamReader::StartElement && reader.name() == QLatin1String("ColorStop"))
			{
				ScXmlStreamAttributes attrs = reader.scAttributes();
				QString name = attrs.valueAsString("Name");
				double ramp = attrs.valueAsDouble("Ramp", 0.0);
				int shade = attrs.valueAsInt("Shade", 100);
				double opa = attrs.valueAsDouble("Opacity", 1.0);
				gra.addStop(SetColor(doc, name, shade), ramp, 0.5, opa, name, shade);
			}
	}
	return !reader.hasError();
}

void Scribus171Format::readCharacterStyleAttrs(ScribusDoc *doc, const ScXmlStreamAttributes& attrs, CharStyle & newStyle) const
{
	//Remove uppercase in 1.8 format

	static const QString CPARENT("CPARENT");
	static const QString CPARENT171("CParent");
	if (attrs.hasAttribute(CPARENT))
	{
		QString parentStyle = attrs.valueAsString(CPARENT);
		if (!parentStyle.isEmpty())
			parentStyle = charStyleMap.value(parentStyle, parentStyle);
		newStyle.setParent(parentStyle);
	}
	else if (attrs.hasAttribute(CPARENT171))
	{
		QString parentStyle = attrs.valueAsString(CPARENT171);
		if (!parentStyle.isEmpty())
			parentStyle = charStyleMap.value(parentStyle, parentStyle);
		newStyle.setParent(parentStyle);
	}

	static const QString FONT("FONT");
	static const QString FONT171("Font");
	if (attrs.hasAttribute(FONT))
	{
		const ScFace& face = m_AvailableFonts->findFont(attrs.valueAsString(FONT), doc);
		if (!face.isNone())
			newStyle.setFont(face);
	}
	else if (attrs.hasAttribute(FONT171))
	{
		const ScFace& face = m_AvailableFonts->findFont(attrs.valueAsString(FONT171), doc);
		if (!face.isNone())
			newStyle.setFont(face);
	}

	static const QString FONTSIZE("FONTSIZE");
	static const QString FONTSIZE171("FontSize");
	if (attrs.hasAttribute(FONTSIZE))
		newStyle.setFontSize(qRound(attrs.valueAsDouble(FONTSIZE) * 10));
	else if (attrs.hasAttribute(FONTSIZE171))
		newStyle.setFontSize(qRound(attrs.valueAsDouble(FONTSIZE171) * 10));

	static const QString FONTFEATURES("FONTFEATURES");
	static const QString FONTFEATURES171("FontFeatures");
	if (attrs.hasAttribute(FONTFEATURES))
		newStyle.setFontFeatures(attrs.valueAsString(FONTFEATURES));
	else if (attrs.hasAttribute(FONTFEATURES171))
		newStyle.setFontFeatures(attrs.valueAsString(FONTFEATURES171));

	static const QString FCOLOR("FCOLOR");
	static const QString FCOLOR171("FontColor");
	if (attrs.hasAttribute(FCOLOR))
		newStyle.setFillColor(attrs.valueAsString(FCOLOR));
	else if (attrs.hasAttribute(FCOLOR171))
		newStyle.setFillColor(attrs.valueAsString(FCOLOR171));

	static const QString HyphenChar("HyphenChar");
	static const QString HyphenChar171("HyphenCharacter");
	if (attrs.hasAttribute(HyphenChar))
		newStyle.setHyphenChar(attrs.valueAsInt(HyphenChar));
	else if (attrs.hasAttribute(HyphenChar171))
		newStyle.setHyphenChar(attrs.valueAsInt(HyphenChar171));

	static const QString HyphenWordMin("HyphenWordMin");
	static const QString HyphenWordMin171("HyphenWordMinimum");
	if (attrs.hasAttribute(HyphenWordMin))
		newStyle.setHyphenWordMin(attrs.valueAsInt(HyphenWordMin));
	else if (attrs.hasAttribute(HyphenWordMin171))
		newStyle.setHyphenWordMin(attrs.valueAsInt(HyphenWordMin171));

	static const QString KERN("KERN");
	static const QString KERN171("Kerning");
	if (attrs.hasAttribute(KERN))
		newStyle.setTracking(qRound(attrs.valueAsDouble(KERN) * 10));
	else if (attrs.hasAttribute(KERN171))
		newStyle.setTracking(qRound(attrs.valueAsDouble(KERN171) * 10));

	static const QString FSHADE("FSHADE");
	static const QString FSHADE171("FillShade");
	if (attrs.hasAttribute(FSHADE))
		newStyle.setFillShade(attrs.valueAsInt(FSHADE));
	else if (attrs.hasAttribute(FSHADE171))
		newStyle.setFillShade(attrs.valueAsInt(FSHADE171));

	static const QString EFFECTS("EFFECTS");
	if (attrs.hasAttribute(EFFECTS))
		newStyle.setFeatures(static_cast<StyleFlag>(attrs.valueAsInt(EFFECTS)).featureList());

	static const QString EFFECT("EFFECT");
	if (attrs.hasAttribute(EFFECT))
		newStyle.setFeatures(static_cast<StyleFlag>(attrs.valueAsInt(EFFECT)).featureList());

	static const QString FEATURES("FEATURES");
	static const QString FEATURES171("Features");
	if (attrs.hasAttribute(FEATURES))
		newStyle.setFeatures(attrs.valueAsString(FEATURES).split(" ", Qt::SkipEmptyParts));
	else if (attrs.hasAttribute(FEATURES171))
		newStyle.setFeatures(attrs.valueAsString(FEATURES171).split(" ", Qt::SkipEmptyParts));

	static const QString SCOLOR("SCOLOR");
	static const QString SCOLOR171("StrokeColor");
	if (attrs.hasAttribute(SCOLOR))
		newStyle.setStrokeColor(attrs.valueAsString(SCOLOR, CommonStrings::None));
	else if (attrs.hasAttribute(SCOLOR171))
		newStyle.setStrokeColor(attrs.valueAsString(SCOLOR171, CommonStrings::None));

	static const QString BCOLOR("BGCOLOR");
	static const QString BCOLOR171("BackgroundColor");
	if (attrs.hasAttribute(BCOLOR))
		newStyle.setBackColor(attrs.valueAsString(BCOLOR, CommonStrings::None));
	else if (attrs.hasAttribute(BCOLOR171))
		newStyle.setBackColor(attrs.valueAsString(BCOLOR171, CommonStrings::None));

	static const QString BSHADE("BGSHADE");
	static const QString BSHADE171("BackgroundShade");
	if (attrs.hasAttribute(BSHADE))
		newStyle.setBackShade(attrs.valueAsInt(BSHADE, 100));
	if (attrs.hasAttribute(BSHADE171))
		newStyle.setBackShade(attrs.valueAsInt(BSHADE171, 100));

	static const QString SSHADE("SSHADE");
	static const QString SSHADE171("StrokeShade");
	if (attrs.hasAttribute(SSHADE))
		newStyle.setStrokeShade(attrs.valueAsInt(SSHADE));
	else if (attrs.hasAttribute(SSHADE171))
		newStyle.setStrokeShade(attrs.valueAsInt(SSHADE171));

	static const QString SCALEH("SCALEH");
	static const QString SCALEH171("ScaleHorizontal");
	if (attrs.hasAttribute(SCALEH))
		newStyle.setScaleH(qRound(attrs.valueAsDouble(SCALEH) * 10));
	else if (attrs.hasAttribute(SCALEH171))
		newStyle.setScaleH(qRound(attrs.valueAsDouble(SCALEH171) * 10));

	static const QString SCALEV("SCALEV");
	static const QString SCALEV171("ScaleVertical");
	if (attrs.hasAttribute(SCALEV))
		newStyle.setScaleV(qRound(attrs.valueAsDouble(SCALEV) * 10));
	else if (attrs.hasAttribute(SCALEV171))
		newStyle.setScaleV(qRound(attrs.valueAsDouble(SCALEV171) * 10));

	static const QString BASEO("BASEO");
	static const QString BASEO171("BaselineOffset");
	if (attrs.hasAttribute(BASEO))
		newStyle.setBaselineOffset(qRound(attrs.valueAsDouble(BASEO) * 10));
	else if (attrs.hasAttribute(BASEO171))
		newStyle.setBaselineOffset(qRound(attrs.valueAsDouble(BASEO171) * 10));

	static const QString TXTSHX("TXTSHX");
	static const QString TXTSHX171("TextShadowXOffset");
	if (attrs.hasAttribute(TXTSHX))
		newStyle.setShadowXOffset(qRound(attrs.valueAsDouble(TXTSHX) * 10));
	else if (attrs.hasAttribute(TXTSHX171))
		newStyle.setShadowXOffset(qRound(attrs.valueAsDouble(TXTSHX171) * 10));

	static const QString TXTSHY("TXTSHY");
	static const QString TXTSHY171("TextShadowYOffset");
	if (attrs.hasAttribute(TXTSHY))
		newStyle.setShadowYOffset(qRound(attrs.valueAsDouble(TXTSHY) * 10));
	else if (attrs.hasAttribute(TXTSHY171))
		newStyle.setShadowYOffset(qRound(attrs.valueAsDouble(TXTSHY171) * 10));

	static const QString TXTOUT("TXTOUT");
	static const QString TXTOUT171("TextOutlineWidth");
	if (attrs.hasAttribute(TXTOUT))
		newStyle.setOutlineWidth(qRound(attrs.valueAsDouble(TXTOUT) * 10));
	else if (attrs.hasAttribute(TXTOUT171))
		newStyle.setOutlineWidth(qRound(attrs.valueAsDouble(TXTOUT171) * 10));

	static const QString TXTULP("TXTULP");
	static const QString TXTULP171("TextUnderlineOffset");
	if (attrs.hasAttribute(TXTULP))
		newStyle.setUnderlineOffset(qRound(attrs.valueAsDouble(TXTULP) * 10));
	else if (attrs.hasAttribute(TXTULP171))
		newStyle.setUnderlineOffset(qRound(attrs.valueAsDouble(TXTULP171) * 10));

	static const QString TXTULW("TXTULW");
	static const QString TXTULW171("TextUnderlineWidth");
	if (attrs.hasAttribute(TXTULW))
		newStyle.setUnderlineWidth(qRound(attrs.valueAsDouble(TXTULW) * 10));
	else if (attrs.hasAttribute(TXTULW171))
		newStyle.setUnderlineWidth(qRound(attrs.valueAsDouble(TXTULW171) * 10));

	static const QString TXTSTP("TXTSTP");
	static const QString TXTSTP171("TextStrikeThroughOffset");
	if (attrs.hasAttribute(TXTSTP))
		newStyle.setStrikethruOffset(qRound(attrs.valueAsDouble(TXTSTP) * 10));
	else if (attrs.hasAttribute(TXTSTP171))
		newStyle.setStrikethruOffset(qRound(attrs.valueAsDouble(TXTSTP171) * 10));

	static const QString TXTSTW("TXTSTW");
	static const QString TXTSTW171("TextStrikeThroughWidth");
	if (attrs.hasAttribute(TXTSTW))
		newStyle.setStrikethruWidth(qRound(attrs.valueAsDouble(TXTSTW) * 10));
	else if (attrs.hasAttribute(TXTSTW171))
		newStyle.setStrikethruWidth(qRound(attrs.valueAsDouble(TXTSTW171) * 10));

	static const QString LANGUAGE("LANGUAGE");
	static const QString LANGUAGE171("Language");
	if (attrs.hasAttribute(LANGUAGE))
	{
		QString l(attrs.valueAsString(LANGUAGE));
		if (LanguageManager::instance()->langTableIndex(l) != -1)
			newStyle.setLanguage(l); //new style storage
		else
		{ //old style storage
			QString lnew = LanguageManager::instance()->getAbbrevFromLang(l, false);
			if (lnew.isEmpty())
				lnew = LanguageManager::instance()->getAbbrevFromLang(l, false);
			newStyle.setLanguage(lnew);
		}
	}
	else if (attrs.hasAttribute(LANGUAGE171))
	{
		QString l(attrs.valueAsString(LANGUAGE171));
		if (LanguageManager::instance()->langTableIndex(l) != -1)
			newStyle.setLanguage(l); //new style storage
		else
		{ //old style storage
			QString lnew = LanguageManager::instance()->getAbbrevFromLang(l, false);
			if (lnew.isEmpty())
				lnew = LanguageManager::instance()->getAbbrevFromLang(l, false);
			newStyle.setLanguage(lnew);
		}
	}

	static const QString SHORTCUT("SHORTCUT");
	static const QString SHORTCUT171("Shortcut");
	if (attrs.hasAttribute(SHORTCUT))
		newStyle.setShortcut(attrs.valueAsString(SHORTCUT));
	else if (attrs.hasAttribute(SHORTCUT171))
		newStyle.setShortcut(attrs.valueAsString(SHORTCUT171));

	static const QString WORDTRACK("wordTrack");
	if (attrs.hasAttribute(WORDTRACK))
		newStyle.setWordTracking(attrs.valueAsDouble(WORDTRACK));
}

void Scribus171Format::readNamedCharacterStyleAttrs(ScribusDoc *doc, const ScXmlStreamAttributes& attrs, CharStyle & newStyle) const
{
	//Remove uppercase in 1.8 format
	static const QString CNAME("CNAME");
	if (attrs.hasAttribute("Name"))
		newStyle.setName(attrs.valueAsString("Name"));
	else
		newStyle.setName(attrs.valueAsString(CNAME));

	// The default style attribute must be correctly set before trying to assign a parent
	static const QString DEFAULTSTYLE("DefaultStyle");
	if (newStyle.hasName() && attrs.hasAttribute(DEFAULTSTYLE))
		newStyle.setDefaultStyle(attrs.valueAsInt(DEFAULTSTYLE));
	else if (newStyle.name() == CommonStrings::DefaultCharacterStyle || newStyle.name() == CommonStrings::trDefaultCharacterStyle)
		newStyle.setDefaultStyle(true);
	else
		newStyle.setDefaultStyle(false);

	readCharacterStyleAttrs(doc, attrs, newStyle);

	// Check that a style is not its own parent
	QString parentStyle = newStyle.parent();
	if (parentStyle == newStyle.name())
		newStyle.setParent(QString());
}

void Scribus171Format::readParagraphStyle(ScribusDoc *doc, ScXmlStreamReader& reader, ParagraphStyle& newStyle) const
{
	ScXmlStreamAttributes attrs = reader.scAttributes();
	//Remove uppercase in 1.8 format
	newStyle.erase();
	if (attrs.hasAttribute("Name"))
		newStyle.setName(attrs.valueAsString("Name"));
	else
		newStyle.setName(attrs.valueAsString("NAME"));

	// The default style attribute must be correctly set before trying to assign a parent
	static const QString DEFAULTSTYLE("DefaultStyle");
	if (attrs.hasAttribute(DEFAULTSTYLE))
		newStyle.setDefaultStyle(attrs.valueAsInt(DEFAULTSTYLE));
	else if (newStyle.name() == CommonStrings::DefaultParagraphStyle || newStyle.name() == CommonStrings::trDefaultParagraphStyle)
		newStyle.setDefaultStyle(true);
	else
		newStyle.setDefaultStyle(false);
	//Remove uppercase in 1.8 format
	QString parentStyle;
	if (attrs.hasAttribute("PARENT"))
		parentStyle = attrs.valueAsString("PARENT", QString());
	else
		parentStyle = attrs.valueAsString("Parent", QString());
	if (!parentStyle.isEmpty() && (parentStyle != newStyle.name()))
	{
		parentStyle = parStyleMap.value(parentStyle, parentStyle);
		if (m_Doc->styleExists(parentStyle))
			newStyle.setParent(parentStyle);
		else
			newStyle.setParent(CommonStrings::DefaultParagraphStyle);
	}

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("LINESPMode"))
		newStyle.setLineSpacingMode(static_cast<ParagraphStyle::LineSpacingMode>(attrs.valueAsInt("LINESPMode")));
	else if (attrs.hasAttribute("LineSpacingMode"))
		newStyle.setLineSpacingMode(static_cast<ParagraphStyle::LineSpacingMode>(attrs.valueAsInt("LineSpacingMode")));

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("LINESP"))
		newStyle.setLineSpacing(attrs.valueAsDouble("LINESP"));
	else if (attrs.hasAttribute("LineSpacing"))
		newStyle.setLineSpacing(attrs.valueAsDouble("LineSpacing"));

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("INDENT"))
		newStyle.setLeftMargin(attrs.valueAsDouble("INDENT"));
	else if (attrs.hasAttribute("LeftMargin"))
		newStyle.setLeftMargin(attrs.valueAsDouble("LeftMargin"));

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("RMARGIN"))
		newStyle.setRightMargin(attrs.valueAsDouble("RMARGIN"));
	else if (attrs.hasAttribute("RightMargin"))
		newStyle.setRightMargin(attrs.valueAsDouble("RightMargin"));

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("FIRST"))
		newStyle.setFirstIndent(attrs.valueAsDouble("FIRST"));
	else if (attrs.hasAttribute("FirstIndent"))
		newStyle.setFirstIndent(attrs.valueAsDouble("FirstIndent"));

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("ALIGN"))
		newStyle.setAlignment(static_cast<ParagraphStyle::AlignmentType>(attrs.valueAsInt("ALIGN")));
	else if (attrs.hasAttribute("Alignment"))
		newStyle.setAlignment(static_cast<ParagraphStyle::AlignmentType>(attrs.valueAsInt("Alignment")));

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("DIRECTION"))
		newStyle.setDirection(static_cast<ParagraphStyle::DirectionType>(attrs.valueAsInt("DIRECTION")));
	else if (attrs.hasAttribute("Direction"))
		newStyle.setDirection(static_cast<ParagraphStyle::DirectionType>(attrs.valueAsInt("Direction")));

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("VOR"))
		newStyle.setGapBefore(attrs.valueAsDouble("VOR"));
	else if (attrs.hasAttribute("GapBefore"))
		newStyle.setGapBefore(attrs.valueAsDouble("GapBefore"));

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("NACH"))
		newStyle.setGapAfter(attrs.valueAsDouble("NACH"));
	else if (attrs.hasAttribute("GapAfter"))
		newStyle.setGapAfter(attrs.valueAsDouble("GapAfter"));

	static const QString ParagraphEffectCharStyle("ParagraphEffectCharStyle");
	if (attrs.hasAttribute(ParagraphEffectCharStyle))
		newStyle.setPeCharStyleName(attrs.valueAsString(ParagraphEffectCharStyle));

	static const QString ParagraphEffectOffset("ParagraphEffectOffset");
	if (attrs.hasAttribute(ParagraphEffectOffset))
		newStyle.setParEffectOffset(attrs.valueAsDouble(ParagraphEffectOffset));

	static const QString ParagraphEffectIndent("ParagraphEffectIndent");
	if (attrs.hasAttribute(ParagraphEffectIndent))
		newStyle.setParEffectIndent(attrs.valueAsDouble(ParagraphEffectIndent));

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("DROP"))
		newStyle.setHasDropCap(static_cast<bool>(attrs.valueAsInt("DROP")));
	else if (attrs.hasAttribute("HasDropCap"))
		newStyle.setHasDropCap(static_cast<bool>(attrs.valueAsInt("HasDropCap")));

	// static const QString DROPCHSTYLE("DROPCHSTYLE");
	// if (attrs.hasAttribute(DROPCHSTYLE))
	// 	newStyle.setPeCharStyleName(attrs.valueAsString(DROPCHSTYLE));

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("DROPLIN"))
		newStyle.setDropCapLines(attrs.valueAsInt("DROPLIN"));
	else if (attrs.hasAttribute("DropCapLines"))
		newStyle.setDropCapLines(attrs.valueAsInt("DropCapLines"));

	// static const QString DROPDIST("DROPDIST");
	// if (attrs.hasAttribute(DROPDIST))
	// 	newStyle.setParEffectOffset(attrs.valueAsDouble(DROPDIST));

	static const QString Bullet("Bullet");
	if (attrs.hasAttribute(Bullet))
		newStyle.setHasBullet(static_cast<bool>(attrs.valueAsInt(Bullet)));

	static const QString BulletStr("BulletStr");
	if (attrs.hasAttribute(BulletStr))
		newStyle.setBulletStr(attrs.valueAsString(BulletStr));

	static const QString Numeration("Numeration");
	if (attrs.hasAttribute(Numeration))
		newStyle.setHasNum(static_cast<bool>(attrs.valueAsInt(Numeration)));

	static const QString NumerationName("NumerationName");
	if (attrs.hasAttribute(NumerationName))
		newStyle.setNumName(attrs.valueAsString(NumerationName));

	static const QString NumerationFormat("NumerationFormat");
	if (attrs.hasAttribute(NumerationFormat))
		newStyle.setNumFormat(attrs.valueAsInt(NumerationFormat));

	static const QString NumerationLevel("NumerationLevel");
	if (attrs.hasAttribute(NumerationLevel))
		newStyle.setNumLevel(attrs.valueAsInt(NumerationLevel));

	static const QString NumerationStart("NumerationStart");
	if (attrs.hasAttribute(NumerationStart))
		newStyle.setNumStart(attrs.valueAsInt(NumerationStart));

	static const QString NumearationPrefix("NumerationPrefix");
	if (attrs.hasAttribute(NumearationPrefix))
		newStyle.setNumPrefix(attrs.valueAsString(NumearationPrefix));

	static const QString NumerationSuffix("NumerationSuffix");
	if (attrs.hasAttribute(NumerationSuffix))
		newStyle.setNumSuffix(attrs.valueAsString(NumerationSuffix));

	static const QString NumerationRestart("NumerationRestart");
	if (attrs.hasAttribute(NumerationRestart))
		newStyle.setNumRestart(attrs.valueAsInt(NumerationRestart));

	static const QString NumerationOther("NumerationOther");
	if (attrs.hasAttribute(NumerationOther))
		newStyle.setNumOther(static_cast<bool>(attrs.valueAsInt(NumerationOther)));

	static const QString NumerationHigher("NumerationHigher");
	if (attrs.hasAttribute(NumerationHigher))
		newStyle.setNumHigher(static_cast<bool>(attrs.valueAsInt(NumerationHigher)));

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("PSHORTCUT"))
		newStyle.setShortcut(attrs.valueAsString("PSHORTCUT"));
	else if (attrs.hasAttribute("ParagraphStyleShortcut"))
		newStyle.setShortcut(attrs.valueAsString("ParagraphStyleShortcut"));

	static const QString OpticalMargins("OpticalMargins");
	if (attrs.hasAttribute(OpticalMargins))
		newStyle.setOpticalMargins(attrs.valueAsInt(OpticalMargins));

	static const QString OpticalMarginSetId("OpticalMarginSetId");
	if (attrs.hasAttribute(OpticalMarginSetId))
		newStyle.setOpticalMarginSetId(attrs.valueAsString(OpticalMarginSetId));

	static const QString HyphenConsecutiveLines("HyphenConsecutiveLines");
	if (attrs.hasAttribute(HyphenConsecutiveLines))
		newStyle.setHyphenConsecutiveLines(attrs.valueAsInt(HyphenConsecutiveLines));

	static const QString HyphenationMode("HyphenationMode");
	if (attrs.hasAttribute(HyphenationMode))
		newStyle.setHyphenationMode(attrs.valueAsInt(HyphenationMode));

	static const QString MinWordTrack("MinWordTrack");
	if (attrs.hasAttribute(MinWordTrack))
		newStyle.setMinWordTracking(attrs.valueAsDouble(MinWordTrack));

	static const QString NormWordTrack("NormWordTrack");
	if (attrs.hasAttribute(NormWordTrack))
		newStyle.charStyle().setWordTracking(attrs.valueAsDouble(NormWordTrack));

	static const QString MinGlyphShrink("MinGlyphShrink");
	if (attrs.hasAttribute(MinGlyphShrink))
		newStyle.setMinGlyphExtension(attrs.valueAsDouble(MinGlyphShrink));

	static const QString MaxGlyphExtend("MaxGlyphExtend");
	if (attrs.hasAttribute(MaxGlyphExtend))
		newStyle.setMaxGlyphExtension(attrs.valueAsDouble(MaxGlyphExtend));

	static const QString KeepLinesStart("KeepLinesStart");
	if (attrs.hasAttribute(KeepLinesStart))
		newStyle.setKeepLinesStart(attrs.valueAsInt(KeepLinesStart));

	static const QString KeepLinesEnd("KeepLinesEnd");
	if (attrs.hasAttribute(KeepLinesEnd))
		newStyle.setKeepLinesEnd(attrs.valueAsInt(KeepLinesEnd));

	static const QString KeepWithNext("KeepWithNext");
	if (attrs.hasAttribute(KeepWithNext))
		newStyle.setKeepWithNext(attrs.valueAsInt(KeepWithNext));

	static const QString KeepTogether("KeepTogether");
	if (attrs.hasAttribute(KeepTogether))
		newStyle.setKeepTogether(attrs.valueAsInt(KeepTogether));
	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("BCOLOR"))
		newStyle.setBackgroundColor(attrs.valueAsString("BCOLOR", CommonStrings::None));
	else if (attrs.hasAttribute("ParagraphBackgroundColor"))
		newStyle.setBackgroundColor(attrs.valueAsString("ParagraphBackgroundColor", CommonStrings::None));
	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("BSHADE"))
		newStyle.setBackgroundShade(attrs.valueAsInt("BSHADE", 100));
	else if (attrs.hasAttribute("ParagraphBackgroundColorShade"))
		newStyle.setBackgroundShade(attrs.valueAsInt("ParagraphBackgroundColorShade", 100));

	readCharacterStyleAttrs(doc, attrs, newStyle.charStyle());

	//	newStyle.tabValues().clear();
	QList<ParagraphStyle::TabRecord> tbs;
	newStyle.resetTabValues();
	QString thisTagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == thisTagName)
			break;
		if (reader.isStartElement() && reader.name() == QLatin1String("Tabs"))
		{
			ParagraphStyle::TabRecord tb;
			ScXmlStreamAttributes attrs2 = reader.scAttributes();
			tb.tabPosition = attrs2.valueAsDouble("Pos");
			tb.tabType = attrs2.valueAsInt("Type");
			QString tbCh(attrs2.valueAsString("Fill",""));
			if (!tbCh.isEmpty())
				tb.tabFillChar = tbCh[0];
			tbs.append(tb);
		}
	}
	if (tbs.count() > 0)
		newStyle.setTabValues(tbs);
	
	fixLegacyParStyle(newStyle);
}

void Scribus171Format::readTableStyle(ScribusDoc *doc, ScXmlStreamReader& reader, TableStyle& newStyle) const
{
	//Remove uppercase in 1.8 format
	ScXmlStreamAttributes attrs = reader.scAttributes();
	newStyle.erase();
	if (attrs.hasAttribute("Name"))
		newStyle.setName(attrs.valueAsString("Name", ""));
	else
		newStyle.setName(attrs.valueAsString("NAME", ""));

	// The default style attribute must be correctly set before trying to assign a parent
	if (attrs.hasAttribute("DefaultStyle"))
		newStyle.setDefaultStyle(attrs.valueAsInt("DefaultStyle"));
	else if (newStyle.name() == CommonStrings::DefaultTableStyle || newStyle.name() == CommonStrings::trDefaultTableStyle)
		newStyle.setDefaultStyle(true);
	else
		newStyle.setDefaultStyle(false);
	//Remove uppercase in 1.8 format
	QString parentStyle;
	if (attrs.hasAttribute("Parent"))
		parentStyle = attrs.valueAsString("Parent", "");
	else
		parentStyle = attrs.valueAsString("PARENT", "");
	if (!parentStyle.isEmpty() && (parentStyle != newStyle.name()))
		newStyle.setParent(parentStyle);
	if (attrs.hasAttribute("FillColor"))
		newStyle.setFillColor(attrs.valueAsString("FillColor"));
	if (attrs.hasAttribute("FillShade"))
		newStyle.setFillShade(attrs.valueAsDouble("FillShade"));

	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (!reader.isStartElement())
			continue;
		if (reader.name() == QLatin1String("TableBorderLeft"))
		{
			TableBorder border;
			readTableBorderLines(doc, reader, border);
			newStyle.setLeftBorder(border);
		}
		else if (reader.name() == QLatin1String("TableBorderRight"))
		{
			TableBorder border;
			readTableBorderLines(doc, reader, border);
			newStyle.setRightBorder(border);
		}
		else if (reader.name() == QLatin1String("TableBorderTop"))
		{
			TableBorder border;
			readTableBorderLines(doc, reader, border);
			newStyle.setTopBorder(border);
		}
		else if (reader.name() == QLatin1String("TableBorderBottom"))
		{
			TableBorder border;
			readTableBorderLines(doc, reader, border);
			newStyle.setBottomBorder(border);
		}
		else
			reader.skipCurrentElement();
	}
}

void Scribus171Format::readTableBorderLines(ScribusDoc* /*doc*/, ScXmlStreamReader& reader, TableBorder& border) const
{
	QStringView tagName = reader.name();
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (!reader.isStartElement())
			continue;
		if (reader.name() == QLatin1String("TableBorderLine"))
		{
			ScXmlStreamAttributes tAttB = reader.scAttributes();
			double width = tAttB.valueAsDouble("Width", 0.0);
			QString color = tAttB.valueAsString("Color", CommonStrings::None);
			double shade = tAttB.valueAsDouble("Shade", 100.0);
			int style = tAttB.valueAsInt("PenStyle", 1);
			border.addBorderLine(TableBorderLine(width, static_cast<Qt::PenStyle>(style), color, shade));
		}
		else
			reader.skipCurrentElement();
	}
}

void Scribus171Format::readCellStyle(ScribusDoc *doc, ScXmlStreamReader& reader, CellStyle& newStyle) const
{
	//Remove uppercase in 1.8 format
	ScXmlStreamAttributes attrs = reader.scAttributes();
	newStyle.erase();
	newStyle.erase();
	if (attrs.hasAttribute("Name"))
		newStyle.setName(attrs.valueAsString("Name", ""));
	else
		newStyle.setName(attrs.valueAsString("NAME", ""));

	// The default style attribute must be correctly set before trying to assign a parent
	if (attrs.hasAttribute("DefaultStyle"))
		newStyle.setDefaultStyle(attrs.valueAsInt("DefaultStyle"));
	else if (newStyle.name() == CommonStrings::DefaultCellStyle || newStyle.name() == CommonStrings::trDefaultCellStyle)
		newStyle.setDefaultStyle(true);
	else
		newStyle.setDefaultStyle(false);
	//Remove uppercase in 1.8 format
	QString parentStyle;
	if (attrs.hasAttribute("Parent"))
		parentStyle = attrs.valueAsString("Parent", "");
	else
		parentStyle = attrs.valueAsString("PARENT", "");
	if (!parentStyle.isEmpty() && (parentStyle != newStyle.name()))
		newStyle.setParent(parentStyle);
	if (attrs.hasAttribute("FillColor"))
		newStyle.setFillColor(attrs.valueAsString("FillColor"));
	if (attrs.hasAttribute("FillShade"))
		newStyle.setFillShade(attrs.valueAsDouble("FillShade"));
	if (attrs.hasAttribute("LeftPadding"))
		newStyle.setLeftPadding(attrs.valueAsDouble("LeftPadding", 0.0));
	if (attrs.hasAttribute("RightPadding"))
		newStyle.setRightPadding(attrs.valueAsDouble("RightPadding", 0.0));
	if (attrs.hasAttribute("TopPadding"))
		newStyle.setTopPadding(attrs.valueAsDouble("TopPadding", 0.0));
	if (attrs.hasAttribute("BottomPadding"))
		newStyle.setBottomPadding(attrs.valueAsDouble("BottomPadding", 0.0));

	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (!reader.isStartElement())
			break;
		if (reader.name() == QLatin1String("TableBorderLeft"))
		{
			TableBorder border;
			readTableBorderLines(doc, reader, border);
			newStyle.setLeftBorder(border);
		}
		else if (reader.name() == QLatin1String("TableBorderRight"))
		{
			TableBorder border;
			readTableBorderLines(doc, reader, border);
			newStyle.setRightBorder(border);
		}
		else if (reader.name() == QLatin1String("TableBorderTop"))
		{
			TableBorder border;
			readTableBorderLines(doc, reader, border);
			newStyle.setTopBorder(border);
		}
		else if (reader.name() == QLatin1String("TableBorderBottom"))
		{
			TableBorder border;
			readTableBorderLines(doc, reader, border);
			newStyle.setBottomBorder(border);
		}
		else
		{
			reader.skipCurrentElement();
		}
	}
}

void Scribus171Format::readLayers(ScLayer& layer, const ScXmlStreamAttributes& attrs) const
{
	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("NUMMER"))
	{
		int lId = attrs.valueAsInt("NUMMER");
		int level = attrs.valueAsInt("LEVEL");
		layer = ScLayer( attrs.valueAsString("NAME"), level, lId);
		layer.isViewable = attrs.valueAsInt("SICHTBAR");
		layer.isPrintable = attrs.valueAsInt("DRUCKEN");
		layer.isEditable = attrs.valueAsInt("EDIT", 1);
		layer.flowControl = attrs.valueAsInt("FLOW", 1);
		layer.isSelectable = attrs.valueAsInt("SELECT", 0);
		layer.transparency = attrs.valueAsDouble("TRANS", 1.0);
		layer.blendMode = attrs.valueAsInt("BLEND", 0);
		layer.outlineMode = attrs.valueAsInt("OUTL", 0);
		if (attrs.hasAttribute("LAYERC"))
			layer.markerColor =  QColor(attrs.valueAsString("LAYERC","#000000"));
		return;
	}
	if (attrs.hasAttribute("Number"))
	{
		int lId = attrs.valueAsInt("Number");
		int level = attrs.valueAsInt("Level");
		layer = ScLayer( attrs.valueAsString("Name"), level, lId);
		layer.isViewable = attrs.valueAsInt("IsViewable");
		layer.isPrintable = attrs.valueAsInt("IsPrintable");
		layer.isEditable = attrs.valueAsInt("IsEditable", 1);
		layer.flowControl = attrs.valueAsInt("FlowControl", 1);
		layer.isSelectable = attrs.valueAsInt("IsSelectable", 0);
		layer.transparency = attrs.valueAsDouble("Transparency", 1.0);
		layer.blendMode = attrs.valueAsInt("BlendMode", 0);
		layer.outlineMode = attrs.valueAsInt("OutlineMode", 0);
		if (attrs.hasAttribute("LayerColor"))
			layer.markerColor =  QColor(attrs.valueAsString("LayerColor","#000000"));
	}
}

bool Scribus171Format::readArrows(ScribusDoc* doc, ScXmlStreamAttributes& attrs) const
{
	double x;
	double y;
	struct ArrowDesc arrow;
	arrow.name = attrs.valueAsString("Name");
	arrow.userArrow = true;
	QString tmp = attrs.valueAsString("Points");
	ScTextStream fp(&tmp, QIODevice::ReadOnly);
	unsigned int numPoints = attrs.valueAsUInt("NumPoints");
	for (uint i = 0; i < numPoints; ++i)
	{
		fp >> x;
		fp >> y;
		arrow.points.addPoint(x, y);
	}
	if (!doc->hasArrowStyle(arrow.name))
		doc->appendToArrowStyles(arrow);
	return true;
}

bool Scribus171Format::readMultiline(MultiLine& ml, ScXmlStreamReader& reader) const
{
	ml = MultiLine();
	ScXmlStreamAttributes rattrs = reader.scAttributes();
	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		ScXmlStreamReader::TokenType tType = reader.readNext();
		if (tType == ScXmlStreamReader::EndElement && reader.name() == tagName)
			break;
		if (tType == ScXmlStreamReader::StartElement && reader.name() == QLatin1String("SubLine"))
		{
			struct SingleLine sl;
			ScXmlStreamAttributes attrs = reader.scAttributes();
			sl.Color = attrs.valueAsString("Color");
			sl.Dash = attrs.valueAsInt("Dash");
			sl.LineEnd = attrs.valueAsInt("LineEnd");
			sl.LineJoin = attrs.valueAsInt("LineJoin");
			sl.Shade = attrs.valueAsInt("Shade");
			sl.Width = attrs.valueAsDouble("Width");
			ml.shortcut = attrs.valueAsString("Shortcut");
			ml.push_back(sl);
		}
	}
	return !reader.hasError();
}

bool Scribus171Format::readBookMark(ScribusDoc::BookMa& bookmark, int& elem, const ScXmlStreamAttributes& attrs) const
{
	elem = attrs.valueAsInt("Element");
	bookmark.PageObject = nullptr;
	bookmark.Title = attrs.valueAsString("Title");
	bookmark.Text = attrs.valueAsString("Text");
	//Fix in 1.8 format
	if (attrs.hasAttribute("Aktion"))
		bookmark.Action = attrs.valueAsString("Aktion");
	else
		bookmark.Action = attrs.valueAsString("Action");
	//Fix in 1.8 format
	if (attrs.hasAttribute("ItemNr"))
		bookmark.ItemNr = attrs.valueAsInt("ItemNr");
	else
		bookmark.ItemNr = attrs.valueAsInt("ItemNumber");
	bookmark.First = attrs.valueAsInt("First");
	bookmark.Last = attrs.valueAsInt("Last");
	//Fix in 1.8 format
	if (attrs.hasAttribute("Prev"))
		bookmark.Prev = attrs.valueAsInt("Prev");
	else
		bookmark.Prev = attrs.valueAsInt("Previous");
	bookmark.Next = attrs.valueAsInt("Next");
	bookmark.Parent = attrs.valueAsInt("Parent");
	return true;
}

bool Scribus171Format::readPDFOptions(ScribusDoc* doc, ScXmlStreamReader& reader)
{
	ScXmlStreamAttributes attrs = reader.scAttributes();

	doc->pdfOptions().firstUse = attrs.valueAsBool("firstUse", true);
	doc->pdfOptions().Articles = attrs.valueAsBool("Articles");
	doc->pdfOptions().Thumbnails = attrs.valueAsBool("Thumbnails");
	doc->pdfOptions().Compress = attrs.valueAsBool("Compress");
	doc->pdfOptions().CompressMethod = (PDFOptions::PDFCompression) attrs.valueAsInt("CMethod", 0);
	doc->pdfOptions().Quality = attrs.valueAsInt("Quality", 0);
	doc->pdfOptions().RecalcPic = attrs.valueAsBool("RecalcPic");
	doc->pdfOptions().embedPDF = attrs.valueAsBool("EmbedPDF", false);
	doc->pdfOptions().Bookmarks = attrs.valueAsBool("Bookmarks");
	doc->pdfOptions().MirrorH = attrs.valueAsBool("MirrorH", false);
	doc->pdfOptions().MirrorV = attrs.valueAsBool("MirrorV", false);
	doc->pdfOptions().RotateDeg = attrs.valueAsInt("RotateDeg", 0);
	doc->pdfOptions().pageRangeSelection = attrs.valueAsInt("rangeSel", 0);
	doc->pdfOptions().pageRangeString = attrs.valueAsString("rangeTxt", "");
	doc->pdfOptions().doClip = attrs.valueAsBool("Clip", false);
	doc->pdfOptions().PresentMode = attrs.valueAsBool("PresentMode");
	doc->pdfOptions().PicRes = attrs.valueAsInt("PicRes");
	// Fixme: check input pdf version
	doc->pdfOptions().Version = (PDFVersion::Version) attrs.valueAsInt("Version");
	doc->pdfOptions().Resolution = attrs.valueAsInt("Resolution");
	doc->pdfOptions().Binding = attrs.valueAsInt("Binding");
	doc->pdfOptions().fileName = "";

	doc->pdfOptions().FontEmbedding = (PDFOptions::PDFFontEmbedding) attrs.valueAsInt("FontEmbedding", 0);
	doc->pdfOptions().isGrayscale = attrs.valueAsBool("Grayscale", false);
	doc->pdfOptions().UseRGB = attrs.valueAsBool("RGBMode", false);
	doc->pdfOptions().UseProfiles = attrs.valueAsBool("UseProfiles", false);
	doc->pdfOptions().UseProfiles2 = attrs.valueAsBool("UseProfiles2", false);
	doc->pdfOptions().Intent = attrs.valueAsInt("Intent", 1);
	doc->pdfOptions().Intent2 = attrs.valueAsInt("Intent2", 1);
	doc->pdfOptions().SolidProf = attrs.valueAsString("SolidP", "");
	doc->pdfOptions().ImageProf = attrs.valueAsString("ImageP", "");
	doc->pdfOptions().PrintProf = attrs.valueAsString("PrintP", "");
	doc->pdfOptions().Info = attrs.valueAsString("InfoString", "");
	doc->pdfOptions().bleeds.setTop(attrs.valueAsDouble("BTop", 0.0));
	doc->pdfOptions().bleeds.setLeft(attrs.valueAsDouble("BLeft", 0.0));
	doc->pdfOptions().bleeds.setRight(attrs.valueAsDouble("BRight", 0.0));
	doc->pdfOptions().bleeds.setBottom(attrs.valueAsDouble("BBottom", 0.0));
	doc->pdfOptions().useDocBleeds = attrs.valueAsBool("useDocBleeds", true);
	doc->pdfOptions().cropMarks = attrs.valueAsBool("cropMarks", false);
	doc->pdfOptions().bleedMarks = attrs.valueAsBool("bleedMarks", false);
	doc->pdfOptions().registrationMarks = attrs.valueAsBool("registrationMarks", false);
	doc->pdfOptions().colorMarks = attrs.valueAsBool("colorMarks", false);
	doc->pdfOptions().docInfoMarks = attrs.valueAsBool("docInfoMarks", false);
	doc->pdfOptions().markLength = attrs.valueAsDouble("markLength", 20.0);
	doc->pdfOptions().markOffset = attrs.valueAsDouble("markOffset", 0.0);
	doc->pdfOptions().EmbeddedI = attrs.valueAsBool("ImagePr", false);
	doc->pdfOptions().PassOwner = attrs.valueAsString("PassOwner", "");
	doc->pdfOptions().PassUser = attrs.valueAsString("PassUser", "");
	doc->pdfOptions().Permissions = attrs.valueAsInt("Permissions", -4);
	doc->pdfOptions().Encrypt = attrs.valueAsBool("Encrypt", false);
	doc->pdfOptions().useLayers = attrs.valueAsBool("UseLayers", false);
	doc->pdfOptions().UseLPI = attrs.valueAsBool("UseLpi", false);
	doc->pdfOptions().UseSpotColors = attrs.valueAsBool("UseSpotColors", true);
	doc->pdfOptions().doMultiFile = attrs.valueAsBool("doMultiFile", false);
	doc->pdfOptions().displayBookmarks =  attrs.valueAsBool("displayBookmarks", false);
	doc->pdfOptions().displayFullscreen = attrs.valueAsBool("displayFullscreen", false);
	doc->pdfOptions().displayLayers = attrs.valueAsBool("displayLayers", false);
	doc->pdfOptions().displayThumbs = attrs.valueAsBool("displayThumbs", false);
	doc->pdfOptions().hideMenuBar = attrs.valueAsBool("hideMenuBar", false);
	doc->pdfOptions().hideToolBar = attrs.valueAsBool("hideToolBar", false);
	doc->pdfOptions().fitWindow = attrs.valueAsBool("fitWindow", false);
	doc->pdfOptions().openAfterExport = attrs.valueAsBool("openAfterExport", false);
	doc->pdfOptions().PageLayout = attrs.valueAsInt("PageLayout", 0);
	doc->pdfOptions().openAction = attrs.valueAsString("openAction", "");

	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && (reader.name() == tagName))
			break;
		if (!reader.isStartElement())
			continue;
		QString tName = reader.name().toString();
		attrs = reader.scAttributes();
		if (tName == QLatin1String("LPI"))
		{
			struct LPIData lpo;
			lpo.Angle = attrs.valueAsInt("Angle");
			lpo.Frequency = attrs.valueAsInt("Frequency");
			lpo.SpotFunc = attrs.valueAsInt("SpotFunction");
			doc->pdfOptions().LPISettings[attrs.valueAsString("Color")] = lpo;
		}
		if (tName == QLatin1String("Fonts"))
		{
			QString fname = attrs.valueAsString("Name");
			if (!doc->pdfOptions().EmbedList.contains(fname))
				doc->pdfOptions().EmbedList.append(fname);
		}
		if (tName == QLatin1String("Subset"))
		{
			QString sname = attrs.valueAsString("Name");
			if (!doc->pdfOptions().SubsetList.contains(sname))
				doc->pdfOptions().SubsetList.append(sname);
		}
		if (tName == QLatin1String("Effekte"))
		{
			struct PDFPresentationData ef;
			ef.pageEffectDuration =  attrs.valueAsInt("pageEffectDuration");
			ef.pageViewDuration =  attrs.valueAsInt("pageViewDuration");
			ef.effectType = attrs.valueAsInt("effectType");
			ef.Dm = attrs.valueAsInt("Dm");
			ef.M = attrs.valueAsInt("M");
			ef.Di = attrs.valueAsInt("Di");
			pdfPresEffects.append(ef);
		}
	}
	return !reader.hasError();
}

bool Scribus171Format::readPrinterOptions(ScribusDoc* doc, ScXmlStreamReader& reader) const
{
	ScXmlStreamAttributes attrs = reader.scAttributes();
	doc->Print_Options.firstUse = attrs.valueAsBool("firstUse");
	if (doc->Print_Options.firstUse)
	{
		// Formerly we were writing uninitialized structure values in documents
		// so set these uninitialized values to something more meaningful
		PrinterUtil::getDefaultPrintOptions(doc->Print_Options, doc->bleedsVal());
		reader.readToElementEnd();
		return !reader.hasError();
	}

	doc->Print_Options.toFile = attrs.valueAsBool("toFile");
	doc->Print_Options.useAltPrintCommand = attrs.valueAsBool("useAltPrintCommand");
	doc->Print_Options.outputSeparations = attrs.valueAsBool("outputSeparations");
	doc->Print_Options.useSpotColors = attrs.valueAsBool("useSpotColors");
	doc->Print_Options.useColor = attrs.valueAsBool("useColor");
	doc->Print_Options.mirrorH = attrs.valueAsBool("mirrorH");
	doc->Print_Options.mirrorV = attrs.valueAsBool("mirrorV");
	doc->Print_Options.doGCR = attrs.valueAsBool("doGCR");
	doc->Print_Options.doClip = attrs.valueAsBool("doClip");
	doc->Print_Options.setDevParam = attrs.valueAsBool("setDevParam");
	doc->Print_Options.useDocBleeds = attrs.valueAsBool("useDocBleeds");
	doc->Print_Options.cropMarks = attrs.valueAsBool("cropMarks");
	doc->Print_Options.bleedMarks = attrs.valueAsBool("bleedMarks");
	doc->Print_Options.registrationMarks = attrs.valueAsBool("registrationMarks");
	doc->Print_Options.colorMarks = attrs.valueAsBool("colorMarks");
	doc->Print_Options.includePDFMarks = attrs.valueAsBool("includePDFMarks", true);
	if (attrs.hasAttribute("PrintEngine"))
		doc->Print_Options.prnLanguage = (PrintLanguage) attrs.valueAsInt("PrintEngine", 3);
	else
		doc->Print_Options.prnLanguage = (PrintLanguage) attrs.valueAsInt("PSLevel", 3);
	doc->Print_Options.markLength = attrs.valueAsDouble("markLength");
	doc->Print_Options.markOffset = attrs.valueAsDouble("markOffset");
	doc->Print_Options.bleeds.setTop(attrs.valueAsDouble("BleedTop"));
	doc->Print_Options.bleeds.setLeft(attrs.valueAsDouble("BleedLeft"));
	doc->Print_Options.bleeds.setRight(attrs.valueAsDouble("BleedRight"));
	doc->Print_Options.bleeds.setBottom(attrs.valueAsDouble("BleedBottom"));
	doc->Print_Options.printer = attrs.valueAsString("printer");
	doc->Print_Options.filename = attrs.valueAsString("filename");
	doc->Print_Options.separationName = attrs.valueAsString("separationName");
	doc->Print_Options.printerCommand = attrs.valueAsString("printerCommand");
	doc->Print_Options.copies = 1;

	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		ScXmlStreamReader::TokenType tType = reader.readNext();
		QString tName = reader.name().toString();
		if (tType == ScXmlStreamReader::StartElement && tName == QLatin1String("Separation"))
			doc->Print_Options.allSeparations.append(reader.attributes().value("Name").toString());
		if (tType == ScXmlStreamReader::EndElement && tName == tagName)
			break;
	}
	return !reader.hasError();
}

bool Scribus171Format::readDocItemAttributes(ScribusDoc *doc, ScXmlStreamReader& reader) const
{
	QString tagName(reader.nameAsString());
	doc->clearItemAttributes();
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (reader.isStartElement() && reader.name() == QLatin1String("ItemAttribute"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			ObjectAttribute objattr;
			objattr.name = attrs.valueAsString("Name");
			objattr.type = attrs.valueAsString("Type");
			objattr.value = attrs.valueAsString("Value");
			objattr.parameter = attrs.valueAsString("Parameter");
			objattr.relationship = attrs.valueAsString("Relationship");
			objattr.relationshipto = attrs.valueAsString("RelationshipTo");
			objattr.autoaddto = attrs.valueAsString("AutoAddTo");
			doc->appendToItemAttributes(objattr);
		}
	}
	return !reader.hasError();
}

bool Scribus171Format::readIndexes(ScribusDoc* doc, ScXmlStreamReader& reader) const
{
	IndexSetup indexSetup;
	ScXmlStreamAttributes attrs;
	doc->clearIndexSetups();
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		QString tagName(reader.nameAsString());
		if (reader.isStartElement())
			attrs = reader.attributes();
		if (reader.isEndElement() && tagName == QLatin1String("Indexes"))
			break;
		if (reader.isStartElement() && reader.name() == QLatin1String("Index"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			indexSetup.name = attrs.valueAsString("Name");
			indexSetup.frameName = attrs.valueAsString("FrameName");

			if (attrs.hasAttribute("ListNonPrinting"))
				indexSetup.listNonPrintingFrames = QVariant(attrs.valueAsString("ListNonPrinting")).toBool();
			else
				indexSetup.listNonPrintingFrames = false;
			if (attrs.hasAttribute("CombineIdenticalEntries"))
				indexSetup.combineIdenticalEntries = QVariant(attrs.valueAsString("CombineIdenticalEntries")).toBool();
			else
				indexSetup.combineIdenticalEntries = false;
			if (attrs.hasAttribute("CaseSensitiveCombination"))
				indexSetup.caseSensitiveCombination = QVariant(attrs.valueAsString("CaseSensitiveCombination")).toBool();
			else
				indexSetup.caseSensitiveCombination = false;
			if (attrs.hasAttribute("AutoCapitalizeEntries"))
				indexSetup.autoCapitalizeEntries = QVariant(attrs.valueAsString("AutoCapitalizeEntries")).toBool();
			else
				indexSetup.autoCapitalizeEntries = false;
			if (attrs.hasAttribute("AddAlphaSeparators"))
				indexSetup.addAlphaSeparators = QVariant(attrs.valueAsString("AddAlphaSeparators")).toBool();
			else
				indexSetup.addAlphaSeparators = false;
			indexSetup.headingStyle = attrs.valueAsString("HeadingStyle");
			indexSetup.separatorStyle = attrs.valueAsString("SeparatorStyle");
			indexSetup.level1Style = attrs.valueAsString("Level1Style");
			indexSetup.level2Style = attrs.valueAsString("Level2Style");
			indexSetup.level3Style = attrs.valueAsString("Level3Style");
		}
		if (reader.isEndElement() && tagName == QLatin1String("Index"))
		{
			doc->appendToIndexSetups(indexSetup);
		}
	}
	return !reader.hasError();
}

bool Scribus171Format::readTableOfContents(ScribusDoc* doc, ScXmlStreamReader& reader) const
{
	ToCSetup tocsetup;
	ScXmlStreamAttributes attrs;
	doc->clearTocSetups();
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		QString tagName(reader.nameAsString());
		if (reader.isStartElement())
			attrs = reader.attributes();
		if (reader.isEndElement() && tagName == QLatin1String("TablesOfContents"))
			break;
		if (reader.isStartElement() && reader.name() == QLatin1String("TableOfContents"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			tocsetup.name = attrs.valueAsString("Name");
			tocsetup.tocSource = attrs.valueAsString("ToCSource");
			//Up to 1.6/1.7, only Attributes were used for TOCs
			if (tocsetup.tocSource.isEmpty())
				tocsetup.tocSource = "Attribute";
			tocsetup.itemAttrName = attrs.valueAsString("ItemAttributeName");
			tocsetup.frameName = attrs.valueAsString("FrameName");
			tocsetup.textStyle = attrs.valueAsString("Style");
			if (attrs.hasAttribute("ListNonPrinting"))
				tocsetup.listNonPrintingFrames = QVariant(attrs.valueAsString("ListNonPrinting")).toBool();
			else
				tocsetup.listNonPrintingFrames = false;
			if (attrs.hasAttribute("NumberPlacement"))
			{
				QString numberPlacement(attrs.valueAsString("NumberPlacement"));
				if (numberPlacement == "Beginning")
					tocsetup.pageLocation = Beginning;
				if (numberPlacement == "End")
					tocsetup.pageLocation = End;
				if (numberPlacement == "NotShown")
					tocsetup.pageLocation = NotShown;
			}
			else
				tocsetup.pageLocation = End;
		}
		if (reader.isStartElement() && tagName == QLatin1String("StyleInTOC"))
		{
			ToCSetupEntryStyleData entryData;
			entryData.styleToFind = attrs.valueAsString("StyleName");
			entryData.styleForText = attrs.valueAsString("TOCStyle");
			entryData.removeLineBreaks = QVariant(attrs.valueAsString("RemoveLineBreaks")).toBool();
			if (attrs.hasAttribute("NumberPlacement"))
			{
				QString numberPlacement(attrs.valueAsString("NumberPlacement"));
				if (numberPlacement == "Beginning")
					entryData.pageLocation = Beginning;
				if (numberPlacement == "End")
					entryData.pageLocation = End;
				if (numberPlacement == "NotShown")
					entryData.pageLocation = NotShown;
			}
			else
				entryData.pageLocation = End;
			tocsetup.entryData.append(entryData);
		}
		if (reader.isEndElement() && tagName == QLatin1String("TableOfContents"))
		{
			doc->appendToTocSetups(tocsetup);
			tocsetup.styleListToFind.clear();
			tocsetup.styleListForTOC.clear();
		}
	}
	return !reader.hasError();
}

bool Scribus171Format::readNotesStyles(ScribusDoc* /*doc*/, ScXmlStreamReader& reader) const
{
	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		//read notes styles
		if (reader.isStartElement() && reader.name() == QLatin1String("notesStyle"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			NotesStyle NS;
			NS.setName(attrs.valueAsString("Name"));
			NS.setStart(attrs.valueAsInt("Start"));
			NS.setEndNotes(attrs.valueAsBool("Endnotes"));
			QString type = attrs.valueAsString("Type");
			if (type == "Type_1_2_3")
				NS.setType(Type_1_2_3);
			else if (type == "Type_1_2_3_ar")
				NS.setType(Type_1_2_3_ar);
			else if (type == "Type_i_ii_iii")
				NS.setType(Type_i_ii_iii);
			else if (type == "Type_I_II_III")
				NS.setType(Type_I_II_III);
			else if (type == "Type_a_b_c")
				NS.setType(Type_a_b_c);
			else if (type == "Type_A_B_C")
				NS.setType(Type_A_B_C);
			else if (type == "Type_Alphabet_ar")
				NS.setType(Type_Alphabet_ar);
			else if (type == "Type_Abjad_ar")
				NS.setType(Type_Abjad_ar);
			else if (type == "Type_Hebrew")
				NS.setType(Type_Hebrew);
			else if (type == "Type_asterix")
				NS.setType(Type_asterix);
			else if (type == "Type_CJK")
				NS.setType(Type_CJK);
			else //if (type == "Type_None")
				NS.setType(Type_None);
			// Fix deprecated numeration ranges
			NumerationRange numRange = (NumerationRange) attrs.valueAsInt("Range");
			if (numRange != NSRdocument && numRange != NSRstory)
				numRange = NSRstory;
			NS.setRange(numRange);
			NS.setPrefix(attrs.valueAsString("Prefix"));
			NS.setSuffix(attrs.valueAsString("Suffix"));
			NS.setAutoNotesHeight(attrs.valueAsBool("AutoHeight"));
			NS.setAutoNotesWidth(attrs.valueAsBool("AutoWidth"));
			NS.setAutoRemoveEmptyNotesFrames(attrs.valueAsBool("AutoRemove"));
			NS.setAutoWeldNotesFrames(attrs.valueAsBool("AutoWeld"));
			NS.setSuperscriptInNote(attrs.valueAsBool("SuperNote"));
			NS.setSuperscriptInMaster(attrs.valueAsBool("SuperMaster"));
			NS.setMarksCharStyle("");
			NS.setNotesParStyle("");
			QString name;
			name = attrs.valueAsString("MarksStyle");
			if (!name.isEmpty())
				NS.setMarksCharStyle(name);
			name = attrs.valueAsString("NotesStyle");
			if (!name.isEmpty())
				NS.setNotesParStyle(name);

			m_Doc->newNotesStyle(NS);
		}
	}
	return !reader.hasError();
}

bool Scribus171Format::readNotesFrames(ScXmlStreamReader& reader)
{
	notesFramesData.clear();
	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (reader.isStartElement())
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			NoteFrameData eF;
			//Remove old attribute format in 1.8 format
			if (attrs.hasAttribute("NSname"))
				eF.NSname = attrs.valueAsString("NSname");
			else
				eF.NSname = attrs.valueAsString("NoteStyleName");
			eF.myID = attrs.valueAsInt("myID");
			//Remove old attribute format in 1.8 format
			if ((reader.name() == QLatin1String("ENDNOTEFRAME")) || (reader.name() == QLatin1String("EndNoteFrame")))
			{
				eF.index = attrs.valueAsInt("index");
				eF.NSrange = (NumerationRange) attrs.valueAsInt("range");
				 // Fix deprecated numeration ranges
				if (eF.NSrange != NSRdocument && eF.NSrange != NSRstory)
					eF.NSrange = NSRstory;
				eF.itemID = attrs.valueAsInt("ItemID");
			}
			if (reader.name() == QLatin1String("FootNoteFrame"))
				eF.itemID = attrs.valueAsInt("MasterID");
			if (reader.name() == QLatin1String("FOOTNOTEFRAME"))
				eF.itemID = attrs.valueAsInt("MasterID");
			notesFramesData.append(eF);
		}
	}
	return !reader.hasError();
}

bool Scribus171Format::readNotes(ScribusDoc* /*doc*/, ScXmlStreamReader& reader)
{
	//read notes
	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (reader.isStartElement() && reader.name() == QLatin1String("Note"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			TextNote* note = m_Doc->newNote(nullptr);
			note->setSaxedText(attrs.valueAsString("Text"));
			//temporarily insert names of master mark and notes style into maps with note pointer
			//will be resolved to pointers by updateNames2Ptr() after all will read
			notesMasterMarks.insert(attrs.valueAsString("Master"), note);
			//Remove old attribute format in 1.8 format
			if (attrs.hasAttribute("NStyle"))
				notesNSets.insert(note, attrs.valueAsString("NStyle"));
			else
				notesNSets.insert(note, attrs.valueAsString("NoteStyle"));
		}
	}
	return !reader.hasError();
}

bool Scribus171Format::readMarks(ScribusDoc* doc, ScXmlStreamReader& reader)
{
	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (reader.isStartElement() && reader.name() == QLatin1String("Mark"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();

			QString label;
			if (attrs.hasAttribute("label"))
				label = attrs.valueAsString("label");

			MarkType type = MARKNoType;
			if (attrs.hasAttribute("type"))
				type = (MarkType) attrs.valueAsInt("type");

			if (!label.isEmpty() && type != MARKNoType)
			{
				Mark* mark = doc->newMark();
				mark->label = attrs.valueAsString("label");
				mark->setType(type);
				if ((type == MARKVariableTextType || type == MARKIndexType) && attrs.hasAttribute("str"))
					mark->setString(attrs.valueAsString("str"));

				if (type == MARK2ItemType && attrs.hasAttribute("ItemID"))
				{
					//QString itemName = attrs.valueAsString("itemName");
					markeredItemsMap.insert(mark, attrs.valueAsInt("ItemID"));
				}
				if (type == MARK2MarkType && attrs.hasAttribute("MARKlabel"))
				{
					QString mark2Label = attrs.valueAsString("MARKlabel");
					MarkType mark2Type = (MarkType) attrs.valueAsInt("MARKtype");
					Mark* mark2 = doc->getMark(mark2Label, mark2Type);
					if (mark2 != nullptr) //mark is not defined yet, insert into temp list for update to pointers later
						mark->setDestMark(mark2);
					else
					{
						QMap<QString, MarkType> mark2map;
						mark2map.insert(mark2Label, mark2Type);
						markeredMarksMap.insert(mark, mark2map);
					}
				}
			}
		}
	}
	return !reader.hasError();
}

bool Scribus171Format::readOpticalMarginSets(ScribusDoc *doc, ScXmlStreamReader &reader) const
{
	QString tagName(reader.nameAsString());
	OpticalMarginSets sets;
	OpticalMarginSet set;
	OpticalMarginRules rules;

	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;

		if (reader.isStartElement() && reader.name() == QLatin1String("Set"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			set.id = attrs.valueAsString("Id");
			set.type = attrs.valueAsString("Type");
			set.name = attrs.valueAsString("Name");
		}

		if (reader.isStartElement() && reader.name() == QLatin1String("Rule"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			OpticalMarginRule rule(unicodeToString(attrs.valueAsString("Characters")), attrs.valueAsDouble("Left"), attrs.valueAsDouble("Right"), attrs.valueAsInt("Unit"));
			rules.append(rule);
		}

		if (reader.isEndElement() && reader.name() == QLatin1String("Rules"))
		{
			set.rules = OpticalMarginLookup::instance().splitRulesToChars(rules);
			rules.clear();
		}

		if (reader.isEndElement() && reader.name() == QLatin1String("Set"))
			sets.insert(set.id, set);
	}

	doc->typographicPrefs().opticalMarginSets = sets;
	m_Doc->setOpticalMarginSets(sets);

	return !reader.hasError();
}

bool Scribus171Format::readSections(ScribusDoc* doc, ScXmlStreamReader& reader) const
{
	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (reader.isStartElement() && reader.name() == QLatin1String("Section"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			struct DocumentSection newSection;
			newSection.number = attrs.valueAsInt("Number");
			newSection.name = attrs.valueAsString("Name");
			newSection.fromindex = attrs.valueAsInt("From");
			newSection.toindex = attrs.valueAsInt("To");
			QString type = attrs.valueAsString("Type");
			if (type == "Type_1_2_3")
				newSection.type = Type_1_2_3;
			if (type == "Type_1_2_3_ar")
				newSection.type = Type_1_2_3_ar;
			if (type == "Type_i_ii_iii")
				newSection.type = Type_i_ii_iii;
			if (type == "Type_I_II_III")
				newSection.type = Type_I_II_III;
			if (type == "Type_a_b_c")
				newSection.type = Type_a_b_c;
			if (type == "Type_A_B_C")
				newSection.type = Type_A_B_C;
			if (type == "Type_Alphabet_ar")
				newSection.type = Type_Alphabet_ar;
			if (type == "Type_Abjad_ar")
				newSection.type = Type_Abjad_ar;
			if (type == "Type_Hebrew")
				newSection.type = Type_Hebrew;
			if (type == "Type_CJK")
				newSection.type = Type_CJK;
			if (type == "Type_None")
				newSection.type = Type_None;
			newSection.sectionstartindex = attrs.valueAsInt("Start");
			newSection.reversed = attrs.valueAsBool("Reversed");
			newSection.active = attrs.valueAsBool("Active");
			if (attrs.hasAttribute("FillChar"))
				newSection.pageNumberFillChar = QChar(attrs.valueAsInt("FillChar"));
			else
				newSection.pageNumberFillChar = QChar();
			if (attrs.hasAttribute("FieldWidth"))
				newSection.pageNumberWidth = attrs.valueAsInt("FieldWidth");
			else
				newSection.pageNumberWidth = 0;
			doc->sections().insert(newSection.number, newSection);
		}
	}
	return !reader.hasError();
}

bool Scribus171Format::readHyphen(ScribusDoc *doc, ScXmlStreamReader& reader) const
{
	auto& hyphenationPrefs = doc->hyphenatorPrefs();

	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (reader.isStartElement() && reader.name() == QLatin1String("Exception"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			QString word = attrs.valueAsString("Word");
			QString hyph = attrs.valueAsString("Hyphenated");
			hyphenationPrefs.specialWords.insert(word, hyph);
		}
		else if (reader.isStartElement() && reader.name() == QLatin1String("Ignore"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			QString word = attrs.valueAsString("Word");
			hyphenationPrefs.ignoredWords.insert(word);
		}
		//Remove uppercase in 1.8
		else if (reader.isStartElement() && reader.name() == QLatin1String("EXCEPTION"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			QString word = attrs.valueAsString("WORD");
			QString hyph = attrs.valueAsString("HYPHENATED");
			hyphenationPrefs.specialWords.insert(word, hyph);
		}
		else if (reader.isStartElement() && reader.name() == QLatin1String("IGNORE"))
		{
			ScXmlStreamAttributes attrs = reader.scAttributes();
			QString word = attrs.valueAsString("WORD");
			hyphenationPrefs.ignoredWords.insert(word);
		}
	}
	return !reader.hasError();
}

bool Scribus171Format::readPage(ScribusDoc* doc, ScXmlStreamReader& reader)
{
	QString tagName(reader.nameAsString());

	ScXmlStreamAttributes attrs = reader.scAttributes();
	int pageNum = 0;
	QString pageName;
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("NUM"))
		pageNum = attrs.valueAsInt("NUM");
	else
		pageNum = attrs.valueAsInt("PageNumber");
	if (attrs.hasAttribute("NAM"))
		pageName = attrs.valueAsString("NAM", "");
	else
		pageName = attrs.valueAsString("PageName", "");
	//Remove uppercase in 1.8
	if (tagName == QLatin1String("MASTERPAGE") && pageName.isEmpty())
	{
		qDebug() << "scribus171format: corrupted masterpage with empty name detected";
		return true;
	}
	else
		if (tagName == QLatin1String("MasterPage") && pageName.isEmpty())
		{
			qDebug() << "scribus171format: corrupted masterpage with empty name detected";
			return true;
		}

	bool savedMasterPageMode = m_Doc->masterPageMode();
	m_Doc->setMasterPageMode(!pageName.isEmpty());
	ScPage* newPage = pageName.isEmpty() ? doc->addPage(pageNum) : doc->addMasterPage(pageNum, pageName);

	//Remove uppercase in 1.8
	if (attrs.hasAttribute("LEFT"))
		newPage->LeftPg = attrs.valueAsInt("LEFT", 0);
	else
		newPage->LeftPg = attrs.valueAsInt("LeftPage", 0);
	//Remove uppercase in 1.8
	QString mpName;
	if (attrs.hasAttribute("MNAM"))
		mpName = attrs.valueAsString("MNAM", "Normal");
	else
		mpName = attrs.valueAsString("MasterPageName", "Normal");
	newPage->setMasterPageName(m_Doc->masterPageMode() ? QString() : mpName);
	if (attrs.hasAttribute("Size"))
		newPage->setSize(attrs.valueAsString("Size"));
	if (attrs.hasAttribute("Orientation"))
		newPage->setOrientation(attrs.valueAsInt("Orientation"));

	//Remove uppercase in 1.8
	if (attrs.hasAttribute("PAGEXPOS"))
		newPage->setXOffset(attrs.valueAsDouble("PAGEXPOS"));
	else
		newPage->setXOffset(attrs.valueAsDouble("PageXPosition"));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("PAGEYPOS"))
		newPage->setYOffset(attrs.valueAsDouble("PAGEYPOS"));
	else
		newPage->setYOffset(attrs.valueAsDouble("PageYPosition"));


	//Remove uppercase in 1.8
	if (attrs.hasAttribute("PAGEWIDTH"))
		newPage->setWidth(attrs.valueAsDouble("PAGEWIDTH"));
	else
		newPage->setWidth(attrs.valueAsDouble("PageWidth"));

	//Remove uppercase in 1.8
	if (attrs.hasAttribute("PAGEHEIGHT"))
		newPage->setHeight(attrs.valueAsDouble("PAGEHEIGHT"));
	else
		newPage->setHeight(attrs.valueAsDouble("PageHeight"));

	//14704: Double check the page size should not be Custom in case the size doesn't match a standard size
	if (attrs.hasAttribute("Size"))
	{
		QString pageSize(attrs.valueAsString("Size"));
		PageSize ps(pageSize);
		if (!compareDouble(ps.width(), newPage->width()) || !compareDouble(ps.height(), newPage->height()))
			newPage->setSize(CommonStrings::customPageSize);
		else
			newPage->setSize(pageSize);
	}

	newPage->setInitialHeight(newPage->height());
	newPage->setInitialWidth(newPage->width());
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("BORDERTOP"))
		newPage->initialMargins.setTop(qMax(0.0, attrs.valueAsDouble("BORDERTOP")));
	else
		newPage->initialMargins.setTop(qMax(0.0, attrs.valueAsDouble("BorderTop")));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("BORDERBOTTOM"))
		newPage->initialMargins.setBottom(qMax(0.0, attrs.valueAsDouble("BORDERBOTTOM")));
	else
		newPage->initialMargins.setBottom(qMax(0.0, attrs.valueAsDouble("BorderBottom")));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("BORDERLEFT"))
		newPage->initialMargins.setLeft(qMax(0.0, attrs.valueAsDouble("BORDERLEFT")));
	else
		newPage->initialMargins.setLeft(qMax(0.0, attrs.valueAsDouble("BorderLeft")));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("BORDERRIGHT"))
		newPage->initialMargins.setRight(qMax(0.0, attrs.valueAsDouble("BORDERRIGHT")));
	else
		newPage->initialMargins.setRight(qMax(0.0, attrs.valueAsDouble("BorderRight")));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("PRESET"))
		newPage->marginPreset = attrs.valueAsInt("PRESET", 0);
	else
		newPage->marginPreset = attrs.valueAsInt("Preset", 0);
	newPage->Margins.setTop(newPage->initialMargins.top());
	newPage->Margins.setBottom(newPage->initialMargins.bottom());

	m_Doc->setMasterPageMode(savedMasterPageMode);

	// guides reading
	//Remove old ones in 1.8
	if (attrs.hasAttribute("AGhorizontalAutoGap"))
	{
		newPage->guides.setHorizontalAutoGap(attrs.valueAsDouble("AGhorizontalAutoGap", 0.0));
		newPage->guides.setVerticalAutoGap(attrs.valueAsDouble("AGverticalAutoGap", 0.0));
		newPage->guides.setHorizontalAutoCount(attrs.valueAsInt("AGhorizontalAutoCount", 0));
		newPage->guides.setVerticalAutoCount(attrs.valueAsInt("AGverticalAutoCount", 0));
		newPage->guides.setHorizontalAutoRefer(attrs.valueAsInt("AGhorizontalAutoRefer", 0));
		newPage->guides.setVerticalAutoRefer(attrs.valueAsInt("AGverticalAutoRefer", 0));
	}
	else
	{
		newPage->guides.setHorizontalAutoGap(attrs.valueAsDouble("AutoGuideHorizontalGap", 0.0));
		newPage->guides.setVerticalAutoGap(attrs.valueAsDouble("AutoGuideVerticalGap", 0.0));
		newPage->guides.setHorizontalAutoCount(attrs.valueAsInt("AutoGuideHorizontalCount", 0));
		newPage->guides.setVerticalAutoCount(attrs.valueAsInt("AutoGuideVerticalCount", 0));
		newPage->guides.setHorizontalAutoRefer(attrs.valueAsInt("AutoGuideHorizontalReference", 0));
		newPage->guides.setVerticalAutoRefer(attrs.valueAsInt("AutoGuideVerticalReference", 0));
	}
	GuideManagerIO::readVerticalGuides(attrs.valueAsString("VerticalGuides"), newPage, GuideManagerCore::Standard, false);
	GuideManagerIO::readHorizontalGuides(attrs.valueAsString("HorizontalGuides"), newPage, GuideManagerCore::Standard, false);
	if (attrs.hasAttribute("AGSelection"))
		GuideManagerIO::readSelection(attrs.valueAsString("AGSelection"), newPage);
	else
		GuideManagerIO::readSelection(attrs.valueAsString("AutoGuideSelection"), newPage);

	newPage->guides.addHorizontals(newPage->guides.getAutoHorizontals(newPage), GuideManagerCore::Auto);
	newPage->guides.addVerticals(newPage->guides.getAutoVerticals(newPage), GuideManagerCore::Auto);
	struct PDFPresentationData ef;
	//Remove old ones in 1.8
	if (attrs.hasAttribute("pageEffectDuration"))
		ef.pageEffectDuration =  attrs.valueAsInt("pageEffectDuration", 1);
	else
		ef.pageEffectDuration =  attrs.valueAsInt("PageEffectDuration", 1);
	//Remove old ones in 1.8
	if (attrs.hasAttribute("pageViewDuration"))
		ef.pageViewDuration =  attrs.valueAsInt("pageViewDuration", 1);
	else
		ef.pageViewDuration =  attrs.valueAsInt("PageViewDuration", 1);
	//Remove old ones in 1.8
	if (attrs.hasAttribute("effectType"))
		ef.effectType = attrs.valueAsInt("effectType", 0);
	else
		ef.effectType = attrs.valueAsInt("PageEffectType", 0);
	//Remove old ones in 1.8
	if (attrs.hasAttribute("Dm"))
		ef.Dm = attrs.valueAsInt("Dm", 0);
	else
		ef.Dm = attrs.valueAsInt("PageEffectLineDirection", 0);
	//Remove old ones in 1.8
	if (attrs.hasAttribute("Dm"))
		ef.M = attrs.valueAsInt("M", 0);
	else
		ef.M = attrs.valueAsInt("PageEffectInsideOutside", 0);
	//Remove old ones in 1.8
	if (attrs.hasAttribute("Dm"))
		ef.Di = attrs.valueAsInt("Di", 0);
	else
		ef.Di = attrs.valueAsInt("PageEffectDirection", 0);
	newPage->PresentVals = ef;
	return true;
}

bool Scribus171Format::readObject(ScribusDoc* doc, ScXmlStreamReader& reader, const ReadObjectParams& readObjectParams, ItemInfo& info)
{
	QString tagName(reader.nameAsString());
	ScXmlStreamAttributes attrs = reader.scAttributes();

	bool savedMasterPageMode = doc->masterPageMode();
	if (!readObjectParams.loadingPage)
	{
		if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("PageObject") || tagName == QLatin1String("FrameObject") ||
				tagName == QLatin1String("PatternItem") || tagName == QLatin1String("ITEM") || tagName == QLatin1String("Item"))
			doc->setMasterPageMode(false);
		else
			doc->setMasterPageMode(true);
	}

	PageItem::ItemKind itemKind = PageItem::StandardItem;
	if (tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject"))
		itemKind = PageItem::InlineItem;
	else if (tagName == QLatin1String("PatternItem"))
		itemKind = PageItem::PatternItem;

	// We are loading patterns, force itemKind to PatternItem
	if (readObjectParams.itemKind == PageItem::PatternItem)
		itemKind = PageItem::PatternItem;

	int pageNr = -1;
	QString masterPageName = attrs.valueAsString("OnMasterPage");
	if ((!masterPageName.isEmpty()) && (tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("MasterObject")))
	{
		if (!readObjectParams.renamedMasterPage.isEmpty())
			masterPageName = readObjectParams.renamedMasterPage;
		doc->setCurrentPage(doc->MasterPages.at(doc->MasterNames[masterPageName]));
		pageNr = -2;
	}
	layerFound = false;
	clipPath.clear();

	PageItem* newItem = pasteItem(doc, attrs, readObjectParams.baseDir, itemKind, pageNr);
	newItem->setRedrawBounding();
	if (tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("MasterObject"))
		newItem->setOwnerPage(doc->OnPage(newItem));
	else
		newItem->setOwnerPage(attrs.valueAsInt("OwnPage"));
	if ((tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject")) || (tagName == QLatin1String("ITEM") || tagName == QLatin1String("Item")))
		newItem->setMasterPageName(QString());
	if (tagName == QLatin1String("ITEM") || tagName == QLatin1String("Item"))
	{
		newItem->setLayer(LayerToPaste);
		newItem->setMasterPage(doc->OnPage(newItem), doc->currentPage()->pageName());
	}
	QString tmpf = attrs.valueAsString("IFONT", doc->itemToolPrefs().textFont);
	m_AvailableFonts->findFont(tmpf, doc);

//	newItem->Language = ScMW->GetLang(pg.attribute("LANGUAGE", doc->Language));
	if (attrs.hasAttribute("AUTOTEXT"))
		newItem->isAutoText = attrs.valueAsBool("AUTOTEXT", false);
	else
		newItem->isAutoText = attrs.valueAsBool("AutomaticTextFrame", false);
	newItem->isEmbedded = attrs.valueAsBool("isInline", false);
	newItem->gXpos = attrs.valueAsDouble("gXpos", 0.0);
	newItem->gYpos = attrs.valueAsDouble("gYpos", 0.0);
	newItem->gWidth = attrs.valueAsDouble("gWidth", newItem->width());
	newItem->gHeight = attrs.valueAsDouble("gHeight", newItem->height());
	if (newItem->isAutoText)
		doc->LastAuto = newItem;

	if (tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject"))
	{
		if (newItem->inlineCharID == -1)
			FrameItems.append(m_Doc->Items->takeAt(m_Doc->Items->indexOf(newItem)));
		else
			doc->FrameItems.insert(newItem->inlineCharID, doc->Items->takeAt(doc->Items->indexOf(newItem)));
		newItem->setLayer(doc->firstLayerID());
	}

	info.item = newItem;
	isNewFormat = attrs.hasAttribute("ItemID");
	if (isNewFormat)
	{
		info.itemID = attrs.valueAsInt("ItemID", 0);
		LinkID.insert(info.itemID, newItem);
	}
	if (attrs.hasAttribute("NEXTITEM"))
		info.nextItem = attrs.valueAsInt("NEXTITEM", -1);
	else
		info.nextItem = attrs.valueAsInt("NextItem", -1);
	if (isNewFormat)
	{
		if (info.nextItem != -1)
			itemNext[info.itemID] = info.nextItem;
	}
	info.ownLink = newItem->isTableItem ? attrs.valueAsInt("OwnLINK", 0) : 0;
	info.groupLastItem = 0;
	info.isGroupFlag = attrs.valueAsBool("isGroupControl", false);
	if (info.isGroupFlag)
		info.groupLastItem = attrs.valueAsInt("groupsLastItem", 0);
	//Remove lowercase in 1.8
	if (attrs.hasAttribute("isWeldItem"))
		info.isWeldFlag = attrs.valueAsBool("isWeldItem", false);
	else
		info.isWeldFlag = attrs.valueAsBool("IsWeldItem", false);
	info.ownWeld = attrs.valueAsInt("WeldSource", 0);
	info.ownNr = doc->Items->indexOf(newItem);

#ifdef HAVE_OSG
	struct PageItem_OSGFrame::viewDefinition currentView;
#endif
	QList<ParagraphStyle::TabRecord> tabValues;

	int mGArrayRows = 0;
	int mGArrayCols = 0;
	LastStyles * lastStyle = new LastStyles();
	while (!reader.atEnd() && !reader.hasError())
	{
		ScXmlStreamReader::TokenType tType = reader.readNext();
		if (reader.isEndElement() && tagName == reader.name())
			break;
		if (tType != ScXmlStreamReader::StartElement)
			continue;
		QString tName = reader.name().toString();
		ScXmlStreamAttributes tAtt = reader.scAttributes();
		//Remove old format in 1.8
		if (tName == QLatin1String("CSTOP"))
		{
			QString name = tAtt.valueAsString("NAME");
			double ramp = tAtt.valueAsDouble("RAMP", 0.0);
			int shade = tAtt.valueAsInt("SHADE", 100);
			double opa = tAtt.valueAsDouble("TRANS", 1.0);
			newItem->fill_gradient.addStop(SetColor(doc, name, shade), ramp, 0.5, opa, name, shade);
		}
		if (tName == QLatin1String("ColorStop"))
		{
			QString name = tAtt.valueAsString("Name");
			double ramp = tAtt.valueAsDouble("Ramp", 0.0);
			int shade = tAtt.valueAsInt("Shade", 100);
			double opa = tAtt.valueAsDouble("Opacity", 1.0);
			newItem->fill_gradient.addStop(SetColor(doc, name, shade), ramp, 0.5, opa, name, shade);
		}
		//Remove old format in 1.8
		if (tName == QLatin1String("S_CSTOP"))
		{
			QString name = tAtt.valueAsString("NAME");
			double ramp = tAtt.valueAsDouble("RAMP", 0.0);
			int shade = tAtt.valueAsInt("SHADE", 100);
			double opa = tAtt.valueAsDouble("TRANS", 1.0);
			newItem->stroke_gradient.addStop(SetColor(doc, name, shade), ramp, 0.5, opa, name, shade);
		}
		if (tName == QLatin1String("ColorStopStroke"))
		{
			QString name = tAtt.valueAsString("Name");
			double ramp = tAtt.valueAsDouble("Ramp", 0.0);
			int shade = tAtt.valueAsInt("Shade", 100);
			double opa = tAtt.valueAsDouble("Opacity", 1.0);
			newItem->stroke_gradient.addStop(SetColor(doc, name, shade), ramp, 0.5, opa, name, shade);
		}
		//Remove old format in 1.8
		if (tName == QLatin1String("M_CSTOP"))
		{
			QString name = tAtt.valueAsString("NAME");
			double ramp = tAtt.valueAsDouble("RAMP", 0.0);
			int shade = tAtt.valueAsInt("SHADE", 100);
			double opa = tAtt.valueAsDouble("TRANS", 1.0);
			newItem->mask_gradient.addStop(SetColor(doc, name, shade), ramp, 0.5, opa, name, shade);
		}
		if (tName == QLatin1String("ColorStopMaskGradient"))
		{
			QString name = tAtt.valueAsString("Name");
			double ramp = tAtt.valueAsDouble("Ramp", 0.0);
			int shade = tAtt.valueAsInt("Shade", 100);
			double opa = tAtt.valueAsDouble("Opacity", 1.0);
			newItem->mask_gradient.addStop(SetColor(doc, name, shade), ramp, 0.5, opa, name, shade);
		}
		//Remove in 1.8
		if (tName == QLatin1String("MPoint"))
		{
			MeshPoint mp;
			mp.colorName = tAtt.valueAsString("NAME");
			mp.shade = tAtt.valueAsInt("SHADE", 100);
			mp.transparency = tAtt.valueAsDouble("TRANS", 1.0);
			mp.gridPoint = FPoint(tAtt.valueAsDouble("GX", 0.0), tAtt.valueAsDouble("GY", 0.0));
			mp.controlTop = FPoint(tAtt.valueAsDouble("CTX", 0.0), tAtt.valueAsDouble("CTY", 0.0));
			mp.controlBottom = FPoint(tAtt.valueAsDouble("CBX", 0.0), tAtt.valueAsDouble("CBY", 0.0));
			mp.controlLeft = FPoint(tAtt.valueAsDouble("CLX", 0.0), tAtt.valueAsDouble("CLY", 0.0));
			mp.controlRight = FPoint(tAtt.valueAsDouble("CRX", 0.0), tAtt.valueAsDouble("CRY", 0.0));
			mp.controlColor = FPoint(tAtt.valueAsDouble("CCX", mp.gridPoint.x()), tAtt.valueAsDouble("CCY", mp.gridPoint.y()));
			mp.color = SetColor(doc, mp.colorName, mp.shade);
			mp.color.setAlphaF(mp.transparency);
			newItem->meshGradientArray[mGArrayRows][mGArrayCols] = mp;
			mGArrayCols++;
			if (mGArrayCols == newItem->meshGradientArray[mGArrayRows].count())
			{
				mGArrayCols = 0;
				mGArrayRows++;
			}
		}
		if (tName == QLatin1String("MeshPoint"))
		{
			MeshPoint mp;
			mp.colorName = tAtt.valueAsString("Name");
			mp.shade = tAtt.valueAsInt("Shade", 100);
			mp.transparency = tAtt.valueAsDouble("Transparency", 1.0);
			mp.gridPoint = FPoint(tAtt.valueAsDouble("GridPointX", 0.0), tAtt.valueAsDouble("GridPointY", 0.0));
			mp.controlTop = FPoint(tAtt.valueAsDouble("ControlTopX", 0.0), tAtt.valueAsDouble("ControlTopY", 0.0));
			mp.controlBottom = FPoint(tAtt.valueAsDouble("ControlBottomX", 0.0), tAtt.valueAsDouble("ControlBottomY", 0.0));
			mp.controlLeft = FPoint(tAtt.valueAsDouble("ControlLeftX", 0.0), tAtt.valueAsDouble("ControlLeftY", 0.0));
			mp.controlRight = FPoint(tAtt.valueAsDouble("ControlRightX", 0.0), tAtt.valueAsDouble("ControlRightY", 0.0));
			mp.controlColor = FPoint(tAtt.valueAsDouble("ControlColorX", mp.gridPoint.x()), tAtt.valueAsDouble("ControlColorY", mp.gridPoint.y()));
			mp.color = SetColor(doc, mp.colorName, mp.shade);
			mp.color.setAlphaF(mp.transparency);
			newItem->meshGradientArray[mGArrayRows][mGArrayCols] = mp;
			mGArrayCols++;
			if (mGArrayCols == newItem->meshGradientArray[mGArrayRows].count())
			{
				mGArrayCols = 0;
				mGArrayRows++;
			}
		}
		//Remove in 1.8
		if (tName == QLatin1String("PMPoint"))
		{
			MeshPoint mp;
			mp.colorName = tAtt.valueAsString("NAME");
			mp.shade = tAtt.valueAsInt("SHADE", 100);
			mp.transparency = tAtt.valueAsDouble("TRANS", 1.0);
			mp.gridPoint = FPoint(tAtt.valueAsDouble("GX", 0.0), tAtt.valueAsDouble("GY", 0.0));
			mp.controlTop = FPoint(tAtt.valueAsDouble("CTX", mp.gridPoint.x()), tAtt.valueAsDouble("CTY", mp.gridPoint.y()));
			mp.controlBottom = FPoint(tAtt.valueAsDouble("CBX", mp.gridPoint.x()), tAtt.valueAsDouble("CBY", mp.gridPoint.y()));
			mp.controlLeft = FPoint(tAtt.valueAsDouble("CLX", mp.gridPoint.x()), tAtt.valueAsDouble("CLY", mp.gridPoint.y()));
			mp.controlRight = FPoint(tAtt.valueAsDouble("CRX", mp.gridPoint.x()), tAtt.valueAsDouble("CRY", mp.gridPoint.y()));
			mp.controlColor = FPoint(tAtt.valueAsDouble("CCX", mp.gridPoint.x()), tAtt.valueAsDouble("CCY", mp.gridPoint.y()));
			mp.color = SetColor(doc, mp.colorName, mp.shade);
			mp.color.setAlphaF(mp.transparency);
			if (mGArrayCols == 0)
				newItem->meshGradientPatches[mGArrayRows].TL = mp;
			else if (mGArrayCols == 1)
				newItem->meshGradientPatches[mGArrayRows].TR = mp;
			else if (mGArrayCols == 2)
				newItem->meshGradientPatches[mGArrayRows].BR = mp;
			else if (mGArrayCols == 3)
				newItem->meshGradientPatches[mGArrayRows].BL = mp;
			mGArrayCols++;
			if (mGArrayCols == 4)
			{
				mGArrayCols = 0;
				mGArrayRows++;
			}
		}
		if (tName == QLatin1String("PatchMeshPoint"))
		{
			MeshPoint mp;
			mp.colorName = tAtt.valueAsString("Name");
			mp.shade = tAtt.valueAsInt("Shade", 100);
			mp.transparency = tAtt.valueAsDouble("Transparency", 1.0);
			mp.gridPoint = FPoint(tAtt.valueAsDouble("GridPointX", 0.0), tAtt.valueAsDouble("GridPointY", 0.0));
			mp.controlTop = FPoint(tAtt.valueAsDouble("ControlTopX", mp.gridPoint.x()), tAtt.valueAsDouble("ControlTopY", mp.gridPoint.y()));
			mp.controlBottom = FPoint(tAtt.valueAsDouble("ControlBottomX", mp.gridPoint.x()), tAtt.valueAsDouble("ControlBottomY", mp.gridPoint.y()));
			mp.controlLeft = FPoint(tAtt.valueAsDouble("ControlLeftX", mp.gridPoint.x()), tAtt.valueAsDouble("ControlLeftY", mp.gridPoint.y()));
			mp.controlRight = FPoint(tAtt.valueAsDouble("ControlRightX", mp.gridPoint.x()), tAtt.valueAsDouble("ControlRightY", mp.gridPoint.y()));
			mp.controlColor = FPoint(tAtt.valueAsDouble("ControlColorX", mp.gridPoint.x()), tAtt.valueAsDouble("ControlColorY", mp.gridPoint.y()));
			mp.color = SetColor(doc, mp.colorName, mp.shade);
			mp.color.setAlphaF(mp.transparency);
			if (mGArrayCols == 0)
				newItem->meshGradientPatches[mGArrayRows].TL = mp;
			else if (mGArrayCols == 1)
				newItem->meshGradientPatches[mGArrayRows].TR = mp;
			else if (mGArrayCols == 2)
				newItem->meshGradientPatches[mGArrayRows].BR = mp;
			else if (mGArrayCols == 3)
				newItem->meshGradientPatches[mGArrayRows].BL = mp;
			mGArrayCols++;
			if (mGArrayCols == 4)
			{
				mGArrayCols = 0;
				mGArrayRows++;
			}
		}
		if (tName == QLatin1String("TableData"))
		{
			readItemTableData(newItem->asTable(), reader, doc);
		}
		if (tName == QLatin1String("PageItemAttributes"))
		{
			readPageItemAttributes(newItem, reader);
		}
		if (tName == QLatin1String("PSDLayer"))
		{
			layerFound = true;
			struct ImageLoadRequest loadingInfo;
			loadingInfo.blend = tAtt.valueAsString("Blend");
			loadingInfo.opacity = tAtt.valueAsInt("Opacity");
			loadingInfo.visible = tAtt.valueAsBool("Visible");
			loadingInfo.useMask = tAtt.valueAsBool("useMask", true);
			newItem->pixm.imgInfo.RequestProps.insert(tAtt.valueAsInt("Layer"), loadingInfo);
		}
#ifdef HAVE_OSG
		if (tName == QLatin1String("OSGViews"))
		{
			currentView.angleFOV = tAtt.valueAsDouble("angleFOV");
			QString tmp(tAtt.valueAsString("trackM"));
			ScTextStream fp(&tmp, QIODevice::ReadOnly);
			double m1, m2, m3, m4;
			double m5, m6, m7, m8;
			double m9, m10, m11, m12;
			double m13, m14, m15, m16;
			fp >> m1 >> m2 >> m3 >> m4;
			fp >> m5 >> m6 >> m7 >> m8;
			fp >> m9 >> m10 >> m11 >> m12;
			fp >> m13 >> m14 >> m15 >> m16;
			currentView.trackerMatrix.set(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16);
			tmp = tAtt.valueAsString("trackC");
			ScTextStream fp2(&tmp, QIODevice::ReadOnly);
			double v1, v2, v3;
			fp2 >> v1 >> v2 >> v3;
			currentView.trackerCenter.set(v1, v2, v3);
			tmp = tAtt.valueAsString("cameraP");
			ScTextStream fp3(&tmp, QIODevice::ReadOnly);
			fp3 >> v1 >> v2 >> v3;
			currentView.cameraPosition.set(v1, v2, v3);
			tmp = tAtt.valueAsString("cameraU");
			ScTextStream fp4(&tmp, QIODevice::ReadOnly);
			fp4 >> v1 >> v2 >> v3;
			currentView.cameraUp.set(v1, v2, v3);
			currentView.trackerDist = tAtt.valueAsDouble("trackerDist");
			currentView.trackerSize = tAtt.valueAsDouble("trackerSize");
			currentView.illumination = static_cast<PageItem_OSGFrame::LightType>(tAtt.valueAsInt("illumination"));
			currentView.rendermode = static_cast<PageItem_OSGFrame::RenderType>(tAtt.valueAsInt("rendermode"));
			currentView.addedTransparency = tAtt.valueAsDouble("trans");
			currentView.colorAC = QColor(tAtt.valueAsString("colorAC"));
			currentView.colorFC = QColor(tAtt.valueAsString("colorFC"));
			if (newItem->isOSGFrame())
				newItem->asOSGFrame()->viewMap.insert(tAtt.valueAsString("viewName"), currentView);
		}
#endif
		if (tName == QLatin1String("ImageEffect"))
		{
			struct ImageEffect ef;
			ef.effectParameters = tAtt.valueAsString("Param");
			ef.effectCode = tAtt.valueAsInt("Code");
			newItem->effectsInUse.append(ef);
		}
		if (tName == QLatin1String("StoryText"))
			readStoryText(doc, reader, newItem->itemText, newItem);
		if (tName == QLatin1String("Tabs"))
		{
			ParagraphStyle::TabRecord tb;
			tb.tabPosition = tAtt.valueAsDouble("Pos");
			tb.tabType = tAtt.valueAsInt("Type");
			QString tbCh(tAtt.valueAsString("Fill", ""));
			if (!tbCh.isEmpty())
				tb.tabFillChar = tbCh[0];
			tabValues.append(tb);
		}
		//Remove uppercase in 1.8 format
		if (tName == QLatin1String("LATEX") || tName == QLatin1String("LaTeX"))
		{
			if (newItem->isLatexFrame())
			{
				readLatexInfo(newItem->asLatexFrame(), reader);
			}
			else
			{
				while (!reader.atEnd() && !reader.hasError())
				{
					reader.readNext();
					if (reader.isEndElement() && reader.name() == tName)
						break;
				}
			}
		}
		if ((tName == QLatin1String("PAGEOBJECT") || tName == QLatin1String("PageObject")) || (tName == QLatin1String("ITEM") || tagName == QLatin1String("Item")))
		{
			if (newItem->isGroup())
			{
			//	bool success = true;
				QList<PageItem*>* docItems = doc->Items;
				QList<PageItem*> groupItems;
				doc->Items = &groupItems;
				ItemInfo itemInfo;
				// #12313: set the 'loadPage' parameter to true in order to
				// avoid the change of page mode and the doc item lists switch
				// when loading groups in masterpages
				ReadObjectParams readObjectParams2;
				readObjectParams2.baseDir = readObjectParams.baseDir;
				readObjectParams2.itemKind = (itemKind == PageItem::PatternItem) ? PageItem::PatternItem : PageItem::StandardItem;
				readObjectParams2.loadingPage = true;
				readObject(doc, reader, readObjectParams2, itemInfo);
				for (int as = 0; as < groupItems.count(); ++as)
				{
					PageItem* currItem = groupItems.at(as);
					newItem->groupItemList.append(currItem);
					currItem->Parent = newItem;
					currItem->m_layerID = newItem->m_layerID;
					currItem->OwnPage = newItem->OwnPage;
					currItem->OnMasterPage = newItem->OnMasterPage;
				}
				doc->Items = docItems;
			}
		}
		if (tName == QLatin1String("MARK"))
		{
			if (newItem->isTextFrame())
			{
				QString l = tAtt.valueAsString("label");
				MarkType t = (MarkType) tAtt.valueAsInt("type");
				Mark* mark = nullptr;
				if (m_Doc->isLoading())
				{
					mark = m_Doc->getMark(l, t);
				}
				else
				{	//doc is not loading so it is copy/paste task
					if (t == MARKVariableTextType)
						mark = m_Doc->getMark(l, t);
					else
					{
						//create copy of mark
						Mark* oldMark = m_Doc->getMark(l, t);
						if (oldMark == nullptr)
						{
							qWarning() << "wrong copy of oldMark";
							mark = m_Doc->newMark();
							mark->setType(t);
						}
						else
						{
							mark = m_Doc->newMark(oldMark);
							getUniqueName(l,doc->marksLabelsList(t), "_");
						}
						mark->label = l;
						if (t == MARKNoteMasterType)
						{  //create copy of note
							TextNote* old = mark->getNotePtr();
							TextNote* note = m_Doc->newNote(old->notesStyle());
							mark->setNotePtr(note);
							note->setMasterMark(mark);
							note->setSaxedText(old->saxedText());
							m_Doc->setNotesChanged(true);
						}
					}
				}
				if (mark == nullptr)
					qDebug() << "Undefined mark label ["<< l << "] type " << t;
				else
				{
					//set pointer to item holds mark in his text
					CharStyle newStyle;
					if (t == MARKAnchorType)
						mark->setItemPtr(newItem);
					mark->OwnPage = newItem->OwnPage;
					newItem->itemText.insertMark(mark, newItem->itemText.length());
					readCharacterStyleAttrs(doc, tAtt, newStyle);
					newItem->itemText.setCharStyle(newItem->itemText.length() - 1, 1, newStyle);
					lastStyle->StyleStart = newItem->itemText.length() - 1;
					lastStyle->Style = newStyle;
				}
			}
		}
		if (tName == QLatin1String("WeldEntry"))
		{
			PageItem::WeldingInfo wInf;
			wInf.weldItem = nullptr;
			wInf.weldPoint = FPoint(tAtt.valueAsDouble("WX", 0.0), tAtt.valueAsDouble("WY", 0.0));
			wInf.weldID = tAtt.valueAsInt("Target", 0);
			newItem->weldList.append(wInf);
		}
	}
	delete lastStyle;

	if (tabValues.count() > 0) 
	{
		ParagraphStyle newDefault(newItem->itemText.defaultStyle());
		newDefault.setTabValues(tabValues);
		newItem->itemText.setDefaultStyle(newDefault);
	}

	if (newItem->fill_gradient.stops() == 0)
	{
		const ScColor& col1 = doc->PageColors[doc->itemToolPrefs().shapeFillColor];
		const ScColor& col2 = doc->PageColors[doc->itemToolPrefs().shapeLineColor];
		newItem->fill_gradient.addStop(ScColorEngine::getRGBColor(col1, doc), 0.0, 0.5, 1.0, doc->itemToolPrefs().shapeFillColor, 100);
		newItem->fill_gradient.addStop(ScColorEngine::getRGBColor(col2, doc), 1.0, 0.5, 1.0, doc->itemToolPrefs().shapeLineColor, 100);
	}

	if (newItem->stroke_gradient.stops() == 0)
	{
		const ScColor& col1 = doc->PageColors[doc->itemToolPrefs().shapeFillColor];
		const ScColor& col2 = doc->PageColors[doc->itemToolPrefs().shapeLineColor];
		newItem->stroke_gradient.addStop(ScColorEngine::getRGBColor(col1, doc), 0.0, 0.5, 1.0, doc->itemToolPrefs().shapeFillColor, 100);
		newItem->stroke_gradient.addStop(ScColorEngine::getRGBColor(col2, doc), 1.0, 0.5, 1.0, doc->itemToolPrefs().shapeLineColor, 100);
	}

	if (newItem->mask_gradient.stops() == 0)
	{
		const ScColor& col1 = doc->PageColors[doc->itemToolPrefs().shapeFillColor];
		const ScColor& col2 = doc->PageColors[doc->itemToolPrefs().shapeLineColor];
		newItem->mask_gradient.addStop(ScColorEngine::getRGBColor(col1, doc), 0.0, 0.5, 1.0, doc->itemToolPrefs().shapeFillColor, 100);
		newItem->mask_gradient.addStop(ScColorEngine::getRGBColor(col2, doc), 1.0, 0.5, 1.0, doc->itemToolPrefs().shapeLineColor, 100);
	}
	if (newItem->GrType == Gradient_Conical)
	{
		if (!newItem->gradient().isEmpty())
			newItem->fill_gradient = doc->docGradients[newItem->gradient()];
		newItem->createConicalMesh();
	}

	if (newItem->isPathText())
		newItem->updatePolyClip();
#ifdef HAVE_OSG
	if (newItem->isImageFrame() || newItem->isLatexFrame() || newItem->isOSGFrame())
#else
	if (newItem->isImageFrame() || newItem->isLatexFrame())
#endif
	{
		if (!newItem->Pfile.isEmpty())
		{
			double imageXOffset = newItem->imageXOffset();
			double imageYOffset = newItem->imageYOffset();
			QString imageProfile = newItem->ImageProfile;
			QString embeddedProfile = newItem->EmbeddedProfile;
			bool useEmbeddedProfile = newItem->UseEmbedded;
			doc->loadPict(newItem->Pfile, newItem, false);
			newItem->setImageXYOffset(imageXOffset, imageYOffset);
			newItem->ImageProfile = imageProfile;
			newItem->EmbeddedProfile = embeddedProfile;
			newItem->UseEmbedded = useEmbeddedProfile;
			if (newItem->pixm.imgInfo.PDSpathData.contains(clipPath))
			{
				newItem->imageClip = newItem->pixm.imgInfo.PDSpathData[clipPath].copy();
				newItem->pixm.imgInfo.usedPath = clipPath;
				QTransform cl;
				cl.translate(newItem->imageXOffset() * newItem->imageXScale(), newItem->imageYOffset() * newItem->imageYScale());
				cl.scale(newItem->imageXScale(), newItem->imageYScale());
				newItem->imageClip.map(cl);
			}
			if (layerFound)
			{
				newItem->pixm.imgInfo.isRequest = true;
				doc->loadPict(newItem->Pfile, newItem, true);
				newItem->setImageXYOffset(imageXOffset, imageYOffset);
				newItem->ImageProfile = imageProfile;
				newItem->EmbeddedProfile = embeddedProfile;
				newItem->UseEmbedded = useEmbeddedProfile;
			}
		}
	}
	if (!readObjectParams.loadingPage)
		doc->setMasterPageMode(savedMasterPageMode);
	return !reader.hasError();
}

bool Scribus171Format::readPattern(ScribusDoc* doc, ScXmlStreamReader& reader, const QString& baseDir)
{
	ScPattern pat;
	ScXmlStreamAttributes attrs = reader.scAttributes();
	QString patternName = attrs.valueAsString("Name");
	bool success = true;
	isNewFormat = false;

	if (patternName.isEmpty())
	{
		reader.readToElementEnd();
		return true;
	}

	QStack< QList<PageItem*> > groupStack;
	QStack< QList<PageItem*> > groupStackP;
	QStack<int> groupStack2;
	QMap<int, PageItem*> TableID2;
	QList<PageItem*> TableItems2;
	QMap<int, PageItem*> WeldID;
	QList<PageItem*> WeldItems;

	pat.setDoc(doc);
	pat.width = attrs.valueAsDouble("width", 0.0);
	pat.height = attrs.valueAsDouble("height", 0.0);
	pat.scaleX = attrs.valueAsDouble("scaleX", 0.0);
	pat.scaleY = attrs.valueAsDouble("scaleY", 0.0);
	pat.xoffset = attrs.valueAsDouble("xoffset", 0.0);
	pat.yoffset = attrs.valueAsDouble("yoffset", 0.0);
	
	ReadObjectParams readObjectParams;
	readObjectParams.baseDir = baseDir;
	readObjectParams.itemKind = PageItem::PatternItem;
	readObjectParams.loadingPage = false;

	bool savedAlignGrid = m_Doc->SnapGrid;
	bool savedAlignGuides = m_Doc->SnapGuides;
	bool savedAlignElement = m_Doc->SnapElement;
	bool savedMasterPageMode = m_Doc->masterPageMode();
	m_Doc->SnapGrid = false;
	m_Doc->SnapGuides = false;
	m_Doc->SnapElement = false;

	m_Doc->setMasterPageMode(false);
	int itemCount1 = m_Doc->Items->count();

	QString tagName(reader.nameAsString());
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (!reader.isStartElement() || reader.name() != QLatin1String("PatternItem"))
			continue;

		ScXmlStreamAttributes tAtt = reader.attributes();
			
		ItemInfo itemInfo;

		success = readObject(doc, reader, readObjectParams, itemInfo);
		if (!success) break;

		itemInfo.item->OwnPage = -1 /*ownPage*/;
		itemInfo.item->OnMasterPage.clear();
		if (isNewFormat)
		{
			if (itemInfo.item->isTableItem)
				TableItems2.append(itemInfo.item);
			if (itemInfo.isWeldFlag)
				WeldItems.append(itemInfo.item);
		}
		else
		{
			if (itemInfo.item->isTableItem)
			{
				TableItems2.append(itemInfo.item);
				TableID2.insert(itemInfo.ownLink, itemInfo.item);
			}
			if (itemInfo.isWeldFlag)
			{
				WeldItems.append(itemInfo.item);
				WeldID.insert(itemInfo.ownWeld, itemInfo.item);
			}
		}
		if (groupStack.count() > 0)
		{
			groupStack.top().append(itemInfo.item);
			while (itemInfo.ownNr == groupStack2.top())
			{
				groupStackP.push(groupStack.pop());
				groupStack2.pop();
				if (groupStack2.count() == 0)
					break;
			}
		}
		if (itemInfo.isGroupFlag)
		{
			QList<PageItem*> GroupItems;
			GroupItems.append(itemInfo.item);
			groupStack.push(GroupItems);
			groupStack2.push(itemInfo.groupLastItem + itemInfo.ownNr);
		}
	}

	doc->SnapGrid = savedAlignGrid;
	doc->SnapGuides = savedAlignGuides;
	doc->SnapElement = savedAlignElement;
	if (!success)
	{
		doc->setMasterPageMode(savedMasterPageMode);
		return false;
	}
	if (isNewFormat)
	{
		if (TableItems2.count() != 0)
		{
			for (int ttc = 0; ttc < TableItems2.count(); ++ttc)
			{
				PageItem* ta = TableItems2.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = LinkID[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = LinkID[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = LinkID[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = LinkID[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (WeldItems.count() != 0)
		{
			for (int ttc = 0; ttc < WeldItems.count(); ++ttc)
			{
				PageItem* ta = WeldItems.at(ttc);
				for (int i = 0 ; i < ta->weldList.count(); ++i)
				{
					PageItem::WeldingInfo wInf = ta->weldList.at(i);
					ta->weldList[i].weldItem = LinkID.value(wInf.weldID, 0);
					if (ta->weldList[i].weldItem == nullptr)
						ta->weldList.removeAt(i--);
				}
			}
		}
	}
	else
	{
		if (TableItems2.count() != 0)
		{
			for (int ttc = 0; ttc < TableItems2.count(); ++ttc)
			{
				PageItem* ta = TableItems2.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = TableID2[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = TableID2[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = TableID2[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = TableID2[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (WeldItems.count() != 0)
		{
			for (int ttc = 0; ttc < WeldItems.count(); ++ttc)
			{
				PageItem* ta = WeldItems.at(ttc);
				for (int i = 0 ; i < ta->weldList.count(); ++i)
				{
					PageItem::WeldingInfo wInf = ta->weldList.at(i);
					ta->weldList[i].weldItem = WeldID.value(wInf.weldID, 0);
					if (ta->weldList[i].weldItem == nullptr)
						ta->weldList.removeAt(i--);
				}
			}
		}
	}

	while (groupStackP.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackP.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->DocItems.removeOne(cItem);
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackP, &m_Doc->DocItems);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	int itemCount2 = m_Doc->Items->count();
	if (itemCount2 > itemCount1)
	{
		PageItem* currItem = doc->Items->at(itemCount1);
		PageItem* newItem;
		double minx =  std::numeric_limits<double>::max();
		double miny =  std::numeric_limits<double>::max();
		double maxx = -std::numeric_limits<double>::max();
		double maxy = -std::numeric_limits<double>::max();
		for (int i = itemCount1; i < itemCount2; ++i)
		{
			currItem = doc->Items->at(i);
			double x1, x2, y1, y2;
			currItem->getVisualBoundingRect(&x1, &y1, &x2, &y2);
			minx = qMin(minx, x1);
			miny = qMin(miny, y1);
			maxx = qMax(maxx, x2);
			maxy = qMax(maxy, y2);
		}
		for (int i = itemCount1; i < itemCount2; ++i)
		{
			newItem = doc->Items->takeAt(itemCount1);
			newItem->gXpos = newItem->xPos() - minx;
			newItem->gYpos = newItem->yPos() - miny;
			newItem->gWidth = maxx - minx;
			newItem->gHeight = maxy - miny;
			newItem->setXYPos(newItem->gXpos, newItem->gYpos, true);
			newItem->moveBy(pat.xoffset, pat.yoffset, true);
			newItem->gXpos += pat.xoffset;
			newItem->gYpos += pat.yoffset;
			pat.items.append(newItem);
		}
		pat.createPreview();
	}
	doc->docPatterns.insert(patternName, pat);

	doc->setMasterPageMode(savedMasterPageMode);
	return success;
}

bool Scribus171Format::readStoryText(ScribusDoc *doc, ScXmlStreamReader& reader, StoryText& story, PageItem* item) const
{
	QString tagName(reader.nameAsString());
	ScXmlStreamAttributes attrs = reader.scAttributes();

	LastStyles * lastStyle = new LastStyles();
	while (!reader.atEnd() && !reader.hasError())
	{
		ScXmlStreamReader::TokenType tType = reader.readNext();
		if (reader.isEndElement() && tagName == reader.name())
			break;
		if (tType != ScXmlStreamReader::StartElement)
			continue;
		QString tName = reader.name().toString();
		ScXmlStreamAttributes tAtt = reader.scAttributes();

		if (tName == QLatin1String("DefaultStyle"))
		{
			ParagraphStyle newStyle;
			readParagraphStyle(doc, reader, newStyle);
			story.setDefaultStyle(newStyle);
		}
		//Remove ITEXT in 1.8
		if (tName == QLatin1String("ITEXT"))
			readItemText(story, tAtt, lastStyle);
		else if (tName == QLatin1String("Content"))
			readItemText(story, tAtt, lastStyle);
		else if (tName == QLatin1String("para"))
		{
			story.insertChars(story.length(), SpecialChars::PARSEP);
			ParagraphStyle newStyle;
			readParagraphStyle(doc, reader, newStyle);
			story.setStyle(story.length() - 1, newStyle);
			story.setCharStyle(story.length() - 1, 1, lastStyle->Style);
		}
		else if (tName == QLatin1String("trail"))
		{
			ParagraphStyle newStyle;
			readParagraphStyle(doc, reader, newStyle);
			story.setStyle(story.length(), newStyle);
		}
		else if (tName == QLatin1String("tab"))
		{
			CharStyle newStyle;
			story.insertChars(story.length(), SpecialChars::TAB);
			readCharacterStyleAttrs(doc, tAtt, newStyle);
			story.setCharStyle(story.length() - 1, 1, newStyle);
			lastStyle->StyleStart = story.length() - 1;
			lastStyle->Style = newStyle;
		}
		else if (tName == QLatin1String("breakline"))
			story.insertChars(story.length(), SpecialChars::LINEBREAK);
		else if (tName == QLatin1String("breakcol"))
			story.insertChars(story.length(), SpecialChars::COLBREAK);
		else if (tName == QLatin1String("breakframe"))
			story.insertChars(story.length(), SpecialChars::FRAMEBREAK);
		else if (tName == QLatin1String("nbhyphen"))
		{
			CharStyle newStyle;
			story.insertChars(story.length(), SpecialChars::NBHYPHEN);
			readCharacterStyleAttrs(doc, tAtt, newStyle);
			story.setCharStyle(story.length() - 1, 1, newStyle);
			lastStyle->StyleStart = story.length() - 1;
			lastStyle->Style = newStyle;
		}
		else if (tName == QLatin1String("nbspace"))
		{
			CharStyle newStyle;
			story.insertChars(story.length(), SpecialChars::NBSPACE);
			readCharacterStyleAttrs(doc, tAtt, newStyle);
			story.setCharStyle(story.length() - 1, 1, newStyle);
			lastStyle->StyleStart = story.length() - 1;
			lastStyle->Style = newStyle;
		}
		else if (tName == QLatin1String("zwnbspace"))
		{
			CharStyle newStyle;
			story.insertChars(story.length(), SpecialChars::ZWNBSPACE);
			readCharacterStyleAttrs(doc, tAtt, newStyle);
			story.setCharStyle(story.length() - 1, 1, newStyle);
			lastStyle->StyleStart = story.length() - 1;
			lastStyle->Style = newStyle;
		}
		else if (tName == QLatin1String("zwspace"))
		{
			CharStyle newStyle;
			story.insertChars(story.length(), SpecialChars::ZWSPACE);
			readCharacterStyleAttrs(doc, tAtt, newStyle);
			story.setCharStyle(story.length() - 1, 1, newStyle);
			lastStyle->StyleStart = story.length() - 1;
			lastStyle->Style = newStyle;
		}
		else if (tName == QLatin1String("var"))
		{
			CharStyle newStyle;
			if (tAtt.value("name") == QLatin1String("pgno"))
				story.insertChars(story.length(), SpecialChars::PAGENUMBER);
			else
				story.insertChars(story.length(), SpecialChars::PAGECOUNT);
			readCharacterStyleAttrs(doc, tAtt, newStyle);
			story.setCharStyle(story.length() - 1, 1, newStyle);
			lastStyle->StyleStart = story.length() - 1;
			lastStyle->Style = newStyle;
		}

		if (tName == QLatin1String("MARK"))
		{
			QString label = tAtt.valueAsString("label");
			MarkType type = (MarkType) tAtt.valueAsInt("type");
			Mark* mark = nullptr;
			if (m_Doc->isLoading())
			{
				mark = m_Doc->getMark(label, type);
				// qDebug()<<"doc loading"<<label<<type<<item->OwnPage<<item->itemName();
			}
			else
			{	//doc is not loading so it is copy/paste task
				if (type == MARKVariableTextType)
					mark = m_Doc->getMark(label, type);
				else
				{
					//create copy of mark
					Mark* oldMark = m_Doc->getMark(label, type);
					if (oldMark == nullptr)
					{
						qWarning() << "wrong copy of oldMark";
						mark = m_Doc->newMark();
						mark->setType(type);
					}
					else
					{
						mark = m_Doc->newMark(oldMark);
						getUniqueName(label, doc->marksLabelsList(type), "_");
					}
					mark->label = label;
					if (type == MARKNoteMasterType)
					{  //create copy of note
						TextNote* old = mark->getNotePtr();
						TextNote* note = m_Doc->newNote(old->notesStyle());
						mark->setNotePtr(note);
						note->setMasterMark(mark);
						note->setSaxedText(old->saxedText());
						m_Doc->setNotesChanged(true);
					}
				}
			}
			if (mark == nullptr)
				qDebug() << "Undefined mark label ["<< label << "] type " << type;
			else
			{
				//set pointer to item holds mark in his text
				CharStyle newStyle;
				if (type == MARKAnchorType)
					mark->setItemPtr(item);
				mark->OwnPage = item->OwnPage;
				story.insertMark(mark, story.length());
				readCharacterStyleAttrs(doc, tAtt, newStyle);
				story.setCharStyle(story.length() - 1, 1, newStyle);
				lastStyle->StyleStart = story.length() - 1;
				lastStyle->Style = newStyle;
			}
		}
	}
	delete lastStyle;

	return !reader.hasError();
}

bool Scribus171Format::readItemText(StoryText& story, const ScXmlStreamAttributes& attrs, LastStyles* last) const
{
	QString tmp2;
	CharStyle newStyle;
	ScribusDoc* doc = story.doc();
	
	readCharacterStyleAttrs(doc, attrs, newStyle);

	if (attrs.hasAttribute(QLatin1String("Unicode")))
	{
		int uniValue = attrs.valueAsInt("Unicode");
		if (uniValue >= 0 && uniValue <= 0xFFFF)
			tmp2 = QChar(uniValue);
		else
		{
			qDebug() << "scribus171format: invalid Unicode character detected:" << uniValue;
			return true;
		}
	}
	else
	{
		//Remove uppercase in 1.8
		if (attrs.hasAttribute("CH"))
			tmp2 = attrs.valueAsString("CH");
		else
			tmp2 = attrs.valueAsString("Chars");
		
		// legacy stuff:
		tmp2.replace(QRegularExpression("\r"), QChar(13));
		tmp2.replace(QRegularExpression("\n"), QChar(13));
		tmp2.replace(QRegularExpression("\t"), QChar(9));
	}

	// more legacy stuff:
	QString pstylename;
	if (attrs.hasAttribute("PSTYLE"))
		pstylename = attrs.valueAsString("PSTYLE", "");
	else
		pstylename = attrs.valueAsString("ParagraphStyle", "");
	fixLegacyCharStyle(newStyle);
	last->ParaStyle = pstylename;
	// end of legacy stuff

	int iobj = -1;
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("COBJ"))
		iobj = attrs.valueAsInt("COBJ", -1);
	else
		iobj = attrs.valueAsInt("Object", -1);

	for (int cxx = 0; cxx < tmp2.length(); ++cxx)
	{
		QChar ch = tmp2.at(cxx);		
		{ // Legacy mode
			if (ch == QChar(5))
				ch = SpecialChars::PARSEP;
			if (ch == QChar(4))
				ch = SpecialChars::TAB;
		}
		
		int pos = story.length();
		if (ch == SpecialChars::OBJECT)
		{
			if (LinkID.contains(iobj))
			{
				if (FrameItems.contains(LinkID[iobj]))
				{
					int fIndex = doc->addToInlineFrames(LinkID[iobj]);
					story.insertObject(pos, fIndex);
				}
			}
			else
			{
				if (doc->FrameItems.contains(iobj))
					story.insertObject(pos, iobj);
				else
					qDebug() << QString("scribus171format: invalid inline frame used in text object : %1").arg(iobj);
			}
		}
		else if (ch == SpecialChars::SHYPHEN && pos > 0)
		{
//			qDebug() << QString("scribus171format: SHYPHEN at %1").arg(pos);
			// double SHY means user provided SHY, single SHY is automatic one
			if (story.hasFlag(pos-1, ScLayout_HyphenationPossible))
			{
				story.clearFlag(pos-1, ScLayout_HyphenationPossible);
				story.insertChars(pos, QString(ch));
			}
			else
			{
				story.setFlag(pos-1, ScLayout_HyphenationPossible);
			}
		}
		else {
			story.insertChars(pos, QString(ch));
		}
//		qDebug() << QString("style at %1: %2 ^ %3 = %4 (%5)").arg(pos).arg((uint)newStyle.effects()).arg((uint)last->Style.effects()).arg((uint)(newStyle.effects() ^ last->Style.effects())).arg(newStyle != last->Style);
		if (newStyle != last->Style) // || (newStyle.effects() ^ last->Style.effects()) == ScStyle_HyphenationPossible) 
		{  // FIXME StyleFlag operator== ignores hyphen flag
//			qDebug() << QString("new style at %1: %2 -> %3").arg(pos).arg(last->Style.asString()).arg(newStyle.asString());
			story.setCharStyle(last->StyleStart, pos-last->StyleStart, last->Style);
			last->Style = newStyle;
			last->StyleStart = pos;
		}
		if (ch == SpecialChars::PARSEP) {
			ParagraphStyle pstyle;
			// Qt4 if (last->ParaStyle >= 0) {
			if (!last->ParaStyle.isEmpty()) {
				pstyle.setParent( last->ParaStyle );
			}
			story.applyStyle(pos, pstyle);
		}
	}

	story.setCharStyle(last->StyleStart, story.length()-last->StyleStart, last->Style);
	last->StyleStart = story.length();
/*
	QString dbg;
	for (int i=0; i < story.length(); ++i)
	{
		dbg += story.text(i,1);
		if (story.item(i)->effects() & ScStyle_HyphenationPossible)
			dbg += "~";
	}
	qDebug("scribus171format: read itemtext %d '%s'", story.length(), dbg.latin1());
	*/
	ParagraphStyle pstyle;

	if (!last->ParaStyle.isEmpty())
	{
		pstyle.setParent(last->ParaStyle);
		story.applyStyle(story.length() - 1, pstyle);
	}

	return true;
}

bool Scribus171Format::readPageItemAttributes(PageItem* item, ScXmlStreamReader& reader) const
{
	QString tagName(reader.nameAsString());
	ObjAttrVector pageItemAttributes;
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (reader.isStartElement() && reader.name() == QLatin1String("ItemAttribute"))
		{
			ScXmlStreamAttributes tAtt = reader.scAttributes();
			ObjectAttribute objattr;
			objattr.name = tAtt.valueAsString("Name");
			objattr.type = tAtt.valueAsString("Type");
			objattr.value = tAtt.valueAsString("Value");
			objattr.parameter = tAtt.valueAsString("Parameter");
			objattr.relationship = tAtt.valueAsString("Relationship");
			objattr.relationshipto = tAtt.valueAsString("RelationshipTo");
			objattr.autoaddto = tAtt.valueAsString("AutoAddTo");
			pageItemAttributes.append(objattr);
		}
	}
	item->setObjectAttributes(&pageItemAttributes);
	return !reader.hasError();
}

PageItem* Scribus171Format::pasteItem(ScribusDoc *doc, const ScXmlStreamAttributes& attrs, const QString& baseDir, PageItem::ItemKind itemKind, int pageNr)
{
	PageItem::ItemType pt;
	if (attrs.hasAttribute("PTYPE"))
		pt = static_cast<PageItem::ItemType>(attrs.valueAsInt("PTYPE"));
	else
		pt = static_cast<PageItem::ItemType>(attrs.valueAsInt("ItemType"));
	bool isGroupFlag = attrs.valueAsBool("isGroupControl", false);
	if (isGroupFlag)
		pt = PageItem::Group;
	bool isNoteFrameFlag = attrs.valueAsBool("isNoteFrame", false);
	if (isNoteFrameFlag && (pt == PageItem::TextFrame))
		pt = PageItem::NoteFrame;

	double x;
	if (attrs.hasAttribute("XPOS"))
		x = attrs.valueAsDouble("XPOS");
	else
		x = attrs.valueAsDouble("XPosition");
	double y;
	if (attrs.hasAttribute("YPOS"))
		y = attrs.valueAsDouble("YPOS");
	else
		y = attrs.valueAsDouble("YPosition");
	x = Xp + x - GrX;
	y = Yp + y - GrY;
	double w;
	if (attrs.hasAttribute("WIDTH"))
		w = attrs.valueAsDouble("WIDTH");
	else
		w = attrs.valueAsDouble("Width");
	double h;
	if (attrs.hasAttribute("HEIGHT"))
		h = attrs.valueAsDouble("HEIGHT");
	else
		h = attrs.valueAsDouble("Height");
	double pw;
	if (attrs.hasAttribute("PWIDTH"))
		pw = attrs.valueAsDouble("PWIDTH");
	else
		pw = attrs.valueAsDouble("LineWidth");
	double imageOffsetX;
	if (attrs.hasAttribute("LOCALX"))
		imageOffsetX = attrs.valueAsDouble("LOCALX");
	else
		imageOffsetX = attrs.valueAsDouble("ImageOffsetX");
	double imageOffsetY;
	if (attrs.hasAttribute("LOCALY"))
		imageOffsetY = attrs.valueAsDouble("LOCALY");
	else
		imageOffsetY = attrs.valueAsDouble("ImageOffsetY");
	double imageScaleX;
	if (attrs.hasAttribute("LOCALSCX"))
		imageScaleX = attrs.valueAsDouble("LOCALSCX");
	else
		imageScaleX = attrs.valueAsDouble("ImageScaleX");
	double imageScaleY;
	if (attrs.hasAttribute("LOCALSCY"))
		imageScaleY = attrs.valueAsDouble("LOCALSCY");
	else
		imageScaleY = attrs.valueAsDouble("ImageScaleY");
	QString fillColor;
	if (attrs.hasAttribute("PCOLOR"))
		fillColor = attrs.valueAsString("PCOLOR");
	else
		fillColor = attrs.valueAsString("FillColor");
	if (fillColor.isEmpty())
		fillColor = CommonStrings::None;
	QString lineColor;
	if (attrs.hasAttribute("PCOLOR2"))
		lineColor = attrs.valueAsString("PCOLOR2");
	else
		lineColor = attrs.valueAsString("LineColor");
	if (lineColor.isEmpty())
		lineColor = CommonStrings::None;
	QColor tmpc;
	PageItem *currItem = nullptr;
	QString clPath;
	int z = 0;
	switch (pt)
	{
	// OBSOLETE CR 2005-02-06
	case PageItem::ItemType1:
		z = doc->itemAdd(PageItem::Polygon, PageItem::Ellipse, x, y, w, h, pw, fillColor, lineColor, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		break;
	//
	case PageItem::ImageFrame:
	case PageItem::OSGFrame:
	case PageItem::LatexFrame: /*Everything that is valid for image frames is also valid for latex frames*/
		z = doc->itemAdd(pt, PageItem::Unspecified, x, y, w, h, 1, doc->itemToolPrefs().imageFillColor, doc->itemToolPrefs().imageStrokeColor, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		UndoManager::instance()->setUndoEnabled(false);
		if (attrs.hasAttribute("SCALETYPE"))
			currItem->ScaleType = attrs.valueAsInt("SCALETYPE", 1);
		else
			currItem->ScaleType = attrs.valueAsInt("ImageScaleType", 1);
		if (attrs.hasAttribute("RATIO"))
			currItem->AspectRatio = attrs.valueAsInt("RATIO", 0);
		else
			currItem->AspectRatio = attrs.valueAsInt("ImageRatio", 0);
		currItem->setImageXYScale(imageScaleX, imageScaleY);
		currItem->setImageXYOffset(imageOffsetX, imageOffsetY);
		if (attrs.hasAttribute("LOCALROT"))
			currItem->setImageRotation(attrs.valueAsDouble("LOCALROT"));
		else
			currItem->setImageRotation(attrs.valueAsDouble("ImageRotation"));
#ifdef HAVE_OSG
		if ((currItem->isImageFrame() || currItem->isOSGFrame()) && (!currItem->isLatexFrame()))
#else
		if ((currItem->isImageFrame()) && (!currItem->isLatexFrame()))
#endif
		{
			bool inlineF = false;
			//Remove lowercase in 1.8
			if (attrs.hasAttribute("isInlineImage"))
				attrs.valueAsBool("isInlineImage", false);
			else
				attrs.valueAsBool("IsInlineImage", false);
			QString dat = attrs.valueAsString("ImageData", "");
			QByteArray inlineImageData;
			inlineImageData.append(dat.toUtf8());
			QString inlineImageExt;
			//Remove lowercase in 1.8
			if (attrs.hasAttribute("inlineImageExt"))
				inlineImageExt = attrs.valueAsString("inlineImageExt", "");
			else
				inlineImageExt = attrs.valueAsString("InlineImageExt", "");
			if (inlineF)
			{
				if (inlineImageData.size() > 0)
				{
					QTemporaryFile *tempFile = new QTemporaryFile(QDir::tempPath() + "/scribus_temp_XXXXXX." + inlineImageExt);
					tempFile->setAutoRemove(false);
					tempFile->open();
					QString fileName = getLongPathName(tempFile->fileName());
					tempFile->close();
					inlineImageData = qUncompress(QByteArray::fromBase64(inlineImageData));
					QFile outFil(fileName);
					if (outFil.open(QIODevice::WriteOnly))
					{
						outFil.write(inlineImageData);
						outFil.close();
						currItem->isInlineImage = true;
						currItem->Pfile = QDir::fromNativeSeparators(fileName);
						currItem->isTempFile = true;
					}
					delete tempFile;
				}
			}
			else
			{
				if (attrs.hasAttribute("PFILE"))
					currItem->Pfile = Relative2Path(attrs.valueAsString("PFILE"), baseDir);
				else
					currItem->Pfile = Relative2Path(attrs.valueAsString("ImageFileName"), baseDir);
				currItem->Pfile = QDir::fromNativeSeparators(currItem->Pfile);
			}
#ifdef HAVE_OSG
			if (currItem->isOSGFrame())
			{
				PageItem_OSGFrame *osgframe = currItem->asOSGFrame();
				//Remove lowercase in 1.8
				if (attrs.hasAttribute("modelFile"))
				{
					osgframe->modelFile = Relative2Path(attrs.valueAsString("modelFile"), baseDir);
					osgframe->currentView = attrs.valueAsString("currentViewName", "");
				}
				else
				{
					osgframe->modelFile = Relative2Path(attrs.valueAsString("ModelFile"), baseDir);
					osgframe->currentView = attrs.valueAsString("CurrentViewName", "");
				}
				osgframe->loadModel();
			}
#endif
		}
		if (attrs.hasAttribute("PRFILE"))
			currItem->ImageProfile = attrs.valueAsString("PRFILE", "");
		else
			currItem->ImageProfile = attrs.valueAsString("ImageProfile", "");
		if (attrs.hasAttribute("IRENDER"))
			currItem->ImageIntent = (eRenderIntent) attrs.valueAsInt("IRENDER" , 1);
		else
			currItem->ImageIntent = (eRenderIntent) attrs.valueAsInt("ImageColorimetricIntent" , 1);
		if (attrs.hasAttribute("EPROF"))
			currItem->EmbeddedProfile = attrs.valueAsString("EPROF", "");
		else
			currItem->EmbeddedProfile = attrs.valueAsString("EmbeddedProfile", "");
		if (attrs.hasAttribute("EMBEDDED"))
			currItem->UseEmbedded = attrs.valueAsInt("EMBEDDED", 1);
		else
			currItem->UseEmbedded = attrs.valueAsInt("UseEmbeddedProfile", 1);
		currItem->pixm.imgInfo.lowResType = attrs.valueAsInt("ImageRes", 1);
		if (attrs.hasAttribute("Pagenumber"))
			currItem->pixm.imgInfo.actualPageNumber = attrs.valueAsInt("Pagenumber", 0);
		else
			currItem->pixm.imgInfo.actualPageNumber = attrs.valueAsInt("ImagePageNumber", 0);
		if (attrs.hasAttribute("COMPRESSIONMETHOD"))
		{
			if ((currItem->OverrideCompressionMethod = attrs.hasAttribute("COMPRESSIONMETHOD")))
				currItem->CompressionMethodIndex = attrs.valueAsInt("COMPRESSIONMETHOD", 0);
			if ((currItem->OverrideCompressionQuality = attrs.hasAttribute("COMPRESSIONQUALITY")))
				currItem->CompressionQualityIndex = attrs.valueAsInt("COMPRESSIONQUALITY");
		}
		else
		{
			if ((currItem->OverrideCompressionMethod = attrs.hasAttribute("CompressionMethod")))
				currItem->CompressionMethodIndex = attrs.valueAsInt("CompressionMethod", 0);
			if ((currItem->OverrideCompressionQuality = attrs.hasAttribute("CompressionQuality")))
				currItem->CompressionQualityIndex = attrs.valueAsInt("CompressionQuality");
		}
		currItem->setImageXYScale(imageScaleX, imageScaleY);
		//Remove uppercase in 1.8
		if (attrs.hasAttribute("LOCALROT"))
			currItem->setImageRotation(attrs.valueAsDouble("LOCALROT"));
		else
			currItem->setImageRotation(attrs.valueAsDouble("ImageRotation"));
		clPath = attrs.valueAsString("ImageClip", "");
		if (!clPath.isEmpty())
		{
			clipPath = clPath;
			layerFound = true;
		}
		if (attrs.hasAttribute("PICART"))
			currItem->setImageVisible( attrs.valueAsInt("PICART"));
		else
			currItem->setImageVisible( attrs.valueAsInt("ImageVisible"));
		currItem->setLineWidth(pw);
		UndoManager::instance()->setUndoEnabled(true);
		break;
	// OBSOLETE CR 2005-02-06
	case PageItem::ItemType3:
		z = doc->itemAdd(PageItem::Polygon, PageItem::Rectangle, x, y, w, h, pw, fillColor, lineColor, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		break;
	//
	case PageItem::PathText:
		z = doc->itemAdd(PageItem::PathText, PageItem::Unspecified, x, y, w, h, pw, CommonStrings::None, fillColor, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		break;
	case PageItem::NoteFrame:
	case PageItem::TextFrame:
		z = doc->itemAdd(pt, PageItem::Unspecified, x, y, w, h, pw, CommonStrings::None, fillColor, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		break;
	case PageItem::Line:
		z = doc->itemAdd(PageItem::Line, PageItem::Unspecified, x, y, w, h, pw, CommonStrings::None, lineColor, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		break;
	case PageItem::Polygon:
		z = doc->itemAdd(PageItem::Polygon, PageItem::Unspecified, x, y, w, h, pw, fillColor, lineColor, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		break;
	case PageItem::PolyLine:
		z = doc->itemAdd(PageItem::PolyLine, PageItem::Unspecified, x, y, w, h, pw, fillColor, lineColor, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		break;
	case PageItem::Symbol:
		z = doc->itemAdd(PageItem::Symbol, PageItem::Unspecified, x, y, w, h, 0, CommonStrings::None, CommonStrings::None, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		currItem->setPattern( attrs.valueAsString("pattern", "") );
		break;
	case PageItem::Group:
		z = doc->itemAdd(PageItem::Group, PageItem::Unspecified, x, y, w, h, 0, CommonStrings::None, CommonStrings::None, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		if (attrs.hasAttribute("groupWidth"))
		{
			currItem->groupWidth = attrs.valueAsDouble("groupWidth", w);
			currItem->groupHeight = attrs.valueAsDouble("groupHeight", h);
		}
		else
		{
			currItem->groupWidth = attrs.valueAsDouble("GroupWidth", w);
			currItem->groupHeight = attrs.valueAsDouble("GroupHeight", h);
		}
		doc->GroupCounter++;
		break;
	case PageItem::RegularPolygon:
		z = doc->itemAdd(PageItem::RegularPolygon, PageItem::Unspecified, x, y, w, h, pw, fillColor, lineColor, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		break;
	case PageItem::Arc:
		z = doc->itemAdd(PageItem::Arc, PageItem::Unspecified, x, y, w, h, pw, fillColor, lineColor, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		break;
	case PageItem::Spiral:
		z = doc->itemAdd(PageItem::Spiral, PageItem::Unspecified, x, y, w, h, pw, fillColor, lineColor, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2) 
			currItem->setOwnerPage(pageNr);
		break;
	case PageItem::Table:
		z = doc->itemAdd(PageItem::Table, PageItem::Unspecified, x, y, w, h, 0.0, CommonStrings::None, CommonStrings::None, itemKind);
		currItem = doc->Items->at(z);
		if (pageNr > -2)
			currItem->setOwnerPage(pageNr);
		break;
	case PageItem::Multiple:
		Q_ASSERT(false);
		break;
	}

	if(attrs.hasAttribute("groupClips"))
		currItem->setGroupClipping(attrs.valueAsBool("groupClips", true));
	else
		currItem->setGroupClipping(attrs.valueAsBool("GroupClipping", true));
	if (attrs.hasAttribute("FRTYPE"))
		currItem->FrameType = attrs.valueAsInt("FRTYPE", 0);
	else
		currItem->FrameType = attrs.valueAsInt("FrameType", 0);
	//Remove wrong case in 1.8
	int startArrowIndex = 0;
	if (attrs.hasAttribute("startArrowIndex"))
		attrs.valueAsInt("startArrowIndex", 0);
	else
		attrs.valueAsInt("StartArrowIndex", 0);
	if ((startArrowIndex < 0) || (startArrowIndex > static_cast<int>(doc->arrowStyles().size())))
	{
		qDebug() << QString("scribus171format: invalid arrow index: %").arg(startArrowIndex);
		startArrowIndex = 0;
	}
	currItem->setStartArrowIndex(startArrowIndex);
	//Remove wrong case in 1.8
	int endArrowIndex = 0;
	if (attrs.hasAttribute("endArrowIndex"))
		attrs.valueAsInt("endArrowIndex", 0);
	else
		attrs.valueAsInt("EndArrowIndex", 0);
	if ((endArrowIndex < 0) || (endArrowIndex > static_cast<int>(doc->arrowStyles().size())))
	{
		qDebug() << QString("scribus171format: invalid arrow index: %").arg(endArrowIndex);
		endArrowIndex = 0;
	}
	currItem->setEndArrowIndex(endArrowIndex);
	//Remove wrong case in 1.8
	if (attrs.hasAttribute("startArrowScale"))
		currItem->setStartArrowScale(attrs.valueAsInt("startArrowScale", 100));
	else
		currItem->setStartArrowScale(attrs.valueAsInt("StartArrowScale", 100));
	//Remove wrong case in 1.8
	if (attrs.hasAttribute("endArrowScale"))
		currItem->setEndArrowScale(attrs.valueAsInt("endArrowScale", 100));
	else
		currItem->setEndArrowScale(attrs.valueAsInt("EndArrowScale", 100));


	if (attrs.hasAttribute("NAMEDLST"))
		currItem->NamedLStyle = attrs.valueAsString("NAMEDLST", "");
	else
		currItem->NamedLStyle = attrs.valueAsString("NamedLineStyle", "");
	if (attrs.hasAttribute("BOOKMARK"))
		currItem->isBookmark = attrs.valueAsInt("BOOKMARK", 0);
	else
		currItem->isBookmark = attrs.valueAsInt("BookMark", 0);
	if (attrs.hasAttribute("FLIPPEDH"))
		currItem->setImageFlippedH( attrs.valueAsInt("FLIPPEDH", 0));
	else
		currItem->setImageFlippedH( attrs.valueAsInt("FlippedHorizontal", 0));
	if (attrs.hasAttribute("FLIPPEDV"))
		currItem->setImageFlippedV( attrs.valueAsInt("FLIPPEDV", 0));
	else
		currItem->setImageFlippedV( attrs.valueAsInt("FlippedVertical", 0));
	if (attrs.hasAttribute("RADRECT"))
		currItem->setCornerRadius( attrs.valueAsDouble("RADRECT", 0.0));
	else
		currItem->setCornerRadius( attrs.valueAsDouble("CornerRadius", 0.0));
	if (attrs.hasAttribute("CLIPEDIT"))
		currItem->ClipEdited = attrs.valueAsInt("CLIPEDIT", 0);
	else
		currItem->ClipEdited = attrs.valueAsInt("ClipEdited", 0);
	currItem->setFillColor(fillColor);
	currItem->setLineColor(lineColor);
	if (attrs.hasAttribute("SHADE"))
		currItem->setFillShade(attrs.valueAsInt("SHADE", 100));
	else
		currItem->setFillShade(attrs.valueAsInt("FillShade", 100));
	if (attrs.hasAttribute("SHADE2"))
		currItem->setLineShade(attrs.valueAsInt("SHADE2", 100));
	else
		currItem->setLineShade(attrs.valueAsInt("LineShade", 100));

	ParagraphStyle pstyle;
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("LINESP"))
		pstyle.setLineSpacing(attrs.valueAsDouble("LINESP"));
	else
		if (attrs.hasAttribute("LineSpacing"))
			pstyle.setLineSpacing(attrs.valueAsDouble("LineSpacing"));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("LINESPMode"))
		pstyle.setLineSpacingMode(static_cast<ParagraphStyle::LineSpacingMode>(attrs.valueAsInt("LINESPMode", 0)));
	else
		if (attrs.hasAttribute("LineSpacingMode"))
			pstyle.setLineSpacingMode(static_cast<ParagraphStyle::LineSpacingMode>(attrs.valueAsInt("LineSpacingMode", 0)));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("ALIGN"))
		pstyle.setAlignment(static_cast<ParagraphStyle::AlignmentType>(attrs.valueAsInt("ALIGN", 0)));
	else
		if (attrs.hasAttribute("Alignment"))
			pstyle.setAlignment(static_cast<ParagraphStyle::AlignmentType>(attrs.valueAsInt("Alignment", 0)));
	//Remove uppercase in 1.8
	if (attrs.valueAsBool("REVERS"))
		pstyle.setDirection(ParagraphStyle::RTL);
	else
		if (attrs.valueAsBool("Reversed"))
			pstyle.setDirection(ParagraphStyle::RTL);
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("DIRECTION"))
		pstyle.setDirection(static_cast<ParagraphStyle::DirectionType>(attrs.valueAsInt("DIRECTION", 0)));
	else
		if (attrs.hasAttribute("Direction"))
			pstyle.setDirection(static_cast<ParagraphStyle::DirectionType>(attrs.valueAsInt("Direction", 0)));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("IFONT"))
		pstyle.charStyle().setFont(m_AvailableFonts->findFont(attrs.valueAsString("IFONT"), doc));
	else
		if (attrs.hasAttribute("Font"))
			pstyle.charStyle().setFont(m_AvailableFonts->findFont(attrs.valueAsString("Font"), doc));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("ISIZE"))
		pstyle.charStyle().setFontSize(qRound(attrs.valueAsDouble("ISIZE") * 10));
	else
		if (attrs.hasAttribute("FontSize"))
			pstyle.charStyle().setFontSize(qRound(attrs.valueAsDouble("FontSize") * 10));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTSTROKE"))
		pstyle.charStyle().setStrokeColor(attrs.valueAsString("TXTSTROKE"));
	else
		if (attrs.hasAttribute("StrokeColor"))
			pstyle.charStyle().setStrokeColor(attrs.valueAsString("StrokeColor"));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTFILL"))
		pstyle.charStyle().setFillColor(attrs.valueAsString("TXTFILL"));
	else
		if (attrs.hasAttribute("FontColor"))
			pstyle.charStyle().setFillColor(attrs.valueAsString("FontColor"));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTSTRSH"))
		pstyle.charStyle().setStrokeShade(attrs.valueAsInt("TXTSTRSH"));
	else
		if (attrs.hasAttribute("StrokeShade"))
			pstyle.charStyle().setStrokeShade(attrs.valueAsInt("StrokeShade"));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTFILLSH"))
		pstyle.charStyle().setFillShade(attrs.valueAsInt("TXTFILLSH"));
	else
		if (attrs.hasAttribute("FillShade"))
			pstyle.charStyle().setFillShade(attrs.valueAsInt("FillShade"));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTSCALE"))
		pstyle.charStyle().setScaleH(qRound(attrs.valueAsDouble("TXTSCALE") * 10));
	else
		if (attrs.hasAttribute("ScaleHorizontal"))
			pstyle.charStyle().setScaleH(qRound(attrs.valueAsDouble("ScaleHorizontal") * 10));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTSCALEV"))
		pstyle.charStyle().setScaleV(qRound(attrs.valueAsDouble("TXTSCALEV") * 10));
	else
		if (attrs.hasAttribute("ScaleVertical"))
			pstyle.charStyle().setScaleV(qRound(attrs.valueAsDouble("ScaleVertical") * 10));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTBASE"))
		pstyle.charStyle().setBaselineOffset(qRound(attrs.valueAsDouble("TXTBASE") * 10));
	else
		if (attrs.hasAttribute("BaselineOffset"))
			pstyle.charStyle().setBaselineOffset(qRound(attrs.valueAsDouble("BaselineOffset") * 10));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTSHX"))
		pstyle.charStyle().setShadowXOffset(qRound(attrs.valueAsDouble("TXTSHX") * 10));
	else
		if (attrs.hasAttribute("TextShadowXOffset"))
			pstyle.charStyle().setShadowXOffset(qRound(attrs.valueAsDouble("TextShadowXOffset") * 10));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTSHY"))
		pstyle.charStyle().setShadowYOffset(qRound(attrs.valueAsDouble("TXTSHY") * 10));
	else
		if (attrs.hasAttribute("TextShadowYOffset"))
			pstyle.charStyle().setShadowYOffset(qRound(attrs.valueAsDouble("TextShadowYOffset") * 10));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTOUT"))
		pstyle.charStyle().setOutlineWidth(qRound(attrs.valueAsDouble("TXTOUT") * 10));
	else
		if (attrs.hasAttribute("TextOutlineWidth"))
			pstyle.charStyle().setOutlineWidth(qRound(attrs.valueAsDouble("TextOutlineWidth") * 10));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTULP"))
		pstyle.charStyle().setUnderlineOffset(qRound(attrs.valueAsDouble("TXTULP") * 10));
	else
		if (attrs.hasAttribute("TextUnderlineOffset"))
			pstyle.charStyle().setUnderlineOffset(qRound(attrs.valueAsDouble("TextUnderlineOffset") * 10));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTULW"))
		pstyle.charStyle().setUnderlineWidth(qRound(attrs.valueAsDouble("TXTULW") * 10));
	else
		if (attrs.hasAttribute("TextUnderlineWidth"))
			pstyle.charStyle().setUnderlineWidth(qRound(attrs.valueAsDouble("TextUnderlineWidth") * 10));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTSTP"))
		pstyle.charStyle().setStrikethruOffset(qRound(attrs.valueAsDouble("TXTSTP") * 10));
	else
		if (attrs.hasAttribute("TextStrikeThroughOffset"))
			pstyle.charStyle().setStrikethruOffset(qRound(attrs.valueAsDouble("TextStrikeThroughOffset") * 10));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTSTW"))
		pstyle.charStyle().setStrikethruWidth(qRound(attrs.valueAsDouble("TXTSTW") * 10));
	else
		if (attrs.hasAttribute("TextStrikeThroughWidth"))
			pstyle.charStyle().setStrikethruWidth(qRound(attrs.valueAsDouble("TextStrikeThroughWidth") * 10));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTFEATURES"))
		pstyle.charStyle().setFeatures(attrs.valueAsString("TXTFEATURES").split(" ", Qt::SkipEmptyParts));
	else
		if (attrs.hasAttribute("Features"))
			pstyle.charStyle().setFeatures(attrs.valueAsString("Features").split(" ", Qt::SkipEmptyParts));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("TXTKERN"))
		pstyle.charStyle().setTracking(qRound(attrs.valueAsDouble("TXTKERN", 0.0) * 10));
	else
		if (attrs.hasAttribute("Kerning"))
			pstyle.charStyle().setTracking(qRound(attrs.valueAsDouble("Kerning", 0.0) * 10));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("wordTrack"))
		pstyle.charStyle().setWordTracking(attrs.valueAsDouble("wordTrack"));
	else
		if (attrs.hasAttribute("WordTrack"))
			pstyle.charStyle().setWordTracking(attrs.valueAsDouble("WordTrack"));
	if (attrs.hasAttribute("MinWordTrack"))
		pstyle.setMinWordTracking(attrs.valueAsDouble("MinWordTrack"));
	if (attrs.hasAttribute("MinGlyphShrink"))
		pstyle.setMinGlyphExtension(attrs.valueAsDouble("MinGlyphShrink"));
	if (attrs.hasAttribute("MaxGlyphExtend"))
		pstyle.setMaxGlyphExtension(attrs.valueAsDouble("MaxGlyphExtend"));
	if (attrs.hasAttribute("OpticalMargins"))
		pstyle.setOpticalMargins(attrs.valueAsInt("OpticalMargins"));
	if (attrs.hasAttribute("OpticalMarginSetId"))
		pstyle.setOpticalMarginSetId(attrs.valueAsString("OpticalMarginSetId"));
	if (attrs.hasAttribute("HyphenationMode"))
		pstyle.setHyphenationMode(attrs.valueAsInt("HyphenationMode"));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("leftMargin"))
		pstyle.setLeftMargin(attrs.valueAsDouble("leftMargin"));
	else
		if (attrs.hasAttribute("LeftMargin"))
			pstyle.setLeftMargin(attrs.valueAsDouble("LeftMargin"));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("rightMargin"))
		pstyle.setRightMargin(attrs.valueAsDouble("rightMargin"));
	else
		if (attrs.hasAttribute("RightMargin"))
			pstyle.setRightMargin(attrs.valueAsDouble("RightMargin"));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("firstIndent"))
		pstyle.setFirstIndent(attrs.valueAsDouble("firstIndent"));
	else
		if (attrs.hasAttribute("FirstIndent"))
			pstyle.setFirstIndent(attrs.valueAsDouble("FirstIndent"));
	if (attrs.hasAttribute("keepLinesStart"))
		pstyle.setKeepLinesStart(attrs.valueAsInt("keepLinesStart"));
	if (attrs.hasAttribute("keepLinesEnd"))
		pstyle.setKeepLinesEnd(attrs.valueAsInt("keepLinesEnd"));
	if (attrs.hasAttribute("keepWithNext"))
		pstyle.setKeepWithNext(attrs.valueAsBool("keepWithNext"));
	if (attrs.hasAttribute("keepTogether"))
		pstyle.setKeepTogether(attrs.valueAsBool("keepTogether"));
	if (attrs.hasAttribute("ParagraphEffectCharStyle"))
		pstyle.setPeCharStyleName(attrs.valueAsString("ParagraphEffectCharStyle"));
	if (attrs.hasAttribute("ParagraphEffectOffset"))
		pstyle.setParEffectOffset(attrs.valueAsDouble("ParagraphEffectOffset"));
	if (attrs.hasAttribute("ParagraphEffectIndent"))
		pstyle.setParEffectIndent(attrs.valueAsDouble("ParagraphEffectIndent"));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("DROP"))
		pstyle.setHasDropCap(static_cast<bool>(attrs.valueAsInt("DROP")));
	else
		if (attrs.hasAttribute("HasDropCap"))
			pstyle.setHasDropCap(static_cast<bool>(attrs.valueAsInt("HasDropCap")));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("DROPLIN"))
		pstyle.setDropCapLines(attrs.valueAsInt("DROPLIN"));
	else
		if (attrs.hasAttribute("DropCapLines"))
			pstyle.setDropCapLines(attrs.valueAsInt("DropCapLines"));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("DROPDIST"))
		pstyle.setParEffectOffset(attrs.valueAsDouble("DROPDIST"));
	else
		if (attrs.hasAttribute("ParagraphEffectOffset"))
			pstyle.setParEffectOffset(attrs.valueAsDouble("ParagraphEffectOffset"));
	if (attrs.hasAttribute("Bullet"))
		pstyle.setHasBullet(static_cast<bool>(attrs.valueAsInt("Bullet")));
	if (attrs.hasAttribute("BulletStr"))
		pstyle.setBulletStr(attrs.valueAsString("BulletStr"));
	if (attrs.hasAttribute("Numeration"))
		pstyle.setHasNum(static_cast<bool>(attrs.valueAsInt("Numeration")));
	if (attrs.hasAttribute("NumerationName"))
		pstyle.setNumName(attrs.valueAsString("NumerationName"));
	if (attrs.hasAttribute("NumerationFormat"))
		pstyle.setNumFormat(attrs.valueAsInt("NumerationFormat"));
	if (attrs.hasAttribute("NumerationLevel"))
		pstyle.setNumLevel(attrs.valueAsInt("NumerationLevel"));
	if (attrs.hasAttribute("NumerationStart"))
		pstyle.setNumStart(attrs.valueAsInt("NumerationStart"));
	if (attrs.hasAttribute("NumerationPrefix"))
		pstyle.setNumPrefix(attrs.valueAsString("NumerationPrefix"));
	if (attrs.hasAttribute("NumerationSuffix"))
		pstyle.setNumSuffix(attrs.valueAsString("NumerationSuffix"));
	if (attrs.hasAttribute("NumerationRestart"))
	{
		NumerationRange numRange = (NumerationRange) attrs.valueAsInt("NumerationRestart");
		// Fix deprecated numeration ranges
		if (numRange != NSRdocument && numRange != NSRstory)
			numRange = NSRstory;
		pstyle.setNumRestart(static_cast<int>(numRange));
	}
	if (attrs.hasAttribute("NumeartionOther"))
		pstyle.setNumOther(static_cast<bool>(attrs.valueAsInt("NumeartionOther")));
	if (attrs.hasAttribute("NumerationHigher"))
		pstyle.setNumHigher(static_cast<bool>(attrs.valueAsInt("NumerationHigher")));
	currItem->itemText.setDefaultStyle(pstyle);

	//Remove uppercase in 1.8
	if (attrs.hasAttribute("PSTYLE"))
	{
		QString pstyleName = attrs.valueAsString("PSTYLE");
		if (!pstyleName.isEmpty())
		{
			ParagraphStyle defStyle(currItem->itemText.defaultStyle());
			defStyle.setParent(pstyleName);
			currItem->itemText.setDefaultStyle(defStyle);
		}
	}
	if (attrs.hasAttribute("ParagraphStyle"))
	{
		QString pstyleName = attrs.valueAsString("ParagraphStyle");
		if (!pstyleName.isEmpty())
		{
			ParagraphStyle defStyle(currItem->itemText.defaultStyle());
			defStyle.setParent(pstyleName);
			currItem->itemText.setDefaultStyle(defStyle);
		}
	}
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("ROT"))
		currItem->setRotation( attrs.valueAsDouble("ROT", 0.0) );
	else
		currItem->setRotation( attrs.valueAsDouble("Rotation", 0.0) );
	currItem->oldRot = currItem->rotation();
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("EXTRA"))
		currItem->setTextToFrameDist(attrs.valueAsDouble("EXTRA", 0.0),
								attrs.valueAsDouble("REXTRA", 0.0),
								attrs.valueAsDouble("TEXTRA", 0.0),
								attrs.valueAsDouble("BEXTRA", 0.0));
	else
		currItem->setTextToFrameDist(attrs.valueAsDouble("TextToFrameDistanceLeft", 0.0),
								attrs.valueAsDouble("TextToFrameDistanceRight", 0.0),
								attrs.valueAsDouble("TextToFrameDistanceTop", 0.0),
								attrs.valueAsDouble("TextToFrameDistanceBottom", 0.0));
	//Remove in 1.8
	if (attrs.hasAttribute("VAlign"))
		currItem->setVerticalAlignment(attrs.valueAsInt("VAlign", 0));
	else
		currItem->setVerticalAlignment(attrs.valueAsInt("VerticalAlignment", 0));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("FLOP"))
		currItem->setFirstLineOffset(static_cast<FirstLineOffsetPolicy>(attrs.valueAsInt("FLOP")));
	else
		currItem->setFirstLineOffset(static_cast<FirstLineOffsetPolicy>(attrs.valueAsInt("FirstLineOffset")));
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("PLINEART"))
	{
		currItem->PLineArt = Qt::PenStyle(attrs.valueAsInt("PLINEART", 0));
		currItem->PLineEnd = Qt::PenCapStyle(attrs.valueAsInt("PLINEEND", 0));
		currItem->PLineJoin = Qt::PenJoinStyle(attrs.valueAsInt("PLINEJOIN", 0));
	}
	else
	{
		currItem->PLineArt = Qt::PenStyle(attrs.valueAsInt("LinePenStyle", 0));
		currItem->PLineEnd = Qt::PenCapStyle(attrs.valueAsInt("LineCapStyle", 0));
		currItem->PLineJoin = Qt::PenJoinStyle(attrs.valueAsInt("LineJoinStyle", 0));
	}
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("PRINTABLE"))
		currItem->setPrintEnabled( attrs.valueAsInt("PRINTABLE", 1));
	else
		currItem->setPrintEnabled( attrs.valueAsInt("PrintEnabled", 1));

	//Remove uppercase in 1.8
	QString itemName;
	if (attrs.hasAttribute("ANNAME"))
		itemName = attrs.valueAsString("ANNAME","");
	else
		if (attrs.hasAttribute("AutoName"))
			itemName = attrs.valueAsString("AutoName","");
	if (!itemName.isEmpty())
	{
		if (currItem->itemName() == itemName)
			currItem->AutoName = true;
		else
		{
			currItem->setItemName(itemName);
			currItem->AutoName = false;
		}
	}
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("ANNOTATION"))
	{
		currItem->setIsAnnotation( attrs.valueAsInt("ANNOTATION", 0));
		currItem->annotation().setType( attrs.valueAsInt("ANTYPE", 0));
		currItem->annotation().setAction( attrs.valueAsString("ANACTION","") );
		currItem->annotation().setE_act ( attrs.valueAsString("ANEACT","") );
		currItem->annotation().setX_act ( attrs.valueAsString("ANXACT","") );
		currItem->annotation().setD_act ( attrs.valueAsString("ANDACT","") );
		currItem->annotation().setFo_act( attrs.valueAsString("ANFOACT","") );
		currItem->annotation().setBl_act( attrs.valueAsString("ANBLACT","") );
		currItem->annotation().setK_act ( attrs.valueAsString("ANKACT","") );
		currItem->annotation().setF_act ( attrs.valueAsString("ANFACT","") );
		currItem->annotation().setV_act ( attrs.valueAsString("ANVACT","") );
		currItem->annotation().setC_act ( attrs.valueAsString("ANCACT","") );
		currItem->annotation().setActionType(attrs.valueAsInt("ANACTYP", 0));
		currItem->annotation().setExtern( attrs.valueAsString("ANEXTERN",""));
		if ((!currItem->annotation().Extern().isEmpty()) && (currItem->annotation().ActionType() != 8))
			currItem->annotation().setExtern(Relative2Path(attrs.valueAsString("ANEXTERN", "") , baseDir));
		currItem->annotation().setZiel( attrs.valueAsInt("ANZIEL", 0));
		currItem->annotation().setToolTip ( attrs.valueAsString("ANTOOLTIP",""));
		currItem->annotation().setRollOver( attrs.valueAsString("ANROLL",""));
		currItem->annotation().setDown( attrs.valueAsString("ANDOWN",""));
		currItem->annotation().setBorderWidth( attrs.valueAsInt("ANBWID", 1));
		currItem->annotation().setBorderStyle( attrs.valueAsInt("ANBSTY", 0));
		currItem->annotation().setFeed( attrs.valueAsInt("ANFEED", 1));
		currItem->annotation().setFlag( attrs.valueAsInt("ANFLAG", 0));
		currItem->annotation().setFont( attrs.valueAsInt("ANFONT", 4));
		currItem->annotation().setFormat( attrs.valueAsInt("ANFORMAT", 0));
		currItem->annotation().setVis( attrs.valueAsInt("ANVIS", 0));
		currItem->annotation().setIsChk( attrs.valueAsBool("ANCHK", false) );
		currItem->annotation().setCheckState(currItem->annotation().IsChk());
		currItem->annotation().setAAact( attrs.valueAsBool("ANAA", false) );
		currItem->annotation().setHTML ( attrs.valueAsInt("ANHTML", 0));
		currItem->annotation().setUseIcons( attrs.valueAsBool("ANICON", false));
		currItem->annotation().setChkStil ( attrs.valueAsInt("ANCHKS", 0));
		currItem->annotation().setMaxChar ( attrs.valueAsInt("ANMC", -1));
		currItem->annotation().setBorderColor( attrs.valueAsString("ANBCOL", CommonStrings::None));
		currItem->annotation().setIPlace(attrs.valueAsInt("ANPLACE", 1));
		currItem->annotation().setScaleW(attrs.valueAsInt("ANSCALE", 0));
		currItem->annotation().setIcon(attrs.valueAsInt("ANITYP", 0));
		currItem->annotation().setAnOpen(attrs.valueAsBool("ANOPEN", false) );
	}
	else
	{
		currItem->setIsAnnotation( attrs.valueAsInt("Annotation", 0));
		currItem->annotation().setType( attrs.valueAsInt("AnnotationType", 0));
		currItem->annotation().setAction( attrs.valueAsString("AnnotationAction","") );
		currItem->annotation().setE_act ( attrs.valueAsString("AnnotationActionE","") );
		currItem->annotation().setX_act ( attrs.valueAsString("AnnotationActionX","") );
		currItem->annotation().setD_act ( attrs.valueAsString("AnnotationActionD","") );
		currItem->annotation().setFo_act( attrs.valueAsString("AnnotationActionFo","") );
		currItem->annotation().setBl_act( attrs.valueAsString("AnnotationActionBL","") );
		currItem->annotation().setK_act ( attrs.valueAsString("AnnotationActionK","") );
		currItem->annotation().setF_act ( attrs.valueAsString("AnnotationActionF","") );
		currItem->annotation().setV_act ( attrs.valueAsString("AnnotationActionV","") );
		currItem->annotation().setC_act ( attrs.valueAsString("AnnotationActionC","") );
		currItem->annotation().setActionType(attrs.valueAsInt("AnnotationActionType", 0));
		currItem->annotation().setExtern( attrs.valueAsString("AnnotationExtern",""));
		if ((!currItem->annotation().Extern().isEmpty()) && (currItem->annotation().ActionType() != 8))
			currItem->annotation().setExtern(Relative2Path(attrs.valueAsString("AnnotationExtern", "") , baseDir));
		currItem->annotation().setZiel( attrs.valueAsInt("AnnotationCount", 0));
		currItem->annotation().setToolTip ( attrs.valueAsString("AnnotationToolTip",""));
		currItem->annotation().setRollOver( attrs.valueAsString("AnnotationRollOver",""));
		currItem->annotation().setDown( attrs.valueAsString("AnnotationDown",""));
		currItem->annotation().setBorderWidth( attrs.valueAsInt("AnnotationBorderWidth", 1));
		currItem->annotation().setBorderStyle( attrs.valueAsInt("AnnotationBorderStyle", 0));
		currItem->annotation().setFeed( attrs.valueAsInt("AnnotationFeed", 1));
		currItem->annotation().setFlag( attrs.valueAsInt("AnnotationFlag", 0));
		currItem->annotation().setFont( attrs.valueAsInt("AnnotationFont", 4));
		currItem->annotation().setFormat( attrs.valueAsInt("AnnotationFormat", 0));
		currItem->annotation().setVis( attrs.valueAsInt("AnnotationVisible", 0));
		currItem->annotation().setIsChk( attrs.valueAsBool("AnnotationIsCheck", false) );
		currItem->annotation().setCheckState(currItem->annotation().IsChk());
		currItem->annotation().setAAact( attrs.valueAsBool("AnnotationAdditionalAction", false) );
		currItem->annotation().setHTML ( attrs.valueAsInt("AnnotationHTML", 0));
		currItem->annotation().setUseIcons( attrs.valueAsBool("AnnotationUseIcons", false));
		currItem->annotation().setChkStil ( attrs.valueAsInt("AnnotationCheckStyle", 0));
		currItem->annotation().setMaxChar ( attrs.valueAsInt("AnnotationMaxChar", -1));
		currItem->annotation().setBorderColor( attrs.valueAsString("AnnotationBorderColor", CommonStrings::None));
		currItem->annotation().setIPlace(attrs.valueAsInt("AnnotationTextPosition", 1));
		currItem->annotation().setScaleW(attrs.valueAsInt("AnnotationScaleWidth", 0));
		currItem->annotation().setIcon(attrs.valueAsInt("AnnotationIcon", 0));
		currItem->annotation().setAnOpen(attrs.valueAsBool("AnnotationTextOpen", false) );
	}

	if (currItem->isTextFrame() || currItem->isPathText())
	{
		UndoManager::instance()->setUndoEnabled(false);
		if (currItem->isAnnotation() && currItem->annotation().UseIcons())
		{
			currItem->setImageXYScale(imageScaleX, imageScaleY);
			currItem->setImageXYOffset(imageOffsetX, imageOffsetY);
			//Remove uppercase in 1.8
			if (attrs.hasAttribute("LOCALROT"))
				currItem->setImageRotation(attrs.valueAsDouble("LOCALROT"));
			else
				currItem->setImageRotation(attrs.valueAsDouble("ImageRotation"));
			if (attrs.hasAttribute("PFILE"))
			{
				currItem->Pfile = Relative2Path(attrs.valueAsString("PFILE" , ""), baseDir);
				currItem->Pfile2 = Relative2Path(attrs.valueAsString("PFILE2", ""), baseDir);
				currItem->Pfile3 = Relative2Path(attrs.valueAsString("PFILE3", ""), baseDir);
			}
			else
			{
				currItem->Pfile = Relative2Path(attrs.valueAsString("ImageFileName" , ""), baseDir);
				currItem->Pfile2 = Relative2Path(attrs.valueAsString("PDFButtonIconFileName", ""), baseDir);
				currItem->Pfile3 = Relative2Path(attrs.valueAsString("PDFButtonIconHoverFileName", ""), baseDir);
			}

			currItem->Pfile = QDir::fromNativeSeparators(currItem->Pfile);
			currItem->Pfile2 = QDir::fromNativeSeparators(currItem->Pfile2);
			currItem->Pfile3 = QDir::fromNativeSeparators(currItem->Pfile3);
			if (attrs.hasAttribute("PRFILE"))
				currItem->ImageProfile = attrs.valueAsString("PRFILE", "");
			else
				currItem->ImageProfile = attrs.valueAsString("ImageProfile", "");
			if (attrs.hasAttribute("IRENDER"))
				currItem->ImageIntent = (eRenderIntent) attrs.valueAsInt("IRENDER" , 1);
			else
				currItem->ImageIntent = (eRenderIntent) attrs.valueAsInt("ImageColorimetricIntent" , 1);
			if (attrs.hasAttribute("EPROF"))
				currItem->EmbeddedProfile = attrs.valueAsString("EPROF", "");
			else
				currItem->EmbeddedProfile = attrs.valueAsString("EmbeddedProfile", "");
			if (attrs.hasAttribute("EMBEDDED"))
				currItem->UseEmbedded = attrs.valueAsInt("EMBEDDED", 1);
			else
				currItem->UseEmbedded = attrs.valueAsInt("UseEmbeddedProfile", 1);
			doc->loadPict(currItem->Pfile, currItem);
			currItem->setImageXYScale(imageScaleX, imageScaleY);
			if (attrs.hasAttribute("PICART"))
				currItem->setImageVisible(attrs.valueAsInt("PICART"));
			else
				currItem->setImageVisible(attrs.valueAsInt("ImageVisible"));
			if (attrs.hasAttribute("SCALETYPE"))
				currItem->ScaleType = attrs.valueAsInt("SCALETYPE", 1);
			else
				currItem->ScaleType = attrs.valueAsInt("ImageScaleType", 1);
			if (attrs.hasAttribute("RATIO"))
				currItem->AspectRatio = attrs.valueAsInt("RATIO", 0);
			else
				currItem->AspectRatio = attrs.valueAsInt("ImageRatio", 0);
		}
		UndoManager::instance()->setUndoEnabled(true);
	}

	if (currItem->isTable())
	{
		doc->dontResize = true;
		PageItem_Table *tableitem = currItem->asTable();
		int rows = attrs.valueAsInt("Rows", 1);
		int cols = attrs.valueAsInt("Columns", 1);
		tableitem->insertRows(1, rows - 1);
		tableitem->insertColumns(1, cols - 1);
		tableitem->setStyle(attrs.valueAsString("TableStyle"));
		doc->dontResize = true;

		QString rowHeights(attrs.valueAsString("RowHeights",""));
		if (!rowHeights.isEmpty())
		{
			QStringList slRowHeights = rowHeights.split(" ");
			int i = 0;
			for (const QString& pos : slRowHeights)
			{
				tableitem->resizeRow(i, pos.toDouble());
				++i;
			}
		}

		QString colWidths(attrs.valueAsString("ColumnWidths",""));
		if (!colWidths.isEmpty())
		{
			QStringList slColWidths = colWidths.split(" ");
			int i = 0;
			for (const QString& pos : slColWidths)
			{
				tableitem->resizeColumn(i, pos.toDouble());
				++i;
			}
		}

		QString cellAreas(attrs.valueAsString("CellAreas"));
		if (!cellAreas.isEmpty())
		{
			QStringList slCellAreas = cellAreas.split(" ");
			if (slCellAreas.count() % 4 != 0)
				qDebug() << "Cell Area Count on load ! % 4";
			for (qsizetype i = 0; i < slCellAreas.size(); i += 4)
			{
				int rows = slCellAreas.at(i).toInt();
				int columns = slCellAreas.at(i + 1).toInt();
				int height = slCellAreas.at(i + 2).toInt();
				int width = slCellAreas.at(i + 3).toInt();
				tableitem->mergeCells(rows, columns, height, width);
			}
		}
		doc->dontResize = false;
	}

	currItem->TopLine = attrs.valueAsBool("TopLine", false);
	currItem->LeftLine = attrs.valueAsBool("LeftLine", false);
	currItem->RightLine = attrs.valueAsBool("RightLine", false);
	currItem->BottomLine = attrs.valueAsBool("BottomLine", false);
	currItem->isTableItem = attrs.valueAsBool("isTableItem", false);
	currItem->TopLinkID = attrs.valueAsInt("TopLINK", -1);
	currItem->LeftLinkID = attrs.valueAsInt("LeftLINK", -1);
	currItem->RightLinkID = attrs.valueAsInt("RightLINK", -1);
	currItem->BottomLinkID = attrs.valueAsInt("BottomLINK", -1);
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("PLTSHOW"))
		currItem->PoShow = attrs.valueAsInt("PLTSHOW", 0);
	else
		currItem->PoShow = attrs.valueAsInt("PathTextShowPath", 0);
	if (attrs.hasAttribute("BASEOF"))
		currItem->BaseOffs = attrs.valueAsDouble("BASEOF", 0.0);
	else
		currItem->BaseOffs = attrs.valueAsDouble("PathTextDistanceFromPath", 0.0);
	//Remove lowercase in 1.8
	if (attrs.hasAttribute("textPathType"))
	{
		currItem->textPathType =  attrs.valueAsInt("textPathType", 0);
		currItem->textPathFlipped = attrs.valueAsBool("textPathFlipped", false);
	}
	else
	{
		currItem->textPathType =  attrs.valueAsInt("TextPathType", 0);
		currItem->textPathFlipped = attrs.valueAsBool("TextPathFlipped", false);
	}
	if ( attrs.hasAttribute("TEXTFLOWMODE") )
		currItem->setTextFlowMode((PageItem::TextFlowMode) attrs.valueAsInt("TEXTFLOWMODE", 0));
	else
		if ( attrs.hasAttribute("TextFlowMode") )
			currItem->setTextFlowMode((PageItem::TextFlowMode) attrs.valueAsInt("TextFlowMode", 0));
	else if ( attrs.valueAsInt("TEXTFLOW", 0) != 0)
	{
		if (attrs.valueAsInt("TEXTFLOW2", 0))
			currItem->setTextFlowMode(PageItem::TextFlowUsesBoundingBox);
		else if (attrs.valueAsInt("TEXTFLOW3", 0))
			currItem->setTextFlowMode(PageItem::TextFlowUsesContourLine);
		else
			currItem->setTextFlowMode(PageItem::TextFlowUsesFrameShape);	
	}
	else
		currItem->setTextFlowMode(PageItem::TextFlowDisabled);
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("LOCK"))
	{
		currItem->setLocked (attrs.valueAsBool("LOCK", false));
		currItem->setSizeLocked(attrs.valueAsBool("LOCKR", false));
		currItem->fillRule = attrs.valueAsBool("fillRule", true);
		currItem->doOverprint = attrs.valueAsBool("doOverprint", false);
		currItem->setFillTransparency(attrs.valueAsDouble("TransValue", 0.0));
		currItem->setLineTransparency(attrs.valueAsDouble("TransValueS", 0.0));
		currItem->setFillBlendmode(attrs.valueAsInt("TransBlend", 0));
		currItem->setLineBlendmode(attrs.valueAsInt("TransBlendS", 0));
	}
	else
	{
		currItem->setLocked (attrs.valueAsBool("ItemLocked", false));
		currItem->setSizeLocked(attrs.valueAsBool("ItemSizeLocked", false));
		currItem->fillRule = attrs.valueAsBool("FillRule", true);
		currItem->doOverprint = attrs.valueAsBool("DoOverprint", false);
		currItem->setFillTransparency(attrs.valueAsDouble("FillTransparency", 0.0));
		currItem->setLineTransparency(attrs.valueAsDouble("LineTransparency", 0.0));
		currItem->setFillBlendmode(attrs.valueAsInt("FillBlendMode", 0));
		currItem->setLineBlendmode(attrs.valueAsInt("LineBlendMode", 0));
	}

	if (attrs.valueAsInt("TRANSPARENT", 0) == 1) //Unnecessary?
		currItem->setFillColor(CommonStrings::None);
	if (attrs.hasAttribute("COLUMNS"))
	{
		currItem->m_columns = attrs.valueAsInt("COLUMNS", 1);
		currItem->m_columnGap = attrs.valueAsDouble("COLGAP", 0.0);
	}
	else
	{
		currItem->m_columns = attrs.valueAsInt("Columns", 1);
		currItem->m_columnGap = attrs.valueAsDouble("ColumnGap", 0.0);
	}
	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("LAYER") && (attrs.valueAsInt("LAYER", 0) != -1))
	{
		currItem->setLayer(attrs.valueAsInt("LAYER", 0));
		uint layerCount = doc->Layers.count();
		bool found = false;
		for (uint i = 0; i < layerCount; ++i)
		{
			if (doc->Layers[i].ID == currItem->m_layerID)
			{
				found = true;
				break;
			}
		}
		if (!found)
			currItem->setLayer(doc->firstLayerID());
	}
	else if (attrs.hasAttribute("Layer") && (attrs.valueAsInt("Layer", 0) != -1))
	{
		currItem->setLayer(attrs.valueAsInt("Layer", 0));
		uint layerCount = doc->Layers.count();
		bool found = false;
		for (uint i = 0; i < layerCount; ++i)
		{
			if (doc->Layers[i].ID == currItem->m_layerID)
			{
				found = true;
				break;
			}
		}
		if (!found)
			currItem->setLayer(doc->firstLayerID());
	}
	QString tmp;
	double xf;
	// double yf;
	//Remove uppercase in 1.8 format
	if ((attrs.hasAttribute("DashValues")) && (attrs.valueAsInt("DashValues", 0) != 0))
	{
		tmp = attrs.valueAsString("Dashes");
		ScTextStream dgv(&tmp, QIODevice::ReadOnly);
		currItem->DashValues.clear();
		int numDash = attrs.valueAsInt("DashValues", 0);
		for (int cxv = 0; cxv < numDash; ++cxv)
		{
			dgv >> xf;
			currItem->DashValues.append(xf);
		}
		tmp.clear();
	}
	else if ((attrs.hasAttribute("NUMDASH")) && (attrs.valueAsInt("NUMDASH", 0) != 0))
	{
		tmp = attrs.valueAsString("DASHS");
		ScTextStream dgv(&tmp, QIODevice::ReadOnly);
		currItem->DashValues.clear();
		int numDash = attrs.valueAsInt("NUMDASH", 0);
		for (int cxv = 0; cxv < numDash; ++cxv)
		{
			dgv >> xf;
			currItem->DashValues.append(xf);
		}
		tmp.clear();
	}
	else
		currItem->DashValues.clear();

	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("DASHOFF"))
		currItem->DashOffset = attrs.valueAsDouble("DASHOFF", 0.0);
	else
		currItem->DashOffset = attrs.valueAsDouble("DashOffset", 0.0);

	if (currItem->isRegularPolygon())
	{
		//Remove uppercase in 1.8 format
		PageItem_RegularPolygon *regitem = currItem->asRegularPolygon();
		if (attrs.hasAttribute("POLYC"))
		{
			regitem->polyCorners = attrs.valueAsInt("POLYC", 4);
			regitem->polyFactor = attrs.valueAsDouble("POLYF", 0.5);
			regitem->polyRotation = attrs.valueAsDouble("POLYR", 0.0);
			regitem->polyInnerRot = attrs.valueAsDouble("POLYIR", 0.0);
			regitem->polyCurvature = attrs.valueAsDouble("POLYCUR", 0.0);
			regitem->polyOuterCurvature = attrs.valueAsDouble("POLYOCUR", 0.0);
			regitem->polyUseFactor = attrs.valueAsBool("POLYS", false);
		}
		else if (attrs.hasAttribute("PolygonCorners"))
		{
			regitem->polyCorners = attrs.valueAsInt("PolygonCorners", 4);
			regitem->polyFactor = attrs.valueAsDouble("PolygonFactor", 0.5);
			regitem->polyRotation = attrs.valueAsDouble("PolygonRotation", 0.0);
			regitem->polyInnerRot = attrs.valueAsDouble("PolygonInnerRotation", 0.0);
			regitem->polyCurvature = attrs.valueAsDouble("PolygonCurvature", 0.0);
			regitem->polyOuterCurvature = attrs.valueAsDouble("PolygonOuterCurvature", 0.0);
			regitem->polyUseFactor = attrs.valueAsBool("PolygonUseFactor", false);
		}
		regitem->recalcPath();
	}
	else if (currItem->isArc())
	{
		PageItem_Arc *arcitem = currItem->asArc();
		//Remove lowercase in 1.8
		if (attrs.hasAttribute("arcHeight"))
		{
			arcitem->arcHeight = attrs.valueAsDouble("arcHeight", 1.0);
			arcitem->arcWidth = attrs.valueAsDouble("arcWidth", 1.0);
			arcitem->arcStartAngle = attrs.valueAsDouble("arcStartAngle", 30.0);
			arcitem->arcSweepAngle = attrs.valueAsDouble("arcSweepAngle", 300.0);
		}
		else
		{
			arcitem->arcHeight = attrs.valueAsDouble("ArcHeight", 1.0);
			arcitem->arcWidth = attrs.valueAsDouble("ArcWidth", 1.0);
			arcitem->arcStartAngle = attrs.valueAsDouble("ArcStartAngle", 30.0);
			arcitem->arcSweepAngle = attrs.valueAsDouble("ArcSweepAngle", 300.0);
		}
		arcitem->recalcPath();
	}
	else if (currItem->isSpiral())
	{
		PageItem_Spiral *arcitem = currItem->asSpiral();
		arcitem->FrameType = 3; // Workaround for old docs, otherwise undo breaks spirals
		//Remove lowercase in 1.8
		if (attrs.hasAttribute("spiralStartAngle"))
		{
			arcitem->spiralStartAngle = attrs.valueAsDouble("spiralStartAngle", 0.0);
			arcitem->spiralEndAngle = attrs.valueAsDouble("spiralEndAngle", 360.0);
			arcitem->spiralFactor = attrs.valueAsDouble("spiralFactor", 1.2);
		}
		else
		{
			arcitem->spiralStartAngle = attrs.valueAsDouble("SpiralStartAngle", 0.0);
			arcitem->spiralEndAngle = attrs.valueAsDouble("SpiralEndAngle", 360.0);
			arcitem->spiralFactor = attrs.valueAsDouble("SpiralFactor", 1.2);
		}
		arcitem->recalcPath();
	}
	else
	{
		tmp.clear();
		//Unused, remove in 1.8
		// if (attrs.hasAttribute("NUMPO"))
		// {
		// 	currItem->PoLine.resize(attrs.valueAsUInt("NUMPO"));
		// 	tmp = attrs.valueAsString("POCOOR");
		// 	constexpr double maxVal = std::numeric_limits<double>::max() / 2.0;
		// 	ScTextStream fp(&tmp, QIODevice::ReadOnly);
		// 	uint numPo = attrs.valueAsUInt("NUMPO");
		// 	for (uint cx = 0; cx < numPo; ++cx)
		// 	{
		// 		fp >> xf;
		// 		fp >> yf;
		// 		if (xf >= 999999)
		// 			xf = maxVal;
		// 		if (yf >= 999999)
		// 			yf = maxVal;
		// 		currItem->PoLine.setPoint(cx, xf, yf);
		// 	}
		// }
		// else
		// {
			currItem->PoLine.resize(0);
			//Remove lowercase in 1.8
			if (attrs.hasAttribute("path"))
				currItem->PoLine.parseSVG(attrs.valueAsString("path"));
			else
				currItem->PoLine.parseSVG(attrs.valueAsString("Path"));
		// }
	}
	tmp.clear();
	//Read but not written.. WTF remove in 1.8
	// if (attrs.hasAttribute("NUMCO"))
	// {
	// 	currItem->ContourLine.resize(attrs.valueAsUInt("NUMCO"));
	// 	tmp = attrs.valueAsString("COCOOR");
	// 	constexpr double maxVal = std::numeric_limits<double>::max() / 2.0;
	// 	ScTextStream fp(&tmp, QIODevice::ReadOnly);
	// 	uint numCo = attrs.valueAsUInt("NUMCO");
	// 	for (uint cx = 0; cx < numCo; ++cx)
	// 	{
	// 		fp >> xf;
	// 		fp >> yf;
	// 		if (xf >= 999999)
	// 			xf = maxVal;
	// 		if (yf >= 999999)
	// 			yf = maxVal;
	// 		currItem->ContourLine.setPoint(cx, xf, yf);
	// 	}
	// }
	// else
	if (attrs.hasAttribute("copath") || attrs.hasAttribute("ContourLinePath"))
	{
		currItem->ContourLine.resize(0);
		if (attrs.hasAttribute("copath"))
			currItem->ContourLine.parseSVG(attrs.valueAsString("copath"));
		else
			currItem->ContourLine.parseSVG(attrs.valueAsString("ContourLinePath"));
	}
	else
		currItem->ContourLine = currItem->PoLine.copy();

	if (!currItem->isLine())
		currItem->Clip = flattenPath(currItem->PoLine, currItem->Segments);
	else
	{
		currItem->Segments.clear();
		currItem->PoLine.resize(0);
		currItem->setHeight(1.0);
		currItem->asLine()->setLineClip();
	}

	if (currItem->isPathText())
		currItem->updatePolyClip();
	if (attrs.hasAttribute("GRTYP"))
		currItem->GrType = attrs.valueAsInt("GRTYP", 0);
	else
		currItem->GrType = attrs.valueAsInt("GradientType", 0);
	QString GrColor;
	QString GrColor2;
	int GrShade = 0;
	int GrShade2 = 0;
	if (currItem->GrType != 0)
	{
		if (currItem->GrType == Gradient_Pattern)
		{
			//Remove in 1.8
			if (attrs.hasAttribute("pattern"))
			{
				currItem->setPattern( attrs.valueAsString("pattern", "") );
				ScPatternTransform patternTrans;
				patternTrans.scaleX = attrs.valueAsDouble("pScaleX", 100.0) / 100.0;
				patternTrans.scaleY = attrs.valueAsDouble("pScaleY", 100.0) / 100.0;
				patternTrans.offsetX = attrs.valueAsDouble("pOffsetX", 0.0);
				patternTrans.offsetY = attrs.valueAsDouble("pOffsetY", 0.0);
				patternTrans.rotation = attrs.valueAsDouble("pRotation", 0.0);
				patternTrans.skewX = attrs.valueAsDouble("pSkewX", 0.0);
				patternTrans.skewY = attrs.valueAsDouble("pSkewY", 0.0);
				currItem->setPatternTransform(patternTrans);
				bool mirrorX = attrs.valueAsBool("pMirrorX", false);
				bool mirrorY = attrs.valueAsBool("pMirrorY", false);
				currItem->setPatternFlip(mirrorX, mirrorY);
			}
			else
			{
				currItem->setPattern( attrs.valueAsString("Pattern", "") );
				ScPatternTransform patternTrans;
				patternTrans.scaleX = attrs.valueAsDouble("PatternXScale", 100.0) / 100.0;
				patternTrans.scaleY = attrs.valueAsDouble("PatternYScale", 100.0) / 100.0;
				patternTrans.offsetX = attrs.valueAsDouble("PatternXOffset", 0.0);
				patternTrans.offsetY = attrs.valueAsDouble("PatternYOffset", 0.0);
				patternTrans.rotation = attrs.valueAsDouble("PatternRotation", 0.0);
				patternTrans.skewX = attrs.valueAsDouble("PatternXSkew", 0.0);
				patternTrans.skewY = attrs.valueAsDouble("PatternYSkew", 0.0);
				currItem->setPatternTransform(patternTrans);
				bool mirrorX = attrs.valueAsBool("PatternXMirror", false);
				bool mirrorY = attrs.valueAsBool("PatternYMirror", false);
				currItem->setPatternFlip(mirrorX, mirrorY);
			}
		}
		else if (currItem->GrType == Gradient_Mesh)
		{
			currItem->meshGradientArray.clear();
			int mGArrayRows = 1;
			int mGArrayCols = 1;
			//Remove uppercase in 1.8
			if (attrs.hasAttribute("GMAX"))
			{
				mGArrayRows = attrs.valueAsInt("GMAX", 1);
				mGArrayCols = attrs.valueAsInt("GMAY", 1);
			}
			else
			{
				mGArrayRows = attrs.valueAsInt("GradientMeshArrayRows", 1);
				mGArrayCols = attrs.valueAsInt("GradientMeshArrayColumns", 1);
			}
			for (int mgr = 0; mgr < mGArrayRows; mgr++)
			{
				QList<MeshPoint> ml;
				for (int mgc = 0; mgc < mGArrayCols; mgc++)
				{
					MeshPoint mp;
					ml.append(mp);
				}
				currItem->meshGradientArray.append(ml);
			}
		}
		else if (currItem->GrType == Gradient_PatchMesh)
		{
			currItem->meshGradientPatches.clear();
			int mGArrayRows = 1;
			//Remove uppercase in 1.8
			if (attrs.hasAttribute("GMAX"))
				mGArrayRows = attrs.valueAsInt("GMAX", 1);
			else
				mGArrayRows = attrs.valueAsInt("GradientMeshArrayRows", 1);
			for (int mgr = 0; mgr < mGArrayRows; mgr++)
			{
				meshGradientPatch patchM;
				currItem->meshGradientPatches.append(patchM);
			}
		}
		else if (currItem->GrType == Gradient_Hatch)
		{
			if (attrs.hasAttribute("HatchSolidB"))
			{
				int hatchType = attrs.valueAsInt("HatchMode", 0);
				double hatchDistance = attrs.valueAsDouble("HatchDist", 2);
				double hatchAngle = attrs.valueAsDouble("HatchAngle", 0);
				bool hatchUseBackground = attrs.valueAsBool("HatchSolidB", false);
				QString hatchBackground = attrs.valueAsString("HatchBackG", CommonStrings::None);
				QString hatchForeground = attrs.valueAsString("HatchForeC", CommonStrings::None);
				currItem->setHatchParameters(hatchType, hatchDistance, hatchAngle, hatchUseBackground, hatchBackground, hatchForeground);
			}
			else
			{
				int hatchType = attrs.valueAsInt("HatchMode", 0);
				double hatchDistance = attrs.valueAsDouble("HatchDistance", 2);
				double hatchAngle = attrs.valueAsDouble("HatchAngle", 0);
				bool hatchUseBackground = attrs.valueAsBool("HatchUseBackground", false);
				QString hatchBackground = attrs.valueAsString("HatchBackgroundColor", CommonStrings::None);
				QString hatchForeground = attrs.valueAsString("HatchForegroundColor", CommonStrings::None);
				currItem->setHatchParameters(hatchType, hatchDistance, hatchAngle, hatchUseBackground, hatchBackground, hatchForeground);

			}
		}
		else
		{
			//Remove uppercase in 1.8
			if (attrs.hasAttribute("GRSTARTX"))
			{
				currItem->GrStartX = attrs.valueAsDouble("GRSTARTX", 0.0);
				currItem->GrStartY = attrs.valueAsDouble("GRSTARTY", 0.0);
				currItem->GrEndX = attrs.valueAsDouble("GRENDX", currItem->width());
				currItem->GrEndY = attrs.valueAsDouble("GRENDY", 0.0);
				currItem->GrFocalX = attrs.valueAsDouble("GRFOCALX", 0.0);
				currItem->GrFocalY = attrs.valueAsDouble("GRFOCALY", 0.0);
				currItem->GrScale = attrs.valueAsDouble("GRSCALE", 1.0);
				currItem->GrSkew = attrs.valueAsDouble("GRSKEW", 0.0);
				GrColor = attrs.valueAsString("GRCOLOR","");
				if (!GrColor.isEmpty())
				{
					GrColor2 = attrs.valueAsString("GRCOLOR2", "");
					GrShade = attrs.valueAsInt("GRSHADE", 100);
					GrShade2 = attrs.valueAsInt("GRSHADE2", 100);
				}
				QString GrName(attrs.valueAsString("GRNAME", ""));
				if (!GrName.isEmpty())
					currItem->setGradient(GrName);
				if ((currItem->GrType == Gradient_4Colors) || (currItem->GrType == Gradient_Diamond))
				{
					currItem->GrControl1 = FPoint(attrs.valueAsDouble("GRC1X", 0.0), attrs.valueAsDouble("GRC1Y", 0.0));
					currItem->GrControl2 = FPoint(attrs.valueAsDouble("GRC2X", 0.0), attrs.valueAsDouble("GRC2Y", 0.0));
					currItem->GrControl3 = FPoint(attrs.valueAsDouble("GRC3X", 0.0), attrs.valueAsDouble("GRC3Y", 0.0));
					currItem->GrControl4 = FPoint(attrs.valueAsDouble("GRC4X", 0.0), attrs.valueAsDouble("GRC4Y", 0.0));
					currItem->GrControl5 = FPoint(attrs.valueAsDouble("GRC5X", 0.0), attrs.valueAsDouble("GRC5Y", 0.0));
					currItem->GrColorP1 = attrs.valueAsString("GRCOLP1", "Black");
					currItem->GrColorP2 = attrs.valueAsString("GRCOLP2", "Black");
					currItem->GrColorP3 = attrs.valueAsString("GRCOLP3", "Black");
					currItem->GrColorP4 = attrs.valueAsString("GRCOLP4", "Black");
					currItem->GrCol1transp = attrs.valueAsDouble("GRCOLT1", 1.0);
					currItem->GrCol2transp = attrs.valueAsDouble("GRCOLT2", 1.0);
					currItem->GrCol3transp = attrs.valueAsDouble("GRCOLT3", 1.0);
					currItem->GrCol4transp = attrs.valueAsDouble("GRCOLT4", 1.0);
					currItem->GrCol1Shade = attrs.valueAsInt("GRCOLS1", 100);
					currItem->GrCol2Shade = attrs.valueAsInt("GRCOLS2", 100);
					currItem->GrCol3Shade = attrs.valueAsInt("GRCOLS3", 100);
					currItem->GrCol4Shade = attrs.valueAsInt("GRCOLS4", 100);
					currItem->set4ColorColors(currItem->GrColorP1, currItem->GrColorP2, currItem->GrColorP3, currItem->GrColorP4);
				}
			}
			else
			{
				currItem->GrStartX = attrs.valueAsDouble("GradientStartX", 0.0);
				currItem->GrStartY = attrs.valueAsDouble("GradientStartY", 0.0);
				currItem->GrEndX = attrs.valueAsDouble("GradientEndX", currItem->width());
				currItem->GrEndY = attrs.valueAsDouble("GradientEndY", 0.0);
				currItem->GrFocalX = attrs.valueAsDouble("GradientFocalX", 0.0);
				currItem->GrFocalY = attrs.valueAsDouble("GradientFocalY", 0.0);
				currItem->GrScale = attrs.valueAsDouble("GradientScale", 1.0);
				currItem->GrSkew = attrs.valueAsDouble("GradientSkew", 0.0);
				//<<Seems like unused code
				GrColor = attrs.valueAsString("GRCOLOR","");
				if (!GrColor.isEmpty())
				{
					GrColor2 = attrs.valueAsString("GRCOLOR2", "");
					GrShade = attrs.valueAsInt("GRSHADE", 100);
					GrShade2 = attrs.valueAsInt("GRSHADE2", 100);
				}
				//>>
				QString GrName(attrs.valueAsString("GradientName", ""));
				if (!GrName.isEmpty())
					currItem->setGradient(GrName);
				if ((currItem->GrType == Gradient_4Colors) || (currItem->GrType == Gradient_Diamond))
				{
					currItem->GrControl1 = FPoint(attrs.valueAsDouble("GradientControl1X", 0.0), attrs.valueAsDouble("GradientControl1Y", 0.0));
					currItem->GrControl2 = FPoint(attrs.valueAsDouble("GradientControl2X", 0.0), attrs.valueAsDouble("GradientControl2Y", 0.0));
					currItem->GrControl3 = FPoint(attrs.valueAsDouble("GradientControl3X", 0.0), attrs.valueAsDouble("GradientControl3Y", 0.0));
					currItem->GrControl4 = FPoint(attrs.valueAsDouble("GradientControl4X", 0.0), attrs.valueAsDouble("GradientControl4Y", 0.0));
					currItem->GrControl5 = FPoint(attrs.valueAsDouble("GradientControl5X", 0.0), attrs.valueAsDouble("GradientControl5Y", 0.0));
					currItem->GrColorP1 = attrs.valueAsString("GradientColorP1", "Black");
					currItem->GrColorP2 = attrs.valueAsString("GradientColorP2", "Black");
					currItem->GrColorP3 = attrs.valueAsString("GradientColorP3", "Black");
					currItem->GrColorP4 = attrs.valueAsString("GradientColorP4", "Black");
					currItem->GrCol1transp = attrs.valueAsDouble("GradientColorP1Transparency", 1.0);
					currItem->GrCol2transp = attrs.valueAsDouble("GradientColorP2Transparency", 1.0);
					currItem->GrCol3transp = attrs.valueAsDouble("GradientColorP3Transparency", 1.0);
					currItem->GrCol4transp = attrs.valueAsDouble("GradientColorP4Transparency", 1.0);
					currItem->GrCol1Shade = attrs.valueAsInt("GradientColorP1Shade", 100);
					currItem->GrCol2Shade = attrs.valueAsInt("GradientColorP2Shade", 100);
					currItem->GrCol3Shade = attrs.valueAsInt("GradientColorP3Shade", 100);
					currItem->GrCol4Shade = attrs.valueAsInt("GradientColorP4Shade", 100);
					currItem->set4ColorColors(currItem->GrColorP1, currItem->GrColorP2, currItem->GrColorP3, currItem->GrColorP4);
				}
			}
		}
	}
	if (((currItem->GrType != 0) && (currItem->GrType != Gradient_Pattern) && (currItem->GrType != Gradient_Hatch)) && (currItem->gradient().isEmpty()))
	{
		currItem->fill_gradient.clearStops();
		if ((!GrColor.isEmpty()) && (!GrColor2.isEmpty()))
		{
			if (currItem->GrType == Gradient_RadialLegacy5)
			{
				if ((GrColor != CommonStrings::None) && (!GrColor.isEmpty()))
					currItem->SetQColor(&tmpc, GrColor, GrShade);
				currItem->fill_gradient.addStop(tmpc, 0.0, 0.5, 1.0, GrColor, GrShade);
				if ((GrColor2 != CommonStrings::None) && (!GrColor2.isEmpty()))
					currItem->SetQColor(&tmpc, GrColor2, GrShade2);
				currItem->fill_gradient.addStop(tmpc, 1.0, 0.5, 1.0, GrColor2, GrShade2);
			}
			else
			{
				if ((GrColor2 != CommonStrings::None) && (!GrColor2.isEmpty()))
					currItem->SetQColor(&tmpc, GrColor2, GrShade2);
				currItem->fill_gradient.addStop(tmpc, 0.0, 0.5, 1.0, GrColor2, GrShade2);
				if ((GrColor != CommonStrings::None) && (!GrColor.isEmpty()))
					currItem->SetQColor(&tmpc, GrColor, GrShade);
				currItem->fill_gradient.addStop(tmpc, 1.0, 0.5, 1.0, GrColor, GrShade);
			}
		}
//		currItem->updateGradientVectors();
	}
	switch (currItem->GrType)
	{
		case Gradient_LinearLegacy1:
		case Gradient_LinearLegacy2:
		case Gradient_LinearLegacy3:
		case Gradient_LinearLegacy4:
			currItem->GrType = Gradient_Linear;
			break;
		case Gradient_RadialLegacy5:
			currItem->GrType = Gradient_Radial;
			break;
		default:
			break;
	}
	//Remove in 1.8
	if (attrs.hasAttribute("patternS"))
	{
		currItem->setStrokePattern( attrs.valueAsString("patternS", "") );
		ScStrokePatternTransform strokePatternTrans;
		strokePatternTrans.scaleX = attrs.valueAsDouble("pScaleXS", 100.0) / 100.0;
		strokePatternTrans.scaleY = attrs.valueAsDouble("pScaleYS", 100.0) / 100.0;
		strokePatternTrans.offsetX = attrs.valueAsDouble("pOffsetXS", 0.0);
		strokePatternTrans.offsetY = attrs.valueAsDouble("pOffsetYS", 0.0);
		strokePatternTrans.rotation = attrs.valueAsDouble("pRotationS", 0.0);
		strokePatternTrans.skewX = attrs.valueAsDouble("pSkewXS", 0.0);
		strokePatternTrans.skewY = attrs.valueAsDouble("pSkewYS", 0.0);
		strokePatternTrans.space = attrs.valueAsDouble("pSpaceS", 1.0);
		currItem->setStrokePatternTransform(strokePatternTrans);
		bool mirrorX = attrs.valueAsBool("pMirrorXS", false);
		bool mirrorY = attrs.valueAsBool("pMirrorYS", false);
		bool atPath = attrs.valueAsBool("pAtPathS", false);
		currItem->setPatternFlip(mirrorX, mirrorY);
		currItem->setStrokePatternToPath(atPath);
	}
	else
	{
		currItem->setStrokePattern( attrs.valueAsString("StrokePattern", "") );
		ScStrokePatternTransform strokePatternTrans;
		strokePatternTrans.scaleX = attrs.valueAsDouble("StrokePatternXScale", 100.0) / 100.0;
		strokePatternTrans.scaleY = attrs.valueAsDouble("StrokePatternYScale", 100.0) / 100.0;
		strokePatternTrans.offsetX = attrs.valueAsDouble("StrokePatternXOffset", 0.0);
		strokePatternTrans.offsetY = attrs.valueAsDouble("StrokePatternYOffset", 0.0);
		strokePatternTrans.rotation = attrs.valueAsDouble("StrokePatternRotation", 0.0);
		strokePatternTrans.skewX = attrs.valueAsDouble("StrokePatternXSkew", 0.0);
		strokePatternTrans.skewY = attrs.valueAsDouble("StrokePatternYSkew", 0.0);
		strokePatternTrans.space = attrs.valueAsDouble("StrokePatternSpace", 1.0);
		currItem->setStrokePatternTransform(strokePatternTrans);
		bool mirrorX = attrs.valueAsBool("StrokePatternXMirror", false);
		bool mirrorY = attrs.valueAsBool("StrokePatternYMirror", false);
		bool atPath = attrs.valueAsBool("StrokePatternToPath", false);
		currItem->setPatternFlip(mirrorX, mirrorY);
		currItem->setStrokePatternToPath(atPath);
	}
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("GRTYPS"))
		currItem->GrTypeStroke = attrs.valueAsInt("GRTYPS", 0);
	else
		currItem->GrTypeStroke = attrs.valueAsInt("GradientTypeStroke", 0);
	if (((currItem->GrTypeStroke != 0) && (currItem->GrTypeStroke != Gradient_Pattern)) && (currItem->strokeGradient().isEmpty()))
		currItem->stroke_gradient.clearStops();
	if (attrs.hasAttribute("GRSTARTXS"))
	{
		currItem->GrStrokeStartX = attrs.valueAsDouble("GRSTARTXS", 0.0);
		currItem->GrStrokeStartY = attrs.valueAsDouble("GRSTARTYS", 0.0);
		currItem->GrStrokeEndX = attrs.valueAsDouble("GRENDXS", currItem->width());
		currItem->GrStrokeEndY = attrs.valueAsDouble("GRENDYS", 0.0);
		currItem->GrStrokeFocalX = attrs.valueAsDouble("GRFOCALXS", 0.0);
		currItem->GrStrokeFocalY = attrs.valueAsDouble("GRFOCALYS", 0.0);
		currItem->GrStrokeScale = attrs.valueAsDouble("GRSCALES", 1.0);
		currItem->GrStrokeSkew = attrs.valueAsDouble("GRSKEWS", 0.0);
		QString GrNameS(attrs.valueAsString("GRNAMES", ""));
		if (!GrNameS.isEmpty())
			currItem->setStrokeGradient(GrNameS);
	}
	else
	{
		currItem->GrStrokeStartX = attrs.valueAsDouble("GradientStrokeStartX", 0.0);
		currItem->GrStrokeStartY = attrs.valueAsDouble("GradientStrokeStartY", 0.0);
		currItem->GrStrokeEndX = attrs.valueAsDouble("GradientStrokeEndX", currItem->width());
		currItem->GrStrokeEndY = attrs.valueAsDouble("GradientStrokeEndY", 0.0);
		currItem->GrStrokeFocalX = attrs.valueAsDouble("GradientStrokeFocalX", 0.0);
		currItem->GrStrokeFocalY = attrs.valueAsDouble("GradientStrokeFocalY", 0.0);
		currItem->GrStrokeScale = attrs.valueAsDouble("GradientStrokeScale", 1.0);
		currItem->GrStrokeSkew = attrs.valueAsDouble("GradientStrokeSkew", 0.0);
		QString GrNameS(attrs.valueAsString("GradientStrokeName", ""));
		if (!GrNameS.isEmpty())
			currItem->setStrokeGradient(GrNameS);
	}

	//Remove in 1.8
	if (attrs.hasAttribute("patternM"))
	{
		currItem->setPatternMask( attrs.valueAsString("patternM", "") );
		ScMaskTransform maskTransform;
		maskTransform.scaleX = attrs.valueAsDouble("pScaleXM", 100.0) / 100.0;
		maskTransform.scaleY = attrs.valueAsDouble("pScaleYM", 100.0) / 100.0;
		maskTransform.offsetX = attrs.valueAsDouble("pOffsetXM", 0.0);
		maskTransform.offsetY = attrs.valueAsDouble("pOffsetYM", 0.0);
		maskTransform.rotation = attrs.valueAsDouble("pRotationM", 0.0);
		maskTransform.skewX = attrs.valueAsDouble("pSkewXM", 0.0);
		maskTransform.skewY = attrs.valueAsDouble("pSkewYM", 0.0);
		currItem->setMaskTransform(maskTransform);
		bool mirrorXm = attrs.valueAsBool("pMirrorXM", false);
		bool mirrorYm = attrs.valueAsBool("pMirrorYM", false);
		currItem->setMaskFlip(mirrorXm, mirrorYm);
	}
	else
	{
		currItem->setPatternMask( attrs.valueAsString("MaskPattern", "") );
		ScMaskTransform maskTransform;
		maskTransform.scaleX = attrs.valueAsDouble("MaskPatternScaleX", 100.0) / 100.0;
		maskTransform.scaleY = attrs.valueAsDouble("MaskPatternScaleY", 100.0) / 100.0;
		maskTransform.offsetX = attrs.valueAsDouble("MaskPatternOffsetX", 0.0);
		maskTransform.offsetY = attrs.valueAsDouble("MaskPatternOffsetY", 0.0);
		maskTransform.rotation = attrs.valueAsDouble("MaskPatternRotation", 0.0);
		maskTransform.skewX = attrs.valueAsDouble("MaskPatternSkewX", 0.0);
		maskTransform.skewY = attrs.valueAsDouble("MaskPatternSkewY", 0.0);
		currItem->setMaskTransform(maskTransform);
		bool mirrorXm = attrs.valueAsBool("MaskPatternMirrorX", false);
		bool mirrorYm = attrs.valueAsBool("MaskPatternMirrorY", false);
		currItem->setMaskFlip(mirrorXm, mirrorYm);
	}




	QString GrNameM;
	if (attrs.hasAttribute("GRNAMEM"))
		GrNameM = attrs.valueAsString("GRNAMEM", "");
	else
		GrNameM = attrs.valueAsString("GradientMaskName", "");
	if (attrs.hasAttribute("GRTYPM"))
		currItem->GrMask = attrs.valueAsInt("GRTYPM", 0);
	else
		currItem->GrMask = attrs.valueAsInt("GradientMaskType", 0);
	if ((currItem->GrMask == GradMask_Linear) || (currItem->GrMask == GradMask_LinearLumAlpha))
		currItem->mask_gradient = VGradient(VGradient::linear);
	else if ((currItem->GrMask == GradMask_Radial) || (currItem->GrMask == GradMask_RadialLumAlpha))
		currItem->mask_gradient = VGradient(VGradient::radial);
	if (((currItem->GrMask == GradMask_Linear) || (currItem->GrMask == GradMask_Radial) || (currItem->GrMask == GradMask_LinearLumAlpha) || (currItem->GrMask == GradMask_RadialLumAlpha)) && (GrNameM.isEmpty()))
		currItem->mask_gradient.clearStops();
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("GRSTARTXM"))
	{
		currItem->GrMaskStartX = attrs.valueAsDouble("GRSTARTXM", 0.0);
		currItem->GrMaskStartY = attrs.valueAsDouble("GRSTARTYM", 0.0);
		currItem->GrMaskEndX = attrs.valueAsDouble("GRENDXM", currItem->width());
		currItem->GrMaskEndY = attrs.valueAsDouble("GRENDYM", 0.0);
		currItem->GrMaskFocalX = attrs.valueAsDouble("GRFOCALXM", 0.0);
		currItem->GrMaskFocalY = attrs.valueAsDouble("GRFOCALYM", 0.0);
		currItem->GrMaskScale = attrs.valueAsDouble("GRSCALEM", 1.0);
		currItem->GrMaskSkew = attrs.valueAsDouble("GRSKEWM", 0.0);
	}
	else
	{
		currItem->GrMaskStartX = attrs.valueAsDouble("GradientMaskStartX", 0.0);
		currItem->GrMaskStartY = attrs.valueAsDouble("GradientMaskStartY", 0.0);
		currItem->GrMaskEndX = attrs.valueAsDouble("GradientMaskEndX", currItem->width());
		currItem->GrMaskEndY = attrs.valueAsDouble("GradientMaskEndY", 0.0);
		currItem->GrMaskFocalX = attrs.valueAsDouble("GradientMaskFocalX", 0.0);
		currItem->GrMaskFocalY = attrs.valueAsDouble("GradientMaskFocalY", 0.0);
		currItem->GrMaskScale = attrs.valueAsDouble("GradientMaskScale", 1.0);
		currItem->GrMaskSkew = attrs.valueAsDouble("GradientMaskSkew", 0.0);
	}
	if (!GrNameM.isEmpty())
		currItem->setGradientMask(GrNameM);
	if (attrs.hasAttribute("InID"))
		currItem->inlineCharID = attrs.valueAsInt("InID", -1);
	else
		currItem->inlineCharID = -1;
	//Remove uppercase in 1.8
	if (attrs.hasAttribute("GRExt"))
	{
		currItem->setGradientExtend((VGradient::VGradientRepeatMethod)(attrs.valueAsInt("GRExt", VGradient::pad)));
		currItem->setStrokeGradientExtend((VGradient::VGradientRepeatMethod)(attrs.valueAsInt("GRExtS", VGradient::pad)));
		currItem->mask_gradient.setRepeatMethod((VGradient::VGradientRepeatMethod)(attrs.valueAsInt("GRExtM", VGradient::pad)));
	}
	else
	{
		currItem->setGradientExtend((VGradient::VGradientRepeatMethod)(attrs.valueAsInt("GradientExtend", VGradient::pad)));
		currItem->setStrokeGradientExtend((VGradient::VGradientRepeatMethod)(attrs.valueAsInt("GradientStrokeExtend", VGradient::pad)));
		currItem->mask_gradient.setRepeatMethod((VGradient::VGradientRepeatMethod)(attrs.valueAsInt("GradientMaskRepeatMethod", VGradient::pad)));
	}

	//Remove uppercase in 1.8
	if (attrs.hasAttribute("HASSOFTSHADOW"))
	{
		currItem->setHasSoftShadow(attrs.valueAsBool("HASSOFTSHADOW", false));
		currItem->setSoftShadowXOffset(attrs.valueAsDouble("SOFTSHADOWXOFFSET", 5.0));
		currItem->setSoftShadowYOffset(attrs.valueAsDouble("SOFTSHADOWYOFFSET", 5.0));
		currItem->setSoftShadowColor(attrs.valueAsString("SOFTSHADOWCOLOR", "Black"));
		currItem->setSoftShadowShade(attrs.valueAsInt("SOFTSHADOWSHADE", 100));
		currItem->setSoftShadowBlurRadius(attrs.valueAsDouble("SOFTSHADOWBLURRADIUS", 5.0));
		currItem->setSoftShadowBlendMode(attrs.valueAsInt("SOFTSHADOWBLENDMODE", 0));
		currItem->setSoftShadowOpacity(attrs.valueAsDouble("SOFTSHADOWOPACITY", 0.0));
		currItem->setSoftShadowErasedByObject(attrs.valueAsBool("SOFTSHADOWERASE", false));
		currItem->setSoftShadowHasObjectTransparency(attrs.valueAsBool("SOFTSHADOWOBJTRANS", false));
	}
	else
	{
		currItem->setHasSoftShadow(attrs.valueAsBool("HasSoftShadow", false));
		currItem->setSoftShadowBlendMode(attrs.valueAsInt("SoftShadowBlendMode", 0));
		currItem->setSoftShadowBlurRadius(attrs.valueAsDouble("SoftShadowBlurRadius", 5.0));
		currItem->setSoftShadowColor(attrs.valueAsString("SoftShadowColor", "Black"));
		currItem->setSoftShadowErasedByObject(attrs.valueAsBool("SoftShadowErase", false));
		currItem->setSoftShadowHasObjectTransparency(attrs.valueAsBool("SoftShadowHasObjectTransparency", false));
		currItem->setSoftShadowOpacity(attrs.valueAsDouble("SoftShadowOpacity", 0.0));
		currItem->setSoftShadowShade(attrs.valueAsInt("SoftShadowShade", 100));
		currItem->setSoftShadowXOffset(attrs.valueAsDouble("SoftShadowXOffset", 5.0));
		currItem->setSoftShadowYOffset(attrs.valueAsDouble("SoftShadowYOffset", 5.0));
	}
	//currItem->setRedrawBounding();
	//currItem->OwnPage = view->OnPage(currItem);
	return currItem;
}

bool Scribus171Format::readItemTableData(PageItem_Table* item, ScXmlStreamReader& reader, ScribusDoc* doc) const
{
	ScXmlStreamAttributes attrs = reader.scAttributes();
	QString fColor = attrs.valueAsString("FillColor");
	if ((fColor != CommonStrings::None) && (!fColor.isEmpty()))
		item->setFillColor(fColor);
	item->setFillShade(attrs.valueAsInt("FillShade", 100));
	QString tagName(reader.nameAsString());
	LastStyles lastStyle;
	doc->dontResize = true;
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (!reader.isStartElement())
			continue;
			
		if (reader.name() == QLatin1String("Cell"))
		{
			readItemTableCell(item, reader, doc);
		}
		else if (reader.name() == QLatin1String("TableBorderLeft"))
		{
			TableBorder border;
			QString tagName(reader.nameAsString());
			while (!reader.atEnd() && !reader.hasError())
			{
				reader.readNext();
				if (reader.isEndElement() && reader.name() == tagName)
					break;
				if (reader.isStartElement() && reader.name() == QLatin1String("TableBorderLine"))
				{
					ScXmlStreamAttributes tAttB = reader.scAttributes();
					double width = tAttB.valueAsDouble("Width", 0.0);
					QString color = tAttB.valueAsString("Color", CommonStrings::None);
					double shade = tAttB.valueAsDouble("Shade", 100.0);
					int style = tAttB.valueAsInt("PenStyle", 1);
					border.addBorderLine(TableBorderLine(width, static_cast<Qt::PenStyle>(style), color, shade));
				}
			}
			item->setLeftBorder(border);
		}
		else if (reader.name() == QLatin1String("TableBorderRight"))
		{
			TableBorder border;
			QString tagName(reader.nameAsString());
			while (!reader.atEnd() && !reader.hasError())
			{
				reader.readNext();
				if (reader.isEndElement() && reader.name() == tagName)
					break;
				if (reader.isStartElement() && reader.name() == QLatin1String("TableBorderLine"))
				{
					ScXmlStreamAttributes tAttB = reader.scAttributes();
					double width = tAttB.valueAsDouble("Width", 0.0);
					QString color = tAttB.valueAsString("Color", CommonStrings::None);
					double shade = tAttB.valueAsDouble("Shade", 100.0);
					int style = tAttB.valueAsInt("PenStyle", 1);
					border.addBorderLine(TableBorderLine(width, static_cast<Qt::PenStyle>(style), color, shade));
				}
			}
			item->setRightBorder(border);
		}
		else if (reader.name() == QLatin1String("TableBorderTop"))
		{
			TableBorder border;
			QString tagName(reader.nameAsString());
			while (!reader.atEnd() && !reader.hasError())
			{
				reader.readNext();
				if (reader.isEndElement() && reader.name() == tagName)
					break;
				if (reader.isStartElement() && reader.name() == QLatin1String("TableBorderLine"))
				{
					ScXmlStreamAttributes tAttB = reader.scAttributes();
					double width = tAttB.valueAsDouble("Width", 0.0);
					QString color = tAttB.valueAsString("Color", CommonStrings::None);
					double shade = tAttB.valueAsDouble("Shade", 100.0);
					int style = tAttB.valueAsInt("PenStyle", 1);
					border.addBorderLine(TableBorderLine(width, static_cast<Qt::PenStyle>(style), color, shade));
				}
			}
			item->setTopBorder(border);
		}
		else if (reader.name() == QLatin1String("TableBorderBottom"))
		{
			TableBorder border;
			QString tagName(reader.nameAsString());
			while (!reader.atEnd() && !reader.hasError())
			{
				reader.readNext();
				if (reader.isEndElement() && reader.name() == tagName)
					break;
				if (reader.isStartElement() && reader.name() == QLatin1String("TableBorderLine"))
				{
					ScXmlStreamAttributes tAttB = reader.scAttributes();
					double width = tAttB.valueAsDouble("Width", 0.0);
					QString color = tAttB.valueAsString("Color", CommonStrings::None);
					double shade = tAttB.valueAsDouble("Shade", 100.0);
					int style = tAttB.valueAsInt("PenStyle", 1);
					border.addBorderLine(TableBorderLine(width, static_cast<Qt::PenStyle>(style), color, shade));
				}
			}
			item->setBottomBorder(border);
		}
		else
		{
			reader.skipCurrentElement();
		}
	}
	item->adjustTableToFrame();
	item->adjustFrameToTable();
	doc->dontResize = false;
	return !reader.hasError();
}

bool Scribus171Format::readItemTableCell(PageItem_Table* item, ScXmlStreamReader& reader, ScribusDoc *doc) const
{
	QString tagName(reader.nameAsString());
	ScXmlStreamAttributes tAtt = reader.scAttributes();
	
	int row = tAtt.valueAsInt("Row", -1);
	int col = tAtt.valueAsInt("Column", -1);
	if ((row >= 0) && (col >= 0))
	{
		if (tAtt.hasAttribute("Style"))
		{
			QString Style = tAtt.valueAsString("Style");
			if (!Style.isEmpty())
				item->cellAt(row, col).setStyle(Style);
		}
		QString fColor = tAtt.valueAsString("FillColor");
		if ((fColor != CommonStrings::None) && (!fColor.isEmpty()))
			item->cellAt(row, col).setFillColor(fColor);
		double fShade = tAtt.valueAsDouble("FillShade", -1.0);
		if (fShade >= 0 && fShade <= 100)
			item->cellAt(row, col).setFillShade(fShade);
		if (tAtt.hasAttribute("LeftPadding"))
			item->cellAt(row, col).setLeftPadding(tAtt.valueAsDouble("LeftPadding", 0.0));
		if (tAtt.hasAttribute("RightPadding"))
			item->cellAt(row, col).setRightPadding(tAtt.valueAsDouble("RightPadding", 0.0));
		if (tAtt.hasAttribute("TopPadding"))
			item->cellAt(row, col).setTopPadding(tAtt.valueAsDouble("TopPadding", 0.0));
		if (tAtt.hasAttribute("BottomPadding"))
			item->cellAt(row, col).setBottomPadding(tAtt.valueAsDouble("BottomPadding", 0.0));

		PageItem* newItem = item->cellAt(row, col).textFrame();
		newItem->m_columns = tAtt.valueAsInt("TextColumns", 1);
		newItem->m_columnGap = tAtt.valueAsDouble("TextColGap", 0.0);
		newItem->setTextToFrameDist(tAtt.valueAsDouble("TextDistLeft", 0.0),
							tAtt.valueAsDouble("TextDistRight", 0.0),
							tAtt.valueAsDouble("TextDistTop", 0.0),
							tAtt.valueAsDouble("TextDistBottom", 0.0));
		newItem->setVerticalAlignment(tAtt.valueAsInt("TextVertAlign", 0));
		newItem->setFirstLineOffset(static_cast<FirstLineOffsetPolicy>(tAtt.valueAsInt("Flop")));
	}

	LastStyles lastStyle;
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (!reader.isStartElement())
			continue;

		if (reader.name() == QLatin1String("TableBorderLeft"))
		{
			TableBorder border;
			QString tagName(reader.nameAsString());
			while (!reader.atEnd() && !reader.hasError())
			{
				reader.readNext();
				if (reader.isEndElement() && reader.name() == tagName)
					break;
				if (reader.isStartElement() && reader.name() == QLatin1String("TableBorderLine"))
				{
					ScXmlStreamAttributes tAttB = reader.scAttributes();
					double width = tAttB.valueAsDouble("Width", 0.0);
					QString color = tAttB.valueAsString("Color", CommonStrings::None);
					double shade = tAttB.valueAsDouble("Shade", 100.0);
					int style = tAttB.valueAsInt("PenStyle", 1);
					border.addBorderLine(TableBorderLine(width, static_cast<Qt::PenStyle>(style), color, shade));
				}
			}
			item->cellAt(row, col).setLeftBorder(border);
		}
		else if (reader.name() == QLatin1String("TableBorderRight"))
		{
			TableBorder border;
			QString tagName(reader.nameAsString());
			while (!reader.atEnd() && !reader.hasError())
			{
				reader.readNext();
				if (reader.isEndElement() && reader.name() == tagName)
					break;
				if (reader.isStartElement() && reader.name() == QLatin1String("TableBorderLine"))
				{
					ScXmlStreamAttributes tAttB = reader.scAttributes();
					double width = tAttB.valueAsDouble("Width", 0.0);
					QString color = tAttB.valueAsString("Color", CommonStrings::None);
					double shade = tAttB.valueAsDouble("Shade", 100.0);
					int style = tAttB.valueAsInt("PenStyle", 1);
					border.addBorderLine(TableBorderLine(width, static_cast<Qt::PenStyle>(style), color, shade));
				}
			}
			item->cellAt(row, col).setRightBorder(border);
		}
		else if (reader.name() == QLatin1String("TableBorderTop"))
		{
			TableBorder border;
			QString tagName(reader.nameAsString());
			while (!reader.atEnd() && !reader.hasError())
			{
				reader.readNext();
				if (reader.isEndElement() && reader.name() == tagName)
					break;
				if (reader.isStartElement() && reader.name() == QLatin1String("TableBorderLine"))
				{
					ScXmlStreamAttributes tAttB = reader.scAttributes();
					double width = tAttB.valueAsDouble("Width", 0.0);
					QString color = tAttB.valueAsString("Color", CommonStrings::None);
					double shade = tAttB.valueAsDouble("Shade", 100.0);
					int style = tAttB.valueAsInt("PenStyle", 1);
					border.addBorderLine(TableBorderLine(width, static_cast<Qt::PenStyle>(style), color, shade));
				}
			}
			item->cellAt(row, col).setTopBorder(border);
		}
		else if (reader.name() == QLatin1String("TableBorderBottom"))
		{
			TableBorder border;
			QString tagName(reader.nameAsString());
			while (!reader.atEnd() && !reader.hasError())
			{
				reader.readNext();
				if (reader.isEndElement() && reader.name() == tagName)
					break;
				if (reader.isStartElement() && reader.name() == QLatin1String("TableBorderLine"))
				{
					ScXmlStreamAttributes tAttB = reader.scAttributes();
					double width = tAttB.valueAsDouble("Width", 0.0);
					QString color = tAttB.valueAsString("Color", CommonStrings::None);
					double shade = tAttB.valueAsDouble("Shade", 100.0);
					int style = tAttB.valueAsInt("PenStyle", 1);
					border.addBorderLine(TableBorderLine(width, static_cast<Qt::PenStyle>(style), color, shade));
				}
			}
			item->cellAt(row, col).setBottomBorder(border);
		}
		else if (reader.name() == QLatin1String("StoryText"))
		{
			PageItem* newItem = item->cellAt(row, col).textFrame();
			readStoryText(doc, reader, newItem->itemText, newItem);
		}
		else
		{
			reader.skipCurrentElement();
		}
	}

	return !reader.hasError();
}

bool Scribus171Format::readLatexInfo(PageItem_LatexFrame* latexitem, ScXmlStreamReader& reader) const
{
	ScXmlStreamAttributes attrs = reader.scAttributes();
	QString tagName(reader.nameAsString());

	latexitem->setConfigFile(attrs.valueAsString("ConfigFile"), true);
	latexitem->setDpi(attrs.valueAsInt("DPI"));
	//Remove uppercase in 1.8 format
	if (attrs.hasAttribute("USE_PREAMBLE"))
		latexitem->setUsePreamble(attrs.valueAsBool("USE_PREAMBLE"));
	else
		latexitem->setUsePreamble(attrs.valueAsBool("UsePreamble"));

	QString formula;
	while (!reader.atEnd() && !reader.hasError())
	{
		reader.readNext();
		if (reader.isEndElement() && reader.name() == tagName)
			break;
		if (reader.isCharacters())
			formula += reader.text().toString();
		//Remove uppercase in 1.8 format
		if (reader.isStartElement() && reader.name() == QLatin1String("PROPERTY"))
		{
			ScXmlStreamAttributes tAtt = reader.scAttributes();
			QString name = tAtt.valueAsString("name");
			QString value = tAtt.valueAsString("value");
			if (name.isEmpty())
				continue;
			latexitem->editorProperties[name] = value;
		}
		else
			if (reader.isStartElement() && reader.name() == QLatin1String("Property"))
			{
				ScXmlStreamAttributes tAtt = reader.scAttributes();
				QString name = tAtt.valueAsString("Name");
				QString value = tAtt.valueAsString("Value");
				if (name.isEmpty())
					continue;
				latexitem->editorProperties[name] = value;
			}
	}
	formula = formula.trimmed();
	latexitem->setFormula(formula, false);

	return !reader.hasError();
}

bool Scribus171Format::loadPage(const QString & fileName, int pageNumber, bool Mpage, const QString& renamedPageName)
{
// 	qDebug() << QString("loading page %2 from file '%1' from 1.3.x plugin").arg(fileName).arg(pageNumber);
	if (m_Doc == nullptr || m_AvailableFonts == nullptr)
	{
		Q_ASSERT(m_Doc == nullptr || m_AvailableFonts == nullptr);
		return false;
	}

	Xp = 0.0;
	Yp = 0.0;
	GrX = 0.0;
	GrY = 0.0;
	struct ScribusDoc::BookMa bok;
	QMap<int, ScribusDoc::BookMa> bookmarks;

	ScPage* newPage = nullptr;
	
	QMap<int, PageItem*> TableID;
	QList<PageItem*> TableItems;
	QMap<int, PageItem*> WeldID;
	QList<PageItem*> WeldItems;
	QStack< QList<PageItem*> > groupStackFI;
	QStack< QList<PageItem*> > groupStackMI;
	QStack< QList<PageItem*> > groupStackPI;
	QStack< QList<PageItem*> > groupStackF;
	QStack< QList<PageItem*> > groupStackM;
	QStack< QList<PageItem*> > groupStackP;
	QStack<int> groupStackFI2;
	QStack<int> groupStackMI2;
	QStack<int> groupStackPI2;
	double pageX = 0;
	double pageY = 0;
	QMap<int,int> layerTrans;
	int maxLayer = 0;
	int maxLevel = 0;

	layerTrans.clear();
	uint layerCount = m_Doc->layerCount();
	for (uint la2 = 0; la2 < layerCount; ++la2)
	{
		maxLayer = qMax(m_Doc->Layers[la2].ID, maxLayer);
		maxLevel = qMax(m_Doc->Layers[la2].Level, maxLevel);
	}

	parStyleMap.clear();
	charStyleMap.clear();
	itemRemap.clear();
	itemNext.clear();
	itemCount = 0;
	itemRemapM.clear();
	itemNextM.clear();
	itemCountM = 0;
	itemRemapF.clear();
	itemNextF.clear();

	FrameItems.clear();
	WeldItems.clear();
	WeldID.clear();
	LinkID.clear();

	markeredItemsMap.clear();
	markeredMarksMap.clear();
	nsetRangeItemNamesMap.clear();
	notesFramesData.clear();
	notesMasterMarks.clear();
	notesNSets.clear();

	QScopedPointer<QIODevice> ioDevice(slaReader(fileName));
	if (ioDevice.isNull())
	{
		setFileReadError();
		return false;
	}

	QString fileDir = QFileInfo(fileName).absolutePath();
	
	ReadObjectParams readObjectParams;
	readObjectParams.baseDir = fileDir;
	readObjectParams.itemKind = PageItem::StandardItem;
	readObjectParams.loadingPage = true;
	readObjectParams.renamedMasterPage = Mpage ? renamedPageName : QString();

	bool firstElement = true;
	bool success = true;
	isNewFormat = false;
	
	ScXmlStreamReader reader(ioDevice.data());
	ScXmlStreamAttributes attrs;
	while (!reader.atEnd() && !reader.hasError())
	{
		QXmlStreamReader::TokenType tType = reader.readNext();
		if (tType != QXmlStreamReader::StartElement)
			continue;
		QString tagName(reader.nameAsString());
		attrs = reader.scAttributes();

		if (firstElement)
		{
			if (tagName != QLatin1String("SCRIBUSUTF8NEW"))
			{
				success = false;
				break;
			}
			firstElement = false;
		}
		//Remove uppercase in 1.8 format
		if (tagName == QLatin1String("COLOR") && attrs.valueAsString("NAME") != CommonStrings::None)
		{
			QString colorName = attrs.valueAsString("NAME");
			if (m_Doc->PageColors.contains(colorName))
				continue;
			readColor(m_Doc->PageColors, attrs);
		}
		else if (tagName == QLatin1String("Color") && attrs.valueAsString("Name") != CommonStrings::None)
		{
			QString colorName = attrs.valueAsString("Name");
			if (m_Doc->PageColors.contains(colorName))
				continue;
			readColor(m_Doc->PageColors, attrs);
		}
		if (tagName == QLatin1String("Gradient") && attrs.valueAsString("Name") != CommonStrings::None)
		{
			VGradient gra;
			QString grName = attrs.valueAsString("Name");
			success = readGradient(m_Doc, gra, reader);
			if (!success)
				break;
			gra.setRepeatMethod((VGradient::VGradientRepeatMethod)(attrs.valueAsInt("Ext", VGradient::pad)));
			if (!grName.isEmpty() && !m_Doc->docGradients.contains(grName))
				m_Doc->docGradients.insert(grName, gra);
		}
		//Remove uppercase in 1.8 format
		if (tagName == QLatin1String("JAVA") || tagName == QLatin1String("Java"))
		{
			QString name = attrs.valueAsString("Name");
			if (!name.isEmpty())
				m_Doc->JavaScripts[name] = attrs.valueAsString("Script");
			else
			{
				name = attrs.valueAsString("NAME");
				if (!name.isEmpty())
					m_Doc->JavaScripts[name] = attrs.valueAsString("SCRIPT");
			}
		}
		//Remove uppercase in 1.8 format
		if ((tagName == QLatin1String("LAYERS")) || (tagName == QLatin1String("Layers")))
		{
			ScLayer newLayer;
			readLayers(newLayer, attrs);
			const ScLayer* la2 = m_Doc->Layers.layerByName(newLayer.Name);
			if (la2)
				layerTrans.insert(newLayer.ID, la2->ID);
			else
			{
				maxLayer++;
				maxLevel++;
				layerTrans.insert(newLayer.ID, maxLayer);
				newLayer.ID = maxLayer;
				newLayer.Level = maxLevel;
				m_Doc->Layers.append(newLayer);
			}
		}
		if (tagName == QLatin1String("Arrows"))
		{
			success = readArrows(m_Doc, attrs);
			if (!success) break;
		}
		if (tagName == QLatin1String("MultiLine"))
		{
			MultiLine ml;
			QString mlName = attrs.valueAsString("Name");
			QString mlName2 = mlName;
			readMultiline(ml, reader);
			QHash<QString, MultiLine>::ConstIterator mlit = m_Doc->docLineStyles.constFind(mlName2);
			if (mlit != m_Doc->docLineStyles.constEnd() && ml != mlit.value())
				mlName2 = getUniqueName(mlName2, m_Doc->docLineStyles);
			m_Doc->docLineStyles.insert(mlName2, ml);
		}
		if (tagName == QLatin1String("Pattern"))
		{
			success = readPattern(m_Doc, reader, fileDir);
			if (!success) break;
		}
		if (tagName == QLatin1String("Bookmark"))
		{
			int bmElem = 0;
			struct ScribusDoc::BookMa bookmark;
			success = readBookMark(bookmark, bmElem, attrs);
			if (!success) break;
			bookmarks.insert(bmElem, bookmark);
		}
		//Remove old attribute format in 1.8 format
		if ((tagName == QLatin1String("ParagraphStyle")) || (tagName == QLatin1String("STYLE")))
		{
			ParagraphStyle pstyle;
			getStyle(pstyle, reader, nullptr, m_Doc, true);
		}
		//Remove uppercase in 1.8 format
		if (tagName == QLatin1String("CharacterStyle") || tagName == QLatin1String("CHARSTYLE"))
		{
			CharStyle cstyle;
			getStyle(cstyle, reader, nullptr, m_Doc, true);
		}
		if (tagName == QLatin1String("TableStyle"))
		{
			TableStyle tstyle;
			readTableStyle(m_Doc, reader, tstyle);
			// FIXME: import style under new name if existing
			// Do not break current doc for now
			if (m_Doc->tableStyles().contains(tstyle.name()))
				continue;
			StyleSet<TableStyle> temp;
			temp.create(tstyle);
			m_Doc->redefineTableStyles(temp, false);
		}
		if (tagName == QLatin1String("CellStyle"))
		{
			CellStyle tstyle;
			readCellStyle(m_Doc, reader, tstyle);
			// FIXME: import style under new name if existing
			// Do not break current doc for now
			if (m_Doc->cellStyles().contains(tstyle.name()))
				continue;
			StyleSet<CellStyle> temp;
			temp.create(tstyle);
			m_Doc->redefineCellStyles(temp, false);
		}
		if (((tagName == QLatin1String("PAGE")) || (tagName == QLatin1String("MASTERPAGE")) || (tagName == QLatin1String("Page")) || (tagName == QLatin1String("MasterPage"))) && (attrs.valueAsInt("NUM") == pageNumber || attrs.valueAsInt("PageNumber") == pageNumber))
		{
			if (Mpage && (tagName != QLatin1String("MASTERPAGE") && tagName != QLatin1String("MasterPage")))
				continue;
			int pageNr = m_Doc->currentPage()->pageNr();
			newPage = m_Doc->Pages->at(pageNr);
			SimpleState *ss = nullptr;
			if (UndoManager::undoEnabled())
			{
				ss = new SimpleState(Um::ChangePageAttrs, "", Um::ICreate);
				ss->set("PAGE_ATTRS");
				ss->set("LEFT_OLD", newPage->LeftPg);
				ss->set("NAME_OLD", newPage->pageName());
				ss->set("ORIENTATION_OLD", newPage->orientation());
				ss->set("SIZE_OLD", newPage->size());
				ss->set("WIDTH_OLD", newPage->width());
				ss->set("HEIGHT_OLD", newPage->height());
				ss->set("INIT_HEIGHT_OLD", newPage->initialHeight());
				ss->set("INIT_WIDTH_OLD", newPage->initialWidth());
				ss->set("INIT_MARGINTOP_OLD", newPage->initialMargins.top());
				ss->set("INIT_MARGINBOTTOM_OLD", newPage->initialMargins.bottom());
				ss->set("INIT_MARGINRIGHT_OLD", newPage->initialMargins.right());
				ss->set("INIT_MARGINLEFT_OLD", newPage->initialMargins.left());
				ss->set("MARGINTOP_OLD", newPage->Margins.top());
				ss->set("MARGINBOTTOM_OLD", newPage->Margins.bottom());
				ss->set("MARGINPRESET_OLD", newPage->marginPreset);
				ss->set("HORIZONTAL_AUTOGAP_OLD", newPage->guides.horizontalAutoGap());
				ss->set("VERTICAL_AUTOGAP_OLD", newPage->guides.verticalAutoGap());
				ss->set("HORIZONTAL_AUTOCOUNT_OLD", newPage->guides.horizontalAutoCount());
				ss->set("VERTICAL_AUTOCOUNT_OLD", newPage->guides.verticalAutoCount());
				ss->set("HORIZONTAL_AUTOREFER_OLD", newPage->guides.horizontalAutoRefer());
				ss->set("VERTICAL_AUTOREFER_OLD", newPage->guides.verticalAutoRefer());
			}
			if (Mpage)
			{
				//Remove uppercase in 1.8
				if (attrs.hasAttribute("LEFT"))
					newPage->LeftPg = attrs.valueAsInt("LEFT", 0);
				else
					newPage->LeftPg = attrs.valueAsInt("LeftPage", 0);
				if (!renamedPageName.isEmpty())
					newPage->setPageName(renamedPageName);
				else
				{
					//Remove uppercase in 1.8
					if (attrs.hasAttribute("NAM"))
						newPage->setPageName(attrs.valueAsString("NAM",""));
					else
						newPage->setPageName(attrs.valueAsString("PageName",""));
				}
			}
			if (attrs.hasAttribute("Size"))
				newPage->setSize(attrs.valueAsString("Size"));
			if (attrs.hasAttribute("Orientation"))
				newPage->setOrientation(attrs.valueAsInt("Orientation"));
			if (attrs.hasAttribute("PAGEWIDTH"))
				newPage->setWidth(attrs.valueAsDouble("PAGEWIDTH"));
			else
				newPage->setWidth(attrs.valueAsDouble("PageWidth"));
			//Remove uppercase in 1.8
			if (attrs.hasAttribute("PAGEHEIGHT"))
				newPage->setHeight(attrs.valueAsDouble("PAGEHEIGHT"));
			else
				newPage->setHeight(attrs.valueAsDouble("PageHeight"));
			newPage->setInitialHeight(newPage->height());
			newPage->setInitialWidth(newPage->width());
			if (attrs.hasAttribute("BORDERTOP"))
			{
			newPage->initialMargins.setTop(qMax(0.0, attrs.valueAsDouble("BORDERTOP")));
			newPage->initialMargins.setBottom(qMax(0.0, attrs.valueAsDouble("BORDERBOTTOM")));
			newPage->initialMargins.setLeft(qMax(0.0, attrs.valueAsDouble("BORDERLEFT")));
			newPage->initialMargins.setRight(qMax(0.0, attrs.valueAsDouble("BORDERRIGHT")));
			newPage->marginPreset = attrs.valueAsInt("PRESET", 0);
			newPage->Margins.setTop(newPage->initialMargins.top());
			newPage->Margins.setBottom(newPage->initialMargins.bottom());
			pageX = attrs.valueAsDouble("PAGEXPOS");
			pageY = attrs.valueAsDouble("PAGEYPOS");
			}
			else
			{
				newPage->initialMargins.setTop(qMax(0.0, attrs.valueAsDouble("BorderTop")));
				newPage->initialMargins.setBottom(qMax(0.0, attrs.valueAsDouble("BorderBottom")));
				newPage->initialMargins.setLeft(qMax(0.0, attrs.valueAsDouble("BorderLeft")));
				newPage->initialMargins.setRight(qMax(0.0, attrs.valueAsDouble("BorderRight")));
				newPage->marginPreset = attrs.valueAsInt("Preset", 0);
				newPage->Margins.setTop(newPage->initialMargins.top());
				newPage->Margins.setBottom(newPage->initialMargins.bottom());
				pageX = attrs.valueAsDouble("PageXPosition");
				pageY = attrs.valueAsDouble("PageYPosition");
			}

			// guides reading
			//Remove in 1.8
			if (attrs.hasAttribute("AGhorizontalAutoGap"))
			{
				newPage->guides.setHorizontalAutoGap(attrs.valueAsDouble("AGhorizontalAutoGap", 0.0));
				newPage->guides.setVerticalAutoGap(attrs.valueAsDouble("AGverticalAutoGap", 0.0));
				newPage->guides.setHorizontalAutoCount(attrs.valueAsInt("AGhorizontalAutoCount", 0));
				newPage->guides.setVerticalAutoCount(attrs.valueAsInt("AGverticalAutoCount", 0));
				newPage->guides.setHorizontalAutoRefer(attrs.valueAsInt("AGhorizontalAutoRefer", 0));
				newPage->guides.setVerticalAutoRefer(attrs.valueAsInt("AGverticalAutoRefer", 0));
			}
			else
			{
				newPage->guides.setHorizontalAutoGap(attrs.valueAsDouble("AutoGuideHorizontalGap", 0.0));
				newPage->guides.setVerticalAutoGap(attrs.valueAsDouble("AutoGuideVerticalGap", 0.0));
				newPage->guides.setHorizontalAutoCount(attrs.valueAsInt("AutoGuideHorizontalCount", 0));
				newPage->guides.setVerticalAutoCount(attrs.valueAsInt("AutoGuideVerticalCount", 0));
				newPage->guides.setHorizontalAutoRefer(attrs.valueAsInt("AutoGuideHorizontalReference", 0));
				newPage->guides.setVerticalAutoRefer(attrs.valueAsInt("AutoGuideVerticalReference", 0));
			}
			GuideManagerIO::readVerticalGuides(attrs.valueAsString("VerticalGuides"),
											newPage,
											GuideManagerCore::Standard,
											attrs.hasAttribute("NumVGuides"));
			GuideManagerIO::readHorizontalGuides(attrs.valueAsString("HorizontalGuides"),
											newPage,
											GuideManagerCore::Standard,
											attrs.hasAttribute("NumHGuides"));
			GuideManagerIO::readSelection(attrs.valueAsString("AGSelection"), newPage);

			newPage->guides.addHorizontals(newPage->guides.getAutoHorizontals(newPage), GuideManagerCore::Auto);
			newPage->guides.addVerticals(newPage->guides.getAutoVerticals(newPage), GuideManagerCore::Auto);
			if (UndoManager::undoEnabled())
			{
				ss->set("LEFT", newPage->LeftPg);
				ss->set("NAME", newPage->pageName());
				ss->set("ORIENTATION", newPage->orientation());
				ss->set("SIZE", newPage->size());
				ss->set("WIDTH", newPage->width());
				ss->set("HEIGHT", newPage->height());
				ss->set("INIT_HEIGHT", newPage->initialHeight());
				ss->set("INIT_WIDTH", newPage->initialWidth());
				ss->set("INIT_MARGINTOP", newPage->initialMargins.top());
				ss->set("INIT_MARGINBOTTOM", newPage->initialMargins.bottom());
				ss->set("INIT_MARGINRIGHT", newPage->initialMargins.right());
				ss->set("INIT_MARGINLEFT", newPage->initialMargins.left());
				ss->set("MARGINTOP", newPage->Margins.top());
				ss->set("MARGINBOTTOM", newPage->Margins.bottom());
				ss->set("MARGINPRESET", newPage->marginPreset);
				ss->set("HORIZONTAL_AUTOGAP", newPage->guides.horizontalAutoGap());
				ss->set("VERTICAL_AUTOGAP", newPage->guides.verticalAutoGap());
				ss->set("HORIZONTAL_AUTOCOUNT", newPage->guides.horizontalAutoCount());
				ss->set("VERTICAL_AUTOCOUNT", newPage->guides.verticalAutoCount());
				ss->set("HORIZONTAL_AUTOREFER", newPage->guides.horizontalAutoRefer());
				ss->set("VERTICAL_AUTOREFER", newPage->guides.verticalAutoRefer());
				undoManager->action(newPage, ss);
			}
		}
		if ((tagName == QLatin1String("PAGEOBJECT")) || (tagName == QLatin1String("MASTEROBJECT")) || (tagName == QLatin1String("FRAMEOBJECT")) ||
				(tagName == QLatin1String("PageObject")) || (tagName == QLatin1String("MasterObject")) || (tagName == QLatin1String("FrameObject")))
		{
			if ((Mpage && (tagName != QLatin1String("MASTEROBJECT") && tagName != QLatin1String("MasterObject"))) || (!Mpage && (tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("MasterObject"))))
			{
				// Go to end of node
				reader.readToElementEnd();
				continue;
			}

			if ((attrs.valueAsInt("OwnPage") != pageNumber) && (tagName != "FRAMEOBJECT") && (tagName != "FrameObject"))
			{			
				if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject"))
					itemRemap[itemCount++] = -1;
				else if (tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("MasterObject"))
					itemRemapM[itemCountM++] = -1;
				reader.readToElementEnd();
				continue;
			}

			ItemInfo itemInfo;
			success = readObject(m_Doc, reader, readObjectParams, itemInfo);
			if (!success)
				break;

			PageItem* newItem = itemInfo.item;
			newItem->moveBy(-pageX + newPage->xOffset(), - pageY + newPage->yOffset());
			newItem->setOwnerPage(m_Doc->currentPageNumber());
			if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject"))
				newItem->setMasterPageName(QString());
			else if (Mpage && !renamedPageName.isEmpty())
				newItem->setMasterPageName(renamedPageName);
			newItem->setLayer(layerTrans.value(newItem->m_layerID, newItem->m_layerID));
			if (isNewFormat)
			{
				if (itemInfo.nextItem != -1)
					itemNext[itemInfo.itemID] = itemInfo.nextItem;
				if (itemInfo.item->isTableItem)
					TableItems.append(itemInfo.item);
				if (itemInfo.isWeldFlag)
					WeldItems.append(itemInfo.item);
			}
			else
			{
				// first of linked chain?
				if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject"))
				{
					itemRemap[itemCount++] = m_Doc->DocItems.count();
					if (attrs.hasAttribute("NEXTITEM"))
					{
						if (attrs.valueAsInt("NEXTITEM", -1) != -1)
							itemNext[m_Doc->DocItems.count()] = attrs.valueAsInt("NEXTITEM");
					}
					else
					{
						if (attrs.valueAsInt("NextItem", -1) != -1)
							itemNext[m_Doc->DocItems.count()] = attrs.valueAsInt("NextItem");
					}
				}
				else if (tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("MasterObject"))
				{
					itemRemapM[itemCountM++] = m_Doc->MasterItems.count();
					if (attrs.hasAttribute("NEXTITEM"))
					{
						if (attrs.valueAsInt("NEXTITEM", -1) != -1)
							itemNextM[m_Doc->MasterItems.count()] = attrs.valueAsInt("NEXTITEM");
					}
					else
					{
						if (attrs.valueAsInt("NextItem", -1) != -1)
							itemNextM[m_Doc->MasterItems.count()] = attrs.valueAsInt("NextItem");
					}
				}
				if (newItem->isTableItem)
				{
					TableItems.append(newItem);
					TableID.insert(itemInfo.ownLink, newItem);
				}
				if (itemInfo.isWeldFlag)
				{
					WeldItems.append(itemInfo.item);
					WeldID.insert(itemInfo.ownWeld, itemInfo.item);
				}
			}

			if ((tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject")) && (groupStackPI.count() > 0))
			{
				groupStackPI.top().append(itemInfo.item);
				while (itemInfo.ownNr == groupStackPI2.top())
				{
					groupStackP.push(groupStackPI.pop());
					groupStackPI2.pop();
					if (groupStackPI2.count() == 0)
						break;
				}
			}
			else if ((tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject")) && (groupStackFI.count() > 0))
			{
				groupStackFI.top().append(itemInfo.item);
				while (itemInfo.ownNr == groupStackFI2.top())
				{
					groupStackF.push(groupStackFI.pop());
					groupStackFI2.pop();
					if (groupStackFI2.count() == 0)
						break;
				}
			}
			else if ((tagName == QLatin1String("MASTEROBJECT") || tagName == QLatin1String("MasterObject")) && (groupStackMI.count() > 0))
			{
				groupStackMI.top().append(itemInfo.item);
				while (itemInfo.ownNr == groupStackMI2.top())
				{
					groupStackM.push(groupStackMI.pop());
					groupStackMI2.pop();
					if (groupStackMI2.count() == 0)
						break;
				}
			}

			if (itemInfo.isGroupFlag)
			{
				QList<PageItem*> groupItems;
				groupItems.append(itemInfo.item);
				if (tagName == QLatin1String("PAGEOBJECT") || tagName == QLatin1String("PageObject"))
				{
					groupStackPI.push(groupItems);
					groupStackPI2.push(itemInfo.groupLastItem + itemInfo.ownNr);
				}
				else if (tagName == QLatin1String("FRAMEOBJECT") || tagName == QLatin1String("FrameObject"))
				{
					groupStackFI.push(groupItems);
					groupStackFI2.push(itemInfo.groupLastItem + itemInfo.ownNr);
				}
				else
				{
					groupStackMI.push(groupItems);
					groupStackMI2.push(itemInfo.groupLastItem + itemInfo.ownNr);
				}
			}
		}
	}

	if (reader.hasError())
	{
		setDomParsingError(reader.errorString(), reader.lineNumber(), reader.columnNumber());
		return false;
	}

	for (auto it = bookmarks.begin(); it != bookmarks.end(); ++it)
	{
		int elem = it.key();
		PageItem* item = LinkID.value(elem, (PageItem*) nullptr);
		if (!item)
			continue;
		ScribusDoc::BookMa bookmark = it.value();
		bookmark.PageObject = item;
		m_Doc->BookMarks.append( bookmark );
	}

	if (isNewFormat)
	{
		if (TableItems.count() != 0)
		{
			for (int ttc = 0; ttc < TableItems.count(); ++ttc)
			{
				PageItem* ta = TableItems.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = LinkID[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = LinkID[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = LinkID[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = LinkID[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (WeldItems.count() != 0)
		{
			for (int ttc = 0; ttc < WeldItems.count(); ++ttc)
			{
				PageItem* ta = WeldItems.at(ttc);
				for (int i = 0 ; i < ta->weldList.count(); ++i)
				{
					PageItem::WeldingInfo wInf = ta->weldList.at(i);
					ta->weldList[i].weldItem = LinkID.value(wInf.weldID, 0);
					if (ta->weldList[i].weldItem == nullptr)
						ta->weldList.removeAt(i--);
				}
			}
		}
		if (itemNext.count() != 0)
		{
			for (auto lc = itemNext.begin(); lc != itemNext.end(); ++lc)
			{
				if (lc.value() >= 0)
				{
					PageItem * Its = LinkID[lc.key()];
					PageItem * Itn = LinkID[lc.value()];
					if (!Its->canBeLinkedTo(Itn))
					{
						qDebug() << "scribus171format: corruption in linked textframes detected";
						continue;
					}
					Its->link(Itn);
				}
			}
		}
	}
	else
	{
		if (TableItems.count() != 0)
		{
			for (int ttc = 0; ttc < TableItems.count(); ++ttc)
			{
				PageItem* ta = TableItems.at(ttc);
				if (ta->TopLinkID != -1)
					ta->m_topLink = TableID[ta->TopLinkID];
				else
					ta->m_topLink = nullptr;
				if (ta->LeftLinkID != -1)
					ta->m_leftLink = TableID[ta->LeftLinkID];
				else
					ta->m_leftLink = nullptr;
				if (ta->RightLinkID != -1)
					ta->m_rightLink = TableID[ta->RightLinkID];
				else
					ta->m_rightLink = nullptr;
				if (ta->BottomLinkID != -1)
					ta->m_bottomLink = TableID[ta->BottomLinkID];
				else
					ta->m_bottomLink = nullptr;
			}
		}
		if (WeldItems.count() != 0)
		{
			for (int ttc = 0; ttc < WeldItems.count(); ++ttc)
			{
				PageItem* ta = WeldItems.at(ttc);
				for (int i = 0 ; i < ta->weldList.count(); ++i)
				{
					PageItem::WeldingInfo wInf = ta->weldList.at(i);
					ta->weldList[i].weldItem = WeldID.value(wInf.weldID, 0);
					if (ta->weldList[i].weldItem == nullptr)
						ta->weldList.removeAt(i--);
				}
			}
		}
		// reestablish textframe links
		if (itemNext.count() != 0 && !Mpage)
		{
			for (auto lc = itemNext.begin(); lc != itemNext.end(); ++lc)
			{
				if (itemRemap[lc.value()] >= 0)
				{
					PageItem *Its(nullptr), *Itn(nullptr);
					if (lc.key() < m_Doc->Items->count())
						Its = m_Doc->DocItems.at(lc.key());
					if (itemRemap[lc.value()] < m_Doc->DocItems.count())
						Itn = m_Doc->DocItems.at(itemRemap[lc.value()]);
					if (!Its || !Itn || !Its->canBeLinkedTo(Itn))
					{
						qDebug() << "scribus171format: corruption in linked textframes detected";
						continue;
					}
					Its->link(Itn);
				}
			}
		}
		else if (itemNextM.count() != 0 && Mpage)
		{
			for (auto lc = itemNextM.begin(); lc != itemNextM.end(); ++lc)
			{
				if (itemRemapM[lc.value()] >= 0)
				{
					PageItem *Its(nullptr), *Itn(nullptr);
					if (lc.key() < m_Doc->MasterItems.count())
						Its = m_Doc->MasterItems.at(lc.key());
					if (itemRemapM[lc.value()] < m_Doc->MasterItems.count())
						Itn = m_Doc->MasterItems.at(itemRemapM[lc.value()]);
					if (!Its || !Itn || !Its->canBeLinkedTo(Itn))
					{
						qDebug() << "scribus171format: corruption in linked textframes detected";
						continue;
					}
					Its->link(Itn);
				}
			}
		}
	}

	while (groupStackP.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackP.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->DocItems.removeOne(cItem);
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackP, &m_Doc->DocItems);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	while (groupStackF.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackF.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->FrameItems.remove(m_Doc->FrameItems.key(cItem));
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackF, nullptr);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	while (groupStackM.count() > 0)
	{
		bool isTableIt = false;
		QList<PageItem*> gpL = groupStackM.pop();
		PageItem* gItem = gpL.takeFirst();
		for (int id = 0; id < gpL.count(); id++)
		{
			PageItem* cItem = gpL.at(id);
			isTableIt = cItem->isTableItem;
			cItem->gXpos = cItem->xPos() - gItem->xPos();
			cItem->gYpos = cItem->yPos() - gItem->yPos();
			cItem->Parent = gItem;
			if (gItem->rotation() != 0)
			{
				QTransform ma;
				ma.rotate(-gItem->rotation());
				FPoint n(cItem->gXpos, cItem->gYpos);
				cItem->gXpos = ma.m11() * n.x() + ma.m21() * n.y() + ma.dx();
				cItem->gYpos = ma.m22() * n.y() + ma.m12() * n.x() + ma.dy();
				cItem->setRotation(cItem->rotation() - gItem->rotation());
				cItem->oldRot = cItem->rotation();
			}
			m_Doc->MasterItems.removeOne(cItem);
		}
		bool converted = false;
		if (isTableIt)
			converted = convertOldTable(m_Doc, gItem, gpL, &groupStackM, &m_Doc->MasterItems);
		if (!converted)
			gItem->groupItemList = gpL;
	}

	// reestablish first/lastAuto
	m_Doc->FirstAuto = m_Doc->LastAuto;
	if (m_Doc->LastAuto)
	{
		while (m_Doc->LastAuto->nextInChain())
			m_Doc->LastAuto = m_Doc->LastAuto->nextInChain();
		while (m_Doc->FirstAuto->prevInChain())
			m_Doc->FirstAuto = m_Doc->FirstAuto->prevInChain();
	}

	return true;
}

void Scribus171Format::getStyle(ParagraphStyle& style, ScXmlStreamReader& reader, StyleSet<ParagraphStyle> *tempStyles, ScribusDoc* doc, bool equiv)
{
	bool  found(false);
	const StyleSet<ParagraphStyle> &docParagraphStyles = tempStyles ? *tempStyles : doc->paragraphStyles();
	
	style.erase();
	readParagraphStyle(doc, reader, style);

	// Do not duplicate default style
	if (style.isDefaultStyle())
		style.setDefaultStyle(false);

	const ParagraphStyle* foundStyle = docParagraphStyles.getPointer(style.name());
	if (foundStyle)
	{
		found = style.equiv(*foundStyle);
		if (found)
			return;
		QString newName = docParagraphStyles.getUniqueCopyName(style.name());
		parStyleMap[style.name()] = newName;
		style.setName(newName);
	}

	if (equiv)
	{
		const ParagraphStyle* equivStyle = docParagraphStyles.findEquivalent(style);
		if (equivStyle)
		{
			parStyleMap[style.name()] = equivStyle->name();
			style.setName(equivStyle->name());
			return;
		}
	}

	if (tempStyles)
		tempStyles->create(style);
	else
	{
		StyleSet<ParagraphStyle> tmp;
		tmp.create(style);
		doc->redefineStyles(tmp, false);
	}
}

void Scribus171Format::getStyle(CharStyle& style, ScXmlStreamReader& reader, StyleSet<CharStyle> *tempStyles, ScribusDoc* doc, bool equiv)
{
	bool  found(false);
	const StyleSet<CharStyle> &docCharStyles = tempStyles ? *tempStyles : doc->charStyles();
	
	style.erase();
	ScXmlStreamAttributes attrs = reader.scAttributes();
	readNamedCharacterStyleAttrs(m_Doc, attrs, style);

	// Do not duplicate default style
	if (style.isDefaultStyle())
		style.setDefaultStyle(false);

	const CharStyle* foundStyle = docCharStyles.getPointer(style.name());
	if (foundStyle)
	{
		found = style.equiv(*foundStyle);
		if (found)
			return;
		QString newName = docCharStyles.getUniqueCopyName(style.name());
		parStyleMap[style.name()] = newName;
		style.setName(newName);
	}

	if (equiv)
	{
		const CharStyle* equivStyle = docCharStyles.findEquivalent(style);
		if (equivStyle)
		{
			charStyleMap[style.name()] = equivStyle->name();
			style.setName(equivStyle->name());
			return;
		}
	}

	if (tempStyles)
		tempStyles->create(style);
	else
	{
		StyleSet<CharStyle> tmp;
		tmp.create(style);
		doc->redefineCharStyles(tmp, false);
	}
}

bool Scribus171Format::readStyles(const QString& fileName, ScribusDoc* doc, StyleSet<ParagraphStyle> &docParagraphStyles)
{
	ParagraphStyle pstyle;
	bool firstElement = true;
	bool success = true;

	QScopedPointer<QIODevice> ioDevice(slaReader(fileName));
	if (ioDevice.isNull())
		return false;

	parStyleMap.clear();
	charStyleMap.clear();

	ScXmlStreamReader reader(ioDevice.data());
	ScXmlStreamAttributes attrs;
	while (!reader.atEnd() && !reader.hasError())
	{
		QXmlStreamReader::TokenType tType = reader.readNext();
		if (tType != QXmlStreamReader::StartElement)
			continue;
		QString tagName(reader.nameAsString());
		if (firstElement)
		{
			if (tagName != QLatin1String("SCRIBUSUTF8NEW"))
			{
				success = false;
				break;
			}
			firstElement = false;
			continue;
		}
		//Remove uppercase in 1.8 format
		if ((tagName == QLatin1String("ParagraphStyle")) || (tagName == QLatin1String("STYLE")))
		{
			pstyle.erase();
			getStyle(pstyle, reader, &docParagraphStyles, doc, false);
		}
	}
	return success;
}

bool Scribus171Format::readCharStyles(const QString& fileName, ScribusDoc* doc, StyleSet<CharStyle> &docCharStyles)
{
	CharStyle cstyle;
	bool firstElement = true;
	//bool success = true;

	QScopedPointer<QIODevice> ioDevice(slaReader(fileName));
	if (ioDevice.isNull())
		return false;

	parStyleMap.clear();
	charStyleMap.clear();

	ScXmlStreamReader reader(ioDevice.data());
	ScXmlStreamAttributes attrs;
	while (!reader.atEnd() && !reader.hasError())
	{
		QXmlStreamReader::TokenType tType = reader.readNext();
		if (tType != QXmlStreamReader::StartElement)
			continue;
		QString tagName(reader.nameAsString());
		if (firstElement)
		{
			if (tagName != QLatin1String("SCRIBUSUTF8NEW"))
			{
			//	success = false;
				break;
			}
			firstElement = false;
			continue;
		}
		//Remove uppercase in 1.8 format
		if (tagName == QLatin1String("CharacterStyle") || tagName == QLatin1String("CHARSTYLE"))
		{
			cstyle.erase();
			attrs = reader.scAttributes();
			readNamedCharacterStyleAttrs(doc, attrs, cstyle);
			docCharStyles.create(cstyle);
		}
	}
	return true;
}

bool Scribus171Format::readLineStyles(const QString& fileName, QHash<QString, MultiLine> *styles)
{
	bool firstElement = true;
	bool success = true;

	QScopedPointer<QIODevice> ioDevice(slaReader(fileName));
	if (ioDevice.isNull())
		return false;

	ScXmlStreamReader reader(ioDevice.data());
	ScXmlStreamAttributes attrs;
	while (!reader.atEnd() && !reader.hasError())
	{
		QXmlStreamReader::TokenType tType = reader.readNext();
		if (tType != QXmlStreamReader::StartElement)
			continue;
		QString tagName(reader.nameAsString());
		if (firstElement)
		{
			if (tagName != QLatin1String("SCRIBUSUTF8NEW"))
			{
				success = false;
				break;
			}
			firstElement = false;
			continue;
		}
		if (tagName == QLatin1String("MultiLine"))
		{
			MultiLine ml;
			attrs = reader.scAttributes();
			QString mlName = attrs.valueAsString("Name");
			QString mlName2 = mlName;
			readMultiline(ml, reader);
			int copyC = 1;
			QHash<QString, MultiLine>::ConstIterator mlit = styles->find(mlName2);
			if (mlit != styles->constEnd() && ml != mlit.value())
			{
				while (styles->contains(mlName2))
				{
					mlName2 = tr("Copy #%1 of ").arg(copyC)+mlName;
					copyC++;
				}
			}
			styles->insert(mlName2, ml);
		}
	}
	return success;
}

bool Scribus171Format::readColors(const QString& fileName, ColorList & colors)
{
	bool firstElement = true;
	bool success = true;

	QScopedPointer<QIODevice> ioDevice(slaReader(fileName));
	if (ioDevice.isNull())
		return false;

	ScXmlStreamReader reader(ioDevice.data());
	ScXmlStreamAttributes attrs;
	while (!reader.atEnd() && !reader.hasError())
	{
		QXmlStreamReader::TokenType tType = reader.readNext();
		if (tType != QXmlStreamReader::StartElement)
			continue;
		QString tagName(reader.nameAsString());
		if (firstElement)
		{
			if (tagName != QLatin1String("SCRIBUSUTF8NEW"))
			{
				success = false;
				break;
			}
			firstElement = false;
			continue;
		}
		//Remove uppercase in 1.8
		if (tagName == QLatin1String("COLOR") && attrs.valueAsString("NAME") != CommonStrings::None)
		{
			attrs = reader.scAttributes();
			if (attrs.valueAsString("NAME") != CommonStrings::None)
				readColor(colors, attrs);
		}
		else
			if (tagName == QLatin1String("Color") && attrs.valueAsString("Name") != CommonStrings::None)
			{
				attrs = reader.scAttributes();
				if (attrs.valueAsString("Name") != CommonStrings::None)
					readColor(colors, attrs);
			}
	}
	return success;
}

bool Scribus171Format::readPageCount(const QString& fileName, int *num1, int *num2, QStringList & masterPageNames)
{
	QString pageName;
	int counter = 0;
	int counter2 = 0;
	bool firstElement = true;
	bool success = true;

	markeredItemsMap.clear();
	markeredMarksMap.clear();
	nsetRangeItemNamesMap.clear();
	notesFramesData.clear();
	notesMasterMarks.clear();
	notesNSets.clear();

	QScopedPointer<QIODevice> ioDevice(slaReader(fileName));
	if (ioDevice.isNull())
		return false;

	ScXmlStreamReader reader(ioDevice.data());
	ScXmlStreamAttributes attrs;
	while (!reader.atEnd() && !reader.hasError())
	{
		QXmlStreamReader::TokenType tType = reader.readNext();
		if (tType != QXmlStreamReader::StartElement)
			continue;
		QString tagName(reader.nameAsString());
		if (firstElement)
		{
			if (tagName != QLatin1String("SCRIBUSUTF8NEW"))
			{
				success = false;
				break;
			}
			firstElement = false;
			continue;
		}
		//Remove uppercase in 1.8 format
		if (tagName == QLatin1String("PAGE"))
			counter++;
		if (tagName == QLatin1String("Page"))
			counter++;
		//Remove uppercase in 1.8 format
		if (tagName == QLatin1String("MASTERPAGE"))
		{
			pageName = reader.scAttributes().valueAsString("NAM");
			if (!pageName.isEmpty())
			{
				counter2++;
				masterPageNames.append(pageName);
			}
		}
		if (tagName == QLatin1String("MasterPage"))
		{
			pageName = reader.scAttributes().valueAsString("PageName");
			if (!pageName.isEmpty())
			{
				counter2++;
				masterPageNames.append(pageName);
			}
		}
	}
	*num1 = counter;
	*num2 = counter2;
	return success;
}

void Scribus171Format::updateNames2Ptr() //after document load - items pointers should be updated
{
	if (!markeredItemsMap.isEmpty())
	{
		QMap<Mark*, int>::Iterator end = markeredItemsMap.end();
		for (auto markIt = markeredItemsMap.begin(); markIt != end; ++markIt)
		{
			Mark* mrk = markIt.key();
			int ItemID = markIt.value();
			if (LinkID.contains(ItemID))
			{
				mrk->setItemPtr(LinkID[ItemID]);
				mrk->setString(m_Doc->getSectionPageNumberForPageIndex(mrk->getItemPtr()->OwnPage));
			}
			else
			{
				qWarning() << "Scribus171Format::updateNames2Ptr() : wrong mark [" << mrk->label << "] data - item [" << ItemID << "] not exists - DELETING MARK";
				QString markLabel(mrk->label);
				if (!m_Doc->eraseMark(mrk, true))
					qWarning() << "Erase mark [" << markLabel << "] failed - was it defined?";
			}
		}
		markeredItemsMap.clear();
	}
	if (!markeredMarksMap.isEmpty())
	{
		auto end = markeredMarksMap.end();
		for (auto markIt = markeredMarksMap.begin(); markIt != end; ++markIt)
		{
			Mark* mark = markIt.key();
			QMap<QString, MarkType> mark2map = markIt.value();
			QString label2 = mark2map.begin().key();
			MarkType type2 = mark2map.begin().value();
			Mark* mark2 = m_Doc->getMark(label2, type2);
			if (mark2 != nullptr)
			{
				mark->setDestMark(mark2);
			}
			else
			{
				qWarning() << "Scribus171Format::updateNames2Ptr() : wrong mark [" << mark->label << "] data - pointed mark name [" << label2 << "] not exists - DELETING MARK";
				QString markLabel(mark->label);
				if (!m_Doc->eraseMark(mark, true))
					qWarning() << "Erase mark [" << markLabel << "] failed - was it defined?";

			}
		}
		markeredMarksMap.clear();
	}

	//update endnotes frames pointers
	if (!notesFramesData.isEmpty())
	{
		for (int i = 0; i < notesFramesData.count(); ++i)
		{
			NoteFrameData eF = notesFramesData.at(i);
			NotesStyle* ns = m_Doc->getNotesStyle(eF.NSname);
			if (ns == nullptr)
				continue;

			PageItem* item = LinkID.value(eF.myID);
			if (item == nullptr || !item->isNoteFrame())
			{
				qDebug() << "Scribus171Format::updateNames2Ptr() : update end frames pointers - item is not note frame or name is wrong";
				continue;
			}

			PageItem_NoteFrame* noteFrame = item->asNoteFrame();
			noteFrame->setNoteStyle(ns);
			if (ns->isEndNotes())
			{
				if (eF.NSrange == NSRdocument)
					m_Doc->setEndNoteFrame(noteFrame, (void*) nullptr);
				else if (eF.NSrange == NSRstory)
					m_Doc->setEndNoteFrame(noteFrame, (void*) LinkID.value(eF.itemID));
			}
			else
			{
				PageItem* master = LinkID.value(eF.itemID);
				if (master == nullptr)
					continue;
				noteFrame->setMaster(master);
				master->asTextFrame()->setNoteFrame(noteFrame);
				//FIX welding with note frame
				PageItem::WeldingInfo wInf;
				for (int i = 0 ; i < master->weldList.count(); i++)
				{
					wInf = master->weldList.at(i);
					if (wInf.weldID == eF.myID)
					{
						master->weldList[i].weldItem = item;
						break;
					}
				}
			}
		}
	}
	//update pointers to notes in master notes marks
	if (!notesMasterMarks.isEmpty())
	{
		assert(!m_Doc->marksList().isEmpty() && !m_Doc->notesList().isEmpty());
		QMap<QString, TextNote*>::Iterator end = notesMasterMarks.end();
		for (auto it = notesMasterMarks.begin(); it != end; ++it)
		{
			TextNote* note = it.value();
			assert(note != nullptr);
			const QString& mrkLabel = it.key();
			Mark* mrk = m_Doc->getMark(mrkLabel, MARKNoteMasterType);
			if (mrk == nullptr)
			{
				qWarning() << "Scribus171Format::updateNames2Ptr() : cannot find master mark ("<<mrkLabel <<") for note - note will be deleted";
				m_Doc->deleteNote(note);
				continue;
			}
			note->setMasterMark(mrk);
			mrk->setNotePtr(note);
		}
		notesMasterMarks.clear();
	}
	if (!notesNSets.isEmpty())
	{
		assert(!m_Doc->notesList().isEmpty());
		auto end = notesNSets.end();
		for (auto it = notesNSets.begin(); it != end; ++it)
		{
			TextNote* note = it.key();
			assert(note != nullptr);
			QString nsLabel = it.value();
			NotesStyle* ns = m_Doc->getNotesStyle(nsLabel);
			if (ns != nullptr)
				note->setNotesStyle(ns);
			if (note->notesStyle() == nullptr)
			{
				qWarning() << "Scribus171Format::updateNames2Ptr()  : cannot find notes style ("<<nsLabel <<") for note - note will be deleted";
				m_Doc->deleteNote(note);
				continue;
			}
		}
		notesNSets.clear();
	}
}
