/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/
#include "scriptercore.h"

#include <cstdlib>

#include <QApplication>
#include <QByteArray>
#include <QGlobalStatic>
#include <QMessageBox>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "cmdutil.h"
#include "pconsole.h"
#include "prefscontext.h"
#include "prefsfile.h"
#include "prefsmanager.h"
#include "prefstable.h"
#include "runscriptdialog.h"
#include "scpaths.h"
#include "scraction.h"
#include "scribusapp.h" // need it to access ScQApp->pythonScript & ScQApp->pythonScriptArgs
#include "scribuscore.h"
#include "scribusdoc.h"
#include "scribusview.h"
#include "selection.h"
#include "ui/contentpalette.h" //TODO Move the calls to this to a signal
#include "ui/helpbrowser.h"
#include "ui/layers.h" //TODO Move the calls to this to a signal
#include "ui/marksmanager.h"
#include "ui/notesstyleseditor.h"
#include "ui/outlinepalette.h" //TODO Move the calls to this to a signal
#include "ui/pagepalette.h" //TODO Move the calls to this to a signal
#include "ui/propertiespalette.h" //TODO Move the calls to this to a signal
#include "ui/scmessagebox.h"
#include "ui/scmwmenumanager.h"

ScripterCore::ScripterCore(QWidget* parent)
{
	m_pyConsole = new PythonConsole(parent);
	returnString = "init";

	m_scripterActions.insert("scripterExecuteScript", new ScrAction(QObject::tr("&Execute Script..."), QKeySequence(), this));
	m_scripterActions.insert("scripterShowConsole", new ScrAction(QObject::tr("Show &Console"), QKeySequence(), this));
	m_scripterActions.insert("scripterAboutScript", new ScrAction(QObject::tr("&About Script..."), QKeySequence(), this));

	m_scripterActions["scripterExecuteScript"]->setMenuRole(QAction::NoRole);
	m_scripterActions["scripterShowConsole"]->setMenuRole(QAction::NoRole);
	m_scripterActions["scripterAboutScript"]->setMenuRole(QAction::NoRole);

	m_scripterActions["scripterShowConsole"]->setToggleAction(true);
	m_scripterActions["scripterShowConsole"]->setChecked(false);

	QObject::connect( m_scripterActions["scripterExecuteScript"], SIGNAL(triggered()) , this, SLOT(runScriptDialog()) );
	QObject::connect( m_scripterActions["scripterShowConsole"], SIGNAL(toggled(bool)) , this, SLOT(slotInteractiveScript(bool)) );
	QObject::connect( m_scripterActions["scripterAboutScript"], SIGNAL(triggered()) , this, SLOT(aboutScript()) );

	m_savedRecentScripts.clear();
	readPlugPrefs();

	QObject::connect(m_pyConsole, SIGNAL(runCommand()), this, SLOT(slotExecute()));
	QObject::connect(m_pyConsole, SIGNAL(paletteShown(bool)), this, SLOT(slotInteractiveScript(bool)));

	QObject::connect(ScQApp, SIGNAL(appStarted()) , this, SLOT(runStartupScript()) );
	QObject::connect(ScQApp, SIGNAL(appStarted()) , this, SLOT(slotRunPythonScript()) );

	QObject::connect(&scriptPaths, &ScriptPaths::runScriptFile, this, &ScripterCore::runScriptFile);
}

ScripterCore::~ScripterCore()
{
	savePlugPrefs();
	delete m_pyConsole;
}

