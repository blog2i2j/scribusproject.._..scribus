#include <QMessageBox>

#include "notesstyleseditor.h"
#include "pageitem_noteframe.h"
#include "prefsmanager.h"
#include "prefsfile.h"
#include "scribusdoc.h"

#include "scribus.h"
#include "undomanager.h"
#include "util.h"

NotesStylesEditor::NotesStylesEditor(QWidget *parent, const char *name)
	: ScrPaletteBase(parent, name), m_Doc(nullptr)
{
	setupUi(this);
	QString pname(name);
	if (pname.isEmpty())
		pname = "notesStylesEditor";
	m_prefs = PrefsManager::instance().prefsFile->getContext(pname);

	setBlockSignals(true);
	
	setDoc(nullptr);
	NSlistBox->setInsertPolicy(QComboBox::InsertAlphabetically);

	RangeBox->addItem(tr("Document"), static_cast<int>(NSRdocument));
	RangeBox->addItem(tr("Story"), static_cast<int>(NSRstory));

	languageChange();

	StartSpinBox->setMinimum(1);
	StartSpinBox->setMaximum(99999);
	m_changesMap.clear();

	setBlockSignals(isVisible());
}

NotesStylesEditor::~NotesStylesEditor()
{
	storeVisibility(this->isVisible());
	storePosition();
	storeSize();
}

void NotesStylesEditor::changeEvent(QEvent *e)
{
	if (e->type() == QEvent::LanguageChange)
	{
		languageChange();
		return;
	}
	ScrPaletteBase::changeEvent(e);
}

void NotesStylesEditor::languageChange()
{
	bool wasSignalsBlocked = signalsBlocked();
	setBlockSignals(true);

	retranslateUi(this);

	if (m_addNewNsMode)
	{
		OKButton->setText(tr("Cancel"));
		OKButton->setToolTip(tr("Dialog is in adding new notes style mode. After pressing Cancel button dialog will be switched into normal notes styles edit mode."));
		ApplyButton->setText(tr("Add Style"));
	}
	else
	{
		OKButton->setText(tr("OK"));
		OKButton->setToolTip("");
		ApplyButton->setText(tr("Apply"));
	}

	bool paraStyleBlocked = paraStyleCombo->blockSignals(true);
	int  paraStyleIndex = paraStyleCombo->currentIndex();
	paraStyleCombo->setDoc(m_Doc);
	if (paraStyleIndex >= 0)
		paraStyleCombo->setCurrentIndex(paraStyleIndex);
	paraStyleCombo->blockSignals(paraStyleBlocked);

	bool charStyleBlocked = charStyleCombo->blockSignals(true);
	int  charStyleIndex = charStyleCombo->currentIndex();
	charStyleCombo->setDoc(m_Doc);
	if (charStyleIndex >= 0)
		charStyleCombo->setCurrentIndex(charStyleIndex);
	charStyleCombo->blockSignals(charStyleBlocked);

	bool rangeBlocked = RangeBox->blockSignals(true);
	int  rangeIndex = RangeBox->currentIndex();
	RangeBox->clear();
	RangeBox->addItem(tr("Document"), static_cast<int>(NSRdocument));
	RangeBox->addItem(tr("Story"), static_cast<int>(NSRstory));
	if (rangeIndex >= 0)
		RangeBox->setCurrentIndex(rangeIndex);
	RangeBox->blockSignals(rangeBlocked);

    setBlockSignals(wasSignalsBlocked);
}

void NotesStylesEditor::setDoc(ScribusDoc *doc)
{
	bool wasSignalsBlocked = signalsBlocked();
	setBlockSignals(true);
	if (m_Doc != nullptr)
		disconnect(m_Doc->scMW(), SIGNAL(UpdateRequest(int)), this , SLOT(handleUpdateRequest(int)));
	m_Doc = doc;
	paraStyleCombo->setDoc(m_Doc);
	charStyleCombo->setDoc(m_Doc);
	if (m_Doc != nullptr)
	{
		updateNSList();
		NSlistBox->setCurrentIndex(0);
		readNotesStyle(NSlistBox->currentText());
		setEnabled(true);
		ApplyButton->setEnabled(false);
		connect(m_Doc->scMW(), SIGNAL(UpdateRequest(int)), this , SLOT(handleUpdateRequest(int)));
	}
	else
	{
		m_changesMap.clear();
		NewNameEdit->clear();
		setEnabled(false);
	}
	setBlockSignals(wasSignalsBlocked);
}

void NotesStylesEditor::handleUpdateRequest(int updateFlags)
{
	bool wasSignalsBlocked = signalsBlocked();
	setBlockSignals(true);
	if ((updateFlags & reqCharStylesUpdate) || (updateFlags & reqTextStylesUpdate))
		charStyleCombo->updateStyleList();
	if ((updateFlags & reqParaStylesUpdate) || (updateFlags & reqTextStylesUpdate))
		paraStyleCombo->updateStyleList();
	readNotesStyle(NSlistBox->currentText());
	setBlockSignals(wasSignalsBlocked);
}

