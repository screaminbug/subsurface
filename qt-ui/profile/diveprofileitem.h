#ifndef DIVEPROFILEITEM_H
#define DIVEPROFILEITEM_H

#include <QObject>
#include <QGraphicsPolygonItem>
#include <QModelIndex>

#include "graphicsview-common.h"
#include "divelineitem.h"

/* This is the Profile Item, it should be used for quite a lot of things
 on the profile view. The usage should be pretty simple:

 DiveProfileItem *profile = new DiveProfileItem();
 profile->setVerticalAxis( profileYAxis );
 profile->setHorizontalAxis( timeAxis );
 profile->setModel( DiveDataModel );
 profile->setHorizontalDataColumn( DiveDataModel::TIME );
 profile->setVerticalDataColumn( DiveDataModel::DEPTH );
 scene()->addItem(profile);

 This is a generically item and should be used as a base for others, I think...
*/

class DivePlotDataModel;
class DiveTextItem;
class DiveCartesianAxis;
class QAbstractTableModel;
struct plot_data;

class AbstractProfilePolygonItem : public QObject, public QGraphicsPolygonItem{
	Q_OBJECT
	Q_PROPERTY(QPointF pos WRITE setPos READ pos)
	Q_PROPERTY(qreal x WRITE setX READ x)
	Q_PROPERTY(qreal y WRITE setY READ y)
public:
	AbstractProfilePolygonItem();
	void setVerticalAxis(DiveCartesianAxis *vertical);
	void setHorizontalAxis(DiveCartesianAxis *horizontal);
	void setModel(DivePlotDataModel *model);
	void setHorizontalDataColumn(int column);
	void setVerticalDataColumn(int column);
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0) = 0;
	virtual void clear(){};
public slots:
	virtual void preferencesChanged();
	virtual void modelDataChanged(const QModelIndex& topLeft = QModelIndex(), const QModelIndex& bottomRight = QModelIndex());
	virtual void modelDataRemoved(const QModelIndex& parent, int from, int to);
protected:
	/* when the model emits a 'datachanged' signal, this method below should be used to check if the
	 * modified data affects this particular item ( for example, when setting the '3m increment'
	 * the data for Ceiling and tissues will be changed, and only those. so, the topLeft will be the CEILING
	 * column and the bottomRight will have the TISSUE_16 column. this method takes the vDataColumn and hDataColumn
	 * into consideration when returning 'true' for "yes, continue the calculation', and 'false' for
	 * 'do not recalculate, we already have the right data.
	 */
	bool shouldCalculateStuff(const QModelIndex& topLeft, const QModelIndex& bottomRight);

	DiveCartesianAxis *hAxis;
	DiveCartesianAxis *vAxis;
	DivePlotDataModel *dataModel;
	int hDataColumn;
	int vDataColumn;
	QList<DiveTextItem*> texts;
};

class DiveProfileItem : public AbstractProfilePolygonItem{
	Q_OBJECT

public:
	DiveProfileItem();
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual void modelDataChanged(const QModelIndex& topLeft = QModelIndex(), const QModelIndex& bottomRight = QModelIndex());
	virtual void preferencesChanged();
	void plot_depth_sample(struct plot_data *entry,QFlags<Qt::AlignmentFlag> flags,const QColor& color);
private:
	unsigned int show_reported_ceiling;
	unsigned int reported_ceiling_in_red;
};

class DiveTemperatureItem : public AbstractProfilePolygonItem{
	Q_OBJECT
public:
	DiveTemperatureItem();
	virtual void modelDataChanged(const QModelIndex& topLeft = QModelIndex(), const QModelIndex& bottomRight = QModelIndex());
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
private:
	void createTextItem(int seconds, int mkelvin);
};

class DiveGasPressureItem : public AbstractProfilePolygonItem{
	Q_OBJECT

public:
	virtual void modelDataChanged(const QModelIndex& topLeft = QModelIndex(), const QModelIndex& bottomRight = QModelIndex());
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
private:
	void plot_pressure_value(int mbar, int sec, QFlags<Qt::AlignmentFlag> align);
	void plot_gas_value(int mbar, int sec, QFlags<Qt::AlignmentFlag> align, int o2, int he);
	QVector<QPolygonF> polygons;
};

class DiveCalculatedCeiling : public AbstractProfilePolygonItem{
	Q_OBJECT

public:
	DiveCalculatedCeiling();
	virtual void modelDataChanged(const QModelIndex& topLeft = QModelIndex(), const QModelIndex& bottomRight = QModelIndex());
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual void preferencesChanged();
private:
	bool is3mIncrement;
	DiveTextItem *gradientFactor;
};

class DiveReportedCeiling : public AbstractProfilePolygonItem{
	Q_OBJECT

public:
	virtual void modelDataChanged(const QModelIndex& topLeft = QModelIndex(), const QModelIndex& bottomRight = QModelIndex());
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual void preferencesChanged();
};

class DiveCalculatedTissue : public DiveCalculatedCeiling {
	Q_OBJECT
public:
	DiveCalculatedTissue();
	virtual void preferencesChanged();
};

class MeanDepthLine : public DiveLineItem {
	Q_OBJECT
public:
	MeanDepthLine();
	void setMeanDepth(int value);
	void setLine(qreal x1, qreal y1, qreal x2, qreal y2);
private:
	int meanDepth;
	DiveTextItem *leftText;
	DiveTextItem *rightText;
};

class PartialPressureGasItem : public AbstractProfilePolygonItem{
	Q_OBJECT
public:
	PartialPressureGasItem();
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
	virtual void modelDataChanged(const QModelIndex& topLeft = QModelIndex(), const QModelIndex& bottomRight = QModelIndex());
	virtual void preferencesChanged();
	void setThreshouldSettingsKey(const QString& threshouldSettingsKey);
	void setVisibilitySettingsKey(const QString& setVisibilitySettingsKey);
	void setColors(const QColor& normalColor, const QColor& alertColor);
private:
	QPolygonF alertPoly;
	QString threshouldKey;
	QString visibilityKey;
	QColor normalColor;
	QColor alertColor;
};
#endif
