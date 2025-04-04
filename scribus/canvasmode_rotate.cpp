/*
 For general Scribus (>=1.3.2) copyright and licensing information please refer
 to the COPYING file provided with the program. Following this notice may exist
 a copyright and/or license notice that predates the release of Scribus 1.3.2
 for which a new license (GPL+exception) is in place.
 */
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/



#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include "appmodes.h"
#include "canvas.h"
#include "canvasmode_rotate.h"
#include "fpoint.h"
#include "iconmanager.h"
#include "pageitem.h"
#include "prefsmanager.h"
#include "scribus.h"
#include "scribusdoc.h"
#include "scribusview.h"
#include "selection.h"
#include "ui/basepointwidget.h"
#include "ui/contextmenu.h"
#include "ui/pageselector.h"
#include "ui/propertiespalette.h"
#include "ui/propertiespalette_xyz.h"
#include "ui/scrspinbox.h"
#include "undomanager.h"
#include "util_math.h"

CanvasMode_Rotate::CanvasMode_Rotate(ScribusView* view) : CanvasMode(view)
{
	m_canvasPressCoord.setXY(-1.0, -1.0);
}

inline bool CanvasMode_Rotate::GetItem(PageItem** pi)
{ 
	*pi = m_doc->m_Selection->itemAt(0); 
	return (*pi) != nullptr;
}

void CanvasMode_Rotate::drawControls(QPainter* p)
{
	drawSelection(p, true);
	if (m_inItemRotation)
	{
		drawItemOutlines(p);
	}
}

void CanvasMode_Rotate::drawItemOutlines(QPainter* p)
{
	FPoint itemPos;
	double itemRotation;

	p->save();
	/*p->scale(m_canvas->scale(), m_canvas->scale());
	p->translate(-m_doc->minCanvasCoordinate.x(), -m_doc->minCanvasCoordinate.y());*/

	QColor  drawColor = QApplication::palette().color(QPalette::Active, QPalette::Highlight);
	p->setRenderHint(QPainter::Antialiasing);
	p->setBrush(Qt::NoBrush);
	p->setPen(QPen(drawColor, 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));

	uint docSelectionCount = m_doc->m_Selection->count();
	for (uint i = 0; i < docSelectionCount; ++i)
	{
		PageItem * currItem = m_doc->m_Selection->itemAt(i);
		getNewItemPosition(currItem, itemPos, itemRotation);
		p->save();
		p->translate(itemPos.x(), itemPos.y());
		p->rotate(itemRotation);
		currItem->DrawPolyL(p, currItem->Clip);
		p->restore();
	}

	p->restore();
}