void NotesStylesEditor::updateNSList()
{
	bool wasSignalsBlocked = signalsBlocked();
	NSlistBox->blockSignals(true);
	if (m_Doc == nullptr)
		NSlistBox->setEnabled(false);
	else
	{
		NSlistBox->clear();
		m_changesMap.clear();
		for (int a = 0; a < m_Doc->m_docNotesStylesList.count(); ++a)
		{
			NSlistBox->addItem(m_Doc->m_docNotesStylesList.at(a)->name());
			m_changesMap.insert(m_Doc->m_docNotesStylesList.at(a)->name(), *(m_Doc->m_docNotesStylesList.at(a)));
		}
		if (!m_Doc->m_docNotesStylesList.isEmpty())
			NSlistBox->setEnabled(true);
		if (NSlistBox->currentText() != tr("default"))
			DeleteButton->setEnabled(true);
		else
			DeleteButton->setEnabled(false);
	}
	NSlistBox->blockSignals(wasSignalsBlocked);
	
	DeleteButton->setEnabled(NSlistBox->currentText() != tr("default"));
}

void NotesStylesEditor::setBlockSignals(bool block)
{
	foreach (QWidget* obj, findChildren<QWidget *>())
	{
		obj->blockSignals(block);
	}
	paraStyleCombo->blockSignals(block);
	charStyleCombo->blockSignals(block);
}

void NotesStylesEditor::setNotesStyle(NotesStyle * NS)
{
	if (NS == nullptr)
		return;
	bool wasSignalsBlocked = signalsBlocked();
	setBlockSignals(true);
	NSlistBox->setCurrentIndex(NSlistBox->findText(NS->name()));
	NewNameEdit->setText(NS->name());
	if (NS->name() == tr("default"))
		NewNameEdit->setEnabled(false);
	else
		NewNameEdit->setEnabled(true);
	FootRadio->setChecked(!NS->isEndNotes());
	EndRadio->setEnabled(true);
	EndRadio->setChecked(NS->isEndNotes());
	NumberingBox->setCurrentFormat(NS->getType());
	int rangeIndex = RangeBox->findData((int) NS->range());
	RangeBox->setCurrentIndex((rangeIndex >= 0) ? rangeIndex : 0);
	StartSpinBox->setValue(NS->start());
	PrefixEdit->setText(NS->prefix());
	SuffixEdit->setText(NS->suffix());
	SuperMasterCheck->setChecked(NS->isSuperscriptInMaster());
	SuperNoteCheck->setChecked(NS->isSuperscriptInNote());
	if (!NS->notesParStyle().isEmpty() && (NS->notesParStyle() != tr("No Style")))
		paraStyleCombo->setCurrentIndex(paraStyleCombo->findText(NS->notesParStyle()));
	if (!NS->marksChStyle().isEmpty() && (NS->marksChStyle() != tr("No Style")))
		charStyleCombo->setCurrentIndex(charStyleCombo->findText(NS->marksChStyle()));
	AutoH->setChecked(NS->isAutoNotesHeight());
	AutoW->setChecked(NS->isAutoNotesWidth());
	AutoWeld->setChecked(NS->isAutoWeldNotesFrames());
	//for endnotes disable autofixing size of notes frames
	bool b = !NS->isEndNotes();
	AutoW->setEnabled(b);
	AutoWeld->setEnabled(b);
	AutoRemove->setChecked(NS->isAutoRemoveEmptyNotesFrames());
	ApplyButton->setEnabled(false);
	setBlockSignals(wasSignalsBlocked);
}

void NotesStylesEditor::readNotesStyle(const QString& nsName)
{
	NotesStyle * NS = m_Doc->getNotesStyle(nsName);
	setNotesStyle(NS);
}

void NotesStylesEditor::on_NSlistBox_currentTextChanged(const QString &arg1)
{
	DeleteButton->setEnabled(arg1 != tr("default"));
	readNotesStyle(arg1);
}