void ScripterCore::addToMainWindowMenu(ScribusMainWindow *mw)
{
	m_menuMgr = mw->scrMenuMgr;
	m_menuMgr->createMenu("Scripter", QObject::tr("&Script"));
	scriptPaths.attachToMenu(m_menuMgr);
	m_menuMgr->createMenu("ScribusScripts", QObject::tr("&Scribus Scripts"), "Scripter");
	m_menuMgr->addMenuItemString("ScribusScripts", "Scripter");
	m_menuMgr->addMenuItemString("scripterExecuteScript", "Scripter");
	m_menuMgr->createMenu("RecentScripts", QObject::tr("&Recent Scripts"), "Scripter", false, true);
	m_menuMgr->addMenuItemString("RecentScripts", "Scripter");
	m_menuMgr->addMenuItemString("scripterExecuteScript", "Scripter");
	m_menuMgr->addMenuItemString("SEPARATOR", "Scripter");
	m_menuMgr->addMenuItemString("scripterShowConsole", "Scripter");
	m_menuMgr->addMenuItemString("scripterAboutScript", "Scripter");

	buildScribusScriptsMenu();

	m_menuMgr->addMenuStringToMenuBarBefore("Scripter", "Windows");
	m_menuMgr->addMenuItemStringsToMenuBar("Scripter", m_scripterActions);
	m_recentScripts = m_savedRecentScripts;
	rebuildRecentScriptsMenu();
	scriptPaths.buildMenu();
}

void ScripterCore::enableMainWindowMenu()
{
	if (!m_menuMgr)
		return;
	m_menuMgr->setMenuEnabled("ScribusScripts", true);
	m_menuMgr->setMenuEnabled("RecentScripts", true);
	m_scripterActions["scripterExecuteScript"]->setEnabled(true);
}

void ScripterCore::disableMainWindowMenu()
{
	if (!m_menuMgr)
		return;
	m_menuMgr->setMenuEnabled("ScribusScripts", false);
	m_menuMgr->setMenuEnabled("RecentScripts", false);
	m_scripterActions["scripterExecuteScript"]->setEnabled(false);
}

void ScripterCore::buildScribusScriptsMenu()
{
	QString pfad = ScPaths::instance().scriptDir();
	QString pfad2 = QDir::toNativeSeparators(pfad);
	QDir ds(pfad2, "*.py", QDir::Name | QDir::IgnoreCase, QDir::Files | QDir::NoSymLinks);
	if ((!ds.exists()) || (ds.count() == 0))
		return;

	for (uint dc = 0; dc < ds.count(); ++dc)
	{
		QFileInfo fs(ds[dc]);
		QString strippedName = fs.baseName();
		m_scripterActions.insert(strippedName, new ScrAction(ScrAction::RecentScript, strippedName, QKeySequence(), this, strippedName));
		connect(m_scripterActions[strippedName], SIGNAL(triggeredData(QString)), this, SLOT(StdScript(QString)));
		m_menuMgr->addMenuItemString(strippedName, "ScribusScripts");
	}
}

void ScripterCore::rebuildRecentScriptsMenu()
{
	m_menuMgr->clearMenuStrings("RecentScripts");
	m_recentScriptActions.clear();
	int max = qMin(PrefsManager::instance().appPrefs.uiPrefs.recentDocCount, m_recentScripts.count());
	for (int m = 0; m < max; ++m)
	{
		QString strippedName(m_recentScripts[m]);
		strippedName.remove(QDir::separator());
		m_recentScriptActions.insert(strippedName, new ScrAction(ScrAction::RecentScript, m_recentScripts[m], QKeySequence(), this, m_recentScripts[m]));
		connect(m_recentScriptActions[strippedName], SIGNAL(triggeredData(QString)), this, SLOT(RecentScript(QString)));
		m_menuMgr->addMenuItemString(strippedName, "RecentScripts");
	}
	m_menuMgr->addMenuItemStringsToRememberedMenu("RecentScripts", m_recentScriptActions);
}

