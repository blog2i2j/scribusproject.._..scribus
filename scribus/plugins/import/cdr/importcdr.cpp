/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/

#include <QByteArray>
#include <QCursor>
#include <QDrag>
#include <QFile>
#include <QList>
#include <QMessageBox>
#include <QMimeData>
#include <QStack>
#include <QDebug>

#include <cstdlib>
#include "importcdr.h"
#include "../revenge/rawpainter.h"
#include <libcdr/libcdr.h>

#include "commonstrings.h"
#include "loadsaveplugin.h"
#include "ui/multiprogressdialog.h"
#include "prefsmanager.h"
#include "scconfig.h"
#include "scmimedata.h"
#include "scpattern.h"
#include "scribus.h"
#include "scribusXml.h"
#include "scribuscore.h"
#include "scribusview.h"
#include "selection.h"
#include "undomanager.h"

CdrPlug::CdrPlug(ScribusDoc* doc, int flags)
	   : interactive(flags & LoadSavePlugin::lfInteractive),
	     importerFlags(flags),
	     m_Doc(doc)
{
	tmpSel = new Selection(this, false);
}

QImage CdrPlug::readThumbnail(const QString& fName)
{
	QFileInfo fi(fName);
	docWidth = PrefsManager::instance().appPrefs.docSetupPrefs.pageWidth;
	docHeight = PrefsManager::instance().appPrefs.docSetupPrefs.pageHeight;
	progressDialog = nullptr;
	m_Doc = new ScribusDoc();
	m_Doc->setup(0, 1, 1, 1, 1, "Custom", "Custom");
	m_Doc->setPage(docWidth, docHeight, 0, 0, 0, 0, 0, 0, false, false);
	m_Doc->addPage(0);
	m_Doc->setGUI(false, ScCore->primaryMainWindow(), nullptr);
	baseX = m_Doc->currentPage()->xOffset();
	baseY = m_Doc->currentPage()->yOffset();
	Elements.clear();
	m_Doc->setLoading(true);
	m_Doc->DoDrawing = false;
	m_Doc->scMW()->setScriptRunning(true);
	QString CurDirP = QDir::currentPath();
	QDir::setCurrent(fi.path());
	if (convert(fName))
	{
		tmpSel->clear();
		QDir::setCurrent(CurDirP);
		if (Elements.count() > 1)
			m_Doc->groupObjectsList(Elements);
		m_Doc->DoDrawing = true;
		m_Doc->m_Selection->delaySignalsOn();
		QImage tmpImage;
		if (Elements.count() > 0)
		{
			for (int dre=0; dre<Elements.count(); ++dre)
			{
				tmpSel->addItem(Elements.at(dre), true);
			}
			tmpSel->setGroupRect();
			double xs = tmpSel->width();
			double ys = tmpSel->height();
			tmpImage = Elements.at(0)->DrawObj_toImage(500);
			tmpImage.setText("XSize", QString("%1").arg(xs));
			tmpImage.setText("YSize", QString("%1").arg(ys));
		}
		m_Doc->scMW()->setScriptRunning(false);
		m_Doc->setLoading(false);
		m_Doc->m_Selection->delaySignalsOff();
		delete m_Doc;
		return tmpImage;
	}
	QDir::setCurrent(CurDirP);
	m_Doc->DoDrawing = true;
	m_Doc->scMW()->setScriptRunning(false);
	delete m_Doc;
	return QImage();
}

