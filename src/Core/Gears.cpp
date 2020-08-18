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
 * Adapted BodyMeasures.cpp 2020-07-16 by ssj2son-gohan-ne
*/


#include "Gears.h"
#include "Units.h"

#include <QList>

#include <QMessageBox>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>



// Swim gear part
quint16
SwimGearMeasure::getFingerprint() const
{
    quint64 xs = 0;

    xs += 1000.0 * glassesweightkg;

    QByteArray bas = QByteArray::number(xs);

    return qChecksum(bas, bas.length()) + Measure::getFingerprint();
}


bool
SwimGearMeasureParser::serialize(QString filename, QList<SwimGearMeasure> &data) {

    // open file - truncate contents
    QFile file(filename);
    if (!file.open(QFile::WriteOnly)) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QObject::tr("Problem Saving Swim Gear"));
        msgBox.setInformativeText(QObject::tr("File: %1 cannot be opened for 'Writing'. Please check file properties.").arg(filename));
        msgBox.exec();
        return false;
    };
    file.resize(0);
    QTextStream out(&file);
    out.setCodec("UTF-8");

    SwimGearMeasure *ms = NULL;
    QJsonArray measures;
    for (int i = 0; i < data.count(); i++) {
        ms = &data[i];
        QJsonObject measure;
        measure.insert("when", ms->when.toMSecsSinceEpoch()/1000);
        measure.insert("comment", ms->comment);
        measure.insert("glassesvendor", ms->glassesvendor);
        measure.insert("glassesmodel", ms->glassesmodel);
        measure.insert("glassestype", ms->glassestype);
        measure.insert("glassesweightkg", ms->glassesweightkg);
        measure.insert("source", ms->source);
        measure.insert("originalsource", ms->originalSource);
        measures.append(measure);
    }

    QJsonObject jsonObject;
    // add a version in case of format changes
    jsonObject.insert("version", 1);
    jsonObject.insert("measures",  QJsonValue(measures));

    QJsonDocument json;
    json.setObject(jsonObject);

    out << json.toJson();
    out.flush();
    file.close();
    return true;

}


bool
SwimGearMeasureParser::unserialize(QFile &file, QList<SwimGearMeasure> &data) {

    // open file - truncate contents
    if (!file.open(QFile::ReadOnly)) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QObject::tr("Problem Reading Swim Gear"));
        msgBox.setInformativeText(QObject::tr("File: %1 cannot be opened for 'Reading'. Please check file properties.").arg(file.fileName()));
        msgBox.exec();
        return false;
    };
    QByteArray jsonFileContent = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(jsonFileContent, &parseError);

    if (parseError.error != QJsonParseError::NoError || document.isEmpty() || document.isNull()) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QObject::tr("Problem Parsing Swim Gear"));
        msgBox.setInformativeText(QObject::tr("File: %1 is not a proper JSON file. Parsing error: %2").arg(file.fileName()).arg(parseError.errorString()));
        msgBox.exec();
        return false;
    }

    data.clear();
    QJsonObject object = document.object();
    QJsonArray measures = object["measures"].toArray();
    for (int i = 0; i < measures.count(); i++) {
        QJsonObject measure = measures.at(i).toObject();
        SwimGearMeasure ms;
        ms.when = QDateTime::fromMSecsSinceEpoch(measure["when"].toDouble()*1000);
        ms.comment = measure["comment"].toString();
        ms.glassesvendor = measure["glassesvendor"].toString();
        ms.glassesmodel = measure["glassesmodel"].toString();
        ms.glassestype = measure["glassestype"].toString();
        ms.glassesweightkg = measure["glassesweightkg"].toDouble();
        ms.source = static_cast<Measure::MeasureSource>(measure["source"].toInt());
        ms.originalSource = measure["originalsource"].toString();
        data.append(ms);
    }

    file.close();
    return true;
}

SwimGearMG::SwimGearMG(QDir dir, bool withData) : dir(dir), withData(withData) {
    // don't load data if not requested
    if (!withData) return;

    // get gear measurements if the file exists
    QFile swimGearFile(QString("%1/swimGearMG.json").arg(dir.canonicalPath()));
    if (swimGearFile.exists()) {
        QList<SwimGearMeasure> swimGearData;
        if (SwimGearMeasureParser::unserialize(swimGearFile, swimGearData)){
            setSwimGearMG(swimGearData);
        }
    }
}