void ScripterCore::finishScriptRun()
{
	ScribusMainWindow* mainWin = ScCore->primaryMainWindow();
	if (!mainWin->HaveDoc)
		return;

	mainWin->propertiesPalette->setDoc(mainWin->doc);
	mainWin->contentPalette->setDoc(mainWin->doc);
	mainWin->marksManager->setDoc(mainWin->doc);
	mainWin->nsEditor->setDoc(mainWin->doc);
	mainWin->layerPalette->setDoc(mainWin->doc);
	mainWin->outlinePalette->setDoc(mainWin->doc);
	mainWin->outlinePalette->BuildTree();
	mainWin->pagePalette->setView(mainWin->view);
	mainWin->pagePalette->rebuild();
	mainWin->doc->RePos = false;
	if (mainWin->doc->m_Selection->count() != 0)
		mainWin->doc->m_Selection->itemAt(0)->emitAllToGUI();
	mainWin->HaveNewSel();
	mainWin->view->DrawNew();
	//CB Really only need (want?) this for new docs, but we need it after a call to ScMW doFileNew.
	//We don't want it in cmddoc calls as itll interact with the GUI before a script may be finished.
	mainWin->HaveNewDoc();
}

/**
 * setup the environment for running the script, run the script and clean up the environment
 */
void ScripterCore::runScriptFile(const QString& path)
{
	slotRunScriptFile(path);
	finishScriptRun();
}

void ScripterCore::runScriptDialog()
{
	RunScriptDialog dia( ScCore->primaryMainWindow(), m_enableExtPython );
	if (dia.exec())
	{
		QString fileName(dia.selectedFile());
		slotRunScriptFile(fileName, dia.extensionRequested());

		if (m_recentScripts.indexOf(fileName) == -1)
			m_recentScripts.prepend(fileName);
		else
		{
			m_recentScripts.removeAll(fileName);
			m_recentScripts.prepend(fileName);
		}
		rebuildRecentScriptsMenu();
	}
	finishScriptRun();
}

void ScripterCore::StdScript(const QString& baseFilename)
{
	QString pfad = ScPaths::instance().scriptDir();
	QString pfad2 = QDir::toNativeSeparators(pfad);
	QString fn = pfad2 + baseFilename + ".py";
	QFileInfo fd(fn);
	if (!fd.exists())
		return;
	slotRunScriptFile(fn);
	finishScriptRun();
}

void ScripterCore::RecentScript(const QString& fileName)
{
	if (QFileInfo::exists(fileName))
	{
		m_recentScripts.removeAll(fileName);
		rebuildRecentScriptsMenu();
		return;
	}
	slotRunScriptFile(fileName);
	finishScriptRun();
}

void ScripterCore::slotRunScriptFile(const QString& fileName, bool inMainInterpreter)
{
	slotRunScriptFile(fileName, QStringList(), inMainInterpreter);
}