bool CdrPlug::import(const QString& fNameIn, const TransactionSettings& trSettings, int flags, bool showProgress)
{
	bool success = false;
	interactive = (flags & LoadSavePlugin::lfInteractive);
	importerFlags = flags;
	cancel = false;
	double b, h;
	bool ret = false;
	QFileInfo fi(fNameIn);
	if (!ScCore->usingGUI())
	{
		interactive = false;
		showProgress = false;
	}
	if (showProgress)
	{
		ScribusMainWindow* mw = (m_Doc == nullptr) ? ScCore->primaryMainWindow() : m_Doc->scMW();
		progressDialog = new MultiProgressDialog( tr("Importing: %1").arg(fi.fileName()), CommonStrings::tr_Cancel, mw );
		QStringList barNames, barTexts;
		barNames << "GI";
		barTexts << tr("Analyzing File:");
		QList<bool> barsNumeric;
		barsNumeric << false;
		progressDialog->addExtraProgressBars(barNames, barTexts, barsNumeric);
		progressDialog->setOverallTotalSteps(3);
		progressDialog->setOverallProgress(0);
		progressDialog->setProgress("GI", 0);
		progressDialog->show();
		connect(progressDialog, SIGNAL(canceled()), this, SLOT(cancelRequested()));
		QCoreApplication::processEvents();
	}
	else
		progressDialog = nullptr;
/* Set default Page to size defined in Preferences */
	b = 0.0;
	h = 0.0;
	if (progressDialog)
	{
		progressDialog->setOverallProgress(1);
		QCoreApplication::processEvents();
	}
	if (b == 0.0)
		b = PrefsManager::instance().appPrefs.docSetupPrefs.pageWidth;
	if (h == 0.0)
		h = PrefsManager::instance().appPrefs.docSetupPrefs.pageHeight;
	docWidth = b;
	docHeight = h;
	baseX = 0;
	baseY = 0;
	if (!interactive || (flags & LoadSavePlugin::lfInsertPage))
	{
		m_Doc->setPage(docWidth, docHeight, 0, 0, 0, 0, 0, 0, false, false);
		m_Doc->addPage(0);
		m_Doc->view()->addPage(0, true);
		baseX = 0;
		baseY = 0;
	}
	else if (!m_Doc || (flags & LoadSavePlugin::lfCreateDoc))
	{
		m_Doc = ScCore->primaryMainWindow()->doFileNew(docWidth, docHeight, 0, 0, 0, 0, 0, 0, false, false, 0, false, 0, 1, "Custom", true);
		ScCore->primaryMainWindow()->HaveNewDoc();
		ret = true;
		baseX = 0;
		baseY = 0;
		baseX = m_Doc->currentPage()->xOffset();
		baseY = m_Doc->currentPage()->yOffset();
	}
	if (!ret && interactive)
	{
		baseX = m_Doc->currentPage()->xOffset();
		baseY = m_Doc->currentPage()->yOffset();
	}
	if (ret || !interactive)
	{
		if (docWidth > docHeight)
			m_Doc->setPageOrientation(1);
		else
			m_Doc->setPageOrientation(0);
		m_Doc->setPageSize("Custom");
	}
	if ((!(flags & LoadSavePlugin::lfLoadAsPattern)) && (m_Doc->view() != nullptr))
		m_Doc->view()->deselectItems();
	Elements.clear();
	m_Doc->setLoading(true);
	m_Doc->DoDrawing = false;
	if ((!(flags & LoadSavePlugin::lfLoadAsPattern)) && (m_Doc->view() != nullptr))
		m_Doc->view()->updatesOn(false);
	m_Doc->scMW()->setScriptRunning(true);
	QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	QString CurDirP = QDir::currentPath();
	QDir::setCurrent(fi.path());
	if (convert(fNameIn))
	{
		tmpSel->clear();
		QDir::setCurrent(CurDirP);
		if ((Elements.count() > 1) && (!(importerFlags & LoadSavePlugin::lfCreateDoc)))
			m_Doc->groupObjectsList(Elements);
		m_Doc->DoDrawing = true;
		m_Doc->scMW()->setScriptRunning(false);
		m_Doc->setLoading(false);
		QGuiApplication::changeOverrideCursor(QCursor(Qt::ArrowCursor));
		if ((Elements.count() > 0) && (!ret) && (interactive))
		{
			if (flags & LoadSavePlugin::lfScripted)
			{
				bool loadF = m_Doc->isLoading();
				m_Doc->setLoading(false);
				m_Doc->changed();
				m_Doc->setLoading(loadF);
				if (!(flags & LoadSavePlugin::lfLoadAsPattern))
				{
					m_Doc->m_Selection->delaySignalsOn();
					for (int dre=0; dre<Elements.count(); ++dre)
					{
						m_Doc->m_Selection->addItem(Elements.at(dre), true);
					}
					m_Doc->m_Selection->delaySignalsOff();
					m_Doc->m_Selection->setGroupRect();
					if (m_Doc->view() != nullptr)
						m_Doc->view()->updatesOn(true);
				}
			}
			else
			{
				m_Doc->DragP = true;
				m_Doc->DraggedElem = nullptr;
				m_Doc->DragElements.clear();
				m_Doc->m_Selection->delaySignalsOn();
				for (int dre=0; dre<Elements.count(); ++dre)
				{
					tmpSel->addItem(Elements.at(dre), true);
				}
				tmpSel->setGroupRect();
				ScElemMimeData* md = ScriXmlDoc::writeToMimeData(m_Doc, tmpSel);
				m_Doc->itemSelection_DeleteItem(tmpSel);
				m_Doc->view()->updatesOn(true);
				if (importedPatterns.count() != 0)
				{
					for (int cd = 0; cd < importedPatterns.count(); cd++)
					{
						m_Doc->docPatterns.remove(importedPatterns[cd]);
					}
				}
				if (importedColors.count() != 0)
				{
					for (int cd = 0; cd < importedColors.count(); cd++)
					{
						m_Doc->PageColors.remove(importedColors[cd]);
					}
				}
				m_Doc->m_Selection->delaySignalsOff();
				// We must copy the TransationSettings object as it is owned
				// by handleObjectImport method afterwards
				TransactionSettings* transacSettings = new TransactionSettings(trSettings);
				m_Doc->view()->handleObjectImport(md, transacSettings);
				m_Doc->DragP = false;
				m_Doc->DraggedElem = nullptr;
				m_Doc->DragElements.clear();
			}
		}
		else
		{
			m_Doc->changed();
			m_Doc->reformPages();
			if (!(flags & LoadSavePlugin::lfLoadAsPattern))
				m_Doc->view()->updatesOn(true);
		}
		success = true;
	}
	else
	{
		QDir::setCurrent(CurDirP);
		m_Doc->DoDrawing = true;
		m_Doc->scMW()->setScriptRunning(false);
		m_Doc->view()->updatesOn(true);
		QGuiApplication::changeOverrideCursor(QCursor(Qt::ArrowCursor));
	}
	if (interactive)
		m_Doc->setLoading(false);
	//CB If we have a gui we must refresh it if we have used the progressbar
	if (!(flags & LoadSavePlugin::lfLoadAsPattern))
	{
		if (showProgress && !interactive)
			m_Doc->view()->DrawNew();
	}
	QGuiApplication::restoreOverrideCursor();
	return success;
}

