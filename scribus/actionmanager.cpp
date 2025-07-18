/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/
/***************************************************************************
	begin                : Apr 2005
	copyright            : (C) 2005 by Craig Bradney
	email                : cbradney@scribus.info
***************************************************************************/

/***************************************************************************
*                                                                         *
*   mainWindow program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include <array>

#include "actionmanager.h"

#include "appmodes.h"
#include "iconmanager.h"
#include "scribuscore.h"
#include "scribusdoc.h"
#include "scribusview.h"
#include "selection.h"
#include "undomanager.h"
#include "urllauncher.h"

QMap<QString, QKeySequence> ActionManager::defKeys;
QVector< QPair<QString, QStringList> > ActionManager::defMenuNames;
QVector< QPair<QString, QStringList> > ActionManager::defMenus;
QVector< QPair<QString, QStringList> > ActionManager::defNonMenuNames;
QVector< QPair<QString, QStringList> > ActionManager::defNonMenuActions;

ActionManager::ActionManager ( QObject * parent ) :
	QObject ( parent),
	im (IconManager::instance())
{
}

ActionManager::~ActionManager()
{
	while (!scrActions->isEmpty())
	{
		ScrAction *value = (*scrActions->begin());
		scrActions->erase(scrActions->begin());
		delete value;
 	}
	scrActions->clear();
	delete modeActionNames;
	delete nonEditActionNames;
	delete unicodeCharActionNames;
}

void ActionManager::init(ScribusMainWindow *mw)
{
	mainWindow = mw;
	scrActions = &(mainWindow->scrActions);
	scrActionGroups = &(mainWindow->scrActionGroups);
	modeActionNames = new QStringList();
	nonEditActionNames = new QStringList();
	unicodeCharActionNames = new QStringList();
	undoManager = UndoManager::instance();

#ifdef Q_OS_MACOS
	noIcon = im.loadPixmap("no-icon");
#endif

	createActions();
	createDefaultMenus();
	createDefaultNonMenuActions();
	languageChange();
	setActionTooltips(scrActions);
}

bool ActionManager::compareKeySeqToShortcut(const QKeySequence& ks, const QString& actionName)
{
	if (!scrActions->contains(actionName))
		return false;
	return ks.matches((*scrActions)[actionName]->shortcut()) == QKeySequence::ExactMatch;
}

bool ActionManager::compareKeySeqToShortcut(int k, Qt::KeyboardModifiers km, const QString& actionName)
{
	return compareKeySeqToShortcut(QKeySequence(k | km), actionName);
}

void ActionManager::createActions()
{
	initFileMenuActions();
	initEditMenuActions();
	initStyleMenuActions();
	initItemMenuActions();
	initInsertMenuActions();
	initPageMenuActions();
	initTableMenuActions();
	initViewMenuActions();
	initToolsMenuActions();
	initExtrasMenuActions();
	initWindowsMenuActions();
	initScriptMenuActions();
	initHelpMenuActions();
	initUnicodeActions(scrActions, mainWindow, unicodeCharActionNames);
	enableUnicodeActions(scrActions, false);
	initSpecialActions();
}

void ActionManager::initFileMenuActions()
{
	QString name;
	//File Menu
	name = "fileNew";
	scrActions->insert(name, new ScrAction("document-new", "document-new", "", defaultKey(name), mainWindow));
	name = "fileNewFromTemplate";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "fileOpen";
	scrActions->insert(name, new ScrAction("document-open", "document-open", "", defaultKey(name), mainWindow));
	name = "fileClose";
	scrActions->insert(name, new ScrAction("close", "close", "", defaultKey(name), mainWindow));
	name = "fileSave";
	scrActions->insert(name, new ScrAction("document-save", "document-save", "", defaultKey(name), mainWindow));
	name = "fileSaveAs";
	scrActions->insert(name, new ScrAction("document-save-as", "document-save-as", "", defaultKey(name), mainWindow));
	name = "fileRevert";
	scrActions->insert(name, new ScrAction("revert", QString(), "", defaultKey(name), mainWindow));
	name = "fileCollect";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	//File Import Menu
	name = "fileImportText";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "fileImportText2";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "fileImportAppendText";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "fileImportImage";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "fileImportVector";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));

	//File Export Menu
	name = "fileExportText";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "fileExportAsEPS";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "fileExportAsPDF";
	scrActions->insert(name, new ScrAction("pref-pdf-export", "pref-pdf-export", "", defaultKey(name), mainWindow));
	//Rest of File Menu
//	name = "fileDocSetup";
//	scrActions->insert(name, new ScrAction("document-properties", "document-properties", "", defaultKey(name), mainWindow));
	name = "fileDocSetup150";
	scrActions->insert(name, new ScrAction("document-properties", "document-properties", "", defaultKey(name), mainWindow));
//	name = "filePreferences";
//	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
//	(*scrActions)[name]->setMenuRole(QAction::NoRole);
	name = "filePreferences150";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)[name]->setMenuRole(QAction::PreferencesRole);
	name = "filePrint";
	scrActions->insert(name, new ScrAction("document-print", "document-print", "", defaultKey(name), mainWindow));
	name = "PrintPreview";
	scrActions->insert(name, new ScrAction("document-print-preview", "document-print-preview", "", defaultKey(name), mainWindow));
	name = "OutputPreviewPDF";
	scrActions->insert(name, new ScrAction(QString(), defaultKey(name), mainWindow));
	name = "OutputPreviewPS";
	scrActions->insert(name, new ScrAction(QString(), defaultKey(name), mainWindow));
	name = "fileQuit";
	scrActions->insert(name, new ScrAction("exit", QString(), "", defaultKey(name), mainWindow));
	(*scrActions)[name]->setMenuRole(QAction::QuitRole);

	//Connect our signals and slots
	//File Menu
	connect( (*scrActions)["fileNew"], SIGNAL(triggered()), mainWindow, SLOT(slotFileNew()) );
	connect( (*scrActions)["fileNewFromTemplate"], SIGNAL(triggered()), mainWindow, SLOT(newFileFromTemplate()) );
	connect( (*scrActions)["fileOpen"], SIGNAL(triggered()), mainWindow, SLOT(slotFileOpen()) );
	connect( (*scrActions)["fileClose"], SIGNAL(triggered()), mainWindow, SLOT(slotFileClose()) );
	connect( (*scrActions)["filePrint"], SIGNAL(triggered()), mainWindow, SLOT(slotFilePrint()) );
	connect( (*scrActions)["PrintPreview"], SIGNAL(triggered()), mainWindow, SLOT(printPreview()) );
	connect( (*scrActions)["OutputPreviewPDF"], SIGNAL(triggered()), mainWindow, SLOT(outputPreviewPDF()) );
	connect( (*scrActions)["OutputPreviewPS"], SIGNAL(triggered()), mainWindow, SLOT(outputPreviewPS()) );
	connect( (*scrActions)["fileSave"], SIGNAL(triggered()), mainWindow, SLOT(slotFileSave()) );
	connect( (*scrActions)["fileSaveAs"], SIGNAL(triggered()), mainWindow, SLOT(slotFileSaveAs()) );
	connect( (*scrActions)["fileDocSetup150"], SIGNAL(triggered()), mainWindow, SLOT(slotDocSetup()) );
	connect( (*scrActions)["filePreferences150"], SIGNAL(triggered()), mainWindow, SLOT(slotPrefsOrg()) );
	connect( (*scrActions)["fileRevert"], SIGNAL(triggered()), mainWindow, SLOT(slotFileRevert()) );
	connect( (*scrActions)["fileCollect"], SIGNAL(triggered()), mainWindow, SLOT(fileCollect()) );
	connect( (*scrActions)["fileQuit"], SIGNAL(triggered()), mainWindow, SLOT(slotFileQuit()) );
	//File Import Menu
	connect( (*scrActions)["fileImportText"], SIGNAL(triggered()), mainWindow, SLOT(slotGetContent()) );
	connect( (*scrActions)["fileImportText2"], SIGNAL(triggered()), mainWindow, SLOT(slotGetContent2()) );
	connect( (*scrActions)["fileImportAppendText"], SIGNAL(triggered()), mainWindow, SLOT(slotFileAppend()) );
	connect( (*scrActions)["fileImportImage"], SIGNAL(triggered()), mainWindow, SLOT(slotGetContent()) );
	connect( (*scrActions)["fileImportVector"], SIGNAL(triggered()), mainWindow, SLOT(importVectorFile()) );
	//File Export Menu
	connect( (*scrActions)["fileExportText"], SIGNAL(triggered()), mainWindow, SLOT(SaveText()) );
	connect( (*scrActions)["fileExportAsEPS"], SIGNAL(triggered()), mainWindow, SLOT(SaveAsEps()) );
	connect( (*scrActions)["fileExportAsPDF"], SIGNAL(triggered()), mainWindow, SLOT(SaveAsPDF()) );
	//The rest are plugins
	
	
	(*scrActions)["fileClose"]->setShortcutContext(Qt::WidgetShortcut);
}

void ActionManager::initEditMenuActions()
{
	QString name;
	//Edit Menu
	name = "editUndoAction";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "edit-undo", "edit-undo", "", defaultKey(name), mainWindow, 1));
	name = "editRedoAction";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "edit-redo", "edit-redo", "", defaultKey(name), mainWindow, 1));
	name = "editActionMode";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)["editActionMode"]->setToggleAction(true);

	name = "editCut";
	scrActions->insert(name, new ScrAction("edit-cut", "edit-cut", "", defaultKey(name), mainWindow));
	name = "editCopy";
	scrActions->insert(name, new ScrAction("edit-copy", "edit-copy", "", defaultKey(name), mainWindow));
	name = "editPaste";
	scrActions->insert(name, new ScrAction("edit-paste", "edit-paste", "", defaultKey(name), mainWindow));
	name = "editPastePlainText";
	scrActions->insert(name, new ScrAction("edit-paste", "edit-paste", "", defaultKey(name), mainWindow));
	name = "editCopyContents";
	scrActions->insert(name, new ScrAction("edit-copy", "edit-copy", "", defaultKey(name), mainWindow));
	name = "editPasteContents";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "edit-paste", QString(), "", defaultKey(name), mainWindow, 0));
	name = "editPasteContentsAbs";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "edit-paste", QString(), "", defaultKey(name), mainWindow, 1));
	name = "editPasteImageFromClipboard";
	scrActions->insert(name, new ScrAction("edit-paste", "edit-paste", "", defaultKey(name), mainWindow));
	name = "editClearContents";
	scrActions->insert(name, new ScrAction("edit-delete", "edit-delete", "", defaultKey(name), mainWindow));
	name = "editTruncateContents";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "editSelectAll";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "editSelectAllOnLayer";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "editDeselectAll";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "editSearchReplace";
	scrActions->insert(name, new ScrAction("edit-find-replace", "edit-find-replace", "", defaultKey(name), mainWindow));
	name = "editEditWithImageEditor";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "editEditRenderSource";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "editColorsAndFills";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "editReplaceColors";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "editStyles";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "editMasterPages";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "editJavascripts";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "editMarks";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "editNotesStyles";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));

	(*scrActions)["editStyles"]->setToggleAction(true);
	(*scrActions)["editMarks"]->setToggleAction(true);
	(*scrActions)["editNotesStyles"]->setToggleAction(true);

	connect( (*scrActions)["editUndoAction"], SIGNAL(triggeredData(int)) , undoManager, SLOT(undo(int)) );
	connect( (*scrActions)["editRedoAction"], SIGNAL(triggeredData(int)) , undoManager, SLOT(redo(int)) );
	connect( (*scrActions)["editActionMode"], SIGNAL(toggled(bool)), mainWindow, SLOT(setUndoMode(bool)) );
	connect( (*scrActions)["editCut"], SIGNAL(triggered()), mainWindow, SLOT(slotEditCut()) );
	connect( (*scrActions)["editCopy"], SIGNAL(triggered()), mainWindow, SLOT(slotEditCopy()) );
	connect( (*scrActions)["editPaste"], SIGNAL(triggered()), mainWindow, SLOT(slotEditPaste()) );
	connect( (*scrActions)["editPastePlainText"], SIGNAL(triggered()), mainWindow, SLOT(slotEditPastePlainText()) );
	connect( (*scrActions)["editCopyContents"], SIGNAL(triggered()), mainWindow, SLOT(slotEditCopyContents()) );
	connect( (*scrActions)["editPasteContents"], SIGNAL(triggeredData(int)), mainWindow, SLOT(slotEditPasteContents(int)) );
	connect( (*scrActions)["editPasteContentsAbs"], SIGNAL(triggeredData(int)), mainWindow, SLOT(slotEditPasteContents(int)) );
	connect( (*scrActions)["editPasteImageFromClipboard"], SIGNAL(triggered()), mainWindow, SLOT(slotGetClipboardImage()) );
	connect( (*scrActions)["editSelectAll"], SIGNAL(triggered()), mainWindow, SLOT(SelectAll()) );
	connect( (*scrActions)["editSelectAllOnLayer"], SIGNAL(triggered()), mainWindow, SLOT(SelectAllOnLayer()) );
	connect( (*scrActions)["editDeselectAll"], SIGNAL(triggered()), mainWindow, SLOT(deselectAll()) );
	connect( (*scrActions)["editSearchReplace"], SIGNAL(triggered()), mainWindow, SLOT(SearchText()) );
	connect( (*scrActions)["editEditWithImageEditor"], SIGNAL(triggered()), mainWindow, SLOT(callImageEditor()) );
	connect( (*scrActions)["editEditRenderSource"], SIGNAL(triggered()), mainWindow, SLOT(callImageEditor()) );
	connect( (*scrActions)["editColorsAndFills"], SIGNAL(triggered()), mainWindow, SLOT(manageColorsAndFills()) );
	connect( (*scrActions)["editReplaceColors"], SIGNAL(triggered()), mainWindow, SLOT(slotReplaceColors()) );
	connect( (*scrActions)["editMasterPages"], SIGNAL(triggered()), mainWindow, SLOT(editMasterPagesStart()) );
	connect( (*scrActions)["editJavascripts"], SIGNAL(triggered()), mainWindow, SLOT(ManageJava()) );
}

void ActionManager::initStyleMenuActions()
{
	//Text Size actions
	scrActionGroups->insert("fontSize", new QActionGroup(mainWindow));
	QString name("fontSizeOther");
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("fontSize"), -1));
	connect( (*scrActions)["fontSizeOther"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemFontSize(int)));

	std::array<int, 12> font_sizes = {7, 9, 10, 11, 12, 14, 18, 24, 36, 48, 60, 72};
	for (auto fontSize : font_sizes)
	{
		QString fontSizeName = QString("fontSize%1").arg(fontSize);
		scrActions->insert(fontSizeName, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("fontSize"), fontSize));
		(*scrActions)[fontSizeName]->setToggleAction(true);
		connect( (*scrActions)[fontSizeName], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemFontSize(int)));
	}

	name = "itemStyleSearch";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	connect( (*scrActions)["itemStyleSearch"], SIGNAL(triggered()), mainWindow, SLOT(slotItemStyleSearch()));

	//Alignment actions
	name = "alignLeft";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), mainWindow, 0));
	name = "alignCenter";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), mainWindow, 1));
	name = "alignRight";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), mainWindow, 2));
	name = "alignBlock";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), mainWindow, 3));
	name = "alignForced";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), mainWindow, 4));

//	(*scrActions)["alignLeft"]->setToggleAction(true);
//	(*scrActions)["alignCenter"]->setToggleAction(true);
//	(*scrActions)["alignRight"]->setToggleAction(true);
//	(*scrActions)["alignBlock"]->setToggleAction(true);
//	(*scrActions)["alignForced"]->setToggleAction(true);

	connect( (*scrActions)["alignLeft"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setNewAlignment(int)));
	connect( (*scrActions)["alignCenter"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setNewAlignment(int)));
	connect( (*scrActions)["alignRight"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setNewAlignment(int)));
	connect( (*scrActions)["alignBlock"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setNewAlignment(int)));
	connect( (*scrActions)["alignForced"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setNewAlignment(int)));

	//Shade actions
/*
	scrActionGroups->insert("shade", new QActionGroup(mainWindow));
	name = "shadeOther";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("shade"), -1));
	connect( (*scrActions)["shadeOther"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemShade(int)));
	for (uint i=0; i<=100 ; i+=10)
	{
		QString shadeName = QString("shade%1").arg(i);
		scrActions->insert(shadeName, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("shade"), i));
		(*scrActions)[shadeName]->setToggleAction(true);
		connect( (*scrActions)[shadeName], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemShade(int)));
	}
*/

	//Type Effects actions
	scrActionGroups->insert("typeEffects", new QActionGroup(mainWindow));
	name = "typeEffectNormal";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("typeEffects"), 0));
	name = "typeEffectUnderline";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("typeEffects"), 1));
	name = "typeEffectUnderlineWords";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("typeEffects"), 8));
	name = "typeEffectStrikeThrough";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("typeEffects"), 2));
	name = "typeEffectAllCaps";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("typeEffects"), 7));
	name = "typeEffectSmallCaps";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("typeEffects"), 3));
	name = "typeEffectSuperscript";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("typeEffects"), 4));
	name = "typeEffectSubscript";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("typeEffects"), 5));
	name = "typeEffectOutline";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("typeEffects"), 6));
	name = "typeEffectShadow";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), scrActionGroups->value("typeEffects"), 9));
	(*scrActions)["typeEffectNormal"]->setToggleAction(true);
	(*scrActions)["typeEffectUnderline"]->setToggleAction(true);
	(*scrActions)["typeEffectUnderlineWords"]->setToggleAction(true);
	(*scrActions)["typeEffectStrikeThrough"]->setToggleAction(true);
	(*scrActions)["typeEffectAllCaps"]->setToggleAction(true);
	(*scrActions)["typeEffectSmallCaps"]->setToggleAction(true);
	(*scrActions)["typeEffectSuperscript"]->setToggleAction(true);
	(*scrActions)["typeEffectSubscript"]->setToggleAction(true);
	(*scrActions)["typeEffectOutline"]->setToggleAction(true);
	(*scrActions)["typeEffectShadow"]->setToggleAction(true);
	connect( (*scrActions)["typeEffectNormal"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemTypeStyle(int)));
	connect( (*scrActions)["typeEffectUnderline"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemTypeStyle(int)));
	connect( (*scrActions)["typeEffectUnderlineWords"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemTypeStyle(int)));
	connect( (*scrActions)["typeEffectStrikeThrough"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemTypeStyle(int)));
	connect( (*scrActions)["typeEffectSmallCaps"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemTypeStyle(int)));
	connect( (*scrActions)["typeEffectAllCaps"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemTypeStyle(int)));
	connect( (*scrActions)["typeEffectSuperscript"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemTypeStyle(int)));
	connect( (*scrActions)["typeEffectSubscript"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemTypeStyle(int)));
	connect( (*scrActions)["typeEffectOutline"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemTypeStyle(int)));
	connect( (*scrActions)["typeEffectShadow"], SIGNAL(triggeredData(int)), mainWindow, SLOT(setItemTypeStyle(int)));

	//Other Style menu items that get added in various places
	name = "styleImageEffects";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "styleTabulators";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	connect( (*scrActions)["styleImageEffects"], SIGNAL(triggered()), mainWindow, SLOT(ImageEffects()));
	connect( (*scrActions)["styleTabulators"], SIGNAL(triggered()), mainWindow, SLOT(EditTabs()));
}