void ScripterCore::slotRunScriptFile(const QString& fileName, QStringList arguments, bool inMainInterpreter)
/** run "filename" python script with the additional arguments provided in "arguments" */
{
	// Prevent two scripts to be run concurrently or face crash!
	if (ScCore->primaryMainWindow()->scriptIsRunning())
		return;
	disableMainWindowMenu();

	PyThreadState *state = nullptr;
	QFileInfo fi(fileName);
	QByteArray na = fi.fileName().toLocal8Bit();
	// Set up a sub-interpreter if needed:
	PyThreadState* global_state = nullptr;
	if (!inMainInterpreter)
	{
		ScCore->primaryMainWindow()->propertiesPalette->unsetDoc();
		ScCore->primaryMainWindow()->contentPalette->unsetDoc();
		ScCore->primaryMainWindow()->pagePalette->setView(nullptr);
		ScCore->primaryMainWindow()->setScriptRunning(true);
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		// Create the sub-interpreter
		// FIXME: This calls abort() in a Python debug build. We're doing something wrong.
		//stateo = PyEval_SaveThread();
		global_state = PyThreadState_Get();
		state = Py_NewInterpreter();
		// Init the scripter module in the sub-interpreter
		//initscribus(ScCore->primaryMainWindow());
	}

	// Make sure sys.argv[0] is the path to the script
	arguments.prepend(na.data());
	//convert arguments (QListString) to char** for Python bridge
	/* typically arguments == ['path/to/script.py','--argument1','valueforarg1','--flag']*/
	wchar_t **comm = new wchar_t*[arguments.size()];
	for (int i = 0; i < arguments.size(); i++)
	{
		const QString& argStr = arguments.at(i);
		comm[i] = new wchar_t[argStr.size() + 1]; //+1 to allow adding '\0'. may be useless, don't know how to check.
		comm[i][argStr.size()] = 0;
		argStr.toWCharArray(comm[i]);
	}
	PySys_SetArgv(arguments.size(), comm);

	for (int i = 0; i < arguments.size(); i++)
		delete[] comm[i];
	delete[] comm;
	
	// call python script
	PyObject* m = PyImport_AddModule("__main__");
	if (m == nullptr)
		qDebug("Failed to get __main__ - aborting script");
	else
	{
		// Path separators need to be escaped on Windows
		QString escapedAbsPath  = QDir::toNativeSeparators(fi.absolutePath()).replace("\\", "\\\\");
		QString escapedAbsFilePath  = QDir::toNativeSeparators(fi.absoluteFilePath()).replace("\\", "\\\\");
		QString escapedFileName = QDir::toNativeSeparators(fileName).replace("\\", "\\\\");
		// FIXME: If filename contains chars outside 7bit ascii, might be problems
		PyObject* globals = PyModule_GetDict(m);
		// Build the Python code to run the script
		//QString cm = QString("from __future__ import division\n"); removed due #5252 PV
		QString cm = QString("import sys\n");
		cm        += QString("import io\n");
		/* Implementation of the help() in pydoc.py reads some OS variables
		 * for output settings. I use ugly hack to stop freezing calling help()
		 * in script. pv. */
		cm        += QString("import os\nos.environ['PAGER'] = '/bin/false'\n"); // HACK
		cm        += QString("sys.path[0] = \"%1\"\n").arg(escapedAbsPath);
		// Replace sys.stdin with a dummy StringIO that always returns
		// "" for read
		cm        += QString("sys.stdin = io.StringIO()\n");
		// Provide script path to the interpreter
		cm        += QString("__file__ = \"%1\"\n").arg(escapedAbsFilePath);
		// tell the script if it's running in the main interpreter or a subinterpreter
		cm        += QString("import scribus\n");
		if (inMainInterpreter)
			cm+= QString("scribus.mainInterpreter = True\n");
		else
			cm+= QString("scribus.mainInterpreter = False\n");
		cm        += QString("try:\n");
		cm        += QString("    exec(open(\"%1\", \"rb\").read())\n").arg(escapedFileName);
		cm        += QString("except SystemExit:\n");
		cm        += QString("    pass\n");
		// Capture the text of any other exception that's raised by the interpreter
		// into a StringIO buffer for later extraction.
		cm        += QString("except:\n");
		cm        += QString("    import traceback\n");
		cm        += QString("    _errorMsg = traceback.format_exc()\n");
		if (!ScCore->usingGUI())
			cm += QString("    traceback.print_exc()\n");
		// We re-raise the exception so the return value of PyRun_StringFlags reflects
		// the fact that an exception has occurred.
		cm        += QString("    raise\n");
		// FIXME: if cmd contains chars outside 7bit ascii, might be problems
		QByteArray cmd = cm.toUtf8();
		// Now run the script in the interpreter's global scope. It'll run in a
		// sub-interpreter if we created and switched to one earlier, otherwise
		// it'll run in the main interpreter.
		PyObject* result = PyRun_String(cmd.data(), Py_file_input, globals, globals);
		// nullptr is returned if an exception is set. We don't care about any
		// other return value (most likely None anyway) and can ignore it.
		if (result == nullptr)
		{
			PyObject* errorMsgPyStr = PyMapping_GetItemString(globals, "_errorMsg");
			if (errorMsgPyStr == nullptr)
			{
				// It's rather unlikely that this will ever be reached - to get here
				// we'd have to fail to retrieve the string we just created.
				qDebug("Error retrieving error message content after script exception!");
				qDebug("Exception was:");
				PyErr_Print();
			}
			else if (ScCore->usingGUI())
			{
				QString errorMsg = PyUnicode_asQString(errorMsgPyStr);
				// Display a dialog to the user with the exception
				QClipboard *cp = QApplication::clipboard();
				cp->setText(errorMsg);
				ScCore->closeSplash();
				QApplication::changeOverrideCursor(QCursor(Qt::ArrowCursor));
				ScMessageBox::warning(ScCore->primaryMainWindow(),
									tr("Script error"),
									"<qt><p>"
									+ tr("If you are running an official script report it at <a href=\"https://bugs.scribus.net\">bugs.scribus.net</a> please.")
									+ "</p><pre>" + errorMsg.toHtmlEscaped() + "</pre><p>"
									+ tr("This message is in your clipboard too. Use Ctrl+V to paste it into bug tracker.")
									+ "</p></qt>");
			}
			// We've already processed the exception text, so clear the exception
			PyErr_Clear();
		} // end if result == nullptr
		// Because 'result' may be nullptr, not a PyObject*, we must call PyXDECREF not Py_DECREF
		Py_XDECREF(result);
	} // end if m == nullptr
	if (!inMainInterpreter)
	{
		Py_EndInterpreter(state);
		PyThreadState_Swap(global_state);
		//PyEval_RestoreThread(stateo);
		QApplication::restoreOverrideCursor();
		ScCore->primaryMainWindow()->setScriptRunning(false);
	}

	enableMainWindowMenu();
}