void
SwimGearMG::write() {
    // Nothing to do if data not loaded
    if (!withData) return;

    // now save data away
    SwimGearMeasureParser::serialize(QString("%1/swimGearMG.json").arg(dir.canonicalPath()), swimGearMG_);
}

void
SwimGearMG::setSwimGearMG(QList<SwimGearMeasure>&xs)
{
    swimGearMG_ = xs;
    qSort(swimGearMG_); // date order
}

QStringList
SwimGearMG::getFieldSymbols() const {
	static const QStringList symbols = QStringList()<<"GlassesVendor"<<"GlassesModel"<<"GlassesType"<<"GlassesWeightKg";
    return symbols;
}

QStringList
SwimGearMG::getFieldNames() const {
    static const QStringList names = QStringList()<<tr("Glasses Vendor")<<tr("Glasses Model")<<tr("Glasses Type")<<tr("Glasses Weight");
    return names;
}

QDate
SwimGearMG::getStartDate() const {
    if (!swimGearMG_.isEmpty())
        return swimGearMG_.first().when.date();
    else
        return QDate();
}

QDate
SwimGearMG::getEndDate() const {
    if (!swimGearMG_.isEmpty())
        return swimGearMG_.last().when.date();
    else
        return QDate();
}

QString
SwimGearMG::getFieldUnits(int field, bool useMetricUnits) const {
    static const QStringList metricUnits = QStringList()<<tr("kg");
    static const QStringList imperialUnits = QStringList()<<tr("lbs");
    return useMetricUnits ? metricUnits.value(field) : imperialUnits.value(field);
}

void
SwimGearMG::getSwimGearMeasure(QDate date, SwimGearMeasure &here) const {
    // the optimisation below is not thread safe and should be encapsulated
    // by a mutex, but this kind of defeats to purpose of the optimisation!

    //if (withings_.count() && withings_.last().when.date() <= date) here = withings_.last();
    //if (!withings_.count() || withings_.first().when.date() > date) here = WithingsReading();

    // always set to not found before searching
    here = SwimGearMeasure();

    // loop
    foreach(SwimGearMeasure xs, swimGearMG_) {

        // we only look for defaultvendor readings at present
        // some readings may not include this so skip them
        if (xs.glassesweightkg <= 0) continue;

        if (xs.when.date() <= date) here = xs;
        if (xs.when.date() > date) break;
    }

    // will be empty if none found
    return;
}

double
SwimGearMG::getFieldValue(QDate date, int field, bool useMetricUnits) const {
    const double units_factor = useMetricUnits ? 1.0 : LB_PER_KG;
    SwimGearMeasure swimgearweight;
    getSwimGearMeasure(date, swimgearweight);

    // return what was asked for!
    switch(field) {

        default:
        case SwimGearMeasure::GlassesWeightKg : return swimgearweight.glassesweightkg * units_factor;
    }
}




// Bike gear part
quint16
BikeGearMeasure::getFingerprint() const
{
    quint64 xb = 0;

    xb += 1000.0 * bikeweightkg;

    QByteArray bab = QByteArray::number(xb);

    return qChecksum(bab, bab.length()) + Measure::getFingerprint();
}


bool
BikeGearMeasureParser::serialize(QString filename, QList<BikeGearMeasure> &data) {

    // open file - truncate contents
    QFile file(filename);
    if (!file.open(QFile::WriteOnly)) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QObject::tr("Problem Saving Bike Gear"));
        msgBox.setInformativeText(QObject::tr("File: %1 cannot be opened for 'Writing'. Please check file properties.").arg(filename));
        msgBox.exec();
        return false;
    };
    file.resize(0);
    QTextStream out(&file);
    out.setCodec("UTF-8");

    BikeGearMeasure *mb = NULL;
    QJsonArray measures;
    for (int i = 0; i < data.count(); i++) {
        mb = &data[i];
        QJsonObject measure;
        measure.insert("when", mb->when.toMSecsSinceEpoch()/1000);
        measure.insert("comment", mb->comment);
        measure.insert("bikevendor", mb->bikevendor);
        measure.insert("bikemodel", mb->bikemodel);
        measure.insert("biketype", mb->biketype);
        measure.insert("bikeweightkg", mb->bikeweightkg);
        measure.insert("source", mb->source);
        measure.insert("originalsource", mb->originalSource);
        measures.append(measure);
    }

    QJsonObject jsonObject;
    // add a version in case of format changes
    jsonObject.insert("version", 1);
    jsonObject.insert("measures",  QJsonValue(measures));

    QJsonDocument json;
    json.setObject(jsonObject);

    out << json.toJson();
    out.flush();
    file.close();
    return true;

}