void ActionManager::initItemMenuActions()
{
	QString name;
	//Item Menu
	name = "itemDuplicate";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemMulDuplicate";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemTransform";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemDelete";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemGroup";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemUngroup";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemGroupAdjust";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemLock";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemLockSize";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemPrintingEnabled";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemFlipH";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemFlipV";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)["itemLock"]->setToggleAction(true, true);
	(*scrActions)["itemLockSize"]->setToggleAction(true, true);
	(*scrActions)["itemPrintingEnabled"]->setToggleAction(true, true);
	(*scrActions)["itemFlipH"]->setToggleAction(true, true);
	(*scrActions)["itemFlipV"]->setToggleAction(true, true);
	name = "itemLowerToBottom";
	scrActions->insert(name, new ScrAction("go-bottom", "go-bottom", "", defaultKey(name), mainWindow));
	name = "itemRaiseToTop";
	scrActions->insert(name, new ScrAction("go-top", "go-top", "", defaultKey(name), mainWindow));
	name = "itemLower";
	scrActions->insert(name, new ScrAction("go-down", "go-down", "", defaultKey(name), mainWindow));
	name = "itemRaise";
	scrActions->insert(name, new ScrAction("go-up", "go-up", "", defaultKey(name), mainWindow));
	name = "itemSendToPattern";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemSendToInline";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemImageInfo";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemAttributes";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemImageIsVisible";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemUpdateImage";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemAdjustFrameHeightToText";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemAdjustFrameToImage";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemAdjustImageToFrame";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemExtendedImageProperties";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemToggleInlineImage";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));

	QActionGroup* itemPreviewGroup = new QActionGroup(mainWindow);
	scrActionGroups->insert("itemPreview", itemPreviewGroup);
	itemPreviewGroup->setExclusive(true);
	name = "itemPreviewFull";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), itemPreviewGroup, 0));
	name = "itemPreviewNormal";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), itemPreviewGroup, 1));
	name = "itemPreviewLow";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), itemPreviewGroup, 2));

	(*scrActions)["itemImageIsVisible"]->setToggleAction(true);
	(*scrActions)["itemPreviewFull"]->setToggleAction(true);
	(*scrActions)["itemPreviewNormal"]->setToggleAction(true);
	(*scrActions)["itemPreviewLow"]->setToggleAction(true);
	(*scrActions)["itemToggleInlineImage"]->setToggleAction(true);

	name = "itemPDFIsBookmark";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)["itemPDFIsBookmark"]->setToggleAction(true);
	name = "itemPDFIsAnnotation";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)["itemPDFIsAnnotation"]->setToggleAction(true);
	name = "itemPDFAnnotationProps";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemPDFFieldProps";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));

	name = "itemShapeEdit";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)["itemShapeEdit"]->setToggleAction(true);
	(*scrActions)["itemShapeEdit"]->setChecked(false);
	name = "itemAttachTextToPath";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemDetachTextFromPath";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemCombinePolygons";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemSplitPolygons";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemConvertToBezierCurve";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemConvertToImageFrame";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemConvertToOutlines";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemConvertToPolygon";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemConvertToTextFrame";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemConvertToSymbolFrame";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));

	name = "itemsUnWeld";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemWeld";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "itemEditWeld";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));

	connect( (*scrActions)["itemDuplicate"], SIGNAL(triggered()), mainWindow, SLOT(duplicateItem()) );
	connect( (*scrActions)["itemMulDuplicate"], SIGNAL(triggered()), mainWindow, SLOT(duplicateItemMulti()) );
	connect( (*scrActions)["itemTransform"], SIGNAL(triggered()), mainWindow, SLOT(slotItemTransform()) );
	connect( (*scrActions)["itemGroup"], SIGNAL(triggered()), mainWindow, SLOT(GroupObj()) );
	connect( (*scrActions)["itemUngroup"], SIGNAL(triggered()), mainWindow, SLOT(UnGroupObj()) );
	connect( (*scrActions)["itemGroupAdjust"], SIGNAL(triggered()), mainWindow, SLOT(AdjustGroupObj()) );
	connect( (*scrActions)["itemPDFAnnotationProps"], SIGNAL(triggered()), mainWindow, SLOT(ModifyAnnot()) );
	connect( (*scrActions)["itemPDFFieldProps"], SIGNAL(triggered()), mainWindow, SLOT(ModifyAnnot()) );
	connect( (*scrActions)["itemSendToPattern"], SIGNAL(triggered()), mainWindow, SLOT(PutToPatterns()) );
	connect( (*scrActions)["itemSendToInline"], SIGNAL(triggered()), mainWindow, SLOT(PutToInline()) );
	connect( (*scrActions)["itemConvertToSymbolFrame"], SIGNAL(triggered()), mainWindow, SLOT(ConvertToSymbol()) );
	connect( (*scrActions)["itemAttributes"], SIGNAL(triggered()), mainWindow, SLOT(objectAttributes()) );
	connect( (*scrActions)["itemShapeEdit"], SIGNAL(triggered()), mainWindow, SLOT(toggleNodeEdit()) );
	connect( (*scrActions)["itemImageInfo"], SIGNAL(triggered()), mainWindow, SLOT(getImageInfo()) );
	connect( (*scrActions)["itemToggleInlineImage"], SIGNAL(triggered()), mainWindow, SLOT(toogleInlineState()) );
}

void ActionManager::initInsertMenuActions()
{
	QString name;
	//Insert Menu
	name = "insertFrame";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "insertGlyph";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)["insertGlyph"]->setToggleAction(true);
	name = "insertSampleText";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "stickyTools";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)["stickyTools"]->setToggleAction(true);

	//Marks
	name = "insertMarkAnchor";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)[name]->setEnabled(true);
	name = "insertMarkVariableText";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)[name]->setEnabled(true);
	name = "insertMarkItem";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)[name]->setEnabled(true);
	name = "insertMark2Mark";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)[name]->setEnabled(true);
	name = "insertMarkNote";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)[name]->setEnabled(true);
	name = "insertMarkIndex";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)[name]->setEnabled(true);
	name = "editMark";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)[name]->setEnabled(false);
	name = "itemUpdateMarks";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)[name]->setEnabled(false);

	connect( (*scrActions)["insertFrame"], SIGNAL(triggered()), mainWindow, SLOT(slotInsertFrame()) );
	connect( (*scrActions)["insertGlyph"], SIGNAL(triggered()), mainWindow, SLOT(slotCharSelect()) );
	connect( (*scrActions)["insertSampleText"], SIGNAL(triggered()), mainWindow, SLOT(insertSampleText()) );
	connect( (*scrActions)["stickyTools"], SIGNAL(triggered()), mainWindow, SLOT(ToggleStickyTools()) );

	connect( (*scrActions)["insertMarkAnchor"], SIGNAL(triggered()), mainWindow, SLOT(slotInsertMarkAnchor()) );
	connect( (*scrActions)["insertMarkVariableText"], SIGNAL(triggered()), mainWindow, SLOT(slotInsertMarkVariableText()) );
	connect( (*scrActions)["insertMarkItem"], SIGNAL(triggered()), mainWindow, SLOT(slotInsertMarkItem()) );
	connect( (*scrActions)["insertMark2Mark"], SIGNAL(triggered()), mainWindow, SLOT(slotInsertMark2Mark()) );
	connect( (*scrActions)["insertMarkNote"], SIGNAL(triggered()), mainWindow, SLOT(slotInsertMarkNote()) );
	connect( (*scrActions)["insertMarkIndex"], SIGNAL(triggered()), mainWindow, SLOT(slotInsertMarkIndex()) );
	connect( (*scrActions)["editMark"], SIGNAL(triggered()), mainWindow, SLOT(slotEditMark()) );
	connect( (*scrActions)["itemUpdateMarks"], SIGNAL(triggered()), mainWindow, SLOT(slotUpdateMarks()) );
}

void ActionManager::initPageMenuActions()
{
	QString name;
	//Page menu
	name = "pageInsert";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "pageImport";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "pageDelete";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "pageCopy";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "pageMove";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "pageApplyMasterPage";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "pageCopyToMasterPage";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "pageManageGuides";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)["pageManageGuides"]->setToggleAction(true);
	name = "pageManageProperties";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));

	connect( (*scrActions)["pageInsert"], SIGNAL(triggered()), mainWindow, SLOT(slotNewPageM()) );
	connect( (*scrActions)["pageImport"], SIGNAL(triggered()), mainWindow, SLOT(slotPageImport()) );
	connect( (*scrActions)["pageDelete"], SIGNAL(triggered()), mainWindow, SLOT(deletePage()) );
	connect( (*scrActions)["pageCopy"], SIGNAL(triggered()), mainWindow, SLOT(copyPage()) );
	connect( (*scrActions)["pageMove"], SIGNAL(triggered()), mainWindow, SLOT(movePage()) );
	connect( (*scrActions)["pageApplyMasterPage"], SIGNAL(triggered()), mainWindow, SLOT(ApplyMasterPage()) );
	connect( (*scrActions)["pageCopyToMasterPage"], SIGNAL(triggered()), mainWindow, SLOT(duplicateToMasterPage()) );
	connect( (*scrActions)["pageManageProperties"], SIGNAL(triggered()), mainWindow, SLOT(changePageProperties()) );
}

void ActionManager::initTableMenuActions()
{
	QString name;

	name = "tableInsertRows";
	scrActions->insert(name, new ScrAction("insert-table-rows", "insert-table-rows", "", defaultKey(name), mainWindow));
	name = "tableInsertColumns";
	scrActions->insert(name, new ScrAction("insert-table-columns", "insert-table-columns", "", defaultKey(name), mainWindow));
	name = "tableDeleteRows";
	scrActions->insert(name, new ScrAction("delete-table-rows", "delete-table-rows", "", defaultKey(name), mainWindow));
	name = "tableDeleteColumns";
	scrActions->insert(name, new ScrAction("delete-table-columns", "delete-table-columns", "", defaultKey(name), mainWindow));
	name = "tableMergeCells";
	scrActions->insert(name, new ScrAction("merge-table-cells", "merge-table-cells", "", defaultKey(name), mainWindow));
	name = "tableSplitCells";
	scrActions->insert(name, new ScrAction("split-table-cells", "split-table-cells", "", defaultKey(name), mainWindow));
	name = "tableSetRowHeights";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "tableSetColumnWidths";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "tableDistributeRowsEvenly";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "tableDistributeColumnsEvenly";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "tableAdjustFrameToTable";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "tableAdjustTableToFrame";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
}

void ActionManager::initViewMenuActions()
{
	QString name;
	name = "viewFitInWindow";
	scrActions->insert(name, new ScrAction(ScrAction::DataDouble, QString(), QString(), "", defaultKey(name), mainWindow, -100.0));
	name = "viewFitWidth";
	scrActions->insert(name, new ScrAction(ScrAction::DataDouble, QString(), QString(), "", defaultKey(name), mainWindow, -200.0));
	name = "viewFit50";
	scrActions->insert(name, new ScrAction(ScrAction::DataDouble, QString(), QString(), "", defaultKey(name), mainWindow, 50.0));
	name = "viewFit75";
	scrActions->insert(name, new ScrAction(ScrAction::DataDouble, QString(), QString(), "", defaultKey(name), mainWindow, 75.0));
	name = "viewFit100";
	scrActions->insert(name, new ScrAction(ScrAction::DataDouble, "zoom-original", "zoom-original", "", defaultKey(name), mainWindow, 100.0));
	name = "viewFit200";
	scrActions->insert(name, new ScrAction(ScrAction::DataDouble, QString(), QString(), "", defaultKey(name), mainWindow, 200.0));
	name = "viewFit400";
	scrActions->insert(name, new ScrAction(ScrAction::DataDouble, QString(), QString(), "", defaultKey(name), mainWindow, 400.0));
	name = "viewPreviewMode";
	scrActions->insert(name, new ScrAction("preview-on", "preview-on", "", defaultKey(name), mainWindow));
	name = "viewEditInPreview";
	scrActions->insert(name, new ScrAction("editdoc", "editdoc", "", defaultKey(name), mainWindow));
	name = "viewToggleCMS";
	scrActions->insert(name, new ScrAction("cms-on", "cms-on", "", defaultKey(name), mainWindow));
	name = "viewToggleWhiteSpaceMode";
	scrActions->insert(name, new ScrAction("whitespace-review-mode", "whitespace-review-mode", "", defaultKey(name), mainWindow));
	name = "viewShowMargins";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewShowBleeds";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewShowFrames";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewShowLayerMarkers";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewShowImages";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewShowGrid";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewShowGuides";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewShowColumnBorders";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewShowBaseline";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewShowTextChain";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewShowTextControls";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewShowRulers";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewRulerMode";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "viewSnapToGrid";
	scrActions->insert(name, new ScrAction("snap-grid", "snap-grid", "", defaultKey(name), mainWindow));
	name = "viewSnapToGuides";
	scrActions->insert(name, new ScrAction("snap-guides", "snap-guides", "", defaultKey(name), mainWindow));
	name = "viewSnapToElements";
	scrActions->insert(name, new ScrAction("snap-items", "snap-items", "", defaultKey(name), mainWindow));
	name = "viewShowContextMenu";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "showMouseCoordinates";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));

	name = "viewToggleCM";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
//	name = "viewVisionNormal";
//	name = "viewVisionProtanopia";
//	name = "viewVisionDeuteranopia";
//	name = "viewVisionTritanopia";
//	name = "viewVisionFullColorBlind";