// needed for running script from CLI
void ScripterCore::slotRunPythonScript()
{
	if (ScQApp->pythonScript.isNull())
		return;
	slotRunScriptFile(ScQApp->pythonScript, ScQApp->pythonScriptArgs, true);
	finishScriptRun();
}

void ScripterCore::slotRunScript(const QString& Script)
{
	// Prevent two scripts to be run concurrently or face crash!
	if (ScCore->primaryMainWindow()->scriptIsRunning())
		return;
	disableMainWindowMenu();

	ScCore->primaryMainWindow()->propertiesPalette->unsetDoc();
	ScCore->primaryMainWindow()->contentPalette->unsetDoc();
	ScCore->primaryMainWindow()->pagePalette->setView(nullptr);
	ScCore->primaryMainWindow()->setScriptRunning(true);
	inValue = Script;
	QString cm("# -*- coding: utf8 -*- \n");
	if (PyThreadState_Get() != nullptr)
	{
		//initscribus(ScCore->primaryMainWindow());
		/* HACK: following loop handles all input line by line.
		It *should* use I.C. because of docstrings etc. I.I. cannot
		handle docstrings right.
		Calling all code in one command:
		ia = code.InteractiveInterpreter() ia.runsource(getval())
		works fine in plain Python. Not here. WTF? */
		cm += (
				"try:\n"
				"    import io\n"
				"    scribus._bu = io.StringIO()\n"
				"    sys.stdout = scribus._bu\n"
				"    sys.stderr = scribus._bu\n"
				"    sys.argv = ['scribus']\n" // this is the PySys_SetArgv replacement
				"    scribus.mainInterpreter = True\n" // the scripter console runs everything in the main interpreter
				"    for scribus._i_str in scribus.getval().splitlines():\n"
				"        scribus._ia.push(scribus._i_str)\n"
				"    scribus.retval(scribus._bu.getvalue())\n"
				"    sys.stdout = sys.__stdout__\n"
				"    sys.stderr = sys.__stderr__\n"
				"except SystemExit:\n"
				"    print ('Catched SystemExit - it is not good for Scribus')\n"
				"except KeyboardInterrupt:\n"
				"    print ('Catched KeyboardInterrupt - it is not good for Scribus')\n"
			  );
	}
	// Set up sys.argv
	/* PV - WARNING: THIS IS EVIL! This code summons a crash - see
	bug #3510. I don't know why as the Python C API is a little
	bit magic for me. It looks like it replaces the cm QString or what...
	"In file tools/qgarray.cpp, line 147: Out of memory"
	Anyway - sys.argv is set above
	char* comm[1];
	comm[0] = const_cast<char*>("scribus");
	PySys_SetArgv(1, comm); */
	// then run the code
	PyObject* m = PyImport_AddModule("__main__");
	if (m == nullptr)
		qDebug("Failed to get __main__ - aborting script");
	else
	{
		PyObject* globals = PyModule_GetDict(m);
		PyObject* result = PyRun_String(cm.toUtf8().data(), Py_file_input, globals, globals);
		if (result == nullptr)
		{
			PyErr_Print();
			ScMessageBox::warning(ScCore->primaryMainWindow(), tr("Script error"),
					"<qt>" + tr("There was an internal error while trying the "
					   "command you entered. Details were printed to "
					   "stderr. ") + "</qt>");
		}
		else
		// Because 'result' may be nullptr, not a PyObject*, we must call PyXDECREF not Py_DECREF
			Py_XDECREF(result);
	}
	ScCore->primaryMainWindow()->setScriptRunning(false);

	enableMainWindowMenu();
}