void CanvasMode_Rotate::getNewItemPosition(const PageItem* item, FPoint& pos, double& rotation)
{
	double newAngle = xy2Deg(m_canvasCurrCoord.x() - m_rotCenter.x(), m_canvasCurrCoord.y() - m_rotCenter.y());
	if (m_angleConstrained)
	{
		newAngle = constrainAngle(newAngle, m_doc->opToolPrefs().constrain);
		/*double oldAngle = constrainAngle(m_startAngle, m_doc->opToolPrefs.constrain);
		newAngle = m_doc->m_Selection->isMultipleSelection() ? (newAngle - oldAngle) : newAngle;*/
		m_view->oldW = constrainAngle(m_view->oldW, m_doc->opToolPrefs().constrain);
		newAngle = m_doc->m_Selection->isMultipleSelection() ? (newAngle - m_view->oldW) : newAngle;
	}
	else if (m_doc->m_Selection->isMultipleSelection())
		newAngle = (newAngle - m_startAngle);
	else
		newAngle = item->rotation() - (m_startAngle - newAngle);
	if (m_doc->m_Selection->isMultipleSelection())
	{
		QTransform ma;
		ma.translate(m_rotCenter.x(), m_rotCenter.y());
		ma.scale(1, 1);
		ma.rotate(newAngle);
		FPoint n(item->xPos() - m_rotCenter.x(), item->yPos() - m_rotCenter.y());
		pos.setXY(ma.m11() * n.x() + ma.m21() * n.y() + ma.dx(), ma.m22() * n.y() + ma.m12() * n.x() + ma.dy());
		rotation = item->rotation() + newAngle;
	}
	else if (m_rotMode != AnchorPoint::TopLeft)
	{
		FPoint n(0,0);
		QTransform ma;
		ma.translate(item->xPos(), item->yPos());
		ma.scale(1, 1);
		ma.rotate(item->rotation());
		double ro = newAngle - item->rotation();
		switch (m_rotMode)
		{
		case AnchorPoint::None:
		case AnchorPoint::TopLeft:
			// No translation
			break;
		case AnchorPoint::Top:
			ma.translate(item->width()/2.0, 0);
			n = FPoint(-item->width()/2.0, 0);
			break;
		case AnchorPoint::TopRight:
			ma.translate(item->width(), 0);
			n = FPoint(-item->width(), 0);
			break;
		case AnchorPoint::Left:
			ma.translate(0, item->height()/2.0);
			n = FPoint(0, -item->height()/2.0);
			break;
		case AnchorPoint::Center:
			ma.translate(item->width()/2.0, item->height()/2.0);
			n = FPoint(-item->width()/2.0, -item->height()/2.0);
			break;
		case AnchorPoint::Right:
			ma.translate(item->width(), item->height()/2.0);
			n = FPoint(-item->width(), -item->height()/2.0);
			break;
		case AnchorPoint::BottomLeft:
			ma.translate(0, item->height());
			n = FPoint(0, -item->height());
			break;
		case AnchorPoint::Bottom:
			ma.translate(item->width()/2.0, item->height());
			n = FPoint(-item->width()/2.0, -item->height());
			break;
		case AnchorPoint::BottomRight:
			ma.translate(item->width(), item->height());
			n = FPoint(-item->width(), -item->height());
			break;
		}
		ma.rotate(ro);
		pos.setXY(ma.m11() * n.x() + ma.m21() * n.y() + ma.dx(), ma.m22() * n.y() + ma.m12() * n.x() + ma.dy());
		rotation = newAngle;
	}
	else
	{
		pos.setXY(item->xPos(), item->yPos());
		rotation = newAngle;
	}

	while (rotation < 0)
		rotation += 360.0;
	while (rotation > 360)
		rotation -= 360.0;
}

void CanvasMode_Rotate::activate(bool fromGesture)
{
	CanvasMode::activate(fromGesture);

	m_canvas->m_viewMode.m_MouseButtonPressed = false;
	m_canvas->resetRenderMode();
	m_doc->leaveDrag = false;
	m_view->MidButt  = false;
	m_inItemRotation = false;
	m_canvasPressCoord.setXY(-1.0, -1.0);
	m_oldRotMode   = m_rotMode   = AnchorPoint::TopLeft;
	m_oldRotCenter = m_rotCenter = FPoint(0.0, 0.0);
	m_startAngle   = 0.0;
	setModeCursor();
	if (fromGesture)
	{
		m_view->update();
	}
}

void CanvasMode_Rotate::deactivate(bool forGesture)
{
	m_view->setRedrawMarkerShown(false);
	m_inItemRotation = false;
	CanvasMode::deactivate(forGesture);
}

void CanvasMode_Rotate::enterEvent(QEvent *e)
{
	if (!m_canvas->m_viewMode.m_MouseButtonPressed)
	{
		setModeCursor();
	}
}

void CanvasMode_Rotate::leaveEvent(QEvent *e)
{
}