//	scrActions->insert("viewNewView", new ScrAction("", defaultKey(name), mainWindow));

	(*scrActions)["viewPreviewMode"]->setToggleAction(true);
	(*scrActions)["viewEditInPreview"]->setToggleAction(true);
	(*scrActions)["viewToggleCMS"]->setToggleAction(true);
	(*scrActions)["viewToggleWhiteSpaceMode"]->setToggleAction(true);
	(*scrActions)["viewShowMargins"]->setToggleAction(true);
	(*scrActions)["viewShowBleeds"]->setToggleAction(true);
	(*scrActions)["viewShowFrames"]->setToggleAction(true);
	(*scrActions)["viewShowLayerMarkers"]->setToggleAction(true);
	(*scrActions)["viewShowImages"]->setToggleAction(true);
	(*scrActions)["viewShowGrid"]->setToggleAction(true);
	(*scrActions)["viewShowGuides"]->setToggleAction(true);
	(*scrActions)["viewShowColumnBorders"]->setToggleAction(true);
	(*scrActions)["viewShowBaseline"]->setToggleAction(true);
	(*scrActions)["viewShowTextChain"]->setToggleAction(true);
	(*scrActions)["viewShowTextControls"]->setToggleAction(true);
	(*scrActions)["viewShowRulers"]->setToggleAction(true);
	(*scrActions)["viewRulerMode"]->setToggleAction(true);
	(*scrActions)["viewSnapToGrid"]->setToggleAction(true);
	(*scrActions)["viewSnapToGuides"]->setToggleAction(true);
	(*scrActions)["viewSnapToElements"]->setToggleAction(true);
	(*scrActions)["showMouseCoordinates"]->setToggleAction(true);

	(*scrActions)["viewPreviewMode"]->setChecked(false);
	(*scrActions)["viewEditInPreview"]->setChecked(false);
	(*scrActions)["viewToggleWhiteSpaceMode"]->setChecked(false);
	(*scrActions)["viewToggleCMS"]->setChecked(false);
	(*scrActions)["viewShowMargins"]->setChecked(true);
	(*scrActions)["viewShowBleeds"]->setChecked(true);
	(*scrActions)["viewShowFrames"]->setChecked(true);
	(*scrActions)["viewShowLayerMarkers"]->setChecked(false);
	(*scrActions)["viewShowImages"]->setChecked(true);
	(*scrActions)["viewShowGuides"]->setChecked(true);
	(*scrActions)["viewShowColumnBorders"]->setChecked(false);
	(*scrActions)["viewShowRulers"]->setChecked(true);
	(*scrActions)["viewRulerMode"]->setChecked(true);
	(*scrActions)["showMouseCoordinates"]->setChecked(true);

	connect( (*scrActions)["viewFitInWindow"], SIGNAL(triggeredData(double)), mainWindow, SLOT(slotZoom(double)) );
	connect( (*scrActions)["viewFitWidth"], SIGNAL(triggeredData(double)), mainWindow, SLOT(slotZoom(double)) );
	connect( (*scrActions)["viewFit50"], SIGNAL(triggeredData(double)), mainWindow, SLOT(slotZoom(double)) );
	connect( (*scrActions)["viewFit75"], SIGNAL(triggeredData(double)), mainWindow, SLOT(slotZoom(double)) );
	connect( (*scrActions)["viewFit100"], SIGNAL(triggeredData(double)), mainWindow, SLOT(slotZoom(double)) );
	connect( (*scrActions)["viewFit200"], SIGNAL(triggeredData(double)), mainWindow, SLOT(slotZoom(double)) );
	connect( (*scrActions)["viewFit400"], SIGNAL(triggeredData(double)), mainWindow, SLOT(slotZoom(double)) );
	connect( (*scrActions)["viewShowMargins"], SIGNAL(triggered()), mainWindow, SLOT(toggleMarks()) );
	connect( (*scrActions)["viewShowBleeds"], SIGNAL(triggered()), mainWindow, SLOT(toggleBleeds()) );
	connect( (*scrActions)["viewShowFrames"], SIGNAL(triggered()), mainWindow, SLOT(toggleFrames()) );
	connect( (*scrActions)["viewShowLayerMarkers"], SIGNAL(triggered()), mainWindow, SLOT(toggleLayerMarkers()) );
	connect( (*scrActions)["viewShowImages"], SIGNAL(triggered()), mainWindow, SLOT(toggleImageVisibility()) );
	connect( (*scrActions)["viewShowGrid"], SIGNAL(triggered()), mainWindow, SLOT(toggleGrid()) );
	connect( (*scrActions)["viewShowGuides"], SIGNAL(triggered()), mainWindow, SLOT(toggleGuides()) );
	connect( (*scrActions)["viewShowColumnBorders"], SIGNAL(triggered()), mainWindow, SLOT(toggleColumnBorders()) );
	connect( (*scrActions)["viewShowBaseline"], SIGNAL(triggered()), mainWindow, SLOT(toggleBase()) );
	connect( (*scrActions)["viewShowTextChain"], SIGNAL(triggered()), mainWindow, SLOT(toggleTextLinks()) );
	connect( (*scrActions)["viewShowTextControls"], SIGNAL(triggered()), mainWindow, SLOT(toggleTextControls()) );
	connect( (*scrActions)["viewShowRulers"], SIGNAL(triggered()), mainWindow, SLOT(toggleRulers()) );
	connect( (*scrActions)["viewRulerMode"], SIGNAL(triggered()), mainWindow, SLOT(toggleRulerMode()) );
	connect( (*scrActions)["viewSnapToGrid"], SIGNAL(triggered()), mainWindow, SLOT(toggleSnapGrid()) );
	connect( (*scrActions)["viewSnapToGuides"], SIGNAL(triggered()), mainWindow, SLOT(toggleSnapGuides()) );
	connect( (*scrActions)["viewSnapToElements"], SIGNAL(triggered()), mainWindow, SLOT(toggleSnapElements()) );
	connect( (*scrActions)["showMouseCoordinates"], SIGNAL(triggered()), mainWindow, SLOT(ToggleMouseTips()) );
//	connect( (*scrActions)["viewNewView"], SIGNAL(triggered()), mainWindow, SLOT(newView()) );

}

void ActionManager::initToolsMenuActions()
{
	QString name;
	//Tool menu
	name = "toolsProperties";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name="toolsContent";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsOutline";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsScrapbook";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsLayers";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsPages";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsBookmarks";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsDownloads";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsResources";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsMeasurements";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-measure", "tool-measure", "", defaultKey(name), mainWindow, modeMeasurementTool));
	name = "toolsActionHistory";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsPreflightVerifier";
	scrActions->insert(name, new ScrAction("preflight-verifier", "preflight-verifier","", defaultKey(name), mainWindow));
	name = "toolsAlignDistribute";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsSymbols";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsInline";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsToolbarTools";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsToolbarPDF";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "toolsToolbarView";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));

	//toolbar only items
	name = "toolsSelect";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-select", "tool-select", "", defaultKey(name), mainWindow, modeNormal));
	name = "toolsInsertTextFrame";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-insert-text-frame", "tool-insert-text-frame", "", defaultKey(name), mainWindow, modeDrawText));
	name = "toolsInsertImageFrame";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-insert-image", "tool-insert-image", "", defaultKey(name), mainWindow, modeDrawImage));
	name = "toolsInsertRenderFrame";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-insert-render-frame", "tool-insert-render-frame", "", defaultKey(name), mainWindow, modeDrawLatex));
	name = "toolsInsertTable";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-insert-table", "tool-insert-table", "", defaultKey(name), mainWindow, modeDrawTable2));
	name = "toolsInsertShape";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), mainWindow, modeDrawShapes));
	name = "toolsInsertPolygon";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-insert-polygon", "tool-insert-polygon", "", defaultKey(name), mainWindow, modeDrawRegularPolygon));
	name = "toolsInsertArc";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-insert-arc", "tool-insert-arc", "", defaultKey(name), mainWindow, modeDrawArc));
	name = "toolsInsertSpiral";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-insert-spiral", "tool-insert-spiral", "", defaultKey(name), mainWindow, modeDrawSpiral));
	name = "toolsInsertLine";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-insert-line", "tool-insert-line", "", defaultKey(name), mainWindow, modeDrawLine));
	name = "toolsInsertBezier";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-draw-bezier", "tool-draw-bezier", "", defaultKey(name), mainWindow, modeDrawBezierLine));
	name = "toolsInsertFreehandLine";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-draw-freehand", "tool-draw-freehand", "", defaultKey(name), mainWindow, modeDrawFreehandLine));
	name = "toolsInsertCalligraphicLine";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-draw-calligraphy", "tool-draw-calligraphy", "", defaultKey(name), mainWindow, modeDrawCalligraphicLine));
	name = "toolsRotate";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-rotate", "tool-rotate", "", defaultKey(name), mainWindow, modeRotation));
	name = "toolsZoom";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-zoom", "tool-zoom", "", defaultKey(name), mainWindow, modeMagnifier));
	name = "toolsZoomIn";
	scrActions->insert(name, new ScrAction("zoom-in", "zoom-in", "", defaultKey(name), mainWindow));
	name = "toolsZoomOut";
	scrActions->insert(name, new ScrAction("zoom-out", "zoom-out", "", defaultKey(name), mainWindow));
	name = "toolsEditContents";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-edit-content", "tool-edit-content", "", defaultKey(name), mainWindow, modeEdit));
	name = "toolsEditWithStoryEditor";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-story-editor", "tool-story-editor", "", defaultKey(name), mainWindow, modeStoryEditor));
	name = "toolsLinkTextFrame";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-link-text-frame", "tool-link-text-frame", "", defaultKey(name), mainWindow, modeLinkFrames));
	name = "toolsUnlinkTextFrame";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-unlink-text-frame", "tool-unlink-text-frame", "", defaultKey(name), mainWindow, modeUnlinkFrames));
	name = "toolsEyeDropper";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-color-picker", "tool-color-picker", "", defaultKey(name), mainWindow, modeEyeDropper));
	name = "toolsCopyProperties";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "tool-copy-style", "tool-copy-style", "", defaultKey(name), mainWindow, modeCopyProperties));
	name = "toolsUnlinkTextFrameAndCutText";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, QString(), QString(), "", defaultKey(name), mainWindow, modeUnlinkFrames));

	//PDF toolbar
	name = "toolsPDFPushButton";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "pdf-button", "pdf-button", "", defaultKey(name), mainWindow, modeInsertPDFButton));
	name = "toolsPDFRadioButton";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "pdf-radiobutton", "pdf-radiobutton", "", defaultKey(name), mainWindow, modeInsertPDFRadioButton));
	name = "toolsPDFTextField";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "pdf-textfield", "pdf-textfield", "", defaultKey(name), mainWindow, modeInsertPDFTextfield));
	name = "toolsPDFCheckBox";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "pdf-checkbox", "pdf-checkbox", "", defaultKey(name), mainWindow, modeInsertPDFCheckbox));
	name = "toolsPDFComboBox";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "pdf-combobox", "pdf-combobox", "", defaultKey(name), mainWindow, modeInsertPDFCombobox));
	name = "toolsPDFListBox";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "pdf-listbox", "pdf-listbox", "", defaultKey(name), mainWindow, modeInsertPDFListbox));
	name = "toolsPDFAnnotText";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "pdf-annotation-text", "pdf-annotation-text", "", defaultKey(name), mainWindow, modeInsertPDFTextAnnotation));
	name = "toolsPDFAnnotLink";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "pdf-annotation-link", "pdf-annotation-link", "", defaultKey(name), mainWindow, modeInsertPDFLinkAnnotation));
#ifdef HAVE_OSG
	name = "toolsPDFAnnot3D";
	scrActions->insert(name, new ScrAction(ScrAction::DataInt, "pdf-annotation-3d", "pdf-annotation-3d", "", defaultKey(name), mainWindow, modeInsertPDF3DAnnotation));
#endif
	//Set the application wide palette shortcuts
	(*scrActions)["toolsProperties"]->setShortcutContext(Qt::ApplicationShortcut);
	(*scrActions)["toolsContent"]->setShortcutContext(Qt::ApplicationShortcut);
	(*scrActions)["toolsScrapbook"]->setShortcutContext(Qt::ApplicationShortcut);
	(*scrActions)["toolsLayers"]->setShortcutContext(Qt::ApplicationShortcut);
	(*scrActions)["toolsPages"]->setShortcutContext(Qt::ApplicationShortcut);
	(*scrActions)["toolsBookmarks"]->setShortcutContext(Qt::ApplicationShortcut);
	(*scrActions)["toolsDownloads"]->setShortcutContext(Qt::ApplicationShortcut);
	(*scrActions)["toolsActionHistory"]->setShortcutContext(Qt::ApplicationShortcut);
	(*scrActions)["toolsPreflightVerifier"]->setShortcutContext(Qt::ApplicationShortcut);
	(*scrActions)["toolsAlignDistribute"]->setShortcutContext(Qt::ApplicationShortcut);
	(*scrActions)["toolsSymbols"]->setShortcutContext(Qt::ApplicationShortcut);
	(*scrActions)["toolsInline"]->setShortcutContext(Qt::ApplicationShortcut);

	(*scrActions)["toolsDownloads"]->setToggleAction(true);
	(*scrActions)["toolsMeasurements"]->setToggleAction(true);
	(*scrActions)["toolsPreflightVerifier"]->setToggleAction(true);
	(*scrActions)["toolsToolbarTools"]->setToggleAction(true);
	(*scrActions)["toolsToolbarPDF"]->setToggleAction(true);
	(*scrActions)["toolsToolbarView"]->setToggleAction(true);

	*modeActionNames << "toolsSelect" << "toolsInsertTextFrame" << "toolsInsertImageFrame" << "toolsInsertTable";
	*modeActionNames << "toolsInsertShape" << "toolsInsertPolygon" << "toolsInsertArc" << "toolsInsertSpiral" << "toolsInsertLine" << "toolsInsertBezier";
	*modeActionNames << "toolsInsertFreehandLine" << "toolsInsertCalligraphicLine" << "toolsInsertRenderFrame" << "toolsRotate" << "toolsZoom" << "toolsEditContents";
	*modeActionNames << "toolsEditWithStoryEditor" << "toolsLinkTextFrame" << "toolsUnlinkTextFrame"; //<< "toolsUnlinkTextFrameAndCutText";
	*modeActionNames << "toolsEyeDropper" << "toolsCopyProperties";
	*modeActionNames << "toolsPDFPushButton" << "toolsPDFRadioButton" << "toolsPDFTextField" << "toolsPDFCheckBox" << "toolsPDFComboBox" << "toolsPDFListBox" << "toolsPDFAnnotText" << "toolsPDFAnnotLink";
#ifdef HAVE_OSG
	*modeActionNames << "toolsPDFAnnot3D";
#endif
	for (auto it = modeActionNames->cbegin(); it != modeActionNames->cend(); ++it)
	{
 		(*scrActions)[*it]->setEnabled(false);
		(*scrActions)[*it]->setToggleAction(true);
	}


	*nonEditActionNames << "itemLowerToBottom" << "itemRaiseToTop" << "itemRaise" << "itemLower";

	connect( (*scrActions)["toolsResources"], SIGNAL(triggered()), mainWindow, SLOT(slotResourceManager()) );
	connectModeActions();
}

void ActionManager::initExtrasMenuActions()
{
	QString name;
	name = "extrasManageImages";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "extrasHyphenateText";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "extrasDeHyphenateText";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "extrasGenerateTableOfContents";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "extrasUpdateDocument";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	connect( (*scrActions)["extrasManageImages"], SIGNAL(triggered()), mainWindow, SLOT(StatusPic()) );
	connect( (*scrActions)["extrasGenerateTableOfContents"], SIGNAL(triggered()), mainWindow, SLOT(generateTableOfContents()) );
	connect( (*scrActions)["extrasUpdateDocument"], SIGNAL(triggered()), mainWindow, SLOT(updateDocument()) );
}


void ActionManager::initWindowsMenuActions()
{
	QString name;
	name = "windowsCascade";
	scrActions->insert(name, new ScrAction( "", defaultKey(name), mainWindow));
	name = "windowsTile";
	scrActions->insert(name, new ScrAction( "", defaultKey(name), mainWindow));
	name = "specialToggleAllPalettes";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	connect( (*scrActions)["specialToggleAllPalettes"], SIGNAL(triggered()), mainWindow, SLOT(ToggleAllPalettes()) );
}

void ActionManager::initScriptMenuActions()
{
}

void ActionManager::initHelpMenuActions()
{
	QString name;
	name = "helpAboutScribus";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)[name]->setMenuRole(QAction::AboutRole);
	name = "helpAboutPlugins";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)[name]->setMenuRole(QAction::ApplicationSpecificRole);
	name = "helpAboutQt";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	(*scrActions)[name]->setMenuRole(QAction::AboutQtRole);
	name = "helpActionSearch";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "helpTooltips";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "helpManual";
	scrActions->insert(name, new ScrAction("help-browser", QString(), "", defaultKey(name), mainWindow));
	name = "helpOnlineWWW";
	scrActions->insert(name, new ScrAction(ScrAction::DataQString, QString(), QString(), "", defaultKey(name), mainWindow, "https://www.scribus.net"));
	name = "helpOnlineDocs";
	scrActions->insert(name, new ScrAction(ScrAction::DataQString, QString(), QString(), "", defaultKey(name), mainWindow, "https://docs.scribus.net"));
	name = "helpOnlineWiki";
	scrActions->insert(name, new ScrAction(ScrAction::DataQString, QString(), QString(), "", defaultKey(name), mainWindow, "https://wiki.scribus.net"));
	name = "helpOnlineTutorial1";
	scrActions->insert(name, new ScrAction(ScrAction::DataQString, QString(), QString(), "", defaultKey(name), mainWindow, ""));
	name = "helpChat";
	scrActions->insert(name, new ScrAction(ScrAction::DataQString, QString(), QString(), "", defaultKey(name), mainWindow, "https://web.libera.chat/?channels=scribus"));
	name = "helpCheckUpdates";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));

	(*scrActions)["helpTooltips"]->setToggleAction(true);
	(*scrActions)["helpTooltips"]->setChecked(true);

	connect( (*scrActions)["helpAboutScribus"], SIGNAL(triggered()), mainWindow, SLOT(slotHelpAbout()) );
	connect( (*scrActions)["helpAboutPlugins"], SIGNAL(triggered()), mainWindow, SLOT(slotHelpAboutPlugins()) );
	connect( (*scrActions)["helpAboutQt"], SIGNAL(triggered()), mainWindow, SLOT(slotHelpAboutQt()) );
	connect( (*scrActions)["helpActionSearch"], SIGNAL(triggered()), mainWindow, SLOT(slotHelpActionSearch()) );
	connect( (*scrActions)["helpTooltips"], SIGNAL(triggered()), mainWindow, SLOT(ToggleTips()) );
	connect( (*scrActions)["helpManual"], SIGNAL(triggered()), mainWindow, SLOT(slotOnlineHelp()) );
	connect( (*scrActions)["helpCheckUpdates"], SIGNAL(triggered()), mainWindow, SLOT(slotHelpCheckUpdates()) );
	UrlLauncher& ul = UrlLauncher::instance();
	connect( (*scrActions)["helpOnlineWWW"], SIGNAL(triggeredData(QString)), &ul, SLOT(launchUrlExt(QString)) );
	connect( (*scrActions)["helpOnlineDocs"], SIGNAL(triggeredData(QString)), &ul, SLOT(launchUrlExt(QString)) );
	connect( (*scrActions)["helpOnlineWiki"], SIGNAL(triggeredData(QString)), &ul, SLOT(launchUrlExt(QString)) );
	connect( (*scrActions)["helpOnlineTutorial1"], SIGNAL(triggeredData(QString)), &ul, SLOT(launchUrlExt(QString)) );
	connect( (*scrActions)["helpChat"], SIGNAL(triggeredData(QString)), &ul, SLOT(launchUrlExt(QString)) );
}