void ScripterCore::slotInteractiveScript(bool visible)
{
	QObject::disconnect( m_scripterActions["scripterShowConsole"], SIGNAL(toggled(bool)) , this, SLOT(slotInteractiveScript(bool)) );

	m_scripterActions["scripterShowConsole"]->setChecked(visible);
	m_pyConsole->setFonts();
	m_pyConsole->setVisible(visible);

	QObject::connect( m_scripterActions["scripterShowConsole"], SIGNAL(toggled(bool)) , this, SLOT(slotInteractiveScript(bool)) );
}

void ScripterCore::slotExecute()
{
	slotRunScript(m_pyConsole->command());
	m_pyConsole->outputEdit->append(returnString);
	m_pyConsole->commandEdit->ensureCursorVisible();
	finishScriptRun();
}

void ScripterCore::readPlugPrefs()
{
	PrefsContext* prefs = PrefsManager::instance().prefsFile->getPluginContext("scriptplugin");
	if (!prefs)
	{
		qDebug("scriptplugin: Unable to load prefs");
		return;
	}
	PrefsTable* prefRecentScripts = prefs->getTable("recentscripts");
	if (!prefRecentScripts)
	{
		qDebug("scriptplugin: Unable to get recent scripts");
		return;
	}
	// Load recent scripts from the prefs
	for (int i = 0; i < prefRecentScripts->getRowCount(); i++)
	{
		QString rs(prefRecentScripts->get(i,0));
		m_savedRecentScripts.append(rs);
	}
	// then get more general preferences
	m_enableExtPython = prefs->getBool("extensionscripts",false);
	m_importAllNames = prefs->getBool("importall",true);
	m_startupScript = prefs->get("startupscript", QString());
	// and have the console window set up its position
}

void ScripterCore::savePlugPrefs()
{
	PrefsContext* prefs = PrefsManager::instance().prefsFile->getPluginContext("scriptplugin");
	if (!prefs)
	{
		qDebug("scriptplugin: Unable to load prefs");
		return;
	}
	PrefsTable* prefRecentScripts = prefs->getTable("recentscripts");
	if (!prefRecentScripts)
	{
		qDebug("scriptplugin: Unable to get recent scripts");
		return;
	}
	for (int i = 0; i < m_recentScripts.count(); i++)
	{
		prefRecentScripts->set(i, 0, m_recentScripts[i]);
	}
	// then save more general preferences
	prefs->set("extensionscripts", m_enableExtPython);
	prefs->set("importall", m_importAllNames);
	prefs->set("startupscript", m_startupScript);
}