void NotesStylesEditor::on_ApplyButton_clicked()
{
	if (m_addNewNsMode)
	{
		QString newName = NSlistBox->currentText();
		NotesStyle newNS = m_changesMap.value(newName);
		if (!m_Doc->validateNSet(newNS))
			return;

		m_addNewNsMode = false;
		OKButton->setText(tr("OK"));
		OKButton->setToolTip("");
		ApplyButton->setText(tr("Apply"));
		m_Doc->newNotesStyle(newNS);
		updateNSList();
		NSlistBox->setCurrentIndex(NSlistBox->findText(newNS.name()));
	}
	else
	{
		//remember current NStyle
		QString currNS = NSlistBox->currentText();
		NotesStyle* NS = nullptr;
		
		foreach (const QString &nsName, m_changesMap.keys())
		{
			NotesStyle n = m_changesMap.value(nsName);

			//validate settings
			if (!m_Doc->validateNSet(n))
			{
				NSlistBox->setCurrentIndex(NSlistBox->findText(n.name()));
				break;
			}
			//rename
			if (nsName != n.name())
			{
				//new name for existing set
				QString newName = n.name();
				getUniqueName(newName, m_changesMap.keys(),"=");
				n.setName(newName);
				NewNameEdit->setText(newName);
				//current NSet name change
				if (currNS == nsName)
					currNS = newName;
				NS = m_Doc->getNotesStyle(nsName);
				m_Doc->renameNotesStyle(NS, newName);
				m_Doc->setNotesChanged(true);
			}
			//change settings and update marks
			NS = m_Doc->getNotesStyle(n.name());
			Q_ASSERT(NS != nullptr);
			if (*NS != n)
			{
				SimpleState* ss = nullptr;
				if (UndoManager::undoEnabled())
				{
					ss = new SimpleState(UndoManager::EditNotesStyle);
					ss->set("NSTYLE", QString("edit"));
					m_Doc->undoSetNotesStyle(ss, NS);
				}
				//converting foot <--> end notes or changing footnotes range
				if ((NS->isEndNotes() != n.isEndNotes()) || (NS->isEndNotes() && n.isEndNotes() && NS->range() != n.range()))
				{
					foreach (PageItem_NoteFrame* nF, m_Doc->listNotesFrames(NS))
						m_Doc->delNoteFrame(nF, false);
					if (n.isEndNotes())
						m_Doc->flag_updateEndNotes = true;
				}
				m_Doc->setNotesChanged(true); //notesframes width must be updated
				*NS = n;
				if (ss)
				{
					ss->set("NEWname", NS->name());
					ss->set("NEWstart", NS->start());
					ss->set("NEWendNotes", NS->isEndNotes());
					ss->set("NEWnumFormat", (int) NS->getType());
					ss->set("NEWrange", (int) NS->range());
					ss->set("NEWprefix", NS->prefix());
					ss->set("NEWsuffix", NS->suffix());
					ss->set("NEWautoH", NS->isAutoNotesHeight());
					ss->set("NEWautoW", NS->isAutoNotesWidth());
					ss->set("NEWautoWeld", NS->isAutoWeldNotesFrames());
					ss->set("NEWautoRemove", NS->isAutoRemoveEmptyNotesFrames());
					ss->set("NEWsuperMaster", NS->isSuperscriptInMaster());
					ss->set("NEWsuperNote", NS->isSuperscriptInNote());
					ss->set("NEWmarksChStyle", NS->marksChStyle());
					ss->set("NEWnotesParStyle", NS->notesParStyle());
					UndoManager::instance()->action(m_Doc, ss);
				}
				//invalidate all text frames with marks from current changed notes style
				foreach (PageItem* item, m_Doc->DocItems)
				{
					if (item->isTextFrame() && !item->isNoteFrame() && item->asTextFrame()->hasNoteMark(NS))
						item->invalid = true;
				}
				m_Doc->updateNotesNums(NS);
				m_Doc->updateNotesFramesSettings(NS);
				if (m_Doc->flag_updateEndNotes)
					m_Doc->updateEndnotesFrames(NS);
				m_Doc->updateNotesFramesStyles(NS);
			}
		}
		if (m_Doc->notesChanged())
		{
			updateNSList();
			m_Doc->flag_updateMarksLabels = true;
			m_Doc->changed();
			m_Doc->regionsChanged()->update(QRectF());
		}
		//restore NStyle index
		readNotesStyle(currNS);
	}

	ApplyButton->setEnabled(false);
	NSlistBox->setEnabled(true);
	NewButton->setEnabled(true);
}

void NotesStylesEditor::on_DeleteButton_clicked()
{
	QString nsName(NSlistBox->currentText());
	int t = ScMessageBox::warning(m_Doc->scMW(), tr("Warning! Deleting Notes Style"), "<qt>" +
								 tr("You are going to delete notes style %1. All notes and marks using that style are also going to be deleted.").arg(nsName) + "</qt>",
								 QMessageBox::Ok | QMessageBox::Abort,
								 QMessageBox::Abort,	// GUI default
								 QMessageBox::Ok);	// batch default
	if (t != QMessageBox::Ok)
		return;
	m_Doc->deleteNotesStyle(nsName);
	m_Doc->changed();
	m_Doc->regionsChanged()->update(QRectF());
	setDoc(m_Doc);
}