void ActionManager::initUnicodeActions(QMap<QString, QPointer<ScrAction> > *actionMap, QWidget *actionParent, QStringList *actionNamesList)
{
	QString name;
	//typography
	name = "unicodeZWNJ";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, SpecialChars::ZWNJ.unicode()));
	name = "unicodeZWJ";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, SpecialChars::ZWJ.unicode()));
	name = "unicodeSoftHyphen";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, SpecialChars::SHYPHEN.unicode()));
	name = "unicodeNonBreakingHyphen";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, SpecialChars::NBHYPHEN.unicode()));
	name = "unicodeNonBreakingSpace";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, SpecialChars::NBSPACE.unicode()));
	name = "unicodeNarrowNoBreakSpace";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x202F));
	name = "unicodePageNumber";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, SpecialChars::PAGENUMBER.unicode()));
	name = "unicodePageCount";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, SpecialChars::PAGECOUNT.unicode()));
	//Spaces
	name = "unicodeSpaceEN";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2002));
	name = "unicodeSpaceEM";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2003));
	name = "unicodeSpaceThin";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2009));
	name = "unicodeSpaceThick";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2004));
	name = "unicodeSpaceMid";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2005));
	name = "unicodeSpaceHair";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x200A));
	//Breaks
	name = "unicodeNewLine";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, SpecialChars::LINEBREAK.unicode()));
	name = "unicodeFrameBreak";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, SpecialChars::FRAMEBREAK.unicode()));
	name = "unicodeColumnBreak";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, SpecialChars::COLBREAK.unicode()));
	name = "unicodeZerowidthSpace";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, SpecialChars::ZWSPACE.unicode()));
	name = "unicodeZerowidthNonBreakingSpace";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, SpecialChars::ZWNBSPACE.unicode()));
	//Special
	name = "unicodeCopyRight";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x0A9));
	name = "unicodeRegdTM";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x00AE));
	name = "unicodeTM";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2122));
	name = "unicodeBullet";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2022));
	name = "unicodeMidpoint";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x00B7));
	name = "unicodeSolidus";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2044));
	//Dashes
	name = "unicodeDashEm";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2014));
	name = "unicodeDashEn";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2013));
	name = "unicodeDashFigure";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2012));
	name = "unicodeDashQuotation";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2015));
	//Quotes
	name = "unicodeQuoteApostrophe";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x0027));
	name = "unicodeQuoteStraight";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x0022));
	name = "unicodeQuoteSingleLeft";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2018));
	name = "unicodeQuoteSingleRight";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2019));
	name = "unicodeQuoteDoubleLeft";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x201C));
	name = "unicodeQuoteDoubleRight";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x201D));
	name = "unicodeQuoteSingleReversed";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x201B));
	name = "unicodeQuoteDoubleReversed";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x201F));
	name = "unicodeQuoteSingleLeftGuillemet";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x2039));
	name = "unicodeQuoteSingleRightGuillemet";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x203A));
	name = "unicodeQuoteDoubleLeftGuillemet";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x00AB));
	name = "unicodeQuoteDoubleRightGuillemet";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x00BB));
	name = "unicodeQuoteLowSingleComma";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x201A));
	name = "unicodeQuoteLowDoubleComma";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x201E));
	name = "unicodeQuoteCJKSingleLeft";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x300C));
	name = "unicodeQuoteCJKSingleRight";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x300D));
	name = "unicodeQuoteCJKDoubleLeft";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x300E));
	name = "unicodeQuoteCJKDoubleRight";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0x300F));
	//Ligatures
	name = "unicodeLigature_ff";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0xFB00));
	name = "unicodeLigature_fi";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0xFB01));
	name = "unicodeLigature_fl";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0xFB02));
	name = "unicodeLigature_ffi";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0xFB03));
	name = "unicodeLigature_ffl";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0xFB04));
	name = "unicodeLigature_ft";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0xFB05));
	name = "unicodeLigature_st";
	actionMap->insert(name, new ScrAction(defaultKey(name), actionParent, 0xFB06));

	//Spaces and special characters

	*actionNamesList << "unicodeZWNJ" << "unicodeZWJ";
	*actionNamesList << "unicodeSoftHyphen" << "unicodeNonBreakingHyphen" << "unicodeNonBreakingSpace" << "unicodeNarrowNoBreakSpace" << "unicodePageNumber" << "unicodePageCount";
	*actionNamesList << "unicodeSpaceEN" << "unicodeSpaceEM" << "unicodeSpaceThin" << "unicodeSpaceThick" << "unicodeSpaceMid" << "unicodeSpaceHair";
	//Breaks
	*actionNamesList << "unicodeNewLine" << "unicodeFrameBreak" << "unicodeColumnBreak" << "unicodeZerowidthSpace";
	//Copyrights and TMs
	*actionNamesList << "unicodeCopyRight" << "unicodeRegdTM" << "unicodeTM";
	//Slashes
	*actionNamesList << "unicodeSolidus";
	//Bullets
	*actionNamesList << "unicodeBullet" << "unicodeMidpoint";
	//Dashes
	*actionNamesList << "unicodeDashEm" << "unicodeDashEn" << "unicodeDashFigure" << "unicodeDashQuotation";
	//Straight quotes
	*actionNamesList << "unicodeQuoteApostrophe" << "unicodeQuoteStraight";
	//Double quotes
	*actionNamesList << "unicodeQuoteDoubleLeft" << "unicodeQuoteDoubleRight" << "unicodeQuoteSingleLeft" << "unicodeQuoteSingleRight";
	//Alternative single quotes
	*actionNamesList << "unicodeQuoteSingleReversed" << "unicodeQuoteDoubleReversed";
	//French quotes
	*actionNamesList << "unicodeQuoteSingleLeftGuillemet" << "unicodeQuoteSingleRightGuillemet" << "unicodeQuoteDoubleLeftGuillemet" << "unicodeQuoteDoubleRightGuillemet";
	//German quotes
	*actionNamesList << "unicodeQuoteLowSingleComma" << "unicodeQuoteLowDoubleComma";
	//CJK Quotes
	*actionNamesList << "unicodeQuoteCJKSingleLeft" << "unicodeQuoteCJKSingleRight" << "unicodeQuoteCJKDoubleLeft" << "unicodeQuoteCJKDoubleRight";
	//Ligatures
	*actionNamesList << "unicodeLigature_ff" << "unicodeLigature_fi" << "unicodeLigature_fl" << "unicodeLigature_ffi" << "unicodeLigature_ffl" << "unicodeLigature_ft" << "unicodeLigature_st";
	for (auto it = actionNamesList->cbegin(); it != actionNamesList->cend(); ++it)
		connect( (*actionMap)[*it], SIGNAL(triggeredUnicodeShortcut(int)), actionParent, SLOT(specialActionKeyEvent(int)) );
}

void ActionManager::initSpecialActions()
{
	QString name;
	//GUI
	name = "specialToggleAllGuides";
	scrActions->insert(name, new ScrAction("", defaultKey(name), mainWindow));
	name = "specialUnicodeSequenceBegin";
	scrActions->insert(name, new ScrAction( "", defaultKey(name), mainWindow));
	connect( (*scrActions)["specialToggleAllGuides"], SIGNAL(triggered()), mainWindow, SLOT(ToggleAllGuides()) );
}

void ActionManager::setActionTooltips(QMap<QString, QPointer<ScrAction> > *actionMap)
{
	for (auto it = actionMap->begin(); it != actionMap->end(); ++it)
		it.value()->setToolTipFromTextAndShortcut();
}

void ActionManager::disconnectModeActions()
{
	for (auto it = modeActionNames->cbegin(); it != modeActionNames->cend(); ++it)
		disconnect( (*scrActions)[*it], SIGNAL(toggledData(bool,int)) , mainWindow, SLOT(setAppModeByToggle(bool,int)) );
}

void ActionManager::connectModeActions()
{
	for (auto it = modeActionNames->cbegin(); it != modeActionNames->cend(); ++it)
		connect( (*scrActions)[*it], SIGNAL(toggledData(bool,int)) , mainWindow, SLOT(setAppModeByToggle(bool,int)) );
}

void ActionManager::disconnectNewDocActions()
{
	(*scrActions)["tableInsertRows"]->disconnect();
	(*scrActions)["tableInsertColumns"]->disconnect();
	(*scrActions)["tableDeleteRows"]->disconnect();
	(*scrActions)["tableDeleteColumns"]->disconnect();
	(*scrActions)["tableMergeCells"]->disconnect();
	(*scrActions)["tableSplitCells"]->disconnect();
	(*scrActions)["tableSetRowHeights"]->disconnect();
	(*scrActions)["tableSetColumnWidths"]->disconnect();
	(*scrActions)["tableDistributeRowsEvenly"]->disconnect();
	(*scrActions)["tableDistributeColumnsEvenly"]->disconnect();
	(*scrActions)["tableAdjustFrameToTable"]->disconnect();
	(*scrActions)["tableAdjustTableToFrame"]->disconnect();
	(*scrActions)["itemAdjustFrameHeightToText"]->disconnect();
	(*scrActions)["itemAdjustFrameToImage"]->disconnect();
	(*scrActions)["itemAdjustImageToFrame"]->disconnect();
	(*scrActions)["itemLock"]->disconnect();
	(*scrActions)["itemLockSize"]->disconnect();
	(*scrActions)["itemPrintingEnabled"]->disconnect();
	(*scrActions)["itemFlipH"]->disconnect();
	(*scrActions)["itemFlipV"]->disconnect();
	(*scrActions)["itemCombinePolygons"]->disconnect();
	(*scrActions)["itemSplitPolygons"]->disconnect();
	(*scrActions)["itemUpdateImage"]->disconnect();
	(*scrActions)["itemDelete"]->disconnect();
	(*scrActions)["extrasHyphenateText"]->disconnect();
	(*scrActions)["extrasDeHyphenateText"]->disconnect();
	(*scrActions)["itemsUnWeld"]->disconnect();
	(*scrActions)["itemWeld"]->disconnect();
	(*scrActions)["itemEditWeld"]->disconnect();
	(*scrActions)["itemLowerToBottom"]->disconnect();
	(*scrActions)["itemRaiseToTop"]->disconnect();
	(*scrActions)["itemLower"]->disconnect();
	(*scrActions)["itemRaise"]->disconnect();
	(*scrActions)["toolsUnlinkTextFrameAndCutText"]->disconnect();
}

void ActionManager::connectNewDocActions(ScribusDoc *currDoc)
{
	if (currDoc == nullptr)
		return;
	connect( (*scrActions)["itemLock"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_ToggleLock()) );
	connect( (*scrActions)["itemLockSize"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_ToggleSizeLock()));
	connect( (*scrActions)["itemPrintingEnabled"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_TogglePrintEnabled()));
	connect( (*scrActions)["itemFlipH"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_FlipH()));
	connect( (*scrActions)["itemFlipV"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_FlipV()));
	connect( (*scrActions)["itemCombinePolygons"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_UniteItems()) );
	connect( (*scrActions)["itemSplitPolygons"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_SplitItems()) );
	connect( (*scrActions)["itemUpdateImage"], SIGNAL(triggered()), currDoc, SLOT(updatePic()) );
	connect( (*scrActions)["extrasHyphenateText"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_DoHyphenate()) );
	connect( (*scrActions)["extrasDeHyphenateText"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_DoDeHyphenate()) );
	connect( (*scrActions)["itemDelete"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_DeleteItem()) );
	connect( (*scrActions)["tableInsertRows"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_InsertTableRows()));
	connect( (*scrActions)["tableInsertColumns"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_InsertTableColumns()));
	connect( (*scrActions)["tableDeleteRows"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_DeleteTableRows()));
	connect( (*scrActions)["tableDeleteColumns"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_DeleteTableColumns()));
	connect( (*scrActions)["tableMergeCells"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_MergeTableCells()));
	connect( (*scrActions)["tableSplitCells"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_SplitTableCells()));
	connect( (*scrActions)["tableSetRowHeights"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_SetTableRowHeights()));
	connect( (*scrActions)["tableSetColumnWidths"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_SetTableColumnWidths()));
	connect( (*scrActions)["tableDistributeRowsEvenly"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_DistributeTableRowsEvenly()));
	connect( (*scrActions)["tableDistributeColumnsEvenly"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_DistributeTableColumnsEvenly()));
	connect( (*scrActions)["tableAdjustFrameToTable"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_AdjustFrameToTable()));
	connect( (*scrActions)["tableAdjustTableToFrame"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_AdjustTableToFrame()));
	connect( (*scrActions)["itemAdjustFrameHeightToText"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_AdjustFrameHeightToText()) );
	connect( (*scrActions)["itemAdjustFrameToImage"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_AdjustFrametoImageSize()) );
	connect( (*scrActions)["itemAdjustImageToFrame"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_AdjustImagetoFrameSize()) );
	connect( (*scrActions)["itemsUnWeld"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_UnWeld()) );
	connect( (*scrActions)["itemWeld"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_Weld()) );
	connect( (*scrActions)["itemEditWeld"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_EditWeld()) );
	connect( (*scrActions)["itemLowerToBottom"], SIGNAL(triggered()), currDoc, SLOT(sendItemSelectionToBack()) );
	connect( (*scrActions)["itemRaiseToTop"], SIGNAL(triggered()), currDoc, SLOT(bringItemSelectionToFront()) );
	connect( (*scrActions)["itemLower"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_LowerItem()) );
	connect( (*scrActions)["itemRaise"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_RaiseItem()) );
	connect( (*scrActions)["toolsUnlinkTextFrameAndCutText"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_UnlinkTextFrameAndCutText()) );
}

void ActionManager::disconnectNewViewActions()
{
	//qDebug()<<"disconnectNewViewActions"<<mainWindow->doc->DocName;
	(*scrActions)["viewPreviewMode"]->disconnect();
	(*scrActions)["viewEditInPreview"]->disconnect();
	(*scrActions)["viewToggleWhiteSpaceMode"]->disconnect();
	(*scrActions)["viewToggleCMS"]->disconnect();
	(*scrActions)["toolsZoomIn"]->disconnect();
	(*scrActions)["toolsZoomOut"]->disconnect();
	(*scrActions)["itemImageIsVisible"]->disconnect();
	(*scrActions)["itemPreviewFull"]->disconnect(SIGNAL(triggeredData(int)));
	(*scrActions)["itemPreviewNormal"]->disconnect(SIGNAL(triggeredData(int)));
	(*scrActions)["itemPreviewLow"]->disconnect(SIGNAL(triggeredData(int)));
	(*scrActions)["itemConvertToBezierCurve"]->disconnect();
	(*scrActions)["itemConvertToImageFrame"]->disconnect();
	(*scrActions)["itemConvertToOutlines"]->disconnect();
	(*scrActions)["itemConvertToPolygon"]->disconnect();
	(*scrActions)["itemConvertToTextFrame"]->disconnect();
	(*scrActions)["itemAttachTextToPath"]->disconnect();
	(*scrActions)["itemDetachTextFromPath"]->disconnect();
	(*scrActions)["itemExtendedImageProperties"]->disconnect();
}

void ActionManager::connectNewViewActions(ScribusView *currView)
{
	if (currView == nullptr)
		return;
	//qDebug()<<"connectNewViewActions"<<currView->Doc->DocName;
	connect( (*scrActions)["viewPreviewMode"], SIGNAL(toggled(bool)), currView, SLOT(togglePreview(bool)) );
	connect( (*scrActions)["viewEditInPreview"], SIGNAL(toggled(bool)), currView, SLOT(togglePreviewEdit(bool)) );
	connect( (*scrActions)["viewToggleWhiteSpaceMode"], SIGNAL(toggled(bool)), currView, SLOT(toggleWhiteSpaceMode(bool)) );
	connect( (*scrActions)["viewToggleCMS"], SIGNAL(toggled(bool)), currView, SLOT(toggleCMS(bool)));
	connect( (*scrActions)["toolsZoomIn"], SIGNAL(triggered()) , currView, SLOT(slotZoomIn()) );
	connect( (*scrActions)["toolsZoomOut"], SIGNAL(triggered()) , currView, SLOT(slotZoomOut()) );
	connect( (*scrActions)["itemConvertToBezierCurve"], SIGNAL(triggered()), currView, SLOT(ToBezierFrame()) );
	connect( (*scrActions)["itemConvertToImageFrame"], SIGNAL(triggered()), currView, SLOT(ToPicFrame()) );
	connect( (*scrActions)["itemConvertToOutlines"], SIGNAL(triggered()), currView, SLOT(TextToPath()) );
	connect( (*scrActions)["itemConvertToPolygon"], SIGNAL(triggered()), currView, SLOT(ToPolyFrame()) );
	connect( (*scrActions)["itemConvertToTextFrame"], SIGNAL(triggered()), currView, SLOT(ToTextFrame()) );
	connect( (*scrActions)["itemAttachTextToPath"], SIGNAL(triggered()), currView, SLOT(ToPathText()) );
	connect( (*scrActions)["itemDetachTextFromPath"], SIGNAL(triggered()), currView, SLOT(FromPathText()) );
	connect( (*scrActions)["itemExtendedImageProperties"], SIGNAL(triggered()), currView, SLOT(editExtendedImageProperties()) );
}

void ActionManager::disconnectNewSelectionActions()
{
	(*scrActions)["itemImageIsVisible"]->disconnect();
	//Only disconnect triggeredData for data based actions or you will disconnect the internal signal
	(*scrActions)["itemPreviewFull"]->disconnect(SIGNAL(triggeredData(int)));
	(*scrActions)["itemPreviewNormal"]->disconnect(SIGNAL(triggeredData(int)));
	(*scrActions)["itemPreviewLow"]->disconnect(SIGNAL(triggeredData(int)));
	(*scrActions)["editClearContents"]->disconnect();
	(*scrActions)["editTruncateContents"]->disconnect();
}

void ActionManager::connectNewSelectionActions(ScribusView* /*currView*/, ScribusDoc* currDoc)
{
	connect( (*scrActions)["itemImageIsVisible"], SIGNAL(toggled(bool)), currDoc, SLOT(itemSelection_ToggleImageShown()) );
	connect( (*scrActions)["itemPreviewFull"], SIGNAL(triggeredData(int)), currDoc, SLOT(itemSelection_ChangePreviewResolution(int)) );
	connect( (*scrActions)["itemPreviewNormal"], SIGNAL(triggeredData(int)), currDoc, SLOT(itemSelection_ChangePreviewResolution(int)) );
	connect( (*scrActions)["itemPreviewLow"], SIGNAL(triggeredData(int)), currDoc, SLOT(itemSelection_ChangePreviewResolution(int)) );
	connect( (*scrActions)["editClearContents"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_ClearItem()) );
	connect( (*scrActions)["editTruncateContents"], SIGNAL(triggered()), currDoc, SLOT(itemSelection_TruncateItem()) );
}

