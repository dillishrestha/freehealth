/***************************************************************************
 *  The FreeMedForms project is a set of free, open source medical         *
 *  applications.                                                          *
 *  (C) 2008-2012 by Eric MAEKER, MD (France) <eric.maeker@gmail.com>      *
 *  All rights reserved.                                                   *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program (COPYING.FREEMEDFORMS file).                   *
 *  If not, see <http://www.gnu.org/licenses/>.                            *
 ***************************************************************************/
/***************************************************************************
 *   Main Developers :                                                    *
 *       Guillaume Denry <guillaume.denry@gmail.com>                       *
 *       Eric MAEKER, MD <eric.maeker@gmail.com>                           *
 *   Contributors :                                                        *
 *       NAME <MAIL@ADDRESS.COM>                                           *
 ***************************************************************************/
#include "dayrangebody.h"
#include "hour_range_widget.h"
#include "hour_range_node.h"
#include "hourwidget.h"
#include "day_widget.h"
#include "day_node.h"
#include <calendar/common.h>
#include <calendar/modelanditem/abstract_calendar_model.h>
#include <calendar/modelanditem/basic_item_edition_dialog.h>
#include <calendar/calendar_widget.h>

#include <utils/log.h>

#include <QRect>
#include <QPainter>
#include <QDate>
#include <QPixmapCache>
#include <QScrollArea>
#include <QMouseEvent>
#include <QMenu>

#include <QDebug>

enum { WarnBodyMouseEvents = true, WarnHeaderMouseEvents = false, WarnDragDropFromBody = true};

using namespace Calendar;
using namespace Internal;

int DayRangeBody::m_leftScaleWidth = 60;
//int DayRangeBody::m_hourHeight = 40;
int DayRangeBody::m_minimumItemHeight = 20;

namespace Calendar {
namespace Internal {
class DayRangeBodyPrivate
{
public:
    enum MouseMode {
        MouseMode_None,
        MouseMode_Move,
        MouseMode_Resize,
        MouseMode_Creation
    };

    DayRangeBodyPrivate(DayRangeBody *parent)
        : m_hourWidget(0),
          m_pressItemWidget(0),
          m_mouseMode(MouseMode_None),
          m_granularity(30),
          m_itemDefaultDuration(30),
          m_dayScaleHourDivider(2),
          q(parent)
    {
    }

    // if end < begin, the end time will be considered as midnight
    QRect getTimeIntervalRect(int day, const QTime &begin, const QTime &end) const
    {
        int containWidth = q->rect().width() - DayRangeBody::m_leftScaleWidth;

        day--; // convert 1 -> 7 to 0 -> 6 for drawing reasons

        int seconds = end < begin ? begin.secsTo(QTime(23, 59)) + 1 : begin.secsTo(end);
        int top = (QTime(0, 0).secsTo(begin) * m_hourHeight) / 3600;
        int height = (seconds * m_hourHeight) / 3600;

        // vertical lines
        return QRect(DayRangeBody::m_leftScaleWidth + (day * containWidth) / m_rangeWidth,
                     top,
                     ((day + 1) * containWidth) / m_rangeWidth - (day * containWidth) / m_rangeWidth,
                     height);
    }

    // returns the left position and width of the day band
    QPair<int, int> getBand(const QDate &date) const
    {
        int containWidth = q->rect().width() - DayRangeBody::m_leftScaleWidth;
        QPair<int, int> band;

        int day = q->firstDate().daysTo(date);
        band.first = DayRangeBody::m_leftScaleWidth + (day * containWidth) / m_rangeWidth;
        band.second = ((day + 1) * containWidth) / m_rangeWidth - (day * containWidth) / m_rangeWidth - 8;
        return band;
    }

    QPair<int, int> getItemVerticalData(const QTime &begin, const QTime &end) const
    {
        return getItemTopAndHeight(begin, end, m_hourHeight, DayRangeBody::m_minimumItemHeight);
    }