void CanvasMode_Rotate::mousePressEvent(QMouseEvent *m)
{
	const FPoint mousePointDoc = m_canvas->globalToCanvas(m->globalPosition());
	m_canvasPressCoord = mousePointDoc;
	
	m_canvas->PaintSizeRect(QRect());
//	QRect tx;
	QTransform pm;
	m_canvas->m_viewMode.m_MouseButtonPressed = true;
	m_canvas->m_viewMode.operItemMoving = false;
	m_view->HaveSelRect = false;
	m_doc->leaveDrag = false;
	m->accept();
	m_view->registerMousePress(m->globalPosition());
	QRect mpo(m->position().x()-m_doc->guidesPrefs().grabRadius, m->position().y()-m_doc->guidesPrefs().grabRadius, m_doc->guidesPrefs().grabRadius*2, m_doc->guidesPrefs().grabRadius*2);
	double Rxp = m_doc->ApplyGridF(m_canvasPressCoord).x();
	m_canvasPressCoord.setX( qRound(Rxp) );
	double Ryp = m_doc->ApplyGridF(m_canvasPressCoord).y();
	m_canvasPressCoord.setY( qRound(Ryp) );
	if (m->button() == Qt::MiddleButton)
	{
		m_view->MidButt = true;
		if (m->modifiers() & Qt::ControlModifier)
			m_view->DrawNew();
		return;
	}
	if (m->button() != Qt::LeftButton)
		return;
	PageItem *currItem;
	if (GetItem(&currItem))
	{
		m_inItemRotation = true;
		m_oldRotMode   = m_rotMode   = m_doc->rotationMode();
		m_oldRotCenter = m_rotCenter = m_view->RCenter;
		if (m_doc->m_Selection->isMultipleSelection())
		{
			double gx, gy, gh, gw;
			double gxR, gyR, ghR, gwR;
			m_view->getGroupRectScreen(&gx, &gy, &gw, &gh);
			m_doc->m_Selection->getGroupRect(&gxR, &gyR, &gwR, &ghR);
			if (QRect(static_cast<int>(gx), static_cast<int>(gy), static_cast<int>(gw), static_cast<int>(gh)).intersects(mpo))
			{
			m_rotMode   = AnchorPoint::Center;
				m_rotCenter = FPoint(gxR + gwR / 2.0, gyR + ghR / 2.0);
				if (QRect(static_cast<int>(gx + gw) - 6, static_cast<int>(gy + gh) - 6, 6, 6).intersects(mpo))
				{
					m_rotCenter = FPoint(gxR, gyR);
					m_rotMode   = AnchorPoint::TopLeft;
				}
				m_doc->setRotationMode(m_rotMode);
				m_view->RCenter = m_rotCenter;
			}
			m_startAngle = xy2Deg(mousePointDoc.x() - m_view->RCenter.x(), mousePointDoc.y() - m_view->RCenter.y());
			m_view->oldW = m_startAngle;
		}
		else
		{
			QTransform mat;
			m_canvas->Transform(currItem, mat);
			m_rotMode   = AnchorPoint::Center;
			m_rotCenter = FPoint(currItem->width() / 2, currItem->height() / 2, 0, 0, currItem->rotation(), 1, 1, false);
//			if (!currItem->asLine())
//			{
				if (QRegion(mat.map(QPolygon(QRect(0, 0, static_cast<int>(currItem->width()), static_cast<int>(currItem->height()))))).contains(mpo))
				{
					if (mat.mapRect(QRect(0, 0, 6, 6)).intersects(mpo))
					{
						m_rotCenter = FPoint(currItem->width(), currItem->height(), 0, 0, currItem->rotation(), 1, 1, false);
						m_rotMode   = AnchorPoint::BottomRight;
					}
					else if (mat.mapRect(QRect(static_cast<int>(currItem->width() / 2) - 6, 0, 6, 6)).intersects(mpo))
					{
						m_rotCenter = FPoint((currItem->width() / 2), currItem->height(), 0, 0, currItem->rotation(), 1, 1, false);
						m_rotMode   = AnchorPoint::Bottom;
					}
					else if (mat.mapRect(QRect(static_cast<int>(currItem->width()) - 6, 0, 6, 6)).intersects(mpo))
					{
						m_rotCenter = FPoint(0, currItem->height(), 0, 0, currItem->rotation(), 1, 1, false);
						m_rotMode   = AnchorPoint::BottomLeft;
					}
					else if (mat.mapRect(QRect(static_cast<int>(currItem->width()) - 6, static_cast<int>(currItem->height()) - 6, 6, 6)).intersects(mpo))
					{
						m_rotCenter = FPoint(0, 0);
						m_rotMode   = AnchorPoint::TopLeft;
					}
					else if (mat.mapRect(QRect(static_cast<int>(currItem->width() / 2) - 6, static_cast<int>(currItem->height()) - 6, 6, 6)).intersects(mpo))
					{
						m_rotCenter = FPoint(currItem->width() / 2, 0, 0, 0, currItem->rotation(), 1, 1, false);
						m_rotMode   = AnchorPoint::Top;
					}
					else if (mat.mapRect(QRect(0, static_cast<int>(currItem->height()) - 6, 6, 6)).intersects(mpo))
					{
						m_rotCenter = FPoint(currItem->width(), 0, 0, 0, currItem->rotation(), 1, 1, false);
						m_rotMode   = AnchorPoint::TopRight;
					}	
					else if (mat.mapRect(QRect(0, static_cast<int>(currItem->height() / 2) - 6, 6, 6)).intersects(mpo))
					{
						m_rotCenter = FPoint(0, currItem->height() / 2, 0, 0, currItem->rotation(), 1, 1, false);
						m_rotMode   = AnchorPoint::Left;
					}
					else if (mat.mapRect(QRect(currItem->width(), static_cast<int>(currItem->height() / 2) - 6, 6, 6)).intersects(mpo))
					{
						m_rotCenter = FPoint(currItem->width(), currItem->height() / 2, 0, 0, currItem->rotation(), 1, 1, false);
						m_rotMode   = AnchorPoint::Right;
					}
				}
				m_doc->setRotationMode(m_rotMode);
				m_view->RCenter = m_rotCenter;
//			}
			m_view->RCenter = m_rotCenter = FPoint(currItem->xPos()+ m_view->RCenter.x(), currItem->yPos()+ m_view->RCenter.y()); //?????
			m_view->oldW = m_startAngle = xy2Deg(mousePointDoc.x() - m_view->RCenter.x(), mousePointDoc.y() - m_view->RCenter.y());
		}
	}
}