void NotesStylesEditor::on_NewButton_clicked()
{
	QString oldName = NSlistBox->currentText();
	NotesStyle newNS = m_changesMap.value(oldName);
	QString newName = oldName;
	getUniqueName(newName, m_changesMap.keys(), "_");
	newNS.setName(newName);
	m_changesMap.insert(newName, newNS);
	setNotesStyle(&newNS);
	
	NewNameEdit->setEnabled(true);
	NSlistBox->addItem(newName);
	NSlistBox->setCurrentIndex(NSlistBox->findText(newName));
	NSlistBox->setEnabled(false);
	ApplyButton->setText(tr("Add Style"));
	ApplyButton->setEnabled(true);
	DeleteButton->setEnabled(false);
	NewButton->setEnabled(false);
	m_addNewNsMode = true;
	OKButton->setText(tr("Cancel Adding"));
	OKButton->setToolTip(tr("Notes Styles Editor is in adding new notes style mode. After pressing Cancel button Notes Styles Editor switch into normal notes styles edit mode."));
}

void NotesStylesEditor::on_OKButton_clicked()
{
	if (m_addNewNsMode)
	{
		//in adding new style mode go back to normal editing mode
		OKButton->setText(tr("OK"));
		NewButton->setEnabled(true);
		m_addNewNsMode = false;
		QString newName = NSlistBox->currentText();
		m_changesMap.remove(newName);
		int index = NSlistBox->findText(newName);
		NSlistBox->removeItem(index);
		NSlistBox->setCurrentIndex(index - 1);
		on_NSlistBox_currentTextChanged(NSlistBox->currentText());
	}
	else
	{
		if (ApplyButton->isEnabled())
			//apply changes
			on_ApplyButton_clicked();

		//in normal mode close
		close();
	}
}

void NotesStylesEditor::on_NewNameEdit_textChanged(const QString &arg1)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setName(arg1);
	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_FootRadio_toggled(bool checked)
{
	bool wasSignalsBlocked = signalsBlocked();
	setBlockSignals(true);

	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setEndNotes(!checked);
	m_changesMap.insert(NSlistBox->currentText(), ns);
	EndRadio->setChecked(!checked);
	if (checked)
	{
		ns.setAutoNotesWidth(true);
		AutoW->setEnabled(true);
		AutoW->setChecked(true);
		ns.setAutoWeldNotesFrames(true);
		AutoWeld->setEnabled(true);
		AutoWeld->setChecked(true);
	}
	
	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
	setBlockSignals(wasSignalsBlocked);
}

void NotesStylesEditor::on_EndRadio_toggled(bool checked)
{
	bool wasSignalsBlocked = signalsBlocked();
	setBlockSignals(true);

	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setEndNotes(checked);
	FootRadio->setChecked(!checked);
	if (checked)
	{
		ns.setAutoNotesWidth(false);
		AutoW->setChecked(false);
		AutoW->setEnabled(false);
		ns.setAutoWeldNotesFrames(false);
		AutoWeld->setChecked(false);
		AutoWeld->setEnabled(false);
	}
	
	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
	setBlockSignals(wasSignalsBlocked);
}

void NotesStylesEditor::on_NumberingBox_currentIndexChanged(int /*index*/)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());

	NumFormat formatType = NumberingBox->currentFormat();
	ns.setType(formatType);

	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_RangeBox_currentIndexChanged(int index)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setRange((NumerationRange) RangeBox->itemData(index).toInt());

	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_StartSpinBox_valueChanged(int arg1)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setStart(arg1);

	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_PrefixEdit_textChanged(const QString &arg1)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setPrefix(arg1);

	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_SuffixEdit_textChanged(const QString &arg1)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setSuffix(arg1);

	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_SuperMasterCheck_toggled(bool checked)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setSuperscriptInMaster(checked);

	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_SuperNoteCheck_toggled(bool checked)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setSuperscriptInNote(checked);

	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_AutoH_toggled(bool checked)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setAutoNotesHeight(checked);

	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_AutoW_toggled(bool checked)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setAutoNotesWidth(checked);

	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_AutoWeld_toggled(bool checked)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setAutoWeldNotesFrames(checked);

	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_AutoRemove_toggled(bool checked)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	ns.setAutoRemoveEmptyNotesFrames(checked);

	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_paraStyleCombo_currentIndexChanged(const int &arg1)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	if (arg1 == 0)
		ns.setNotesParStyle("");
	else
		ns.setNotesParStyle(paraStyleCombo->itemText(arg1));
	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}

void NotesStylesEditor::on_charStyleCombo_currentIndexChanged(const int &arg1)
{
	NotesStyle ns = m_changesMap.value(NSlistBox->currentText());
	if (arg1 == 0)
		ns.setMarksCharStyle("");
	else
		ns.setMarksCharStyle(charStyleCombo->itemText(arg1));
	m_changesMap.insert(NSlistBox->currentText(), ns);
	ApplyButton->setEnabled(true);
}