    // Return a quantized QDateTime from the \e dateTime according to the granularity of the grid.
    QDateTime quantized(const QDateTime &dateTime) const
    {
        int hour = dateTime.time().hour();
        int minutes = hour * 60 + dateTime.time().minute(); // total minutes of the day

        int low = (minutes / m_granularity) * m_granularity;
        int high = low + m_granularity;

        minutes = minutes - low < high - minutes ? low : high;

    //	qDebug("low: %d, high: %d", low, high);
        hour = minutes / 60;
        minutes = minutes - hour * 60;

        return QDateTime(dateTime.date(), QTime(hour, minutes));
    }

    // Return the QDateTime corresponding to the \e pos (of course this includes day and hour).
    QDateTime getDateTime(const QPoint &pos) const
    {
        // get day and time
        int containWidth = q->rect().width() - DayRangeBody::m_leftScaleWidth;
        int x = pos.x();
        int y = pos.y();
        int day = 0;
        for (int i = 0; i < m_rangeWidth; ++i) {
            if (x >= (i * containWidth) / m_rangeWidth + DayRangeBody::m_leftScaleWidth && x < ((i + 1) * containWidth) / m_rangeWidth + DayRangeBody::m_leftScaleWidth){
                break;
            }
            day++;
        }
        int hour = y / m_hourHeight;
        int remain = y - hour * m_hourHeight;
        int minutes = (remain * 60) / m_hourHeight;

        return QDateTime(q->firstDate().addDays(day), QTime(hour, minutes));
    }

    // refresh all widgets of a day
    void refreshDayWidgets(const QDate &dayDate)
    {
            if (dayDate < q->firstDate() || dayDate >= q->firstDate().addDays(m_rangeWidth)) // day is out of range
            return;

        // at first remove all day widgets
        qDeleteAll(q->getWidgetsByDate(dayDate));

        if (!q->model())
            return;

        // re-create them
        QList<CalendarItem> items = q->model()->getItemsBetween(dayDate, dayDate);
        // filter by date-time types
        for (int i = items.count() - 1; i >= 0; i--) {
            const CalendarItem &item = items[i];
            if (item.beginningType() == Date_Date || item.endingType() == Date_Date)
                items.removeAt(i);
        }
        if (!items.count())
            return;

        // sorting and create the tree
        qSort(items.begin(), items.end(), calendarItemLessThan);

        HourRangeNode::setHourHeight(m_hourHeight);
        HourRangeNode::setMinimumItemHeight(DayRangeBody::m_minimumItemHeight);
        HourRangeNode node(items[0]);

        for (int i = 1; i < items.count(); i++)
            node.store(items[i]);

        node.prepareForWidthsComputing();
        QList<HourRangeNode*> nodes;
        QPair<int, int> band = getBand(dayDate);
        node.computeWidths(band.first, band.second, nodes);

        foreach (HourRangeNode *node, nodes) {
            HourRangeWidget *widget = new HourRangeWidget(q, node->item().uid(), q->model());
            QPair<int, int> verticalData = getItemVerticalData(node->item().beginning().time(), node->item().ending().time());
            widget->setBeginDateTime(node->item().beginning());
            widget->setEndDateTime(node->item().ending());
            widget->move(node->left(), verticalData.first);
            widget->resize(node->width(), verticalData.second);
            widget->show();
        }
    }