void ActionManager::saveActionShortcutsPreEditMode()
{
	for (auto it = modeActionNames->cbegin(); it != modeActionNames->cend(); ++it)
	{
		(*scrActions)[*it]->setShortcutContext(Qt::WidgetShortcut);  // in theory, this should be enough, but...
		(*scrActions)[*it]->saveShortcut();
#ifdef Q_OS_MACOS
		if ((*scrActions)[*it]->menu() != nullptr)
			(*scrActions)[*it]->setEnabled(false);
#endif		
	}
	for (auto it = nonEditActionNames->cbegin(); it != nonEditActionNames->cend(); ++it)
	{
		(*scrActions)[*it]->setShortcutContext(Qt::WidgetShortcut);  // in theory, this should be enough, but...
		(*scrActions)[*it]->saveShortcut();
	}
}

void ActionManager::restoreActionShortcutsPostEditMode()
{
	for (auto it = modeActionNames->cbegin(); it != modeActionNames->cend(); ++it)
	{
		(*scrActions)[*it]->setShortcutContext(Qt::WindowShortcut);  // see above
		(*scrActions)[*it]->restoreShortcut();
#ifdef Q_OS_MACOS
		(*scrActions)[*it]->setEnabled(true);
#endif		
	}
	for (auto it = nonEditActionNames->cbegin(); it != nonEditActionNames->cend(); ++it)
	{
		(*scrActions)[*it]->setShortcutContext(Qt::WindowShortcut);  // see above
		(*scrActions)[*it]->restoreShortcut();
	}
}

void ActionManager::enableActionStringList(QMap<QString, QPointer<ScrAction> > *actionMap, QStringList *list, bool enabled, bool checkingUnicode, const QString& fontName)
{
	for (auto it = list->cbegin(); it != list->cend(); ++it)
	{
		if (!checkingUnicode)
			(*actionMap)[*it]->setEnabled(enabled);
		else
		{
			//For UnicodeChar actions, only enable when the current font has that character.
			if (mainWindow->HaveDoc && (*actionMap)[*it]->actionType() == ScrAction::UnicodeChar)
			{
				int charCode = (*actionMap)[*it]->actionInt();
				if (charCode == -1 ||
					charCode == 23 ||
					charCode == 24 ||
					charCode == 26 ||
					charCode == 27 ||
					charCode == 28 ||
					charCode == 29 ||
					charCode == 30 ||
					((*mainWindow->doc->AllFonts)[fontName].usable() &&
					(*mainWindow->doc->AllFonts)[fontName].canRender( QChar(charCode) )) )
						(*actionMap)[*it]->setEnabled(true);
				else
					(*actionMap)[*it]->setEnabled(false);
			}
		}
	}
}

void ActionManager::enableUnicodeActions(QMap<QString, QPointer<ScrAction> > *actionMap, bool enabled, const QString& fontName)
{
	enableActionStringList(actionMap, unicodeCharActionNames, enabled, enabled, fontName);
	(*actionMap)["insertGlyph"]->setEnabled(enabled);
}

void ActionManager::setPDFActions(ScribusView *currView)
{
	if (currView == nullptr)
		return;
	PageItem* currItem = mainWindow->doc->m_Selection->itemAt(0);
	if (currItem == nullptr)
		return;

	(*scrActions)["itemPDFIsBookmark"]->disconnect();
	(*scrActions)["itemPDFIsAnnotation"]->disconnect();

	if (!currItem->isTextFrame())
	{
		(*scrActions)["itemPDFIsAnnotation"]->setEnabled(false);
		(*scrActions)["itemPDFIsBookmark"]->setEnabled(false);
		(*scrActions)["itemPDFIsAnnotation"]->setChecked(false);
		(*scrActions)["itemPDFIsBookmark"]->setChecked(false);
		(*scrActions)["itemPDFAnnotationProps"]->setEnabled(false);
		(*scrActions)["itemPDFFieldProps"]->setEnabled(false);
		return;
	}

	(*scrActions)["itemPDFIsAnnotation"]->setEnabled(true);
	(*scrActions)["itemPDFIsBookmark"]->setEnabled(true);
	(*scrActions)["itemPDFIsAnnotation"]->setChecked(currItem->isAnnotation());
	(*scrActions)["itemPDFIsBookmark"]->setChecked(currItem->isBookmark);
	if (currItem->isAnnotation())
	{
		int aType = currItem->annotation().Type();
		bool setter = ((aType == 0) || (aType == 1) || ((aType > Annotation::Listbox) && (aType < Annotation::Annot3D)));
		(*scrActions)["itemPDFAnnotationProps"]->setEnabled(setter);
		(*scrActions)["itemPDFFieldProps"]->setEnabled(!setter);
	}
	else
	{
		(*scrActions)["itemPDFAnnotationProps"]->setEnabled(false);
		(*scrActions)["itemPDFFieldProps"]->setEnabled(false);
	}
	connect( (*scrActions)["itemPDFIsAnnotation"], SIGNAL(triggered()), mainWindow->doc, SLOT(itemSelection_ToggleAnnotation()) );
	connect( (*scrActions)["itemPDFIsBookmark"], SIGNAL(triggered()), mainWindow->doc, SLOT(itemSelection_ToggleBookMark()) );
}

void ActionManager::changeEvent(QEvent *e)
{
	if (e->type() == QEvent::LanguageChange)
	{
		languageChange();
	}
}

