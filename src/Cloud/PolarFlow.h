/*
 * Copyright (c) 2017 Mark Liversedge (liversedge@gmail.com)
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

#ifndef GC_PolarFlow_h
#define GC_PolarFlow_h

#include "CloudService.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QImage>

// OAuth domain (requires client id)
#define GC_POLARFLOW_OAUTH_URL "https://flow.polar.com/oauth2/authorization"

// Access token (requires client id and secret)
#define GC_POLARFLOW_TOKEN_URL "https://polarremote.com/v2/oauth2/token"

// Request URL (requires access token)
#define GC_POLARFLOW_URL "https://www.polaraccesslink.com"

class PolarFlow : public CloudService {

    Q_OBJECT

public:

    PolarFlow(Context *context);
    CloudService *clone(Context *context) { return new PolarFlow(context); }
    ~PolarFlow();

    QString id() const { return "PolarFlow"; }
    QString uiName() const { return tr("Polar Flow"); }
    QString description() const { return (tr("Download from the popular Polar website.")); }
    QImage logo() const { return QImage(":images/services/polarflow.png"); }

    // open/connect and close/disconnect transaction
    bool close();

    // polar has more types of service - NOT Default
    //virtual int type() { return Activities | Measures; }

    // polar capabilities of service - NOT Default
    virtual int capabilities() const { return OAuth | Download | Query; }

    QString authiconpath() const { return QString(":images/services/polarflow.png"); }

    static int isoDateToSeconds(QString &string); // get a duration out of ISO-Date Format

    // write a file
    bool writeFile(QByteArray &data, QString remotename, RideFile *ride); //

    //QJsonObject createSampleStream(QJsonDocument &document, int loopnumber); // Create "Strava"-like Streams from Polar Samples
    QJsonObject createSampleStream(QJsonDocument &sampletype_Doc, int actualSampleLoopNumber, int timeSamplesLoopNumber); // Create "Strava"-like Streams from Polar Samples

    static void clearJsonObject(QJsonObject &object); // clear old Stream

    //void addSamples(RideFile* ret, QString exerciseId, QString fileSuffix); // Get available samples and Get Samples
    void addSamples(RideFile* ret, QString exerciseId); // Get available samples and Get Samples

    static void fixLapSwim(RideFile* ret, QJsonArray laps);
    static void fixSmartRecording(RideFile* ret);

    QString saveFile(QByteArray &array, QString exerciseId, QString filetype, QDateTime starttime, QString detailedSportInfo_Str); // Save Data File

    void ImportFiles(QJsonObject &object, QString exerciseId);

    static void addExerciseSummaryInformation(RideFile* ret, QJsonObject &object); //, int duration_time);

    //static Context *context;

public slots:
    // getting data

    // sending data - NOT for now
    void writeFileCompleted(); //

private:
    Context *context;
    QNetworkAccessManager *nam;
    QNetworkReply *reply;

    QMap<QNetworkReply*, QByteArray*> buffers;

private slots:
    void onSslErrors(QNetworkReply *reply, const QList<QSslError>&error);

};
#endif