    void paintBody(QPainter *painter, const QRect &visibleRect)
    {
        painter->fillRect(visibleRect, Qt::white);
        QPen pen = painter->pen();
        pen.setColor(QColor(200, 200, 200));
        pen.setCapStyle(Qt::FlatCap);
        painter->setPen(pen);
        int containWidth = visibleRect.width() - DayRangeBody::m_leftScaleWidth;

        // draw current day?
        QDate now = QDate::currentDate();
        if (now >= q->firstDate() && now < q->firstDate().addDays(m_rangeWidth)){
            int day = now.dayOfWeek() - q->firstDate().dayOfWeek();
            painter->fillRect(DayRangeBody::m_leftScaleWidth + (day * containWidth) / m_rangeWidth, 0,
                              ((day + 1) * containWidth) / m_rangeWidth - (day * containWidth) / m_rangeWidth, visibleRect.height(),
                              QColor(255, 255, 200));
        }

        // vertical lines
        for (int i = 0; i < m_rangeWidth; ++i) {
            painter->drawLine(DayRangeBody::m_leftScaleWidth + (i * containWidth) / m_rangeWidth, 0,
                              DayRangeBody::m_leftScaleWidth + (i * containWidth) / m_rangeWidth, visibleRect.height());
        }

        // hours horizontal lines
        for (int i = 0; i < 24; ++i) {
            painter->drawLine(0, (i + 1) * m_hourHeight,
                              visibleRect.width() - 1, (i + 1) * m_hourHeight);
        }

        // between hours lines. Optimization: draw a single dashed line in a pixmap and copy it N times with drawPixmap because drawing dashed lines is VERY SLOW with X11
        QPen oldPen = pen;
        QPixmap dashPixmap(visibleRect.width(), 1);
        QPainter dashPainter(&dashPixmap);
        dashPainter.fillRect(QRect(0, 0, visibleRect.width(), 1), Qt::white);
        QPen dashPen = dashPainter.pen();
        dashPen.setColor(QColor(200, 200, 200));
        dashPen.setCapStyle(Qt::FlatCap);
        dashPen.setDashPattern(QVector<qreal>() << 1 << 1);
        dashPainter.setPen(dashPen);
        dashPainter.drawLine(0, 0, visibleRect.width(), 0);

        pen.setDashPattern(QVector<qreal>() << 1 << 1);
        painter->setPen(pen);
        for (int i = 0; i < 24; ++i) {
            for (int j = 1; j < m_dayScaleHourDivider; j++)
                painter->drawPixmap(DayRangeBody::m_leftScaleWidth, i * m_hourHeight + (j * m_hourHeight) / m_dayScaleHourDivider,
                                    visibleRect.width(), 1, dashPixmap);
        }

        painter->setPen(oldPen);

        pen = painter->pen();
        pen.setColor(QColor(120, 120, 120));
        painter->setPen(pen);
        for (int i = 0; i < 24; ++i) {
            QRect scaleRect(QPoint(0, i * m_hourHeight + 1),
                            QPoint(DayRangeBody::m_leftScaleWidth - 3, (i + 1) * m_hourHeight - 1));
            painter->drawText(scaleRect, Qt::AlignRight, QString("%1:00").arg(i, 2, 10, QChar('0')));
        }

        // hour widget
        if (now >= q->firstDate() && now < q->firstDate().addDays(m_rangeWidth)) {
            if (!m_hourWidget)
                m_hourWidget = new HourWidget(q);

            int day = now.dayOfWeek() - q->firstDate().dayOfWeek();

            // move and resize
            m_hourWidget->resize(((day + 1) * containWidth) / m_rangeWidth - (day * containWidth) / m_rangeWidth, m_hourWidget->sizeHint().height());

            // compute
            QTime nowTime = QTime::currentTime();
            int y = (q->rect().height() * nowTime.hour()) / 24;
            int minY = (((q->rect().height() * (nowTime.hour() + 1)) / 24 - (q->rect().height() * nowTime.hour()) / 24) * nowTime.minute()) / 60;

            m_hourWidget->move(DayRangeBody::m_leftScaleWidth + (day * containWidth) / m_rangeWidth, y + minY);
            m_hourWidget->raise();
            m_hourWidget->show();
        } else if (m_hourWidget) {
            delete m_hourWidget;
            m_hourWidget = 0;
        }
    }


public:
    HourWidget *m_hourWidget;
    int m_rangeWidth;
    QDateTime m_pressDateTime;
    QDateTime m_previousDateTime;
    QPoint m_pressPos;
    HourRangeWidget *m_pressItemWidget;
    CalendarItem m_pressItem;
    MouseMode m_mouseMode;
    int m_granularity;
    int m_itemDefaultDuration;
    int m_dayScaleHourDivider;
    int m_hourHeight;

private:
    DayRangeBody *q;
};
}  // namespace Internal
}  // namespace Calendar