bool
BikeGearMeasureParser::unserialize(QFile &file, QList<BikeGearMeasure> &data) {

    // open file - truncate contents
    if (!file.open(QFile::ReadOnly)) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QObject::tr("Problem Reading Bike Gear"));
        msgBox.setInformativeText(QObject::tr("File: %1 cannot be opened for 'Reading'. Please check file properties.").arg(file.fileName()));
        msgBox.exec();
        return false;
    };
    QByteArray jsonFileContent = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(jsonFileContent, &parseError);

    if (parseError.error != QJsonParseError::NoError || document.isEmpty() || document.isNull()) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QObject::tr("Problem Parsing Bike Gear"));
        msgBox.setInformativeText(QObject::tr("File: %1 is not a proper JSON file. Parsing error: %2").arg(file.fileName()).arg(parseError.errorString()));
        msgBox.exec();
        return false;
    }

    data.clear();
    QJsonObject object = document.object();
    QJsonArray measures = object["measures"].toArray();
    for (int i = 0; i < measures.count(); i++) {
        QJsonObject measure = measures.at(i).toObject();
        BikeGearMeasure mb;
        mb.when = QDateTime::fromMSecsSinceEpoch(measure["when"].toDouble()*1000);
        mb.comment = measure["comment"].toString();
        mb.bikevendor = measure["bikevendor"].toString();
        mb.bikemodel = measure["bikemodel"].toString();
        mb.biketype = measure["biketype"].toString();
        mb.bikeweightkg = measure["bikeweightkg"].toDouble();
        mb.source = static_cast<Measure::MeasureSource>(measure["source"].toInt());
        mb.originalSource = measure["originalsource"].toString();
        data.append(mb);
    }

    file.close();
    return true;
}

BikeGearMG::BikeGearMG(QDir dir, bool withData) : dir(dir), withData(withData) {
    // don't load data if not requested
    if (!withData) return;

    // get gear measurements if the file exists
    QFile bikeGearFile(QString("%1/bikeGearMG.json").arg(dir.canonicalPath()));
    if (bikeGearFile.exists()) {
        QList<BikeGearMeasure> bikeGearData;
        if (BikeGearMeasureParser::unserialize(bikeGearFile, bikeGearData)){
            setBikeGearMG(bikeGearData);
        }
    }
}

void
BikeGearMG::write() {
    // Nothing to do if data not loaded
    if (!withData) return;

    // now save data away
    BikeGearMeasureParser::serialize(QString("%1/bikeGearMG.json").arg(dir.canonicalPath()), bikeGearMG_);
}

void
BikeGearMG::setBikeGearMG(QList<BikeGearMeasure>&xb)
{
    bikeGearMG_ = xb;
    qSort(bikeGearMG_); // date order
}

QStringList
BikeGearMG::getFieldSymbols() const {
    static const QStringList symbols = QStringList()<<"BikeVendor"<<"BikeModel"<<"BikeType"<<"BikeWeightKg";
    return symbols;
}

QStringList
BikeGearMG::getFieldNames() const {
    static const QStringList names = QStringList()<<tr("Bike Vendor")<<tr("Bike Model")<<tr("Bike Type")<<tr("Bike Weight");
    return names;
}

QDate
BikeGearMG::getStartDate() const {
    if (!bikeGearMG_.isEmpty())
        return bikeGearMG_.first().when.date();
    else
        return QDate();
}

QDate
BikeGearMG::getEndDate() const {
    if (!bikeGearMG_.isEmpty())
        return bikeGearMG_.last().when.date();
    else
        return QDate();
}