void ScripterCore::aboutScript()
{
	QPair<QString, uint> fileNameVersion;
	fileNameVersion = ScCore->primaryMainWindow()->CFileDialog(".", tr("Examine Script"), tr("Python Scripts (*.py *.PY);;All Files (*)"), "", fdNone);
	if (fileNameVersion.first.isNull())
		return;
	QString html("<html><body>");
	QFileInfo fi(fileNameVersion.first);
	QFile input(fileNameVersion.first);
	if (!input.open(QIODevice::ReadOnly))
		return;
	QTextStream intputstream(&input);
	QString content = intputstream.readAll();
	QString docstring = content.section(R"(""")", 1, 1);
	if (!docstring.isEmpty())
	{
		html += QString("<h1>%1 %2</h1>").arg( tr("Documentation for:"), fi.fileName());
		html += QString("<p>%1</p>").arg(docstring.replace("\n\n", "<br><br>"));
	}
	else
	{
		html += QString("<p><b>%1 %2 %3</b></p>").arg( tr("Script"), fi.fileName(), tr(" doesn't contain any docstring!"));
		html += QString("<pre>%4</pre>").arg(content);
	}
	html += "</body></html>";
	input.close();
	HelpBrowser *dia = new HelpBrowser(nullptr, QObject::tr("About Script") + " " + fi.fileName(), "en");
	dia->setHtml(html);
	dia->show();
}

void ScripterCore::initExtensionScripts()
{
	// Nothing to do currently
}

void ScripterCore::runStartupScript()
{
	if (m_enableExtPython && !m_startupScript.isEmpty())
	{
		if (QFile::exists(this->m_startupScript))
		{
			// run the script in the main interpreter. The user will be informed
			// with a dialog if something has gone wrong.
			this->slotRunScriptFile(this->m_startupScript, true);
		}
		else
			ScMessageBox::warning(ScCore->primaryMainWindow(), tr("Startup Script error"),
					      tr("Could not find script: %1.").arg( m_startupScript));
	}
}

void ScripterCore::languageChange()
{
	m_scripterActions["scripterExecuteScript"]->setText(QObject::tr("&Execute Script..."));
	m_scripterActions["scripterShowConsole"]->setText(QObject::tr("Show &Console"));
	m_scripterActions["scripterAboutScript"]->setText(QObject::tr("&About Script..."));

	m_menuMgr->setText("Scripter", QObject::tr("&Script"));
	m_menuMgr->setText("ScribusScripts", QObject::tr("&Scribus Scripts"));
	m_menuMgr->setText("RecentScripts", QObject::tr("&Recent Scripts"));
}

bool ScripterCore::setupMainInterpreter()
{
	QString cm = QString(
		"# -*- coding: utf-8 -*-\n"
		"import scribus\n"
		"import sys\n"
		"import code\n"
		"sys.path.insert(0, \"%1\")\n"
		"import io\n"
		"sys.stdin = io.StringIO()\n"
		"scribus._ia = code.InteractiveConsole(globals())\n"
		).arg(ScPaths::instance().scriptDir());
	if (m_importAllNames)
		cm += "from scribus import *\n";
	QByteArray cmd = cm.toUtf8();
	if (PyRun_SimpleString(cmd.data()))
	{
		PyErr_Print();
		ScMessageBox::warning(ScCore->primaryMainWindow(), tr("Script error"),
				tr("Setting up the Python plugin failed. "
				   "Error details were printed to stderr. "));
		return false;
	}
	return true;
}

void ScripterCore::setStartupScript(const QString& newScript)
{
	m_startupScript = newScript;
}

void ScripterCore::setExtensionsEnabled(bool enable)
{
	m_enableExtPython = enable;
}

void ScripterCore::updateSyntaxHighlighter()
{
	m_pyConsole->updateSyntaxHighlighter();
}

const QString & ScripterCore::startupScript() const
{
	return m_startupScript;
}

bool ScripterCore::extensionsEnabled() const
{
	return m_enableExtPython;
}