/**
  \class Calendar::DayRangeBody
  \todo documentation
*/
DayRangeBody::DayRangeBody(QWidget *parent, int rangeWidth) :
    ViewWidget(parent),
    d_body(new Internal::DayRangeBodyPrivate(this))
{
    d_body->m_rangeWidth = rangeWidth;
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setFirstDate(Calendar::getFirstDateByRandomDate(Calendar::View_Week, QDate::currentDate()));

}

/** Returns a minutes count for the items creation/resize granularity. 30 by default. */
int DayRangeBody::granularity() const
{
    return d_body->m_granularity;
}

/** Define the granularity of the view to \e value minutes (Can only be a divider of 24 * 60). */
void DayRangeBody::setGranularity(int value)
{
	int dayMinutes = 24 * 60;
	if ((dayMinutes / value) * value == dayMinutes) // only accepts divider values
        d_body->m_granularity = value;
}

/** returns the minutes count for an item creation. 30 by default. */
int DayRangeBody::itemDefaultDuration() const
{
    return d_body->m_itemDefaultDuration;
}

/** Define the default duration of newly created item to \e value in minutes */
void DayRangeBody::setItemDefaultDuration(int value)
{
    if (value == d_body->m_itemDefaultDuration)
		return;

    d_body->m_itemDefaultDuration = value;
}

int DayRangeBody::dayScaleHourDivider() const
{
    return d_body->m_dayScaleHourDivider;
}

void DayRangeBody::setDayScaleHourDivider(int value)
{
    if (value == d_body->m_dayScaleHourDivider)
        return;

    d_body->m_dayScaleHourDivider = value;
    update();
}

int DayRangeBody::hourHeight() const
{
    return d_body->m_hourHeight;
}

void DayRangeBody::setHourHeight(int value)
{
    if (value == d_body->m_hourHeight)
        return;

    d_body->m_hourHeight = value;
    update();
}

QSize DayRangeBody::sizeHint() const
{
    return QSize(0, 24 * d_body->m_hourHeight);
}

void DayRangeBody::paintBody(QPainter *painter, const QRect &visibleRect)
{
    d_body->paintBody(painter, visibleRect);
}

void DayRangeBody::refreshItemsSizesAndPositions()
{
    for (int i = 0; i < d_body->m_rangeWidth; i++)
        d_body->refreshDayWidgets(firstDate().addDays(i));
}


/** For a week, the range width is 7 */
int DayRangeBody::rangeWidth() const
{
    return d_body->m_rangeWidth;
}

/** Define the number of days available in the view. */
void DayRangeBody::setRangeWidth(int width)
{
    if (width == d_body->m_rangeWidth)
        return;

    d_body->m_rangeWidth = width;
	forceUpdate();
}

void DayRangeBody::dragEnterEvent(QDragEnterEvent *event)
{
    if (WarnDragDropFromBody)
        WARN_FUNC << event->pos();
}

void DayRangeBody::dragMoveEvent(QDragMoveEvent *event)
{
    if (WarnDragDropFromBody)
        WARN_FUNC << event->pos();
}

void DayRangeBody::dragLeaveEvent(QDragLeaveEvent *event)
{
    if (WarnDragDropFromBody)
        WARN_FUNC;
}

void DayRangeBody::dropEvent(QDropEvent *event)
{
    if (WarnDragDropFromBody)
        WARN_FUNC << event->pos();
}

