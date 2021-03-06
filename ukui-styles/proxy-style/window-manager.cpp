/*
 * Qt5-UKUI's Library
 *
 * Copyright (C) 2020, Tianjin KYLIN Information Technology Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Authors: Yue Lan <lanyue@kylinos.cn>
 *
 */

#include "window-manager.h"

#include <QWidget>
#include <QMouseEvent>

#include <NETWM>
#include <QX11Info>
#include <xcb/xcb.h>

#include <QApplication>

#include <QDebug>

WindowManager::WindowManager(QObject *parent) : QObject(parent)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(100);
    m_timer.setSingleShot(true);
    m_start_point = QPoint(0, 0);

    qApp->installEventFilter(new AppEventFilter(this));
}

void WindowManager::registerWidget(QWidget *w)
{
    w->removeEventFilter(this);
    w->installEventFilter(this);
}

void WindowManager::unregisterWidget(QWidget *w)
{
    w->removeEventFilter(this);
}

bool WindowManager::eventFilter(QObject *obj, QEvent *e)
{
    //qDebug()<<e->type();
    switch (e->type()) {
    case QEvent::MouseButtonPress: {
        QMouseEvent *event = static_cast<QMouseEvent*>(e);

        if (event->button() == Qt::LeftButton) {
            // If the cursor is not normal arrow cursor,
            // we should consider there is another excepted operation
            // which current widget hope do. So we won't trigger
            // the window move event.
            QWidget *w = qobject_cast<QWidget *>(obj);
            if (w->cursor().shape() != Qt::CursorShape::ArrowCursor)
                return false;

            buttonPresseEvent(obj, event);
            return false;
        }
        return false;
    }
    case QEvent::MouseMove: {
        if (m_is_dragging) {
            if (m_current_obj != obj)
                return false;
            if (m_timer.isActive()) {
                return false;
            } else {
                if (QWidget::mouseGrabber()) return false;
                QMouseEvent *event = static_cast<QMouseEvent*>(e);
                //move request
                mouseMoveEvent(obj, event);
                return true;
            }
        }
        return false;
    }
    case QEvent::MouseButtonRelease: {
        QMouseEvent *event = static_cast<QMouseEvent*>(e);
        mouseReleaseEvent(obj, event);
        return false;
    }
    case QEvent::Move: {
        if (m_current_obj && m_is_dragging) {
            //qDebug()<<"drag end";
            endDrag();
        } else {
            //qDebug()<<"move";
        }
        return false;
    }
    default:
        return false;
    }
}

void WindowManager::buttonPresseEvent(QObject *obj, QMouseEvent *e)
{
    //qDebug()<<"mouse press event";
    endDrag();
    QWidget *w = qobject_cast<QWidget*>(obj);
    //NOTE: We have to skip the border for resize event.
    auto pos = w->mapFromGlobal(e->globalPos());
    if (!w->rect().adjusted(10, 10, -10, -10).contains(pos)) {
        //qDebug()<<"skip move event";
        return;
    }

    m_is_dragging = true;
    m_current_obj = obj;
    m_start_point = e->pos();
    m_timer.start();
}

void WindowManager::mouseMoveEvent(QObject *obj, QMouseEvent *e)
{
    if (!m_is_dragging)
        return;
    //qDebug()<<"move";
    QWidget *w = qobject_cast<QWidget*>(obj);

    const QPoint native = e->globalPos();

    auto connection = QX11Info::connection();
    xcb_ungrab_pointer(connection, XCB_TIME_CURRENT_TIME);
    NETRootInfo(connection, NET::WMMoveResize).moveResizeRequest(w->winId(), native.x(), native.y(), NET::Move);

    m_start_point = e->pos();
    m_timer.start();
}

void WindowManager::mouseReleaseEvent(QObject *obj, QMouseEvent *e)
{
    endDrag();
}

void WindowManager::endDrag()
{
    m_is_dragging = false;
    m_current_obj = nullptr;
    m_start_point = QPoint(0, 0);
    m_timer.stop();
}

// AppEventFilter
AppEventFilter::AppEventFilter(WindowManager *parent) : QObject(parent)
{
    m_wm = parent;
}

bool AppEventFilter::eventFilter(QObject *obj, QEvent *e)
{
    if (e->type() == QEvent::MouseButtonRelease) {
        m_wm->m_is_dragging = false;
        m_wm->m_current_obj = nullptr;
        m_wm->m_timer.stop();
        m_wm->m_start_point = QPoint();
    }

//    if (m_wm->m_is_dragging && m_wm->m_current_obj && (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseMove)) {
//        QMouseEvent mouseEvent(QEvent::MouseButtonRelease,
//                               m_wm->m_start_point,
//                               Qt::LeftButton,
//                               Qt::LeftButton,
//                               Qt::NoModifier);
//        qApp->sendEvent(m_wm->m_current_obj, &mouseEvent);
//    }
    return false;
}
