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
 * Adapted Measures.cpp 2020-08-20 by ssj2son-gohan-ne
 *
 * x = xswimgear
 * ba = baswimgear
 * m = mswim
 * m. = mswim.
 * k = kswim
 *
 * measuresFile = swimgearmeasuresFile
 * // setMeasures = setSwimGearMeasures
 * measure = swimgearmeasure
 * originalsource = swimgearoriginalsource
 *
 * WEIGHTKG = GOGGLESKG
 * weightkg = goggleskg
 *
 *
 *
 *
*/


#include "Gears.h"
#include "Units.h"

#include <QList>
#include <QMessageBox>
#include <QTextStream>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include <QDebug>


// //////////////////// Swim Gear Measure class ///////////////////////

quint16
SwimGearMeasure::getFingerprint() const
{
    quint64 xswimgear = 0;

    xswimgear += when.date().toJulianDay();

    for (int i = 0; i<MAX_GEARMEASURES; i++) xswimgear += 1000.0 * values[i];

    QByteArray baswimgear = QByteArray::number(xswimgear);
    baswimgear.append(comment);

    return qChecksum(baswimgear, baswimgear.length());
}

QString
SwimGearMeasure::getSourceDescription() const
{

    switch(source) {
    case SwimGearMeasure::Manual:
        return tr("Manual entry");
    case SwimGearMeasure::Withings:
        return tr("Withings");
    case SwimGearMeasure::TodaysPlan:
        return tr("Today's Plan");
    case SwimGearMeasure::CSV:
        return tr("CSV Upload");
    default:
        return tr("Unknown");
    }
}

///////////////////////////// SwimGearMeasuresGroup class ///////////////////////////

SwimGearMeasuresGroup::SwimGearMeasuresGroup(QString symbol, QString name, QStringList symbols, QStringList names, QStringList metricUnits, QStringList imperialUnits, QList<double>unitsFactors, QList<QStringList> headers,  QDir dir, bool withData) : dir(dir), withData(withData), symbol(symbol), name(name), symbols(symbols), names(names), metricUnits(metricUnits), imperialUnits(imperialUnits), unitsFactors(unitsFactors), headers(headers)
{
    // don't load data if not requested
    if (!withData) return;

    // get measurements if the file exists
    QFile measuresFile(QString("%1/%2measures.json").arg(dir.canonicalPath()).arg(symbol.toLower()));
    if (measuresFile.exists()) {
        QList<SwimGearMeasure> swimgearmeasures;
        if (unserialize(measuresFile, swimgearmeasures)){
            setMeasures(swimgearmeasures);
        }
    }
}

void
SwimGearMeasuresGroup::write()
{
    // Nothing to do if data not loaded
    if (!withData) return;

    // now save data away
    serialize(QString("%1/%2measures.json").arg(dir.canonicalPath()).arg(symbol.toLower()), swimgearmeasures_);
}

void
SwimGearMeasuresGroup::setMeasures(QList<SwimGearMeasure>&xswim)
{
    measures_ = xswim;
    qSort(measures_); // date order
}

QDate
SwimGearMeasuresGroup::getStartDate() const
{
    if (!measures_.isEmpty())
        return measures_.first().when.date();
    else
        return QDate();
}

QDate
SwimGearMeasuresGroup::getEndDate() const
{
    if (!measures_.isEmpty())
        return measures_.last().when.date();
    else
        return QDate();
}

void
SwimGearMeasuresGroup::getMeasure(QDate date, SwimGearMeasure &here) const
{
    // always set to not found before searching
    here = SwimGearMeasure();

    // loop
    foreach(SwimGearMeasure xswim, measures_) {

        if (symbol == "SwimGear" && xswim.when.date() < date) here = xswim;//TODO generalize
        if (xswim.when.date() == date) here = xswim;
        if (xswim.when.date() > date) break;
    }

    // will be empty if none found
    return;
}