void DayRangeBody::mousePressEvent(QMouseEvent *event) {

    if (event->pos().x() < m_leftScaleWidth) {
		QWidget::mousePressEvent(event);
		return;
	}
    d_body->m_pressDateTime = d_body->quantized(d_body->getDateTime(event->pos()));
    d_body->m_previousDateTime = d_body->m_pressDateTime;
    d_body->m_pressPos = event->pos();

	// item under mouse?
    d_body->m_pressItemWidget = qobject_cast<HourRangeWidget*>(childAt(event->pos()));
    if (d_body->m_pressItemWidget) {
        d_body->m_pressItem = model()->getItemByUid(d_body->m_pressItemWidget->uid());
        QPoint pos = d_body->m_pressItemWidget->mapFromParent(event->pos());
        if (pos.y() >= d_body->m_pressItemWidget->height() - 5 && pos.y() < d_body->m_pressItemWidget->height())
            d_body->m_mouseMode = DayRangeBodyPrivate::MouseMode_Resize;
        else
            d_body->m_mouseMode = DayRangeBodyPrivate::MouseMode_Move;
    } else {
        d_body->m_mouseMode = DayRangeBodyPrivate::MouseMode_Creation;
    }

    if (WarnBodyMouseEvents) {
        qWarning() << "DayBody::mousePressed" << d_body->m_pressItem.uid() << d_body->m_pressItem.beginning() << d_body->m_pressItem.ending();
        qWarning() << "   pressed DateTime" << d_body->m_previousDateTime;
    }
}

void DayRangeBody::mouseMoveEvent(QMouseEvent *event)
{
    QDateTime dateTime = d_body->quantized(d_body->getDateTime(event->pos()));
	QRect rect;
	int seconds, limits;
	QDateTime beginning, ending;

    if (d_body->m_mouseMode == DayRangeBodyPrivate::MouseMode_None || !d_body->m_pressItemWidget) {
		QWidget::mouseMoveEvent(event);
		return;
	}

    if (d_body->m_previousDateTime == dateTime)
		return;

    d_body->m_previousDateTime = dateTime;

    switch (d_body->m_mouseMode) {
    case DayRangeBodyPrivate::MouseMode_Creation:
        {
            if (WarnBodyMouseEvents) {
                qWarning() << "DayBody::mouseMove (creationMode)" << d_body->m_pressItem.uid() << d_body->m_pressItem.beginning() << d_body->m_pressItem.ending();
                qWarning() << "   pressDateTime" << d_body->m_pressDateTime;
                qWarning() << "   previousDateTime" << d_body->m_previousDateTime;
            }

            if (dateTime != d_body->m_pressDateTime) {
                if (!d_body->m_pressItemWidget) {
                    d_body->m_pressItemWidget = new HourRangeWidget(this);
                    d_body->m_pressItemWidget->setBeginDateTime(d_body->m_pressDateTime);
                    d_body->m_pressItemWidget->show();
                }

                if (event->pos().y() > d_body->m_pressPos.y()) {
                    rect = d_body->getTimeIntervalRect(d_body->m_pressDateTime.date().dayOfWeek(), d_body->m_pressDateTime.time(), dateTime.time());
                    d_body->m_pressItemWidget->setBeginDateTime(d_body->m_pressDateTime);
                    d_body->m_pressItemWidget->setEndDateTime(dateTime);
                }
                else {
                    rect = d_body->getTimeIntervalRect(d_body->m_pressDateTime.date().dayOfWeek(), dateTime.time(), d_body->m_pressDateTime.time());
                    d_body->m_pressItemWidget->setBeginDateTime(dateTime);
                    d_body->m_pressItemWidget->setEndDateTime(d_body->m_pressDateTime);
                }

                d_body->m_pressItemWidget->move(rect.x(), rect.y());
                d_body->m_pressItemWidget->resize(rect.width(), rect.height() < m_minimumItemHeight ? m_minimumItemHeight : rect.height());
            }
            break;
        }
    case DayRangeBodyPrivate::MouseMode_Move:
    case DayRangeBodyPrivate::MouseMode_Resize:
        {
            if (WarnBodyMouseEvents) {
                qWarning() << "DayBody::mouseMove (resize/moveMode)" << d_body->m_pressItem.uid() << d_body->m_pressItem.beginning() << d_body->m_pressItem.ending();
                qWarning() << "   pressDateTime" << d_body->m_pressDateTime;
                qWarning() << "   previousDateTime" << d_body->m_previousDateTime;
            }
            d_body->m_pressItemWidget->setInMotion(true);
            seconds = d_body->m_pressDateTime.time().secsTo(dateTime.time()); // seconds to add
            if (event->pos().y() > d_body->m_pressPos.y()) {
                QDateTime l = d_body->m_pressItem.ending().addDays(1);
                l.setTime(QTime(0, 0));
                limits = d_body->m_pressItem.ending().secsTo(l);
                if (seconds > limits)
                    seconds = limits;
            } else {
                QDateTime l = d_body->m_pressItem.beginning();
                l.setTime(QTime(0, 0));
                limits = d_body->m_pressItem.beginning().secsTo(l);
                if (seconds < limits)
                    seconds = limits;
            }

            if (d_body->m_mouseMode == DayRangeBodyPrivate::MouseMode_Move) {
                beginning = d_body->m_pressItem.beginning().addSecs(seconds);
                beginning.setDate(dateTime.date());
            } else
                beginning = d_body->m_pressItem.beginning();

            ending = d_body->m_pressItem.ending().addSecs(seconds);
            if (d_body->m_mouseMode == DayRangeBodyPrivate::MouseMode_Move) {
                ending.setDate(dateTime.date());
                d_body->m_pressItemWidget->setBeginDateTime(beginning);
            } else {
                if (ending <= beginning)
                    ending = beginning.addSecs(1800);
            }
            d_body->m_pressItemWidget->setEndDateTime(ending);
            rect = d_body->getTimeIntervalRect(beginning.date().dayOfWeek(), beginning.time(), ending.time());
            d_body->m_pressItemWidget->move(rect.x(), rect.y());
            d_body->m_pressItemWidget->resize(rect.width(), rect.height() < m_minimumItemHeight ? m_minimumItemHeight : rect.height());
            break;
        }
        default:;
        }
}