void CanvasMode_Rotate::mouseReleaseEvent(QMouseEvent *m)
{
#ifdef GESTURE_FRAME_PREVIEW
	clearPixmapCache();
#endif // GESTURE_FRAME_PREVIEW
	const FPoint mousePointDoc = m_canvas->globalToCanvas(m->globalPosition());
	PageItem *currItem;
	m_canvas->m_viewMode.m_MouseButtonPressed = false;
	m_canvas->resetRenderMode();
	m->accept();
//	m_view->stopDragTimer();
	if ((GetItem(&currItem)) && (m->button() == Qt::RightButton))
	{
		createContextMenu(currItem, mousePointDoc.x(), mousePointDoc.y());
		return;
	}
	m_inItemRotation = false;
	if (m_view->moveTimerElapsed() && (GetItem(&currItem)))
	{
//		m_view->stopDragTimer();
		//Always start group transaction as a rotate action is generally a combination of
		//a change of rotation + a change of position
		if (!m_view->groupTransactionStarted() /*&& m_doc->m_Selection->isMultipleSelection()*/)
		{
			m_view->startGroupTransaction(Um::Rotate, "", Um::IRotate);
		}
		double angle = 0;
		double newW  = xy2Deg(mousePointDoc.x() - m_view->RCenter.x(), mousePointDoc.y() - m_view->RCenter.y());
		if (m->modifiers() & Qt::ControlModifier)
		{
			newW = constrainAngle(newW, m_doc->opToolPrefs().constrain);
			m_view->oldW = constrainAngle(m_view->oldW, m_doc->opToolPrefs().constrain);
			//RotateGroup uses MoveBy so its pretty hard to constrain the result
			angle = m_doc->m_Selection->isMultipleSelection() ? (newW - m_view->oldW) : newW;
		}
		else
		{
			angle = m_doc->m_Selection->isMultipleSelection() ? (newW - m_view->oldW) : (currItem->rotation() - (m_view->oldW - newW));
		}
		m_doc->itemSelection_Rotate(angle);
		m_view->oldW = newW;
		m_canvas->setRenderModeUseBuffer(false);
		if (!m_doc->m_Selection->isMultipleSelection())
		{
			m_doc->setRedrawBounding(currItem);
			currItem->OwnPage = m_doc->OnPage(currItem);
			if (currItem->isLine())
				m_view->updateContents();
		}
	}
	m_doc->setRotationMode(m_oldRotMode);
	m_view->RCenter = m_oldRotCenter;
	if (!PrefsManager::instance().appPrefs.uiPrefs.stickyTools)
		m_view->requestMode(modeNormal);
	else
	{
		int appMode = m_doc->appMode;
		m_view->requestMode(appMode);
	}
	if (GetItem(&currItem))
	{
		if (m_doc->m_Selection->count() > 1)
		{
			double x, y, w, h;
			m_doc->m_Selection->getGroupRect(&x, &y, &w, &h);
			m_view->updateContents(QRect(static_cast<int>(x - 5), static_cast<int>(y - 5), static_cast<int>(w + 10), static_cast<int>(h + 10)));
		}
		// Handled normally automatically by Selection in sendSignals()
		/*else
			currItem->emitAllToGUI();*/
	}
	m_canvas->setRenderModeUseBuffer(false);
	m_doc->leaveDrag = false;
	m_view->MidButt  = false;
	if (m_view->groupTransactionStarted())
	{
		for (int i = 0; i < m_doc->m_Selection->count(); ++i)
			m_doc->m_Selection->itemAt(i)->checkChanges(true);
		m_view->endGroupTransaction();
	}
	for (int i = 0; i < m_doc->m_Selection->count(); ++i)
		m_doc->m_Selection->itemAt(i)->checkChanges(true);
	//Make sure the Zoom spinbox and page selector don't have focus if we click on the canvas
	m_view->m_ScMW->zoomSpinBox->clearFocus();
	m_view->m_ScMW->pageSelector->clearFocus();
}