QString
BikeGearMG::getFieldUnits(int field, bool useMetricUnits) const {
    static const QStringList metricUnits = QStringList()<<tr("kg");
    static const QStringList imperialUnits = QStringList()<<tr("lbs");
    return useMetricUnits ? metricUnits.value(field) : imperialUnits.value(field);
}

void
BikeGearMG::getBikeGearMeasure(QDate date, BikeGearMeasure &here) const {
    // the optimisation below is not thread safe and should be encapsulated
    // by a mutex, but this kind of defeats to purpose of the optimisation!

    //if (withings_.count() && withings_.last().when.date() <= date) here = withings_.last();
    //if (!withings_.count() || withings_.first().when.date() > date) here = WithingsReading();

    // always set to not found before searching
    here = BikeGearMeasure();

    // loop
    foreach(BikeGearMeasure xb, bikeGearMG_) {

        // we only look for defaultvendor readings at present
        // some readings may not include this so skip them
        if (xb.bikeweightkg <= 0) continue;


        if (xb.when.date() <= date) here = xb;
        if (xb.when.date() > date) break;
    }

    // will be empty if none found
    return;
}

double
BikeGearMG::getFieldValue(QDate date, int field, bool useMetricUnits) const {
    const double units_factor = useMetricUnits ? 1.0 : LB_PER_KG;
    BikeGearMeasure bikegearweight;
    getBikeGearMeasure(date, bikegearweight);

    // return what was asked for!
    switch(field) {

        default:
        case BikeGearMeasure::BikeWeightKg : return bikegearweight.bikeweightkg * units_factor;
    }
}

// Run gear part
quint16
RunGearMeasure::getFingerprint() const
{
    quint64 xr = 0;

    xr += 1000.0 * shoeweightkg;

    QByteArray bar = QByteArray::number(xr);

    return qChecksum(bar, bar.length()) + Measure::getFingerprint();
}


bool
RunGearMeasureParser::serialize(QString filename, QList<RunGearMeasure> &data) {

    // open file - truncate contents
    QFile file(filename);
    if (!file.open(QFile::WriteOnly)) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QObject::tr("Problem Saving Run Gear"));
        msgBox.setInformativeText(QObject::tr("File: %1 cannot be opened for 'Writing'. Please check file properties.").arg(filename));
        msgBox.exec();
        return false;
    };
    file.resize(0);
    QTextStream out(&file);
    out.setCodec("UTF-8");

    RunGearMeasure *mr = NULL;
    QJsonArray measures;
    for (int i = 0; i < data.count(); i++) {
        mr = &data[i];
        QJsonObject measure;
        measure.insert("when", mr->when.toMSecsSinceEpoch()/1000);
        measure.insert("comment", mr->comment);
        measure.insert("shoevendor", mr->shoevendor);
        measure.insert("shoemodel", mr->shoemodel);
        measure.insert("shoetype", mr->shoetype);
        measure.insert("shoeweightkg", mr->shoeweightkg);
        measure.insert("source", mr->source);
        measure.insert("originalsource", mr->originalSource);
        measures.append(measure);
    }

    QJsonObject jsonObject;
    // add a version in case of format changes
    jsonObject.insert("version", 1);
    jsonObject.insert("measures",  QJsonValue(measures));

    QJsonDocument json;
    json.setObject(jsonObject);

    out << json.toJson();
    out.flush();
    file.close();
    return true;

}