void DayRangeBody::mouseReleaseEvent(QMouseEvent *event)
{
	QDateTime beginning, ending;
	CalendarItem newItem;

    switch (d_body->m_mouseMode) {
    case DayRangeBodyPrivate::MouseMode_Creation:
        if (!d_body->m_pressItemWidget) {
            beginning = d_body->m_pressDateTime;
            ending = d_body->m_pressDateTime.addSecs(d_body->m_itemDefaultDuration * 60);
		} else {
            beginning = d_body->m_pressItemWidget->beginDateTime();
            ending = d_body->m_pressItemWidget->endDateTime();
            beginning.setDate(d_body->m_pressDateTime.date());
            ending.setDate(d_body->m_pressDateTime.date());
            delete d_body->m_pressItemWidget;
		}
//		if (model()) {
//                        newItem = model()->insertItem(beginning, ending);
//		}
		break;
    case DayRangeBodyPrivate::MouseMode_Move:
    case DayRangeBodyPrivate::MouseMode_Resize:
            if (!d_body->m_pressItemWidget->inMotion()) {
                if (!itemContextMenu()) {
                    // display a default contextual menu
                    QMenu menu;
                    QAction *modifyAction = menu.addAction(tr("modify"));
                    connect(modifyAction, SIGNAL(triggered()), this, SLOT(modifyPressItem()));
                    QAction *removeAction = menu.addAction(tr("remove"));
                    connect(removeAction, SIGNAL(triggered()), this, SLOT(removePressItem()));
                    menu.exec(event->globalPos());
                } else {
                    // use the specified menu
                    itemContextMenu()->exec(event->globalPos());
                }
            } else {
                int durationInSeconds = d_body->m_pressItemWidget->durationInSeconds();
                QDateTime end = d_body->m_pressItemWidget->beginDateTime().addSecs(durationInSeconds);
                newItem = d_body->m_pressItem;
                newItem.setBeginning(d_body->m_pressItemWidget->beginDateTime());
                newItem.setEnding(end);
                model()->moveItem(d_body->m_pressItem, newItem);
            }
            break;
        default:;
	}
    d_body->m_pressDateTime = QDateTime();
    d_body->m_pressItemWidget = 0;
    d_body->m_mouseMode = DayRangeBodyPrivate::MouseMode_None;
        if (WarnBodyMouseEvents) {
            qWarning() << "DayBody::mouseReleased" << d_body->m_pressItem.uid() << d_body->m_pressItem.beginning() << d_body->m_pressItem.ending();
            qWarning() << "   pressed DateTime" << d_body->m_previousDateTime;
        }

}

