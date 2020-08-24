/*
 * Copyright (c) 2015 Joern Rischmueller (joern.rm@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


/*
 * 2020-08-20 Adapted Measures.h by ssj2son-gohan-ne
 *
 * Measure = SwimGearMeasure
 * Measures = SwimGearMeasures
 * measuresource = swimgearmeasuresource
 * MeasureSource = SwimGearMeasureSource
 * bodymeasuretype = swimgearmeasuretype
 * MeasuresGroup = SwimGearMeasuresGroup
 * measuresgrouptype = swimgearmeasuresgrouptype
 * MeasuresGroupType = SwimGearMeasuresGroupType
 * WeightKg = GogglesKg
 * MAX_MEASURES = MAX_GEARMEASURES
 * Body = SwimGear
 * //measure = swimgearmeasure
 * measures = swimgearmeasures -> does not work for {return measures ; } do it by hand
 * source = swimgearsource
 * originalSource = swimgearoriginalSource
 * x = xswim
*/

#ifndef _Gc_Gears_h
#define _Gc_Gears_h

#include "GoldenCheetah.h"

#include <QDate>
#include <QDir>
#include <QString>
#include <QStringList>
//#include <QDateTime>

#define MAX_GEARMEASURES 16

// ///////// Swim Gear Measure /////////////
class SwimGearMeasure {
    Q_DECLARE_TR_FUNCTIONS(SwimGearMeasure)
public:

    enum swimgearmeasuresource { Manual, Withings, TodaysPlan, CSV };
    typedef enum swimgearmeasuresource SwimGearMeasureSource;

    enum swimgearmeasuretype { GogglesKg = 0 };

    SwimGearMeasure() : when(QDateTime()), comment(""),
                swimgearsource(Manual), swimgearoriginalSource("") {
        for (int i = 0; i<MAX_GEARMEASURES; i++) values[i] = 0.0;
    }
    SwimGearMeasure(const SwimGearMeasure &other) {
        this->when = other.when;
        this->comment = other.comment;
        this->swimgearsource = other.swimgearsource;
        this->swimgearoriginalSource = other.swimgearoriginalSource;
        for (int i = 0; i<MAX_GEARMEASURES; i++) this->values[i] = other.values[i];
    }
    virtual ~SwimGearMeasure() {}

    QDateTime when;              // when was this reading taken
    QString comment;             // user commentary regarding this measurement

    SwimGearMeasureSource swimgearsource;
    QString swimgearoriginalSource;      // if delivered from the cloud service

    double values[MAX_GEARMEASURES]; // field values for standard swimgearmeasures

    // used by qSort()
    bool operator< (SwimGearMeasure right) const {
        return (when < right.when);
    }
    // calculate a CRC for the SwimGearMeasure data - used to see if
    // data is changed in Configuration pages
    virtual quint16 getFingerprint() const;

    // getdescription text for swimgearsource
    virtual QString getSourceDescription() const;
};

class SwimGearMeasuresGroup {

public:
    // Default constructor intended to access metadata,
    // directory and withData must be provided to access data.
    SwimGearMeasuresGroup(QString symbol, QString name, QStringList symbols, QStringList names, QStringList metricUnits, QStringList imperialUnits, QList<double> unitsFactors, QList<QStringList> headers,  QDir dir=QDir(), bool withData=false);
    SwimGearMeasuresGroup(QDir dir=QDir(), bool withData=false) : dir(dir), withData(withData) {}
    virtual ~SwimGearMeasuresGroup() {}
    virtual void write();
    virtual QList<SwimGearMeasure>& swimgearmeasures() { return swimgearmeasures_; }
    virtual void setMeasures(QList<SwimGearMeasure>&xswim);
    virtual void getMeasure(QDate date, SwimGearMeasure&) const;

    // Common access to SwimGearMeasures
    virtual QString getSymbol() const { return symbol; }
    virtual QString getName() const { return name; }
    virtual QStringList getFieldSymbols() const { return symbols; }
    virtual QStringList getFieldNames() const { return names; }
    virtual QStringList getFieldHeaders(int field) const { return headers.value(field); }
    virtual QString getFieldUnits(int field, bool useMetricUnits=true) const { return useMetricUnits ? metricUnits.value(field) : imperialUnits.value(field); }
    virtual QList<double> getFieldUnitsFactors() const { return unitsFactors; }
    virtual double getFieldValue(QDate date, int field=0, bool useMetricUnits=true) const;
    virtual QDate getStartDate() const;
    virtual QDate getEndDate() const;

protected:
    const QDir dir;
    const bool withData;

private:
    const QString symbol, name;
    const QStringList symbols, names, metricUnits, imperialUnits;
    const QList<double> unitsFactors;
    const QList<QStringList> headers;
    QList<SwimGearMeasure> swimgearmeasures_;

    bool serialize(QString, QList<SwimGearMeasure> &);
    bool unserialize(QFile &, QList<SwimGearMeasure> &);
};

class SwimGearMeasures {
    Q_DECLARE_TR_FUNCTIONS(SwimGearMeasures)
public:
    // Default constructor intended to access metadata,
    // directory and withData must be provided to access data.
    SwimGearMeasures(QDir dir=QDir(), bool withData=false);
    ~SwimGearMeasures();

    QStringList getGroupSymbols() const;
    QStringList getGroupNames() const;
    SwimGearMeasuresGroup* getGroup(int group);

    enum swimgearmeasuresgrouptype { SwimGear = 0 };
    typedef enum swimgearmeasuresgrouptype SwimGearMeasuresGroupType;

    // Convenience methods to simplify client code
    QStringList getFieldSymbols(int group) const;
    QStringList getFieldNames(int group) const;
    QDate getStartDate(int group) const;
    QDate getEndDate(int group) const;
    QString getFieldUnits(int group, int field, bool useMetricUnits=true) const;
    double getFieldValue(int group, QDate date, int field, bool useMetricUnits=true) const;

private:
    QDir dir;
    bool withData;
    QList<SwimGearMeasuresGroup*> groups;
};

#endif