bool
RunGearMeasureParser::unserialize(QFile &file, QList<RunGearMeasure> &data) {

    // open file - truncate contents
    if (!file.open(QFile::ReadOnly)) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QObject::tr("Problem Reading Run Gear"));
        msgBox.setInformativeText(QObject::tr("File: %1 cannot be opened for 'Reading'. Please check file properties.").arg(file.fileName()));
        msgBox.exec();
        return false;
    };
    QByteArray jsonFileContent = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(jsonFileContent, &parseError);

    if (parseError.error != QJsonParseError::NoError || document.isEmpty() || document.isNull()) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QObject::tr("Problem Parsing Run Gear"));
        msgBox.setInformativeText(QObject::tr("File: %1 is not a proper JSON file. Parsing error: %2").arg(file.fileName()).arg(parseError.errorString()));
        msgBox.exec();
        return false;
    }

    data.clear();
    QJsonObject object = document.object();
    QJsonArray measures = object["measures"].toArray();
    for (int i = 0; i < measures.count(); i++) {
        QJsonObject measure = measures.at(i).toObject();
        RunGearMeasure mr;
        mr.when = QDateTime::fromMSecsSinceEpoch(measure["when"].toDouble()*1000);
        mr.comment = measure["comment"].toString();
        mr.shoevendor = measure["shoevendor"].toString();
        mr.shoemodel = measure["shoemodel"].toString();
        mr.shoetype = measure["shoetype"].toString();
        mr.shoeweightkg = measure["shoeweightkg"].toDouble();
        mr.source = static_cast<Measure::MeasureSource>(measure["source"].toInt());
        mr.originalSource = measure["originalsource"].toString();
        data.append(mr);
    }

    file.close();
    return true;
}

RunGearMG::RunGearMG(QDir dir, bool withData) : dir(dir), withData(withData) {
    // don't load data if not requested
    if (!withData) return;

    // get gear measurements if the file exists
    QFile runGearFile(QString("%1/runGearMG.json").arg(dir.canonicalPath()));
    if (runGearFile.exists()) {
        QList<RunGearMeasure> runGearData;
        if (RunGearMeasureParser::unserialize(runGearFile, runGearData)){
            setRunGearMG(runGearData);
        }
    }
}

void
RunGearMG::write() {
    // Nothing to do if data not loaded
    if (!withData) return;

    // now save data away
    RunGearMeasureParser::serialize(QString("%1/runGearMG.json").arg(dir.canonicalPath()), runGearMG_);
}

void
RunGearMG::setRunGearMG(QList<RunGearMeasure>&xr)
{
    runGearMG_ = xr;
    qSort(runGearMG_); // date order
}

QStringList
RunGearMG::getFieldSymbols() const {
    static const QStringList symbols = QStringList()<<"ShoeVendor"<<"ShoeModel"<<"ShoeType"<<"ShoeWeightKg";
    return symbols;
}

QStringList
RunGearMG::getFieldNames() const {
    static const QStringList names = QStringList()<<tr("Shoe Vendor")<<tr("Shoe Model")<<tr("Shoe Type")<<tr("Shoe Weight");
    return names;
}

QDate
RunGearMG::getStartDate() const {
    if (!runGearMG_.isEmpty())
        return runGearMG_.first().when.date();
    else
        return QDate();
}

QDate
RunGearMG::getEndDate() const {
    if (!runGearMG_.isEmpty())
        return runGearMG_.last().when.date();
    else
        return QDate();
}

QString
RunGearMG::getFieldUnits(int field, bool useMetricUnits) const {
    static const QStringList metricUnits = QStringList()<<tr("kg");
    static const QStringList imperialUnits = QStringList()<<tr("lbs");
    return useMetricUnits ? metricUnits.value(field) : imperialUnits.value(field);
}

void
RunGearMG::getRunGearMeasure(QDate date, RunGearMeasure &here) const {
    // the optimisation below is not thread safe and should be encapsulated
    // by a mutex, but this kind of defeats to purpose of the optimisation!

    //if (withings_.count() && withings_.last().when.date() <= date) here = withings_.last();
    //if (!withings_.count() || withings_.first().when.date() > date) here = WithingsReading();

    // always set to not found before searching
    here = RunGearMeasure();

    // loop
    foreach(RunGearMeasure xr, runGearMG_) {

        // we only look for defaultvendor readings at present
        // some readings may not include this so skip them
        if (xr.shoeweightkg <= 0) continue;


        if (xr.when.date() <= date) here = xr;
        if (xr.when.date() > date) break;
    }

    // will be empty if none found
    return;
}

double
RunGearMG::getFieldValue(QDate date, int field, bool useMetricUnits) const {
    const double units_factor = useMetricUnits ? 1.0 : LB_PER_KG;
    RunGearMeasure rungearweight;
    getRunGearMeasure(date, rungearweight);

    // return what was asked for!
    switch(field) {

        default:
        case RunGearMeasure::ShoeWeightKg : return rungearweight.shoeweightkg * units_factor;
    }
}