void CanvasMode_Rotate::mouseMoveEvent(QMouseEvent *m)
{
	const FPoint mousePointDoc = m_canvas->globalToCanvas(m->globalPosition());
	m_canvasCurrCoord  = mousePointDoc;
	m_angleConstrained = false;
	
	PageItem *currItem;
	m->accept();

	if (GetItem(&currItem))
	{
		m_angleConstrained = ((m->modifiers() & Qt::ControlModifier) != Qt::NoModifier);
		if (m_view->moveTimerElapsed() && m_canvas->m_viewMode.m_MouseButtonPressed)
		{
			m_canvas->repaint();
			double itemRotation;
			FPoint itemPos;
			getNewItemPosition(currItem, itemPos, itemRotation);
			m_canvas->displayRotHUD(m->globalPosition(), itemRotation);
		}
		if (!m_canvas->m_viewMode.m_MouseButtonPressed)
		{
			if (m_doc->m_Selection->isMultipleSelection())
			{
				double gx, gy, gh, gw;
				m_doc->m_Selection->getVisualGroupRect(&gx, &gy, &gw, &gh);
				int how = m_canvas->frameHitTest(QPointF(mousePointDoc.x(), mousePointDoc.y()), QRectF(gx, gy, gw, gh));
				if (how >= 0)
					m_view->setCursor(IconManager::instance().loadCursor("cursor-rotate"));
				else
					setModeCursor();
				return;
			}
			for (int a = 0; a < m_doc->m_Selection->count(); ++a)
			{
				currItem = m_doc->m_Selection->itemAt(a);
				if (currItem->locked())
					break;
				QTransform p;
				m_canvas->Transform(currItem, p);
				QRect mpo(m->position().x() - m_doc->guidesPrefs().grabRadius, m->position().y() - m_doc->guidesPrefs().grabRadius, m_doc->guidesPrefs().grabRadius * 2, m_doc->guidesPrefs().grabRadius * 2);
				if ((QRegion(p.map(QPolygon(QRect(-3, -3, static_cast<int>(currItem->width() + 6), static_cast<int>(currItem->height() + 6))))).contains(mpo)))
				{
					QRect tx = p.mapRect(QRect(0, 0, static_cast<int>(currItem->width()), static_cast<int>(currItem->height())));
					if ((tx.intersects(mpo)) && (!currItem->locked()))
						m_view->setCursor(IconManager::instance().loadCursor("cursor-rotate"));
				}
			}
		}
	}
	else
	{
		if ((m_canvas->m_viewMode.m_MouseButtonPressed) && (m->buttons() & Qt::LeftButton))
		{
			QPoint startP = m_canvas->canvasToGlobal(m_canvasPressCoord);
			QPoint globalPos = m->globalPosition().toPoint();
			m_view->redrawMarker->setGeometry(QRect(m_view->mapFromGlobal(startP), m_view->mapFromGlobal(globalPos)).normalized());
			m_view->setRedrawMarkerShown(true);
			m_view->HaveSelRect = true;
		}
	}
}

void CanvasMode_Rotate::keyPressEvent(QKeyEvent *e)
{
	if (e->isAutoRepeat())
		return;

	if (e->key() == Qt::Key_Escape)
	{
		// Go back to normal mode.
		m_view->requestMode(modeNormal);
		return;
	}

	if (m_doc->m_Selection->isMultipleSelection())
	{
		double gx, gy, gh, gw;
		m_oldRotMode   = m_rotMode   = m_doc->rotationMode();
		m_oldRotCenter = m_rotCenter = m_view->RCenter;
		m_doc->m_Selection->getVisualGroupRect(&gx, &gy, &gw, &gh);
		m_rotMode   = m_doc->rotationMode();

		switch(m_rotMode){
		case AnchorPoint::TopLeft:
			m_rotCenter = FPoint(gx, gy);
			break;
		case AnchorPoint::Top:
			m_rotCenter = FPoint(gx + gw / 2.0, gy);
			break;
		case AnchorPoint::TopRight:
			m_rotCenter = FPoint(gx + gw, gy);
			break;
		case AnchorPoint::Left:
			m_rotCenter = FPoint(gx, gy + gh / 2.0);
			break;
		case AnchorPoint::Right:
			m_rotCenter = FPoint(gx + gw, gy + gh / 2.0);
			break;
		case AnchorPoint::BottomLeft:
			m_rotCenter = FPoint(gx, gy + gh);
			break;
		case AnchorPoint::Bottom:
			m_rotCenter = FPoint(gx + gw / 2.0, gy + gh);
			break;
		case AnchorPoint::BottomRight:
			m_rotCenter = FPoint(gx + gw, gy + gh);
			break;
		case AnchorPoint::Center:
		default:
			m_rotCenter = FPoint(gx + gw / 2.0, gy + gh / 2.0);
			break;
		}

	}
}