double
SwimGearMeasuresGroup::getFieldValue(QDate date, int field, bool useMetricUnits) const
{
    SwimGearMeasure swimgearmeasure;
    getMeasure(date, swimgearmeasure);

    // return what was asked for!
    if (field >= 0 && field < MAX_GEARMEASURES)
        return swimgearmeasure.values[field]*(useMetricUnits ? 1.0 : unitsFactors[field]);
    else
        return 0.0;
}

bool
SwimGearMeasuresGroup::serialize(QString filename, QList<SwimGearMeasure> &data)
{

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

    SwimGearMeasure *mswim = NULL;
    QJsonArray swimgearmeasures;
    for (int i = 0; i < data.count(); i++) {
        mswim = &data[i];
        QJsonObject swimgearmeasure;
        swimgearmeasure.insert("when", mswim->when.toMSecsSinceEpoch()/1000);
        swimgearmeasure.insert("comment", mswim->comment);

        int kswim = 0;
        foreach (QString field, getFieldSymbols())
            swimgearmeasure.insert(field.toLower(), (kswim < MAX_GEARMEASURES) ? mswim->values[kswim++] : 0);

        swimgearmeasure.insert("swimgearsource", mswim->swimgearsource);
        swimgearmeasure.insert("swimgearoriginalsource", mswim->swimgearoriginalSource);
        swimgearmeasures.append(swimgearmeasure);
    }

    QJsonObject jsonObject;
    // add a version in case of format changes
    jsonObject.insert("version", 1);
    jsonObject.insert("swimgearmeasures",  QJsonValue(swimgearmeasures));

    QJsonDocument json;
    json.setObject(jsonObject);

    out << json.toJson();
    out.flush();
    file.close();
    return true;

}

bool
SwimGearMeasuresGroup::unserialize(QFile &file, QList<SwimGearMeasure> &data)
{

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
    QJsonArray swimgearmeasures = object["swimgearmeasures"].toArray();
    for (int i = 0; i < swimgearmeasures.count(); i++) {
        QJsonObject swimgearmeasure = swimgearmeasures.at(i).toObject();
        SwimGearMeasure mswim;
        mswim.when = QDateTime::fromMSecsSinceEpoch(swimgearmeasure["when"].toDouble()*1000);
        mswim.comment = swimgearmeasure["comment"].toString();

        int kswim = 0;
        foreach (QString field, getFieldSymbols())
            if (swimgearmeasure.contains(field.toLower()))
                if (kswim < MAX_GEARMEASURES)
                    mswim.values[kswim++] = swimgearmeasure[field.toLower()].toDouble();

        mswim.swimgearsource = static_cast<SwimGearMeasure::SwimGearMeasureSource>(swimgearmeasure["swimgearsource"].toInt());
        mswim.swimgearoriginalSource = swimgearmeasure["swimgearoriginalsource"].toString();
        data.append(mswim);
    }

    return true;
}

// /////////////////////////// Swim Gear Measures class ////////////////////////////////

