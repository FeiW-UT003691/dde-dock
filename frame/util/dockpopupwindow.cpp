/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dockpopupwindow.h"
#include "imageutil.h"
#include "utils.h"

#include <QScreen>
#include <QApplication>
#include <QDesktopWidget>
#include <QAccessible>
#include <QAccessibleEvent>
#include <QCursor>
#include <QGSettings>

DWIDGET_USE_NAMESPACE

DockPopupWindow::DockPopupWindow(QWidget *parent)
    : DArrowRectangle(ArrowBottom, parent),
      m_model(false),
      m_regionInter(new DRegionMonitor(this))
{
    setMargin(0);
    m_wmHelper = DWindowManagerHelper::instance();

    compositeChanged();

    setWindowFlags(Qt::X11BypassWindowManagerHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    if (Utils::IS_WAYLAND_DISPLAY) {
        setAttribute(Qt::WA_NativeWindow);
        windowHandle()->setProperty("_d_dwayland_window-type", "dock");
    }else {
        setAttribute(Qt::WA_InputMethodEnabled, false);
    }

    connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, this, &DockPopupWindow::compositeChanged);
    connect(m_regionInter, &DRegionMonitor::buttonRelease, this, &DockPopupWindow::onGlobMouseRelease);
}

DockPopupWindow::~DockPopupWindow()
{
}

bool DockPopupWindow::model() const
{
    return isVisible() && m_model;
}

void DockPopupWindow::setContent(QWidget *content)
{
    QWidget *lastWidget = getContent();
    if (lastWidget)
        lastWidget->removeEventFilter(this);
    content->installEventFilter(this);

    QAccessibleEvent event(this, QAccessible::NameChanged);
    QAccessible::updateAccessibility(&event);

    if (!content->objectName().trimmed().isEmpty())
        setAccessibleName(content->objectName() + "-popup");

    DArrowRectangle::setContent(content);
}

void DockPopupWindow::show(const QPoint &pos, const bool model)
{
    m_model = model;
    m_lastPoint = pos;

    show(pos.x(), pos.y());

    if (m_regionInter->registered()) {
        m_regionInter->unregisterRegion();
    }

    if (m_model) {
        m_regionInter->registerRegion();
    }
}

void DockPopupWindow::show(const int x, const int y)
{
    m_lastPoint = QPoint(x, y);

    DArrowRectangle::show(x, y);
}

void DockPopupWindow::hide()
{
    if (m_regionInter->registered())
        m_regionInter->unregisterRegion();

    DArrowRectangle::hide();
}

void DockPopupWindow::showEvent(QShowEvent *e)
{
    DArrowRectangle::showEvent(e);
    if (Utils::IS_WAYLAND_DISPLAY)
        updatePopupWindowCursor();

    QTimer::singleShot(1, this, &DockPopupWindow::ensureRaised);
}

void DockPopupWindow::enterEvent(QEvent *e)
{
    DArrowRectangle::enterEvent(e);
    if (Utils::IS_WAYLAND_DISPLAY)
        updatePopupWindowCursor();

    QTimer::singleShot(1, this, &DockPopupWindow::ensureRaised);
}

void DockPopupWindow::updatePopupWindowCursor()
{
    static QCursor *lastArrowCursor = nullptr;
    static QString  lastCursorTheme;
    int lastCursorSize = 0;
    QString theme = Utils::SettingValue("com.deepin.xsettings", "/com/deepin/xsettings/", "gtk-cursor-theme-name", "bloom").toString();
    int cursorSize = Utils::SettingValue("com.deepin.xsettings", "/com/deepin/xsettings/", "gtk-cursor-theme-size", 24).toInt();
    if (theme != lastCursorTheme || cursorSize != lastCursorSize) {
        QCursor *cursor = ImageUtil::loadQCursorFromX11Cursor(theme.toStdString().c_str(), "left_ptr", cursorSize);
        if (!cursor)
            return;
        lastCursorTheme = theme;
        lastCursorSize = cursorSize;
        setCursor(*cursor);
        if (lastArrowCursor)
            delete lastArrowCursor;

        lastArrowCursor = cursor;
    }
}

bool DockPopupWindow::eventFilter(QObject *o, QEvent *e)
{
    if (o != getContent() || e->type() != QEvent::Resize)
        return false;

    // FIXME: ensure position move after global mouse release event
    if (isVisible())
    {
        QTimer::singleShot(10, this, [=] {
            // NOTE(sbw): double check is necessary, in this time, the popup maybe already hided.
            if (isVisible())
                show(m_lastPoint, m_model);
        });
    }

    return false;
}

void DockPopupWindow::onGlobMouseRelease(const QPoint &mousePos, const int flag)
{
    Q_ASSERT(m_model);

    if (!((flag == DRegionMonitor::WatchedFlags::Button_Left) ||
          (flag == DRegionMonitor::WatchedFlags::Button_Right))) {
        return;
    }

    const QRect rect = QRect(pos(), size());
    if (rect.contains(mousePos))
        return;

    emit accept();

    m_regionInter->unregisterRegion();
}

void DockPopupWindow::compositeChanged()
{
    if (m_wmHelper->hasComposite())
        setBorderColor(QColor(255, 255, 255, 255 * 0.05));
    else
        setBorderColor(QColor("#2C3238"));
}

void DockPopupWindow::ensureRaised()
{
    if (isVisible())
        raise();
}