void CanvasMode_Rotate::keyReleaseEvent(QKeyEvent *e)
{
	if (e->key() == Qt::Key_Up)
	{
		auto id = m_view->m_ScMW->propertiesPalette->xyzPal->basePointWidget->selectedAnchor();
		//id = id > 0 ? id - 1 : 4;
		m_view->m_ScMW->propertiesPalette->xyzPal->basePointWidget->setSelectedAnchor(id);
		m_doc->setRotationMode(id);
		return;
	}
	if (e->key() == Qt::Key_Down)
	{
		auto id = m_view->m_ScMW->propertiesPalette->xyzPal->basePointWidget->selectedAnchor();
		//id = (id + 1) % 5;
		m_view->m_ScMW->propertiesPalette->xyzPal->basePointWidget->setSelectedAnchor(id);
		m_doc->setRotationMode(id);
		return;
	}

	double increment = 0.0;
	if (e->key() == Qt::Key_Left)
		increment = 1.0;
	if (e->key() == Qt::Key_Right)
		increment = -1.0;
	if (e->modifiers() & Qt::ControlModifier)
		increment *= 10;
	if (e->modifiers() & Qt::ShiftModifier)
		increment /= 10;

	if (increment == 0.0)
		return;
	PageItem *currItem;
	if (GetItem(&currItem))
	{
		if (!m_view->groupTransactionStarted())
			m_view->startGroupTransaction(Um::Rotate, "", Um::IRotate);

		if (m_doc->m_Selection->isMultipleSelection())
		{
			m_doc->setRotationMode(m_rotMode);
			m_view->RCenter = m_rotCenter;
		}
		if (m_doc->m_Selection->isMultipleSelection())
			m_doc->rotateGroup(increment, m_view->RCenter);
		else
			m_doc->itemSelection_Rotate(currItem->rotation() + increment);
		m_canvas->setRenderModeUseBuffer(false);
		if (m_doc->m_Selection->isMultipleSelection())
		{
			double x, y, w, h;
			m_doc->setRotationMode(m_oldRotMode);
			m_view->RCenter = m_oldRotCenter;
			m_doc->m_Selection->getGroupRect(&x, &y, &w, &h);
			m_view->updateContents(QRect(static_cast<int>(x - 5), static_cast<int>(y - 5), static_cast<int>(w + 10), static_cast<int>(h + 10)));
		}
		else
		{
			m_doc->setRedrawBounding(currItem);
			currItem->OwnPage = m_doc->OnPage(currItem);
			if (currItem->isLine())
				m_view->updateContents();
		}
	}
	if (m_view->groupTransactionStarted())
	{
		for (int i = 0; i < m_doc->m_Selection->count(); ++i)
			m_doc->m_Selection->itemAt(i)->checkChanges(true);
		m_view->endGroupTransaction();
	}
	for (int i = 0; i < m_doc->m_Selection->count(); ++i)
		m_doc->m_Selection->itemAt(i)->checkChanges(true);
}

void CanvasMode_Rotate::createContextMenu(PageItem* currItem, double mx, double my)
{
	ContextMenu* cmen = nullptr;
	m_view->setObjectUndoMode();
	m_canvasPressCoord.setXY(mx, my);
	if (currItem != nullptr)
		cmen = new ContextMenu(*(m_doc->m_Selection), m_view->m_ScMW, m_doc);
	else
		cmen = new ContextMenu(m_view->m_ScMW, m_doc, mx, my);
	if (cmen)
		cmen->exec(QCursor::pos());
	m_view->setGlobalUndoMode();
	delete cmen;
}