void DayRangeBody::mouseDoubleClickEvent(QMouseEvent *)
{
    d_body->m_pressItem = model()->insertItem(d_body->m_previousDateTime, d_body->m_previousDateTime.addSecs(d_body->m_itemDefaultDuration*60));
    BasicItemEditionDialog dialog(model(), this);
    dialog.init(d_body->m_pressItem);
    if (dialog.exec()==QDialog::Rejected) {
        removePressItem();
    }
    if (WarnBodyMouseEvents) {
        qWarning() << "DayBody::mousePressed" << d_body->m_pressItem.uid() << d_body->m_pressItem.beginning() << d_body->m_pressItem.ending();
        qWarning() << "   pressed DateTime" << d_body->m_previousDateTime;
    }
}

void DayRangeBody::itemInserted(const Calendar::CalendarItem &item)
{
    // refresh the entire day band
    d_body->refreshDayWidgets(item.beginning().date());
}

void DayRangeBody::itemRemoved(const Calendar::CalendarItem &removedItem)
{
    // refresh the involved bands
    d_body->refreshDayWidgets(removedItem.beginning().date());
}

void DayRangeBody::itemModified(const Calendar::CalendarItem &oldItem, const Calendar::CalendarItem &newItem)
{
	QList<QDate> daysToRefresh;
    if (!oldItem.intersects(firstDate(), firstDate().addDays(d_body->m_rangeWidth - 1))) { // collect all old item days
		daysToRefresh << oldItem.beginning().date();
		if (daysToRefresh.indexOf(oldItem.ending().date()) < 0)
			daysToRefresh << oldItem.ending().date();
	}
    if (!newItem.intersects(firstDate(), firstDate().addDays(d_body->m_rangeWidth - 1))) { // collect all new item days
		if (daysToRefresh.indexOf(newItem.beginning().date()) < 0)
			daysToRefresh << newItem.beginning().date();
		if (daysToRefresh.indexOf(newItem.ending().date()) < 0)
			daysToRefresh << newItem.ending().date();
	}

	foreach (const QDate &date, daysToRefresh)
        d_body->refreshDayWidgets(date);
}

void DayRangeBody::resetItemWidgets()
{
	deleteAllWidgets();
    for (int i = 0; i < d_body->m_rangeWidth; i++)
        d_body->refreshDayWidgets(firstDate().addDays(i));
}

void DayRangeBody::modifyPressItem()
{
    BasicItemEditionDialog dialog(model(), this);
    dialog.init(d_body->m_pressItem);
    if (dialog.exec()==QDialog::Accepted) {
        d_body->m_previousDateTime = QDateTime();
    }
}

void DayRangeBody::removePressItem()
{
    model()->removeItem(d_body->m_pressItem.uid());
    d_body->m_previousDateTime = QDateTime();
}