CdrPlug::~CdrPlug()
{
	delete progressDialog;
	delete tmpSel;
}

bool CdrPlug::convert(const QString& fn)
{
	importedColors.clear();
	importedPatterns.clear();

	if (!QFile::exists(fn))
	{
		qDebug() << "File " << QFile::encodeName(fn).data() << " does not exist";
		return false;
	}
	QFileInfo fi(fn);
	QString ext = fi.suffix().toLower();
#if HAVE_REVENGE
	librevenge::RVNGFileStream input(QFile::encodeName(fn).data());
#else
	WPXFileStream input(QFile::encodeName(fn).data());
#endif
	bool fail = false;
	if (ext == "cdr")
	{
		if (!libcdr::CDRDocument::isSupported(&input))
		{
			qDebug() << "ERROR: Unsupported file format!";
			fail = true;
		}
		if (!fail)
		{
			RawPainter painter(m_Doc, baseX, baseY, docWidth, docHeight, importerFlags, &Elements, &importedColors, &importedPatterns, tmpSel, "cdr");
			if (!libcdr::CDRDocument::parse(&input, &painter))
				fail = true;
		}
		if (fail)
		{
			qDebug() << "ERROR: Parsing failed!";
			if (progressDialog)
				progressDialog->close();
		/*	if (importerFlags & LoadSavePlugin::lfCreateDoc)
			{
				ScribusMainWindow* mw=(m_Doc==0) ? ScCore->primaryMainWindow() : m_Doc->scMW();
				qApp->changeOverrideCursor(QCursor(Qt::ArrowCursor));
				ScMessageBox::warning(mw, CommonStrings::trWarning, tr("Parsing failed!\n\nPlease submit your file (if possible) to the\nDocument Liberation Project https://www.documentliberation.org"));
				qApp->changeOverrideCursor(QCursor(Qt::WaitCursor));
			}*/
			return false;
		}
	}
	else if (ext == "cmx")
	{
		if (!libcdr::CMXDocument::isSupported(&input))
		{
			qDebug() << "ERROR: Unsupported file format!";
			return false;
		}
		RawPainter painter(m_Doc, baseX, baseY, docWidth, docHeight, importerFlags, &Elements, &importedColors, &importedPatterns, tmpSel, "cmx");
		if (!libcdr::CMXDocument::parse(&input, &painter))
		{
			qDebug() << "ERROR: Parsing failed!";
			if (progressDialog)
				progressDialog->close();
			if (importerFlags & LoadSavePlugin::lfCreateDoc)
			{
				ScribusMainWindow* mw = (m_Doc == nullptr) ? ScCore->primaryMainWindow() : m_Doc->scMW();
				QGuiApplication::changeOverrideCursor(QCursor(Qt::ArrowCursor));
				ScMessageBox::warning(mw, CommonStrings::trWarning, tr("Parsing failed!\n\nPlease submit your file (if possible) to the\nDocument Liberation Project https://www.documentliberation.org"));
				QGuiApplication::changeOverrideCursor(QCursor(Qt::WaitCursor));
			}
			return false;
		}
	}
	else
		return false;
	if (Elements.count() == 0)
	{
		if (importedColors.count() != 0)
		{
			for (int cd = 0; cd < importedColors.count(); cd++)
			{
				m_Doc->PageColors.remove(importedColors[cd]);
			}
		}
		if (importedPatterns.count() != 0)
		{
			for (int cd = 0; cd < importedPatterns.count(); cd++)
			{
				m_Doc->docPatterns.remove(importedPatterns[cd]);
			}
		}
	}
	if (progressDialog)
		progressDialog->close();
	return true;
}