SwimGearMeasures::SwimGearMeasures(QDir dir, bool withData) : dir(dir), withData(withData)
{
    // load user defined swimgearmeasures from swimgearmeasures.ini
    QString filename = dir.canonicalPath() + "/swimgearmeasures.ini";

    if (!QFile(filename).exists()) {

        // By default pre-load mandatory swimgearmeasures in SwimGearMeasuresGroupType order

        groups.append(new SwimGearMeasuresGroup("SwimGear", tr("SwimGear"),
            QStringList()<<"WEIGHTKG"<<"FATKG"<<"MUSCLEKG"<<"BONESKG"<<"LEANKG"<<"FATPERCENT",
            QStringList()<<tr("Weight")<<tr("Fat Mass")<<tr("Muscle Mass")<<tr("Bone Mass")<<tr("Lean Mass")<<tr("Fat Percent"),
            QStringList()<<tr("kg")<<tr("kg")<<tr("kg")<<tr("kg")<<tr("kg")<<tr("%"),
            QStringList()<<tr("lbs")<<tr("lbs")<<tr("lbs")<<tr("lbs")<<tr("lbs")<<tr("%"),
            QList<double>()<<LB_PER_KG<<LB_PER_KG<<LB_PER_KG<<LB_PER_KG<<LB_PER_KG<<1.0,
            QList<QStringList>()<<
                (QStringList()<<"goggleskg")<<
                (QStringList()<<"fatkg")<<
                (QStringList()<<"boneskg")<<
                (QStringList()<<"musclekg")<<
                (QStringList()<<"leankg")<<
                (QStringList()<<"fatpercent"),
            dir, withData));

        // other standard swimgearmeasures can be loaded from resources
        filename = ":/ini/swimgearmeasures.ini";
    }

    QSettings config(filename, QSettings::IniFormat);
    config.setIniCodec("UTF-8"); // to allow translated names

    foreach (QString group, config.childGroups()) {

        if (getGroupSymbols().contains(group)) continue;

        config.beginGroup(group);

        QString name = config.value("Name", group).toString();

        QStringList fields = config.value("Fields", "").toStringList();
        if (fields.count() == 0 || fields.count() > MAX_GEARMEASURES) continue;

        QStringList names = config.value("Names", "").toStringList();
        if (names.count() == 0) names = fields;
        if (fields.count() != names.count()) continue;

        QStringList units = config.value("MetricUnits", "").toStringList();
        if (units.count() == 0) foreach (QString field, fields) units << "";
        if (fields.count() != units.count()) continue;

        QStringList imperialUnits = config.value("ImperialUnits", units).toStringList();
        if (fields.count() != imperialUnits.count()) continue;

        QList<double> unitsFactors;
        foreach (QString field, fields) {
            unitsFactors.append(1.0);
        }

        QList<QStringList> headersList;
        foreach (QString field, fields) {
            QStringList headers = config.value(field, "").toStringList();
            headersList.append(headers);
        }

        groups.append(new SwimGearMeasuresGroup(group, name, fields, names, units, imperialUnits, unitsFactors, headersList, dir, withData));

        config.endGroup();
    }
}

SwimGearMeasures::~SwimGearMeasures()
{
    foreach (SwimGearMeasuresGroup* measuresGroup, groups)
        delete measuresGroup;
}

QStringList
SwimGearMeasures::getGroupSymbols() const
{
    QStringList symbols;
    foreach (SwimGearMeasuresGroup* measuresGroup, groups)
        symbols.append(measuresGroup->getSymbol());
    return symbols;
}

QStringList
SwimGearMeasures::getGroupNames() const
{
    QStringList names;
    foreach (SwimGearMeasuresGroup* measuresGroup, groups)
        names.append(measuresGroup->getName());
    return names;
}

SwimGearMeasuresGroup*
SwimGearMeasures::getGroup(int group)
{
    if (group >= 0 && group < groups.count())
        return groups[group];
    else
        return NULL;
}

QStringList
SwimGearMeasures::getFieldSymbols(int group) const
{
    return groups.at(group) != NULL ? groups.at(group)->getFieldSymbols() : QStringList();
}

QStringList
SwimGearMeasures::getFieldNames(int group) const
{
    return groups.at(group) != NULL ? groups.at(group)->getFieldNames() : QStringList();
}

QDate
SwimGearMeasures::getStartDate(int group) const
{
    return groups.at(group) != NULL ? groups.at(group)->getStartDate() : QDate();
}

QDate
SwimGearMeasures::getEndDate(int group) const
{
    return groups.at(group) != NULL ? groups.at(group)->getEndDate() : QDate();
}

QString
SwimGearMeasures::getFieldUnits(int group, int field, bool useMetricUnits) const
{
    return groups.at(group) != NULL ? groups.at(group)->getFieldUnits(field, useMetricUnits) : QString();
}

double
SwimGearMeasures::getFieldValue(int group, QDate date, int field, bool useMetricUnits) const
{
    return groups.at(group) != NULL ? groups.at(group)->getFieldValue(date, field, useMetricUnits) : 0.0;
}