void ActionManager::languageChange()
{
	createDefaultMenuNames();
	createDefaultNonMenuNames();

	//File Menu
	(*scrActions)["fileNew"]->setTexts( tr("&New..."));
	(*scrActions)["fileNewFromTemplate"]->setTexts( tr("New &from Template..."));
	(*scrActions)["fileOpen"]->setTexts( tr("&Open..."));
	(*scrActions)["fileClose"]->setTexts( tr("&Close"));
	(*scrActions)["fileSave"]->setTexts( tr("&Save"));
	(*scrActions)["fileSaveAs"]->setTexts( tr("Save &As..."));
	(*scrActions)["fileRevert"]->setTexts( tr("Re&vert to Saved"));
	(*scrActions)["fileCollect"]->setTexts( tr("Collect for O&utput..."));
	(*scrActions)["fileImportText"]->setTexts( tr("Get Text..."));
	(*scrActions)["fileImportAppendText"]->setTexts( tr("Append &Text..."));
	(*scrActions)["fileImportImage"]->setTexts( tr("Get Image..."));
	(*scrActions)["fileImportVector"]->setTexts( tr("Get Vector File..."));

	(*scrActions)["fileExportText"]->setTexts( tr("Save &Text..."));
	(*scrActions)["fileExportAsEPS"]->setTexts( tr("Save as &EPS..."));
	(*scrActions)["fileExportAsPDF"]->setTexts( tr("Save as P&DF..."));
//	(*scrActions)["fileDocSetup"]->setTexts( tr("Document &Setup (old)..."));
	(*scrActions)["fileDocSetup150"]->setTexts( tr("Document &Setup..."));
//	(*scrActions)["filePreferences"]->setTexts( tr("P&references (old)..."));
	(*scrActions)["filePreferences150"]->setTexts( tr("P&references..."));
	(*scrActions)["filePrint"]->setTexts( tr("&Print..."));
	(*scrActions)["PrintPreview"]->setTexts( tr("Print Previe&w..."));
	(*scrActions)["OutputPreviewPDF"]->setTexts( tr("PDF..."));
	(*scrActions)["OutputPreviewPS"]->setTexts( tr("PostScript..."));
	(*scrActions)["fileQuit"]->setTexts( tr("&Quit"));
	//Edit Menu
	(*scrActions)["editUndoAction"]->setTexts( tr("&Undo"));
	(*scrActions)["editRedoAction"]->setTexts( tr("&Redo"));
	(*scrActions)["editActionMode"]->setTexts( tr("&Item Action Mode"));
	(*scrActions)["editCut"]->setTexts( tr("Cu&t"));
	(*scrActions)["editCopy"]->setTexts( tr("&Copy"));
	(*scrActions)["editPaste"]->setTexts( tr("&Paste"));
	(*scrActions)["editPastePlainText"]->setTexts( tr("Paste P&lain Text"));
	(*scrActions)["editCopyContents"]->setTexts( tr("&Copy"));
	(*scrActions)["editPasteContents"]->setTexts( tr("&Paste"));
	(*scrActions)["editPasteContentsAbs"]->setTexts( tr("Paste (&Absolute)"));
	(*scrActions)["editPasteImageFromClipboard"]->setTexts( tr("Paste Image from Clipboard"));
	(*scrActions)["editClearContents"]->setTexts( tr("C&lear"));
	(*scrActions)["editTruncateContents"]->setTexts( tr("T&runcate"));
	(*scrActions)["editSelectAll"]->setTexts( tr("Select &All"));
	(*scrActions)["editSelectAllOnLayer"]->setTexts( tr("Advanced Select All..."));
	(*scrActions)["editDeselectAll"]->setTexts( tr("&Deselect All"));
	(*scrActions)["editSearchReplace"]->setTexts( tr("&Search/Replace..."));
	(*scrActions)["editEditWithImageEditor"]->setTexts( tr("Edit Image..."));
	(*scrActions)["editEditRenderSource"]->setTexts( tr("Edit Source..."));
	(*scrActions)["editColorsAndFills"]->setTexts( tr("Colors and Fills..."));
	(*scrActions)["editReplaceColors"]->setTexts( tr("Replace Colors..."));
	(*scrActions)["editStyles"]->setTexts( tr("S&tyles..."));
	(*scrActions)["editMarks"]->setTexts( tr("Marks..."));
	(*scrActions)["editNotesStyles"]->setTexts( tr("Notes Styles..."));
	(*scrActions)["editMasterPages"]->setTexts( tr("&Master Pages..."));
	(*scrActions)["editJavascripts"]->setTexts( tr("&JavaScripts..."));

	std::array<int, 12> font_sizes = { 7, 9, 10, 11, 12, 14, 18, 24, 36, 48, 60, 72 };
	for (auto fontSize : font_sizes)
	{
		QString fontSizeName = QString("fontSize%1").arg(fontSize);
		(*scrActions)[fontSizeName]->setTexts( tr("%1 pt").arg(fontSize));
	}
	(*scrActions)["fontSizeOther"]->setTexts( tr("&Other..."));

	(*scrActions)["itemStyleSearch"]->setTexts( tr("Style &Search"));

	(*scrActions)["alignLeft"]->setTexts( tr("&Left"));
	(*scrActions)["alignCenter"]->setTexts( tr("&Center"));
	(*scrActions)["alignRight"]->setTexts( tr("&Right"));
	(*scrActions)["alignBlock"]->setTexts( tr("&Block"));
	(*scrActions)["alignForced"]->setTexts( tr("&Forced"));

	(*scrActions)["typeEffectNormal"]->setTexts( tr("&Normal"));
	(*scrActions)["typeEffectUnderline"]->setTexts( tr("&Underline"));
	(*scrActions)["typeEffectUnderlineWords"]->setTexts( tr("Underline &Words"));
	(*scrActions)["typeEffectStrikeThrough"]->setTexts( tr("&Strike Through"));
	(*scrActions)["typeEffectAllCaps"]->setTexts( tr("&All Caps"));
	(*scrActions)["typeEffectSmallCaps"]->setTexts( tr("Small &Caps"));
	(*scrActions)["typeEffectSuperscript"]->setTexts( tr("Su&perscript"));
	(*scrActions)["typeEffectSubscript"]->setTexts( tr("Su&bscript"));
	(*scrActions)["typeEffectOutline"]->setTexts( tr("&Outline", "type effect"));
	(*scrActions)["typeEffectShadow"]->setTexts( tr("S&hadow"));

	(*scrActions)["styleImageEffects"]->setTexts( tr("&Image Effects..."));
	(*scrActions)["styleTabulators"]->setTexts( tr("&Tabulators..."));

	//Item Menu
	(*scrActions)["itemDuplicate"]->setTexts( tr("D&uplicate"));
	(*scrActions)["itemMulDuplicate"]->setTexts( tr("&Multiple Duplicate..."));
	(*scrActions)["itemTransform"]->setTexts( tr("&Transform..."));
	(*scrActions)["itemDelete"]->setTexts( tr("&Delete"));
	(*scrActions)["itemGroup"]->setTexts( tr("&Group"));
	(*scrActions)["itemUngroup"]->setTexts( tr("&Ungroup"));
	(*scrActions)["itemGroupAdjust"]->setTexts( tr("Adjust Group"));
	(*scrActions)["itemLock"]->setTexts( tr("Is &Locked"));
	(*scrActions)["itemLockSize"]->setTexts( tr("Si&ze is Locked"));
	(*scrActions)["itemPrintingEnabled"]->setTexts( tr("&Printing Enabled"));
	(*scrActions)["itemFlipH"]->setTexts( tr("&Flip Horizontally"));
	(*scrActions)["itemFlipV"]->setTexts( tr("&Flip Vertically"));
	(*scrActions)["itemLowerToBottom"]->setTexts( tr("Lower to &Bottom"));
	(*scrActions)["itemRaiseToTop"]->setTexts( tr("Raise to &Top"));
	(*scrActions)["itemLower"]->setTexts( tr("&Lower"));
	(*scrActions)["itemRaise"]->setTexts( tr("&Raise"));
	(*scrActions)["itemSendToPattern"]->setTexts( tr("Patterns"));
	(*scrActions)["itemSendToInline"]->setTexts( tr("Inline Items"));
	(*scrActions)["itemAttributes"]->setTexts( tr("&Attributes..."));
	(*scrActions)["itemImageInfo"]->setTexts( tr("More Info..."));
	(*scrActions)["itemImageIsVisible"]->setTexts( tr("I&mage Visible"));
	(*scrActions)["itemUpdateImage"]->setTexts( tr("&Update Image"));
	(*scrActions)["tableInsertRows"]->setTexts(tr("Insert Rows..."));
	(*scrActions)["tableInsertColumns"]->setTexts(tr("Insert Columns..."));
	(*scrActions)["tableDeleteRows"]->setTexts(tr("Delete Rows"));
	(*scrActions)["tableDeleteColumns"]->setTexts(tr("Delete Columns"));
	(*scrActions)["tableMergeCells"]->setTexts(tr("Merge Cells"));
	(*scrActions)["tableSplitCells"]->setTexts(tr("Split Cells..."));
	(*scrActions)["tableSetRowHeights"]->setTexts(tr("Set Row Heights..."));
	(*scrActions)["tableSetColumnWidths"]->setTexts(tr("Set Column Widths..."));
	(*scrActions)["tableDistributeRowsEvenly"]->setTexts(tr("Distribute Rows Evenly"));
	(*scrActions)["tableDistributeColumnsEvenly"]->setTexts(tr("Distribute Columns Evenly"));
	(*scrActions)["tableAdjustFrameToTable"]->setTexts(tr("Adjust Frame to Table"));
	(*scrActions)["tableAdjustTableToFrame"]->setTexts(tr("Adjust Table to Frame"));
	(*scrActions)["itemAdjustFrameHeightToText"]->setTexts( tr("Adjust Frame Height to Text"));
	(*scrActions)["itemAdjustFrameToImage"]->setTexts( tr("Adjust Frame to Image"));
	(*scrActions)["itemAdjustImageToFrame"]->setTexts( tr("Adjust Image to Frame"));
	(*scrActions)["itemToggleInlineImage"]->setTexts( tr("Embed Image"));
	(*scrActions)["itemExtendedImageProperties"]->setTexts( tr("Extended Image Properties..."));
	(*scrActions)["itemPreviewFull"]->setTexts( tr("&Full Resolution"));
	(*scrActions)["itemPreviewNormal"]->setTexts( tr("&Normal Resolution"));
	(*scrActions)["itemPreviewLow"]->setTexts( tr("&Low Resolution"));
	(*scrActions)["itemPDFIsBookmark"]->setTexts( tr("Is PDF &Bookmark"));
	(*scrActions)["itemPDFIsAnnotation"]->setTexts( tr("Is PDF A&nnotation"));
	(*scrActions)["itemPDFAnnotationProps"]->setTexts( tr("Annotation P&roperties..."));
	(*scrActions)["itemPDFFieldProps"]->setTexts( tr("Field P&roperties..."));
	(*scrActions)["itemShapeEdit"]->setTexts( tr("&Edit Shape..."));
	(*scrActions)["itemAttachTextToPath"]->setTexts( tr("&Attach Text to Path"));
	(*scrActions)["itemDetachTextFromPath"]->setTexts( tr("&Detach Text from Path"));
	(*scrActions)["itemCombinePolygons"]->setTexts( tr("&Combine Polygons"));
	(*scrActions)["itemSplitPolygons"]->setTexts( tr("Split &Polygons"));
	(*scrActions)["itemConvertToBezierCurve"]->setTexts( tr("&Bezier Curve"));
	(*scrActions)["itemConvertToImageFrame"]->setTexts( tr("&Image Frame"));
	(*scrActions)["itemConvertToOutlines"]->setTexts( tr("&Outlines", "Convert to outlines"));
	(*scrActions)["itemConvertToPolygon"]->setTexts( tr("&Polygon"));
	(*scrActions)["itemConvertToTextFrame"]->setTexts( tr("&Text Frame"));
	(*scrActions)["itemConvertToSymbolFrame"]->setTexts( tr("&Symbol"));
	(*scrActions)["itemsUnWeld"]->setTexts( tr("Unweld Items"));
	(*scrActions)["itemWeld"]->setTexts( tr("Weld Items"));
	(*scrActions)["itemEditWeld"]->setTexts( tr("Edit Weld"));

	//Insert Menu
	(*scrActions)["insertFrame"]->setTexts( tr("&Frames..."));
	(*scrActions)["insertGlyph"]->setTexts( tr("&Glyph..."));
	(*scrActions)["insertSampleText"]->setTexts( tr("Sample Text..."));
	(*scrActions)["stickyTools"]->setTexts( tr("Sticky Tools"));
	(*scrActions)["insertMarkAnchor"]->setTexts( tr("Anchor Mark"));
	(*scrActions)["insertMarkVariableText"]->setTexts( tr("Variable Text"));
	(*scrActions)["insertMarkItem"]->setTexts( tr("Reference to Item"));
	(*scrActions)["insertMark2Mark"]->setTexts( tr("Reference to Mark"));
	(*scrActions)["insertMarkNote"]->setTexts( tr("Foot/Endnote"));
	(*scrActions)["insertMarkIndex"]->setTexts( tr("Index Entry"));
	(*scrActions)["editMark"]->setTexts( tr("Edit Mark"));
	(*scrActions)["itemUpdateMarks"]->setTexts( tr("Update Marks"));

	//Page menu
	(*scrActions)["pageInsert"]->setTexts( tr("&Insert..."));
	(*scrActions)["pageImport"]->setTexts( tr("Im&port..."));
	(*scrActions)["pageDelete"]->setTexts( tr("&Delete..."));
	(*scrActions)["pageCopy"]->setTexts( tr("&Copy..."));
	(*scrActions)["pageMove"]->setTexts( tr("&Move..."));
	(*scrActions)["pageApplyMasterPage"]->setTexts( tr("&Apply Master Page..."));
	(*scrActions)["pageCopyToMasterPage"]->setTexts( tr("Convert to Master Page..."));
	(*scrActions)["pageManageGuides"]->setTexts( tr("Manage &Guides..."));
	(*scrActions)["pageManageProperties"]->setTexts( tr("Manage Page Properties..."));

	//View Menu
	(*scrActions)["viewFitInWindow"]->setTexts( tr("&Fit to Height"));
	(*scrActions)["viewFitWidth"]->setTexts( tr("Fit to Width"));
	(*scrActions)["viewFit50"]->setTexts( tr("&50%"));
	(*scrActions)["viewFit75"]->setTexts( tr("&75%"));
	(*scrActions)["viewFit100"]->setTexts( tr("&100%"));
	(*scrActions)["viewFit200"]->setTexts( tr("&200%"));
	(*scrActions)["viewFit400"]->setTexts( tr("&400%"));
	(*scrActions)["viewPreviewMode"]->setTexts( tr("Preview Mode"));
	(*scrActions)["viewEditInPreview"]->setTexts( tr("Edit in Preview Mode") );
	(*scrActions)["viewToggleWhiteSpaceMode"]->setTexts( tr("Toggle White Space Review Mode") );
	(*scrActions)["viewToggleCMS"]->setTexts( tr("Toggle Color Management System"));
	(*scrActions)["viewShowMargins"]->setTexts( tr("Show &Margins"));
	(*scrActions)["viewShowBleeds"]->setTexts( tr("Show Bleeds"));
	(*scrActions)["viewShowFrames"]->setTexts( tr("Show &Frames"));
	(*scrActions)["viewShowLayerMarkers"]->setTexts( tr("Show Layer Indicators"));
	(*scrActions)["viewShowImages"]->setTexts( tr("Show &Images"));
	(*scrActions)["viewShowGrid"]->setTexts( tr("Show &Grid"));
	(*scrActions)["viewShowGuides"]->setTexts( tr("Show G&uides"));
	(*scrActions)["viewShowColumnBorders"]->setTexts( tr("Show Text Frame Columns"));
	(*scrActions)["viewShowBaseline"]->setTexts( tr("Show &Baseline Grid"));
	(*scrActions)["viewShowTextChain"]->setTexts( tr("Show &Text Chain"));
	(*scrActions)["viewShowTextControls"]->setTexts( tr("Show Control Characters"));
	(*scrActions)["viewShowRulers"]->setTexts( tr("Show Rulers"));
	(*scrActions)["viewRulerMode"]->setTexts( tr("Rulers Relative to Page"));
	(*scrActions)["viewSnapToGrid"]->setTexts( tr("Sn&ap to Grid"));
	(*scrActions)["viewSnapToGuides"]->setTexts( tr("Sna&p to Guides"));
	(*scrActions)["viewSnapToElements"]->setTexts( tr("Snap to Items"));
	(*scrActions)["viewShowContextMenu"]->setTexts( tr("Show Context Menu"));
//	(*scrActions)["viewNewView"]->setTexts( tr("New View"));

	//Tool menu
	(*scrActions)["toolsProperties"]->setTexts( tr("&Properties"));
	(*scrActions)["toolsContent"]->setTexts( tr("Content Properties"));
	(*scrActions)["toolsOutline"]->setTexts( tr("&Outline", "Document Outline Palette"));
	(*scrActions)["toolsScrapbook"]->setTexts( tr("&Scrapbook"));
	(*scrActions)["toolsLayers"]->setTexts( tr("&Layers"));
	(*scrActions)["toolsPages"]->setTexts( tr("&Arrange Pages"));
	(*scrActions)["toolsBookmarks"]->setTexts( tr("&Bookmarks"));
	(*scrActions)["toolsDownloads"]->setTexts( tr("&Downloads"));
	(*scrActions)["toolsResources"]->setTexts( tr("&Resources"));
	(*scrActions)["toolsMeasurements"]->setTexts( tr("&Measurements"));
	(*scrActions)["toolsActionHistory"]->setTexts( tr("Action &History"));
	(*scrActions)["toolsPreflightVerifier"]->setTexts( tr("Preflight &Verifier"));
	(*scrActions)["toolsAlignDistribute"]->setTexts( tr("&Align and Distribute"));
	(*scrActions)["toolsSymbols"]->setTexts( tr("Symbols"));
	(*scrActions)["toolsInline"]->setTexts( tr("Inline Items"));
	(*scrActions)["toolsToolbarTools"]->setTexts( tr("&Tools"));
	(*scrActions)["toolsToolbarPDF"]->setTexts( tr("P&DF Tools"));
	(*scrActions)["toolsToolbarView"]->setTexts( tr("&View Tools"));

	//toolbar only items
	(*scrActions)["toolsSelect"]->setTexts( tr("Select Item"));
	(*scrActions)["toolsRotate"]->setTexts( tr("Rotate Item"));
	(*scrActions)["toolsZoom"]->setTexts( tr("Zoom in or out"));
	(*scrActions)["toolsZoomIn"]->setTexts( tr("Zoom in"));
	(*scrActions)["toolsZoomOut"]->setTexts( tr("Zoom out"));
	(*scrActions)["toolsEditContents"]->setTexts( tr("Edit Contents of Frame"));
	(*scrActions)["toolsEditWithStoryEditor"]->setText( tr("Edit Text with Story Editor..."));
	(*scrActions)["toolsLinkTextFrame"]->setTexts( tr("Link Text Frames"));
	(*scrActions)["toolsUnlinkTextFrame"]->setTexts( tr("Unlink Text Frames"));
	(*scrActions)["toolsUnlinkTextFrameAndCutText"]->setTexts( tr("Unlink Text Frames and Cut Text"));
	(*scrActions)["toolsEyeDropper"]->setTexts( tr("&Eye Dropper"));
	(*scrActions)["toolsCopyProperties"]->setTexts( tr("Copy Item Properties"));

	(*scrActions)["toolsInsertTextFrame"]->setText( tr("&Text Frame"));
	(*scrActions)["toolsInsertImageFrame"]->setText( tr("&Image Frame"));
	(*scrActions)["toolsInsertRenderFrame"]->setText( tr("&Render Frame"));
	(*scrActions)["toolsInsertTable"]->setText( tr("T&able"));
	(*scrActions)["toolsInsertShape"]->setText( tr("&Shape"));
	(*scrActions)["toolsInsertPolygon"]->setText( tr("&Polygon"));
	(*scrActions)["toolsInsertArc"]->setText( tr("Arc"));
	(*scrActions)["toolsInsertSpiral"]->setText( tr("Spiral"));
	(*scrActions)["toolsInsertLine"]->setText( tr("&Line"));
	(*scrActions)["toolsInsertCalligraphicLine"]->setText( tr("Calligraphic Line"));
	(*scrActions)["toolsInsertBezier"]->setText( tr("&Bezier Curve"));
	(*scrActions)["toolsInsertFreehandLine"]->setText( tr("&Freehand Line"));

	(*scrActions)["toolsPDFPushButton"]->setTexts( tr("PDF Push Button"));
	(*scrActions)["toolsPDFRadioButton"]->setTexts( tr("PDF Radio Button"));
	(*scrActions)["toolsPDFTextField"]->setTexts( tr("PDF Text Field"));
	(*scrActions)["toolsPDFCheckBox"]->setTexts( tr("PDF Check Box"));
	(*scrActions)["toolsPDFComboBox"]->setTexts( tr("PDF Combo Box"));
	(*scrActions)["toolsPDFListBox"]->setTexts( tr("PDF List Box"));
	(*scrActions)["toolsPDFAnnotText"]->setTexts( tr("Text Annotation"));
	(*scrActions)["toolsPDFAnnotLink"]->setTexts( tr("Link Annotation"));
#ifdef HAVE_OSG
	(*scrActions)["toolsPDFAnnot3D"]->setTexts( tr("3D Annotation"));
#endif

	//Extras Menu
	(*scrActions)["extrasManageImages"]->setTexts( tr("&Manage Images..."));
	(*scrActions)["extrasHyphenateText"]->setTexts( tr("&Hyphenate Text"));
	(*scrActions)["extrasDeHyphenateText"]->setTexts( tr("Dehyphenate Text"));
	(*scrActions)["extrasGenerateTableOfContents"]->setTexts( tr("&Generate Table Of Contents and Indexes"));
	(*scrActions)["extrasUpdateDocument"]->setTexts( tr("&Update Document"));
	//(*scrActions)["extrasTestQTQuick2_1"]->setTexts( tr("Test Qt Quick"));
	//Windows Menu
	(*scrActions)["windowsCascade"]->setText( tr("&Cascade"));
	(*scrActions)["windowsTile"]->setText( tr("&Tile"));
	(*scrActions)["specialToggleAllPalettes"]->setTexts( tr("Toggle Palettes"));

	//Help Menu
	(*scrActions)["helpAboutScribus"]->setTexts( tr("&About Scribus"));
	(*scrActions)["helpAboutPlugins"]->setTexts( tr("&About Plugins"));
	(*scrActions)["helpAboutQt"]->setTexts( tr("About &Qt"));
	(*scrActions)["helpActionSearch"]->setTexts( tr("Action &Search"));
	(*scrActions)["helpTooltips"]->setTexts( tr("Toolti&ps"));
	(*scrActions)["showMouseCoordinates"]->setTexts( tr("Move/Resize Value Indicator"));
	(*scrActions)["helpManual"]->setTexts( tr("Scribus &Help..."));
	(*scrActions)["helpOnlineWWW"]->setTexts( tr("Scribus Homepage"));
	(*scrActions)["helpOnlineDocs"]->setTexts( tr("Scribus Online Documentation"));
	(*scrActions)["helpOnlineWiki"]->setTexts( tr("Scribus Wiki"));
	(*scrActions)["helpOnlineTutorial1"]->setTexts( tr("Getting Started with Scribus"));
	(*scrActions)["helpCheckUpdates"]->setTexts( tr("Check for Updates"));
	(*scrActions)["helpChat"]->setTexts( tr("Chat with the Community"));

	//GUI and specials
	(*scrActions)["specialToggleAllGuides"]->setTexts( tr("Toggle Guides"));
	(*scrActions)["specialUnicodeSequenceBegin"]->setTexts( tr("Insert Unicode Character Begin Sequence"));


	//Status texts for toolbar items
	(*scrActions)["editCopy"]->setStatusTextAndShortcut( tr("Copy"));
	(*scrActions)["editCut"]->setStatusTextAndShortcut( tr("Cut"));
	(*scrActions)["editPaste"]->setStatusTextAndShortcut( tr("Paste"));
	(*scrActions)["editPastePlainText"]->setStatusTextAndShortcut( tr("Paste Unformatted"));
	(*scrActions)["editRedoAction"]->setStatusTextAndShortcut( tr("Redo"));
	(*scrActions)["editUndoAction"]->setStatusTextAndShortcut( tr("Undo"));
	(*scrActions)["fileClose"]->setStatusTextAndShortcut( tr("Close the current document"));
	(*scrActions)["fileExportAsPDF"]->setStatusTextAndShortcut( tr("Export the document to PDF"));
	(*scrActions)["fileNew"]->setStatusTextAndShortcut( tr("Create a new document"));
	(*scrActions)["fileOpen"]->setStatusTextAndShortcut( tr("Open an existing document"));
	(*scrActions)["filePrint"]->setStatusTextAndShortcut( tr("Print the document"));
	(*scrActions)["fileSave"]->setStatusTextAndShortcut( tr("Save the current document"));
	(*scrActions)["toolsCopyProperties"]->setStatusTextAndShortcut( tr("Copy item properties"));
	(*scrActions)["toolsEditContents"]->setStatusTextAndShortcut( tr("Edit contents of a frame"));
	(*scrActions)["toolsEditWithStoryEditor"]->setStatusTextAndShortcut( tr("Edit text in story editor"));
	(*scrActions)["toolsEyeDropper"]->setStatusTextAndShortcut( tr("Eye Dropper"));
	(*scrActions)["toolsInsertArc"]->setStatusTextAndShortcut( tr("Insert an arc"));
	(*scrActions)["toolsInsertBezier"]->setStatusTextAndShortcut( tr("Insert a bezier curve"));
	(*scrActions)["toolsInsertCalligraphicLine"]->setStatusTextAndShortcut( tr("Insert a calligraphic line"));
	(*scrActions)["toolsInsertFreehandLine"]->setStatusTextAndShortcut( tr("Insert a freehand line"));
	(*scrActions)["toolsInsertImageFrame"]->setStatusTextAndShortcut( tr("Insert an image frame"));
	(*scrActions)["toolsInsertLine"]->setStatusTextAndShortcut( tr("Insert a line"));
	(*scrActions)["toolsInsertPolygon"]->setStatusTextAndShortcut( tr("Insert a polygon"));
	(*scrActions)["toolsInsertRenderFrame"]->setStatusTextAndShortcut( tr("Insert a render frame"));
	(*scrActions)["toolsInsertShape"]->setStatusTextAndShortcut( tr("Insert a shape"));
	(*scrActions)["toolsInsertSpiral"]->setStatusTextAndShortcut( tr("Insert a spiral"));
	(*scrActions)["toolsInsertTable"]->setStatusTextAndShortcut( tr("Insert a table"));
	(*scrActions)["toolsInsertTextFrame"]->setStatusTextAndShortcut( tr("Insert a text frame"));
	(*scrActions)["toolsLinkTextFrame"]->setStatusTextAndShortcut( tr("Link text frames"));
	(*scrActions)["toolsMeasurements"]->setStatusTextAndShortcut( tr("Measurements"));
	(*scrActions)["toolsPDFAnnotLink"]->setStatusTextAndShortcut( tr("Insert link annotation"));
	(*scrActions)["toolsPDFAnnotText"]->setStatusTextAndShortcut( tr("Insert text annotation"));
	(*scrActions)["toolsPDFCheckBox"]->setStatusTextAndShortcut( tr("Insert PDF check box"));
	(*scrActions)["toolsPDFComboBox"]->setStatusTextAndShortcut( tr("Insert PDF combo box"));
	(*scrActions)["toolsPDFListBox"]->setStatusTextAndShortcut( tr("Insert PDF list box"));
	(*scrActions)["toolsPDFPushButton"]->setStatusTextAndShortcut( tr("Insert PDF push button"));
	(*scrActions)["toolsPDFRadioButton"]->setStatusTextAndShortcut( tr("Insert PDF radio button"));
	(*scrActions)["toolsPDFTextField"]->setStatusTextAndShortcut( tr("Insert PDF text field"));
	(*scrActions)["toolsPreflightVerifier"]->setStatusTextAndShortcut( tr("Analyse the document for issues prior to exporting to PDF"));
	(*scrActions)["toolsRotate"]->setStatusTextAndShortcut( tr("Rotate an item"));
	(*scrActions)["toolsSelect"]->setStatusTextAndShortcut( tr("Select an item"));
	(*scrActions)["toolsUnlinkTextFrame"]->setStatusTextAndShortcut( tr("Unlink text frames"));
	(*scrActions)["toolsZoom"]->setStatusTextAndShortcut( tr("Zoom in or out"));
	(*scrActions)["viewPreviewMode"]->setStatusTextAndShortcut( tr("Enable preview mode"));
	(*scrActions)["viewEditInPreview"]->setStatusTextAndShortcut( tr("Enable editing in preview mode"));
	(*scrActions)["viewToggleWhiteSpaceMode"]->setStatusTextAndShortcut( tr("Toggle white space review mode"));
	(*scrActions)["viewToggleCMS"]->setStatusTextAndShortcut( tr("Toggle color management system"));
#ifdef HAVE_OSG
	(*scrActions)["toolsPDFAnnot3D"]->setStatusTextAndShortcut( tr("Insert 3D annotation"));
#endif

	//////
	languageChangeUnicodeActions(scrActions);
	languageChangeActions();

	//Reset tooltips on actions after the translation process
	for (auto it = scrActions->begin(); it != scrActions->end(); ++it)
	{
		if ((*it) != nullptr)
			(*it)->setToolTipFromTextAndShortcut();
	}
}

