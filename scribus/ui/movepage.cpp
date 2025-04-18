/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/

#include "movepage.h"
#include <QPixmap>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

#include "commonstrings.h"
#include "iconmanager.h"
#include "ui/scrspinbox.h"

/*
 *  Constructs a MovePages which is a child of 'parent', with the 
 *  name 'name' and widget flags set to 'f' 
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  true to construct a modal dialog.
 */
MovePages::MovePages( QWidget* parent, int currentPage, int maxPages, bool moving ) : QDialog( parent )
{
	move = moving;	
	setWindowTitle (move ? tr("Move Pages") : tr("Copy Page"));
	setWindowIcon(IconManager::instance().loadIcon("app-icon"));
	setModal(true);
	dialogLayout = new QVBoxLayout( this );
	dialogLayout->setSpacing(6);
	dialogLayout->setContentsMargins(9, 9, 9, 9);
	fromToLayout = new QGridLayout();
	fromToLayout->setSpacing(6);
	fromToLayout->setContentsMargins(0, 0, 0, 0);
	moveLabel = new QLabel( (move ? tr("Move Page(s)") : tr("Copy Page")) + ":", this );
	fromPageData = new ScrSpinBox(this);
	fromPageData->setDecimals(0);
	fromPageData->setMinimum(1);
	fromPageData->setMaximum(maxPages);
	fromPageData->setValue( currentPage );
	fromPageData->setSuffix("");
	uint currentRow = 0;
	fromToLayout->addWidget( moveLabel, currentRow, 0);
	fromToLayout->addWidget( fromPageData, currentRow, 1);

	toLabel = nullptr;
	toPageData = nullptr;
	numberOfCopiesLabel = nullptr;
	numberOfCopiesData  = nullptr;
	if (move)
	{
		toLabel = new QLabel( tr("To:"), this );
		toPageData = new ScrSpinBox( this );
		toPageData->setDecimals(0);
		toPageData->setMinimum(1);
		toPageData->setMaximum(maxPages);
		toPageData->setValue( currentPage );
		toPageData->setSuffix("");
		fromToLayout->addWidget( toLabel, currentRow, 2);
		fromToLayout->addWidget( toPageData, currentRow, 3);
	}
	else
	{
		numberOfCopiesLabel = new QLabel( tr("Number of Copies:"), this );
		numberOfCopiesData = new ScrSpinBox(this );
		numberOfCopiesData->setDecimals(0);
		numberOfCopiesData->setMinimum(1);
		numberOfCopiesData->setMaximum(999);
		numberOfCopiesData->setSuffix("");
		++currentRow;
		fromToLayout->addWidget(numberOfCopiesLabel, currentRow, 0);
		fromToLayout->addWidget(numberOfCopiesData, currentRow, 1);
	}
	++currentRow;
	mvWhereData = new QComboBox( this );
	mvWhereData->addItem( tr("Before Page"));
	mvWhereData->addItem( tr("After Page"));
	mvWhereData->addItem( tr("At End"));
	if (move)
		mvWhereData->addItem( tr("Swap with Page"));
	mvWhereData->setCurrentIndex(2);
	mvWherePageData = new ScrSpinBox( this );
	mvWherePageData->setMinimum(1);
	mvWherePageData->setDecimals(0);
	mvWherePageData->setMaximum(maxPages);
	mvWherePageData->setValue( currentPage );
	mvWherePageData->setSuffix("");
	mvWherePageData->setDisabled( true );
	fromToLayout->addWidget( mvWhereData, currentRow, 0 );
	fromToLayout->addItem(new QSpacerItem(moveLabel->fontMetrics().horizontalAdvance( tr( "Move Page(s):" )), 0), currentRow, 1);
	fromToLayout->addWidget( mvWherePageData, currentRow, 2 );
//	fromToLayout->addColumnSpacing(0, moveLabel->fontMetrics().width( tr( "Move Page(s):")));
	dialogLayout->addLayout( fromToLayout );

	buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	dialogLayout->addWidget(buttonBox);
	setMaximumSize(sizeHint());

	// signals and slots connections
	if (move)
	{
		connect(fromPageData, SIGNAL(valueChanged(double)), this, SLOT(fromChanged()));
		connect(toPageData, SIGNAL(valueChanged(double)), this, SLOT(toChanged()));
	}
	connect(mvWhereData, SIGNAL(activated(int)), this, SLOT(mvWherePageDataDisable(int)));
	connect(buttonBox, &QDialogButtonBox::accepted, this, &MovePages::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &MovePages::reject);
}

void MovePages::fromChanged()
{
	if (!move)
		return;
	int pageNumber = static_cast<int>(fromPageData->value());
	if (pageNumber > toPageData->value() || mvWhereData->currentIndex() == 3)
		toPageData->setValue(pageNumber);
	if ((pageNumber == 1) && (toPageData->value() == toPageData->maximum()))
		toPageData->setValue(toPageData->maximum()-1);
}

void MovePages::toChanged()
{
	if (!move)
		return;
	int pageNumber = static_cast<int>(toPageData->value());
	if (pageNumber < fromPageData->value())
		fromPageData->setValue(pageNumber);
	if ((fromPageData->value() == 1) && (pageNumber == toPageData->maximum()))
		fromPageData->setValue(2);
}

void MovePages::mvWherePageDataDisable(int index)
{
	mvWherePageData->setDisabled(index == 2);
	if (toPageData)
	{
		toPageData->setDisabled(index == 3);
		if (index == 3)
			toPageData->setValue(fromPageData->value());
	}
}


int MovePages::getFromPage()
{
	return static_cast<int>(fromPageData->value());
}

int MovePages::getToPage()
{
	return static_cast<int>(toPageData->value());
}

int MovePages::getWhere()
{
	return mvWhereData->currentIndex();
}

int MovePages::getWherePage()
{
	return static_cast<int>(mvWherePageData->value());
}

int MovePages::getCopyCount()
{
	return static_cast<int>(numberOfCopiesData->value());
}
