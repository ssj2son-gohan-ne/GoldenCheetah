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
 * 2020-07-16 Adapted BodyMeasures.h by ssj2son-gohan-ne
*/

#ifndef _Gc_Gears_h
#define _Gc_Gears_h

#include "Context.h"
#include "Measures.h"

#include <QString>
#include <QStringList>
#include <QDateTime>



// Swim Measures

class SwimGearMeasure : public Measure {
    Q_DECLARE_TR_FUNCTIONS(SwimGearMeasure)
public:

    enum swimgearmeasuretype { GlassesWeightKg = 0 };
    typedef enum swimgearmeasuretype SwimGearMeasureType;

    //GearMeasure() : glassesweightkg(0), bikeweightkg(0), shoeweightkg(0) {}
    SwimGearMeasure() : glassesweightkg(0) {}

    // depending on datasource not all fields may be filled with actual values

    double glassesweightkg;     // weight in Kilograms
    // calculate a CRC for the GearMeasure data - used to see if
    // data is changed in Configuration pages
    quint16 getFingerprint() const;
};

class SwimGearMeasureParser {

public:
    static bool serialize(QString, QList<SwimGearMeasure> &);
    static bool unserialize(QFile &, QList<SwimGearMeasure> &);
};


class SwimGearMG : public MeasuresGroup {
    Q_DECLARE_TR_FUNCTIONS(SwimGearMG)
public:
    // Default constructor intended to access metadata,
    // directory and withData must be provided to access data.
    SwimGearMG(QDir dir=QDir(), bool withData=false);
    ~SwimGearMG() {}
    void write();
    QList<SwimGearMeasure>& swimGearMG() { return swimGearMG_; }
    void setSwimGearMG(QList<SwimGearMeasure>&x);
    void getSwimGearMeasure(QDate date, SwimGearMeasure&) const;

    QString getSymbol() const { return "SwimGearMGT"; }
    QString getName() const { return tr("SwimGearMGT"); }
    QStringList getFieldSymbols() const ;
    QStringList getFieldNames() const;
    QDate getStartDate() const;
    QDate getEndDate() const;
    QString getFieldUnits(int field, bool useMetricUnits=true) const;
    double getFieldValue(QDate date, int field=SwimGearMeasure::GlassesWeightKg, bool useMetricUnits=true) const;

private:
    QDir dir;
    bool withData;
    QList<SwimGearMeasure> swimGearMG_;
};

// Bike Measures

class BikeGearMeasure : public Measure {
    Q_DECLARE_TR_FUNCTIONS(BikeGearMeasure)
public:

    enum gearmeasuretype { BikeWeightKg = 0 };
    typedef enum gearmeasuretype GearMeasureType;

    BikeGearMeasure() : bikeweightkg(0) {}

    // depending on datasource not all fields may be filled with actual values

    double bikeweightkg;     // weight in Kilograms
    // calculate a CRC for the GearMeasure data - used to see if
    // data is changed in Configuration pages
    quint16 getFingerprint() const;
};

class BikeGearMeasureParser {

public:
    static bool serialize(QString, QList<BikeGearMeasure> &);
    static bool unserialize(QFile &, QList<BikeGearMeasure> &);
};

class BikeGearMG : public MeasuresGroup {
    Q_DECLARE_TR_FUNCTIONS(BikeGearMG)
public:
    // Default constructor intended to access metadata,
    // directory and withData must be provided to access data.
    BikeGearMG(QDir dir=QDir(), bool withData=false);
    ~BikeGearMG() {}
    void write();
    QList<BikeGearMeasure>& bikeGearMG() { return bikeGearMG_; }
    void setBikeGearMG(QList<BikeGearMeasure>&x);
    void getBikeGearMeasure(QDate date, BikeGearMeasure&) const;

    QString getSymbol() const { return "BikeGearMGT"; }
    QString getName() const { return tr("BikeGearMGT"); }
    QStringList getFieldSymbols() const ;
    QStringList getFieldNames() const;
    QDate getStartDate() const;
    QDate getEndDate() const;
    QString getFieldUnits(int field, bool useMetricUnits=true) const;
    double getFieldValue(QDate date, int field=BikeGearMeasure::BikeWeightKg, bool useMetricUnits=true) const;

private:
    QDir dir;
    bool withData;
    QList<BikeGearMeasure> bikeGearMG_;
};

// Run Measures

class RunGearMeasure : public Measure {
    Q_DECLARE_TR_FUNCTIONS(RunGearMeasure)
public:

    enum gearmeasuretype { ShoeWeightKg = 0 };
    typedef enum gearmeasuretype GearMeasureType;

    RunGearMeasure() : shoeweightkg(0) {}

    // depending on datasource not all fields may be filled with actual values

    double shoeweightkg;     // weight in Kilograms
    // calculate a CRC for the GearMeasure data - used to see if
    // data is changed in Configuration pages
    quint16 getFingerprint() const;
};

class RunGearMeasureParser {

public:
    static bool serialize(QString, QList<RunGearMeasure> &);
    static bool unserialize(QFile &, QList<RunGearMeasure> &);
};


class RunGearMG : public MeasuresGroup {
    Q_DECLARE_TR_FUNCTIONS(RunGearMG)
public:
    // Default constructor intended to access metadata,
    // directory and withData must be provided to access data.
    RunGearMG(QDir dir=QDir(), bool withData=false);
    ~RunGearMG() {}
    void write();
    QList<RunGearMeasure>& runGearMG() { return runGearMG_; }
    void setRunGearMG(QList<RunGearMeasure>&x);
    void getRunGearMeasure(QDate date, RunGearMeasure&) const;

    QString getSymbol() const { return "RunGearMGT"; }
    QString getName() const { return tr("RunGearMGT"); }
    QStringList getFieldSymbols() const ;
    QStringList getFieldNames() const;
    QDate getStartDate() const;
    QDate getEndDate() const;
    QString getFieldUnits(int field, bool useMetricUnits=true) const;
    double getFieldValue(QDate date, int field=RunGearMeasure::ShoeWeightKg, bool useMetricUnits=true) const;

private:
    QDir dir;
    bool withData;
    QList<RunGearMeasure> runGearMG_;
};

#endif