void ActionManager::languageChangeUnicodeActions(QMap<QString, QPointer<ScrAction> > *actionMap)
{
	//typography
	(*actionMap)["unicodeZWNJ"]->setText( tr("Zero Width Non-Joiner"));
	(*actionMap)["unicodeZWJ"]->setText( tr("Zero Width Joiner"));
	(*actionMap)["unicodeSoftHyphen"]->setText( tr("Soft &Hyphen"));
	(*actionMap)["unicodeNonBreakingHyphen"]->setText( tr("Non Breaking Hyphen"));
	(*actionMap)["unicodeNonBreakingSpace"]->setText( tr("Non Breaking &Space"));
	(*actionMap)["unicodeNarrowNoBreakSpace"]->setTexts( tr("Narrow No-Break Space"));
	(*actionMap)["unicodePageNumber"]->setText( tr("Page &Number"));
	(*actionMap)["unicodePageCount"]->setText( tr("Number of Pages"));
	(*actionMap)["unicodeNewLine"]->setText( tr("New Line"));
	(*actionMap)["unicodeFrameBreak"]->setText( tr("Frame Break"));
	(*actionMap)["unicodeColumnBreak"]->setText( tr("Column Break"));
	(*actionMap)["unicodeZerowidthSpace"]->setText( tr("&Zero Width Space"));
	(*actionMap)["unicodeZerowidthNonBreakingSpace"]->setText( tr("Zero Width NB Space"));
	(*actionMap)["unicodeCopyRight"]->setTexts( tr("Copyright"));
	(*actionMap)["unicodeRegdTM"]->setTexts( tr("Registered Trademark"));
	(*actionMap)["unicodeTM"]->setTexts( tr("Trademark"));
	(*actionMap)["unicodeSolidus"]->setTexts( tr("Solidus"));
	(*actionMap)["unicodeBullet"]->setTexts( tr("Bullet"));
	(*actionMap)["unicodeMidpoint"]->setTexts( tr("Middle Dot"));
	(*actionMap)["unicodeDashEm"]->setTexts( tr("Em Dash"));
	(*actionMap)["unicodeDashEn"]->setTexts( tr("En Dash"));
	(*actionMap)["unicodeDashFigure"]->setTexts( tr("Figure Dash"));
	(*actionMap)["unicodeDashQuotation"]->setTexts( tr("Quotation Dash"));

	(*actionMap)["unicodeQuoteApostrophe"]->setTexts( tr("Apostrophe", "Unicode 0x0027"));
	(*actionMap)["unicodeQuoteStraight"]->setTexts( tr("Straight Double", "Unicode 0x0022"));
	(*actionMap)["unicodeQuoteSingleLeft"]->setTexts( tr("Single Left", "Unicode 0x2018"));
	(*actionMap)["unicodeQuoteSingleRight"]->setTexts( tr("Single Right", "Unicode 0x2019"));
	(*actionMap)["unicodeQuoteDoubleLeft"]->setTexts( tr("Double Left", "Unicode 0x201C"));
	(*actionMap)["unicodeQuoteDoubleRight"]->setTexts( tr("Double Right", "Unicode 0x201D"));
	(*actionMap)["unicodeQuoteSingleReversed"]->setTexts( tr("Single Reversed", "Unicode 0x201B"));
	(*actionMap)["unicodeQuoteDoubleReversed"]->setTexts( tr("Double Reversed", "Unicode 0x201F"));
	(*actionMap)["unicodeQuoteSingleLeftGuillemet"]->setTexts( tr("Single Left Guillemet", "Unicode 0x2039"));
	(*actionMap)["unicodeQuoteSingleRightGuillemet"]->setTexts( tr("Single Right Guillemet", "Unicode 0x203A"));
	(*actionMap)["unicodeQuoteDoubleLeftGuillemet"]->setTexts( tr("Double Left Guillemet", "Unicode 0x00AB"));
	(*actionMap)["unicodeQuoteDoubleRightGuillemet"]->setTexts( tr("Double Right Guillemet", "Unicode 0x00BB"));
	(*actionMap)["unicodeQuoteLowSingleComma"]->setTexts( tr("Low Single Comma", "Unicode 0x201A"));
	(*actionMap)["unicodeQuoteLowDoubleComma"]->setTexts( tr("Low Double Comma", "Unicode 0x201E"));
	(*actionMap)["unicodeQuoteCJKSingleLeft"]->setTexts( tr("CJK Single Left", "Unicode 0x300C"));
	(*actionMap)["unicodeQuoteCJKSingleRight"]->setTexts( tr("CJK Single Right", "Unicode 0x300D"));
	(*actionMap)["unicodeQuoteCJKDoubleLeft"]->setTexts( tr("CJK Double Left", "Unicode 0x300E"));
	(*actionMap)["unicodeQuoteCJKDoubleRight"]->setTexts( tr("CJK Double Right", "Unicode 0x300F"));

	(*actionMap)["unicodeSpaceEN"]->setTexts( tr("En Space"));
	(*actionMap)["unicodeSpaceEM"]->setTexts( tr("Em Space"));
	(*actionMap)["unicodeSpaceThin"]->setTexts( tr("Thin Space"));
	(*actionMap)["unicodeSpaceThick"]->setTexts( tr("Thick Space"));
	(*actionMap)["unicodeSpaceMid"]->setTexts( tr("Mid Space"));
	(*actionMap)["unicodeSpaceHair"]->setTexts( tr("Hair Space"));

	(*actionMap)["unicodeLigature_ff"]->setTexts( tr("ff"));
	(*actionMap)["unicodeLigature_fi"]->setTexts( tr("fi"));
	(*actionMap)["unicodeLigature_fl"]->setTexts( tr("fl"));
	(*actionMap)["unicodeLigature_ffi"]->setTexts( tr("ffi"));
	(*actionMap)["unicodeLigature_ffl"]->setTexts( tr("ffl"));
	(*actionMap)["unicodeLigature_ft"]->setTexts( tr("ft"));
	(*actionMap)["unicodeLigature_st"]->setTexts( tr("st"));
}

void ActionManager::createDefaultShortcuts()
{
	defKeys.clear();

	defKeys.insert("fileNew", QKeySequence::New);
	defKeys.insert("fileNewFromTemplate", Qt::CTRL | Qt::ALT | Qt::Key_N);
	defKeys.insert("fileOpen", QKeySequence::Open);
	defKeys.insert("fileClose", QKeySequence::Close);
	defKeys.insert("fileSave", QKeySequence::Save);
	defKeys.insert("fileSaveAs", Qt::CTRL | Qt::SHIFT | Qt::Key_S);
	defKeys.insert("fileImportText", Qt::CTRL | Qt::Key_I);
	defKeys.insert("fileImportImage", Qt::CTRL | Qt::Key_I);
	defKeys.insert("filePrint", QKeySequence::Print);
	defKeys.insert("fileExportAsPDF", Qt::CTRL | Qt::SHIFT | Qt::Key_P);
	defKeys.insert("fileQuit", Qt::CTRL | Qt::Key_Q);
	//Edit Menu
	defKeys.insert("editUndoAction", QKeySequence::Undo);
	defKeys.insert("editRedoAction", QKeySequence::Redo);
	defKeys.insert("editCut", QKeySequence::Cut);
	defKeys.insert("editCopy", QKeySequence::Copy);
	defKeys.insert("editPaste", QKeySequence::Paste);
	defKeys.insert("editPastePlainText", Qt::CTRL | Qt::META | Qt::Key_V);
	defKeys.insert("editClearContents", Qt::CTRL | Qt::SHIFT | Qt::Key_Delete);
	defKeys.insert("editCopyContents", Qt::CTRL | Qt::SHIFT | Qt::Key_C);
	defKeys.insert("editPasteContents", Qt::CTRL | Qt::SHIFT | Qt::Key_V);
	defKeys.insert("editPasteContentsAbs", Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_V);
	defKeys.insert("editSelectAll", Qt::CTRL | Qt::Key_A);
	defKeys.insert("editSelectAllOnLayer", Qt::CTRL | Qt::ALT | Qt::Key_A);
	defKeys.insert("editDeselectAll", Qt::CTRL | Qt::SHIFT | Qt::Key_A);
	defKeys.insert("editSearchReplace", Qt::CTRL | Qt::Key_F);
	defKeys.insert("editStyles", QKeySequence(Qt::Key_F4));
	defKeys.insert("styleImageEffects", Qt::CTRL | Qt::Key_E);

	//Item Menu
	defKeys.insert("itemDuplicate", Qt::CTRL | Qt::Key_D);
	defKeys.insert("itemMulDuplicate", Qt::CTRL | Qt::SHIFT | Qt::Key_D);
	defKeys.insert("itemDelete", QKeySequence::Delete);
	defKeys.insert("itemGroup", Qt::CTRL | Qt::Key_G);
	defKeys.insert("itemUngroup", Qt::CTRL | Qt::SHIFT | Qt::Key_G);
	defKeys.insert("itemLock", Qt::CTRL | Qt::Key_L);
	defKeys.insert("itemLockSize", Qt::CTRL | Qt::SHIFT | Qt::Key_L);
	defKeys.insert("itemLowerToBottom", Qt::Key_End);
	defKeys.insert("itemRaiseToTop", Qt::Key_Home);
	defKeys.insert("itemLower", Qt::CTRL | Qt::Key_End);
	defKeys.insert("itemRaise", Qt::CTRL | Qt::Key_Home);
	defKeys.insert("itemStyleSearch", Qt::SHIFT | Qt::Key_Escape);

	//Insert Menu
	//Page menu
	//View Menu
	defKeys.insert("viewFitInWindow", Qt::CTRL | Qt::Key_0);
	defKeys.insert("viewFit100", Qt::CTRL | Qt::Key_1);
	defKeys.insert("viewPreviewMode", Qt::CTRL | Qt::ALT | Qt::Key_P);
	defKeys.insert("viewShowRulers", Qt::CTRL | Qt::SHIFT | Qt::Key_R);
	defKeys.insert("viewShowContextMenu", Qt::Key_Menu); //Context menu key on Windows. Do we have one to use on Linux/OSX? Super_L ?

	//Tool menu
	defKeys.insert("toolsProperties", Qt::Key_F2);
	defKeys.insert("toolsContent", Qt::Key_F3);
	defKeys.insert("toolsLayers", Qt::Key_F6);

	//toolbar only items
	defKeys.insert("toolsSelect", Qt::Key_C);
	defKeys.insert("toolsInsertTextFrame", QKeySequence(Qt::Key_T));
	defKeys.insert("toolsInsertImageFrame", Qt::Key_I);
	defKeys.insert("toolsInsertTable", Qt::Key_A);
	defKeys.insert("toolsInsertShape", Qt::Key_S);
	defKeys.insert("toolsInsertPolygon", Qt::Key_P);
	defKeys.insert("toolsInsertLine", Qt::Key_L);
	defKeys.insert("toolsInsertBezier", Qt::Key_B);
	defKeys.insert("toolsInsertFreehandLine", Qt::Key_F);
	
	defKeys.insert("toolsInsertRenderFrame", Qt::Key_D); //TODO: First free key. Select a meaningful
	defKeys.insert("toolsRotate", Qt::Key_R);
	defKeys.insert("toolsZoom", Qt::Key_Z);
	defKeys.insert("toolsZoomIn", Qt::CTRL | Qt::Key_Plus);
	defKeys.insert("toolsZoomOut", Qt::CTRL | Qt::Key_Minus);
	defKeys.insert("toolsEditContents", Qt::Key_E);
	defKeys.insert("toolsEditWithStoryEditor", Qt::CTRL | Qt::Key_T);
	defKeys.insert("toolsLinkTextFrame", Qt::Key_N);
	defKeys.insert("toolsUnlinkTextFrame", Qt::Key_U);
	defKeys.insert("toolsEyeDropper", Qt::Key_Y);

	//PDF items
	//Extras Menu
	//Windows Menu
	defKeys.insert("specialToggleAllPalettes", Qt::Key_F12);

	//Help Menu
	defKeys.insert("helpActionSearch", Qt::CTRL | Qt::Key_Slash);
	defKeys.insert("helpManual", Qt::Key_F1);

	//GUI and specials
	defKeys.insert("specialToggleAllGuides", Qt::Key_F11);
	defKeys.insert("specialUnicodeSequenceBegin", Qt::CTRL | Qt::SHIFT | Qt::Key_U);

	//typography
	defKeys.insert("unicodeFrameBreak", Qt::CTRL | Qt::Key_Return);
	defKeys.insert("unicodeColumnBreak", Qt::CTRL | Qt::SHIFT | Qt::Key_Return);

	defKeys.insert("unicodeSoftHyphen", Qt::CTRL | Qt::SHIFT | Qt::Key_Minus);
	defKeys.insert("unicodeNonBreakingHyphen", Qt::CTRL | Qt::ALT | Qt::Key_Minus);
	defKeys.insert("unicodeNonBreakingSpace", Qt::CTRL | Qt::Key_Space);
	defKeys.insert("unicodeNarrowNoBreakSpace", Qt::CTRL | Qt::ALT | Qt::Key_Space);
	defKeys.insert("unicodePageNumber", Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_P);
	defKeys.insert("unicodeNewLine", Qt::SHIFT | Qt::Key_Return);

	//Plugins
	defKeys.insert("ExportAsImage", Qt::CTRL | Qt::SHIFT | Qt::Key_E);
	defKeys.insert("NewFromDocumentTemplate", Qt::CTRL | Qt::ALT | Qt::Key_N);
	defKeys.insert("SaveAsDocumentTemplate", Qt::CTRL | Qt::ALT | Qt::Key_S);
}

void ActionManager::createDefaultMenuNames()
{
	defMenuNames.clear();

	defMenuNames.append(QPair<QString, QStringList>("File", QStringList()));
	defMenuNames.append(QPair<QString, QStringList>("Edit", QStringList()));
	defMenuNames.append(QPair<QString, QStringList>("Style", QStringList()));
	defMenuNames.append(QPair<QString, QStringList>("Item", QStringList()));
	defMenuNames.append(QPair<QString, QStringList>("Insert", QStringList()));
	defMenuNames.append(QPair<QString, QStringList>("Page", QStringList()));
	defMenuNames.append(QPair<QString, QStringList>("View", QStringList()));
	defMenuNames.append(QPair<QString, QStringList>("Table", QStringList()));
	defMenuNames.append(QPair<QString, QStringList>("Extras", QStringList()));
	defMenuNames.append(QPair<QString, QStringList>("Windows", QStringList()));
	defMenuNames.append(QPair<QString, QStringList>("Help", QStringList()));

	QVector< QPair<QString, QStringList> >::Iterator itMenuNames = defMenuNames.begin();
	itMenuNames->second << tr("File") << "&File" << tr("&File");
	++itMenuNames;
	itMenuNames->second << tr("Edit") << "&Edit" << tr("&Edit");
	++itMenuNames;
	itMenuNames->second << tr("Style") << "&Style" << tr("&Style");
	++itMenuNames;
	itMenuNames->second << tr("Item") << "&Item" << tr("&Item");
	++itMenuNames;
	itMenuNames->second << tr("Insert") << "I&nsert" << tr("I&nsert");
	++itMenuNames;
	itMenuNames->second << tr("Page") << "&Page" << tr("&Page");
	++itMenuNames;
	itMenuNames->second << tr("View") << "&View" << tr("&View");
	++itMenuNames;
	itMenuNames->second << tr("Table") << "&Table" << tr("&Table");
	++itMenuNames;
	itMenuNames->second << tr("Extras") << "E&xtras" << tr("E&xtras");
	++itMenuNames;
	itMenuNames->second << tr("Windows") << "&Windows" << tr("&Windows");
	++itMenuNames;
	itMenuNames->second << tr("Help") << "&Help" << tr("&Help");
	++itMenuNames;
}

void ActionManager::createDefaultMenus()
{   //CB TODO use this to also create the menus

	if (defMenuNames.empty())
		createDefaultMenuNames();

	defMenus.clear();
	for (int i = 0; i < defMenuNames.size(); ++i)
	{
		if (!defMenuNames.at(i).second.isEmpty())
			defMenus.append(QPair<QString, QStringList>(defMenuNames.at(i).second.at(0), QStringList()));
	}
	QVector< QPair<QString, QStringList> >::Iterator itmenu = defMenus.begin();
	//File
	itmenu->second 
		<< "fileNew"
		<< "fileNewFromTemplate"
		<< "fileOpen"
		<< "fileClose"
		<< "fileSave"
		<< "fileSaveAs"
		<< "fileRevert"
		<< "fileCollect"
		<< "fileImportText"
		<< "fileImportAppendText"
		<< "fileImportImage"
		<< "fileImportVector"
		<< "fileExportText"
		<< "fileExportAsEPS"
		<< "fileExportAsPDF"
//		<< "fileDocSetup"
		<< "fileDocSetup150"
//		<< "filePreferences"
		<< "filePreferences150"
		<< "filePrint"
		<< "PrintPreview"
		<< "OutputPreviewPDF"
		<< "OutputPreviewPS"
		<< "fileQuit";
	++itmenu;
	//Edit
	itmenu->second 
		<< "editUndoAction"
		<< "editRedoAction"
		<< "editActionMode"
		<< "editCut" 
		<< "editCopy"
		<< "editPaste"
		<< "editPastePlainText"
		<< "editCopyContents"
		<< "editPasteContents"
		<< "editPasteContentsAbs"
		<< "editPasteImageFromClipboard"
		<< "editClearContents"
		<< "editTruncateContents"
		<< "editSelectAll"
		<< "editSelectAllOnLayer"
		<< "editDeselectAll"
		<< "editSearchReplace"
		<< "toolsEditWithStoryEditor"
		<< "editEditWithImageEditor"
		<< "editEditRenderSource"
		<< "editColorsAndFills"
		<< "editReplaceColors"
		<< "editStyles"
		<< "editMarks"
		<< "editNotesStyles"
		<< "editMasterPages"
		<< "editJavascripts";
	//Style
	++itmenu;
	std::array<int, 12> font_sizes = { 7, 9, 10, 11, 12, 14, 18, 24, 36, 48, 60, 72 };
	for (auto fontSize : font_sizes)
		itmenu->second << QString("fontSize%1").arg(fontSize);
	itmenu->second
		<< "fontSizeOther"
		<< "alignLeft" 
		<< "alignCenter"
		<< "alignRight"
		<< "alignBlock"
		<< "alignForced";
// 	for (uint i=0; i<=100 ; i+=10)
// 		itmenu->second << QString("shade%1").arg(i);
// 	itmenu->second << "shadeOther";
	itmenu->second
		<< "typeEffectNormal"
		<< "typeEffectUnderline"
		<< "typeEffectUnderlineWords"
		<< "typeEffectStrikeThrough"
		<< "typeEffectAllCaps"
		<< "typeEffectSmallCaps"
		<< "typeEffectSuperscript"
		<< "typeEffectSubscript"
		<< "typeEffectOutline"
		<< "typeEffectShadow"
		<< "styleImageEffects"
		<< "styleTabulators";
	//Item
	++itmenu;
	itmenu->second 
		<< "itemDuplicate" 
		<< "itemMulDuplicate" 
		<< "itemTransform" 
		<< "itemDelete" 
		<< "itemGroup" 
		<< "itemUngroup"
		<< "itemGroupAdjust"
		<< "itemLock" 
		<< "itemLockSize" 
		<< "itemImageIsVisible" 
		<< "itemUpdateImage"
		<< "itemAdjustFrameHeightToText"
		<< "itemAdjustFrameToImage" 
		<< "itemAdjustImageToFrame" 
		<< "itemToggleInlineImage" 
		<< "itemExtendedImageProperties" 
		<< "itemPreviewFull" 
		<< "itemPreviewNormal" 
		<< "itemPreviewLow" 
		<< "itemRaise" 
		<< "itemLower" 
		<< "itemRaiseToTop" 
		<< "itemLowerToBottom" 
		<< "itemSendToPattern" 
		<< "itemSendToInline"
		<< "itemAttributes" 
		<< "itemPDFIsAnnotation" 
		<< "itemPDFIsBookmark" 
		<< "itemPDFAnnotationProps" 
		<< "itemPDFFieldProps" 
		<< "itemShapeEdit" 
		<< "itemConvertToBezierCurve" 
		<< "itemConvertToImageFrame" 
		<< "itemConvertToOutlines" 
		<< "itemConvertToPolygon" 
		<< "itemConvertToTextFrame" 
		<< "itemConvertToSymbolFrame"
		<< "itemAttachTextToPath" 
		<< "itemDetachTextFromPath" 
		<< "itemCombinePolygons" 
		<< "itemSplitPolygons"
		<< "itemsUnWeld"
		<< "itemWeld"
		<< "itemEditWeld"
		<< "toolsUnlinkTextFrameAndCutText";
	
	//Insert
	++itmenu;
	itmenu->second
		<< "insertFrame"
		<< "toolsInsertTextFrame"
		<< "toolsInsertImageFrame"
		<< "toolsInsertTable"
		<< "toolsInsertShape"
		<< "toolsInsertPolygon"
		<< "toolsInsertArc"
		<< "toolsInsertSpiral"
		<< "toolsInsertLine"
		<< "toolsInsertBezier"
		<< "toolsInsertFreehandLine"
		<< "toolsInsertCalligraphicLine"
		<< "toolsInsertRenderFrame"
		<< "stickyTools"
		<< "insertGlyph"
		<< "insertSampleText"
		<< "insertMarkAnchor"
		<< "insertMarkVariableText"
		<< "insertMarkItem"
		<< "insertMark2Mark"
		<< "insertMarkNote"
		<< "insertMarkIndex"
		<< "editMark"
		<< "itemUpdateMarks";

	itmenu->second
		<< "unicodeZWNJ"
		<< "unicodeZWJ"
		<< "unicodeSoftHyphen"
		<< "unicodeNonBreakingHyphen"
		<< "unicodeNonBreakingSpace"
		<< "unicodeNarrowNoBreakSpace"
		<< "unicodePageNumber"
		<< "unicodePageCount"
		<< "unicodeNewLine"
		<< "unicodeFrameBreak"
		<< "unicodeColumnBreak"
		<< "unicodeZerowidthSpace"
		<< "unicodeCopyRight"
		<< "unicodeRegdTM"
		<< "unicodeTM"
		<< "unicodeSolidus"
		<< "unicodeBullet"
		<< "unicodeMidpoint"
		<< "unicodeDashEm"
		<< "unicodeDashEn"
		<< "unicodeDashFigure"
		<< "unicodeDashQuotation";

	 itmenu->second
		 << "unicodeQuoteApostrophe"
		 << "unicodeQuoteStraight"
		 << "unicodeQuoteSingleLeft"
		 << "unicodeQuoteSingleRight"
		 << "unicodeQuoteDoubleLeft"
		 << "unicodeQuoteDoubleRight"
		 << "unicodeQuoteSingleReversed"
		 << "unicodeQuoteDoubleReversed"
		 << "unicodeQuoteSingleLeftGuillemet"
		 << "unicodeQuoteSingleRightGuillemet"
		 << "unicodeQuoteDoubleLeftGuillemet"
		 << "unicodeQuoteDoubleRightGuillemet"
		 << "unicodeQuoteLowSingleComma"
		 << "unicodeQuoteLowDoubleComma"
		 << "unicodeQuoteCJKSingleLeft"
		 << "unicodeQuoteCJKSingleRight"
		 << "unicodeQuoteCJKDoubleLeft"
		 << "unicodeQuoteCJKDoubleRight";

	 itmenu->second
		 << "unicodeSpaceEN"
		 << "unicodeSpaceEM"
		 << "unicodeSpaceThin"
		 << "unicodeSpaceThick"
		 << "unicodeSpaceMid"
		 << "unicodeSpaceHair";

	 itmenu->second
		 << "unicodeSoftHyphen"
		 << "unicodeNonBreakingHyphen"
		 << "unicodeNonBreakingSpace"
		 << "unicodeNarrowNoBreakSpace"
		 << "unicodePageNumber"
		 << "unicodePageCount"
		 << "unicodeNewLine"
		 << "unicodeFrameBreak"
		 << "unicodeColumnBreak";

	 itmenu->second
		 << "unicodeLigature_ff"
		 << "unicodeLigature_fi"
		 << "unicodeLigature_fl"
		 << "unicodeLigature_ffi"
		 << "unicodeLigature_ffl"
		 << "unicodeLigature_ft"
		 << "unicodeLigature_st";


	//Page
	++itmenu;
	itmenu->second
		<< "pageInsert"
		<< "pageImport"
		<< "pageDelete"
		<< "pageCopy"
		<< "pageMove"
		<< "pageApplyMasterPage"
		<< "pageCopyToMasterPage"
		<< "pageManageGuides"
		<< "pageManageProperties"
		<< "viewSnapToGrid"
		<< "viewSnapToGuides"
		<< "viewSnapToElements";
	//View
	++itmenu;
	itmenu->second
		<< "viewFitWidth"
		<< "viewFitInWindow"
		<< "viewFit50"
		<< "viewFit75"
		<< "viewFit100"
		<< "viewFit200"
		<< "viewFit400"
		<< "viewPreviewMode"
		<< "viewEditInPreview"
		<< "viewToggleCMS"
		<< "viewToggleWhiteSpaceMode"
		<< "viewShowMargins"
		<< "viewShowBleeds"
		<< "viewShowFrames"
		<< "viewShowLayerMarkers"
		<< "viewShowImages"
		<< "viewShowGrid"
		<< "viewShowGuides"
		<< "viewShowColumnBorders"
		<< "viewShowBaseline"
		<< "viewShowTextChain"
		<< "viewShowTextControls"
		<< "viewShowRulers"
		<< "viewRulerMode"
		<< "showMouseCoordinates";
	++itmenu;
	itmenu->second
		<< "tableInsertRows"
		<< "tableInsertColumns"
		<< "tableDeleteRows"
		<< "tableDeleteColumns"
		<< "tableMergeCells"
		<< "tableSplitCells"
		<< "tableSetRowHeights"
		<< "tableSetColumnWidths"
		<< "tableDistributeRowsEvenly"
		<< "tableDistributeColumnsEvenly"
		<< "tableAdjustFrameToTable"
		<< "tableAdjustTableToFrame";
	//Extras
	++itmenu;
	itmenu->second
		<< "extrasManageImages"
		<< "extrasHyphenateText"
		<< "extrasDeHyphenateText"
		<< "extrasGenerateTableOfContents"
		<< "extrasUpdateDocument";
	//Windows
	++itmenu;
	itmenu->second
		<< "windowsCascade"
		<< "windowsTile"
		<< "specialToggleAllPalettes"
		<< "toolsProperties"
		<< "toolsContent"
		<< "toolsOutline"
		<< "toolsScrapbook"
		<< "toolsLayers"
		<< "toolsPages"
		<< "toolsBookmarks"
		<< "toolsDownloads"
		<< "toolsResources"
		<< "toolsMeasurements"
		<< "toolsActionHistory"
		<< "toolsPreflightVerifier"
		<< "toolsAlignDistribute"
		<< "toolsSymbols"
		<< "toolsInline"
		<< "toolsToolbarTools"
		<< "toolsToolbarPDF"
		<< "toolsToolbarView";
	//Help
	++itmenu;
	itmenu->second
		<< "helpAboutScribus"
		<< "helpAboutPlugins"
		<< "helpAboutQt"
		<< "helpTooltips"
		<< "helpManual"
		<< "helpOnlineWWW"
		<< "helpOnlineDocs"
		<< "helpOnlineWiki"
		<< "helpOnlineTutorial1"
		<< "helpCheckUpdates"
		<< "helpChat";
	//Other
// 	++itmenu;
// 	itmenu->second << "";
}

void ActionManager::createDefaultNonMenuNames()
{
	defNonMenuNames.clear();

	defNonMenuNames.append(QPair<QString, QStringList>("Plugin Menu Items", QStringList()));
	defNonMenuNames.append(QPair<QString, QStringList>("Others", QStringList()));
	defNonMenuNames.append(QPair<QString, QStringList>("Unicode Characters", QStringList()));

	QVector< QPair<QString, QStringList> >::Iterator itNonMenuNames = defNonMenuNames.begin();
	itNonMenuNames->second << tr("Plugin Menu Items") << "Plugin Menu Items" << tr("Plugin Menu Items");
	++itNonMenuNames;
	itNonMenuNames->second << tr("Others") << "Others" << tr("Others");
	++itNonMenuNames;
	itNonMenuNames->second << tr("Unicode Characters") << "Unicode Characters" << tr("Unicode Characters");
	++itNonMenuNames;
}

void ActionManager::createDefaultNonMenuActions()
{   //CB TODO use this to also create the menus

	if (defNonMenuNames.empty())
		createDefaultNonMenuNames();

	defNonMenuActions.clear();
	for (int i = 0; i < defNonMenuNames.size(); ++i)
	{
		if (!defNonMenuNames.at(i).second.isEmpty())
			defNonMenuActions.append(QPair<QString, QStringList>(defNonMenuNames.at(i).second.at(0), QStringList()));
	}
/*
	defNonMenuActions.append(QPair<QString, QStringList>("Plugin Menu Items", QStringList()));
	defNonMenuActions.append(QPair<QString, QStringList>("Others", QStringList()));
	defNonMenuActions.append(QPair<QString, QStringList>("Unicode Characters", QStringList()));
*/

	QVector< QPair<QString, QStringList> >::Iterator itnmenua = defNonMenuActions.begin();
	//Plugins
//	itnmenua->second << "ExportAsImage";
//	itnmenua->second << "NewFromDocumentTemplate";
//	itnmenua->second << "SaveAsDocumentTemplate";
	
	//Others
	++itnmenua;
	itnmenua->second << "toolsSelect";
	itnmenua->second << "toolsRotate";
	itnmenua->second << "toolsZoom";
	itnmenua->second << "toolsZoomIn";
	itnmenua->second << "toolsZoomOut";
	itnmenua->second << "toolsEditContents";
	itnmenua->second << "toolsLinkTextFrame";
	itnmenua->second << "toolsUnlinkTextFrame";
//	itnmenua->second << "toolsUnlinkTextFrameAndCutText";
	itnmenua->second << "toolsEyeDropper";
	itnmenua->second << "toolsCopyProperties";
	itnmenua->second << "toolsPDFPushButton";
	itnmenua->second << "toolsPDFRadioButton";
	itnmenua->second << "toolsPDFTextField";
	itnmenua->second << "toolsPDFCheckBox";
	itnmenua->second << "toolsPDFListBox";
	itnmenua->second << "toolsPDFAnnotText";
	itnmenua->second << "toolsPDFAnnotLink";
#ifdef HAVE_OSG
	itnmenua->second << "toolsPDFAnnot3D";
#endif
	itnmenua->second << "specialToggleAllGuides";
	itnmenua->second << "specialUnicodeSequenceBegin";
	itnmenua->second << "viewShowContextMenu";

	//Unicode
	++itnmenua;
	itnmenua->second << "unicodeZWNJ";
	itnmenua->second << "unicodeZWJ";
	itnmenua->second << "unicodeSoftHyphen";
	itnmenua->second << "unicodeNonBreakingHyphen";
	itnmenua->second << "unicodeNonBreakingSpace";
	itnmenua->second << "unicodeNarrowNoBreakSpace";
	itnmenua->second << "unicodePageNumber";
	itnmenua->second << "unicodePageCount";
	itnmenua->second << "unicodeNewLine";
	itnmenua->second << "unicodeFrameBreak";
	itnmenua->second << "unicodeColumnBreak";
	itnmenua->second << "unicodeZerowidthSpace";
	itnmenua->second << "unicodeCopyRight";
	itnmenua->second << "unicodeRegdTM";
	itnmenua->second << "unicodeTM";
	itnmenua->second << "unicodeSolidus";
	itnmenua->second << "unicodeBullet";
	itnmenua->second << "unicodeMidpoint";
	itnmenua->second << "unicodeDashEm";
	itnmenua->second << "unicodeDashEn";
	itnmenua->second << "unicodeDashFigure";
	itnmenua->second << "unicodeDashQuotation";

	itnmenua->second << "unicodeQuoteApostrophe";
	itnmenua->second << "unicodeQuoteStraight";
	itnmenua->second << "unicodeQuoteSingleLeft";
	itnmenua->second << "unicodeQuoteSingleRight";
	itnmenua->second << "unicodeQuoteDoubleLeft";
	itnmenua->second << "unicodeQuoteDoubleRight";
	itnmenua->second << "unicodeQuoteSingleReversed";
	itnmenua->second << "unicodeQuoteDoubleReversed";
	itnmenua->second << "unicodeQuoteSingleLeftGuillemet";
	itnmenua->second << "unicodeQuoteSingleRightGuillemet";
	itnmenua->second << "unicodeQuoteDoubleLeftGuillemet";
	itnmenua->second << "unicodeQuoteDoubleRightGuillemet";
	itnmenua->second << "unicodeQuoteLowSingleComma";
	itnmenua->second << "unicodeQuoteLowDoubleComma";
	itnmenua->second << "unicodeQuoteCJKSingleLeft";
	itnmenua->second << "unicodeQuoteCJKSingleRight";
	itnmenua->second << "unicodeQuoteCJKDoubleLeft";
	itnmenua->second << "unicodeQuoteCJKDoubleRight";

	itnmenua->second << "unicodeSpaceEN";
	itnmenua->second << "unicodeSpaceEM";
	itnmenua->second << "unicodeSpaceThin";
	itnmenua->second << "unicodeSpaceThick";
	itnmenua->second << "unicodeSpaceMid";
	itnmenua->second << "unicodeSpaceHair";

	itnmenua->second << "unicodeZWNJ";
	itnmenua->second << "unicodeZWJ";

	itnmenua->second << "unicodeSoftHyphen";
	itnmenua->second << "unicodeNonBreakingHyphen";
	itnmenua->second << "unicodeNonBreakingSpace";
	itnmenua->second << "unicodePageNumber";
	itnmenua->second << "unicodePageCount";
	itnmenua->second << "unicodeNewLine";

	itnmenua->second << "unicodeLigature_ff";
	itnmenua->second << "unicodeLigature_fi";
	itnmenua->second << "unicodeLigature_fl";
	itnmenua->second << "unicodeLigature_ffi";
	itnmenua->second << "unicodeLigature_ffl";
	itnmenua->second << "unicodeLigature_ft";
	itnmenua->second << "unicodeLigature_st";
}

void ActionManager::handleMultipleSelections()
{
	bool isMultiple = true;
	if (mainWindow->HaveDoc && (mainWindow->doc->m_Selection->count() <= 1))
		isMultiple = false;
	(*scrActions)["itemAttributes"]->setEnabled(!isMultiple);
}

void ActionManager::languageChangeActions()
{
	//Here we mangle the URL based on the current GUI language, returning English if we don't get one of these hard coded options.
	//CB TODO make more flexible one day.
	QString language("EN");
	QString langpref(ScCore->getGuiLanguage().left(2));
	if (langpref == "de" || langpref == "fr" || langpref == "po" || langpref == "pt" || langpref == "ru")
		language = langpref.toUpper();
	(*scrActions)["helpOnlineTutorial1"]->setActionQString("https://wiki.scribus.net/index.php/tutorial"+language);
}

QKeySequence ActionManager::defaultKey(const QString& actionName)
{
	if (defKeys.contains(actionName))
		return defKeys.value(actionName);
	return QKeySequence();
}


QString ActionManager::defaultMenuNameEntryTranslated(const QString& index)
{
	for (int i = 0; i < defMenuNames.size(); ++i)
	{
		if (defMenuNames.at(i).first == index)
			return defMenuNames.at(i).second.at(2);
	}
	return QString();
}

