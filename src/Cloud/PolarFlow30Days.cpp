/*
 * Copyright (c) 2016 Damien.Grauser (damien.grauser@pev-geneve.ch)
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

/* TODO - IDEAS
 *
 * addExerciseSummaryInformation - get detailed-sport-info:
 *  - replace works for mor then one underline
 *  - separation of the sections of the string works for one underline
 *    - if separating is needed:
 *    - create function and do it in loop -> elemination underline by underline
 *      - after one underline is eleminated do the next run with rest of string
 *      - do untill no underline is left
 *
 * samples:
 *  - needed steps to get the samples are done
 *  - getting samples and write the to a new Ridefile works
 *  - working with samples and combine them with downloaded file -> needs to be done
 *  - choose if downloaded file is choosen or usage of samples  -> needs to be done
 *
 * next: readdir test_ary No 494
*/

#include "PolarFlow.h"
#include "PolarFlow30Days.h"
#include "JsonRideFile.h"
#include "Athlete.h"
#include "Settings.h"
#include "Secrets.h"
#include "mvjson.h"
#include "Units.h"
#include "RideImportWizard.h"
//#include "MergeActivityWizard.h"
//#include "MainWindow.h"
#include <QByteArray>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#ifndef POLARFLOW_DEBUG
// TODO(gille): This should be a command line flag.
// Default: "false" - false|true
#define POLARFLOW_DEBUG true
#endif
#ifdef Q_CC_MSVC
#define printd(fmt, ...) do {                                                \
if (POLARFLOW_DEBUG) {                                 \
        printf("[%s:%d %s] " fmt , __FILE__, __LINE__,        \
               __FUNCTION__, __VA_ARGS__);                    \
        fflush(stdout);                                       \
}                                                         \
} while(0)
#else
#define printd(fmt, args...)                                            \
do {                                                                \
        if (POLARFLOW_DEBUG) {                                       \
            printf("[%s:%d %s] " fmt , __FILE__, __LINE__,              \
                   __FUNCTION__, ##args);                               \
            fflush(stdout);                                             \
    }                                                               \
} while(0)
#endif

#ifndef POLARFLOW_IMPORTTYPE
// How to import ecxercises via File or via Samples
// Default: "File" - File|Samples
#define POLARFLOW_IMPORTTYPE "Samples"
#endif

#ifndef POLARFLOW_IMPORTFILEFORMAT
// ecxercise format for file import
// Default "TCX" - TCX|FIT|GPX
// Format "TCX" include Notes
#define POLARFLOW_IMPORTFILEFORMAT "TCX"
#endif

#ifndef POLARFLOW_COMMITTRANSACTIONOVERRIDE
// committing the transaction is done by default "false" - false|true
// for debuging purposes you have the posibility to override and not commiting the transaction
#define POLARFLOW_COMMITTRANSACTIONOVERRIDE "false"
#endif

PolarFlow::PolarFlow(Context *context) : CloudService(context), context(context) {

    if (context) {
        nam = new QNetworkAccessManager(this);
        connect(nam, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> & )), this, SLOT(onSslErrors(QNetworkReply*, const QList<QSslError> & )));
    }

    uploadCompression = none;
    downloadCompression = none;
    useMetric = true; // distance and duration metadata

    // config
    settings.insert(OAuthToken, GC_POLARFLOW_TOKEN);
    settings.insert(Local1, GC_POLARFLOW_USER_ID);
    settings.insert(Local2, GC_POLARFLOW_TRANSACTION_ID); //
    //settings.insert(Local3, GC_POLARFLOW_EXERCISE_ID); //
    settings.insert(Local4, GC_POLARFLOW_STATUS_CODE);
    settings.insert(Local5, GC_POLARFLOW_ACTIVITY_COUNTER);
    settings.insert(Local6, GC_POLARFLOW_LAST_EXERCISE_ID);
    //settings.insert(Local7, GC_POLARFLOW_SAMPLE_SIZE); // samples - streams
    //settings.insert(Local8, GC_POLARFLOW_TIMESAMPLES_LOOPNUMBER); // samples - streams
    //settings.insert(Local9, GC_POLARFLOW_SAMPLE_RECORDINGRATE); // samples - streams
    //settings.insert(Metadata1, QString("%1::Activity Name").arg(GC_POLARFLOW_ACTIVITY_NAME));
}

PolarFlow::~PolarFlow() {
    if (context) delete nam;
}

bool
PolarFlow::open(QStringList &errors) //open transaction
{
    printd("open - start\n");
    printd("PolarFlow::open\n");
    // do we have a token
    QString token = getSetting(GC_POLARFLOW_TOKEN, "").toString();
    if (token == "") {
        errors << tr("You must authorise with Polar Flow first");
        return false;
    }
    return true;
}

QList<CloudServiceEntry*>
PolarFlow::readdir(QString path, QStringList &errors)
//PolarFlow::readdir(QString path, QStringList &errors, QDateTime from, QDateTime to)
{
    // use transactionID getting a list of exercises
    // https://www.polar.com/accesslink-api/?shell#list-exercises

    printd("PolarFlow::readdir - start (%s)\n", path.toStdString().c_str());

    QList<CloudServiceEntry*> returning;

    // do we have a token
    QString token = getSetting(GC_POLARFLOW_TOKEN, "").toString();
    if (token == "") {
        errors << tr("You must authorise with Polar Flow first");
        return returning;
    }

    // do we have a transactionID
    QString transaction_id = getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString();
    if (transaction_id == "") {
        errors << tr("No Transaction ID available");
        return returning;
    }

    //Command URL: GET https://www.polaraccesslink.com/v3/exercises
    QString url = QString("%1/v3/exercises")
                      .arg(getSetting(GC_POLARFLOW_URL, "https://www.polaraccesslink.com").toString());
    //.arg(getSetting(GC_POLARFLOW_USER_ID, "").toString())
    //.arg(getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString());

    printd("URL used: %s\n", url.toStdString().c_str());

    // request using the bearer token
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", (QString("Bearer %1").arg(token)).toLatin1());
    request.setRawHeader("Accept", "application/json");

    // make request
    QNetworkReply *reply = nam->get(request);
    printd("response : %s\n", url.toStdString().c_str());

    // blocking request
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    // if successful, lets unpack
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    printd("HTTP response: %d: %s\n", reply->error(), reply->errorString().toStdString().c_str());
    printd("HTTP response status code: %d\n", statusCode);
    //qDebug() << "readdir - reply: " << reply;

    // Save statusCode 200=OK, 204=No Content
    setSetting(GC_POLARFLOW_STATUS_CODE, statusCode);
    CloudServiceFactory::instance().saveSettings(this, context);

    // Answer Exercises list
    QByteArray listExercises_Ary = reply->readAll();
    printd("readdir - listExercises_Array: %s\n", listExercises_Ary.toStdString().c_str());
        //listExercises_Ary.chop(1);
        //auto it = listExercises_Ary.erase(listExercises_Ary.cbegin());
        //auto it = listExercises_Ary.erase(listExercises_Ary.cend());
        //listExercises_Ary.replace(1,1,""); // remove "["
        //listExercises_Ary.replace(listExercises_Ary.size(),1,""); // remove "]"
    //printd("readdir - listExercises_Array shortend: %s\n", listExercises_Ary.toStdString().c_str());
    //listExercises_Ary.insert(0, QByteArray("exercises: ")); // add first
    //listExercises_Ary.prepend(QByteArray("exercises: ")); // add first
    //printd("readdir - listExercises_Array extended: %s\n", listExercises_Ary.toStdString().c_str());

    // parse JSON payload
    QJsonParseError listExercises_Doc_parseError;
    QJsonDocument listExercises_Doc = QJsonDocument::fromJson(listExercises_Ary, &listExercises_Doc_parseError);
    printd("listExercises_Array - Error Number: %d - Error String: %s \n", listExercises_Doc_parseError.error, listExercises_Doc_parseError.errorString().toStdString().c_str());
    qDebug() << "readdir - listExercises_Document: " << listExercises_Doc;  // << "size: " << listExercises_Doc.size();

    //QJsonArray test = QJsonDocument::array() const;
    QJsonArray test_Ary = listExercises_Doc.array();
    qDebug() << "readdir - test_Array: " << test_Ary << " - size: " << test_Ary.size();

    // WORKS TILL HERE NOW CYCLE THROUGHT ELEMENTS OF test_Array and populate list of exercises

    QJsonObject exercises30days_Obj;
    QJsonArray exercises30days_Ary;
    QJsonObject exercisesUrlList_Obj;
    QJsonArray exerciseUrls_Ary;
    QString exerciseUrl_Str;
    // Polar Activities counter, if there is no new activity the number is exactly max or smaller then max
    // https://www.polar.com/accesslink-api/?shell#training-data
    int activityCounter = 0;
    int activityCounterDelta = 0;
    int activityCounterMax = 50;
    // fetch exercise-id from last listed Activity

    // populating array with urls to activites
    if (listExercises_Doc_parseError.error == QJsonParseError::NoError) {

        //dddoes notr t work
        exercises30days_Obj = listExercises_Doc.object();
        qDebug() << "readdir - exercises30days_Object: " << exercises30days_Obj;
        //        exercises30days_Ary = exercises30days_Obj[].toArray();
        exercisesUrlList_Obj = listExercises_Doc.object();
        qDebug() << "readdir - listExercises_Object - Url-List to Execerises: " << exercisesUrlList_Obj;

        exerciseUrls_Ary = exercisesUrlList_Obj["exercises"].toArray();
        qDebug() << "readdir - exerciseUrls_Ary - exerciseUrls: " << exerciseUrls_Ary;

        activityCounter = exerciseUrls_Ary.size() + activityCounterDelta;
        setSetting(GC_POLARFLOW_ACTIVITY_COUNTER, activityCounter);
        CloudServiceFactory::instance().saveSettings(this, context);
        printd("Counter: %d\n", activityCounter);

        if (activityCounter < activityCounterMax) {
            // if Polar Activities counter is not max, there is no new activity available
            // so we can set status code "204"
            statusCode = 204;
            printd("Counter end: %d - new Status Code: %d\n", activityCounter, statusCode);
            printd("no more new activities available\n");
            // Save status code for commit transaction
            //setSetting(GC_POLARFLOW_STATUS_CODE, statusCode);
            //CloudServiceFactory::instance().saveSettings(this, context);
        }
        if (activityCounter == activityCounterMax) {
            // if it is max, it is possible that there is no new activity available
            // so we may set status code "204"
            //statusCode = 204;
            printd("Counter end: %d - new Status Code: %d\n", activityCounter, statusCode);
            printd("there may be more new activities available\n");
            // Save status code for commit transaction
            //setSetting(GC_POLARFLOW_STATUS_CODE, statusCode);
            //CloudServiceFactory::instance().saveSettings(this, context);
        }
        // after saving, reset activity counter to zero
        //activityCounter = 0;
    }

    // running through list of exercise Urls
    for(int i = 0 ; i < exerciseUrls_Ary.size(); i++) {
        int loopCounter = i + 1; // Loop Counter starts with one not with zero
        exerciseUrl_Str = exerciseUrls_Ary[i].toString();
        printd("loop i: %d (max=%d) exerciseUrl_String - Url to exercise: %s \n", loopCounter, activityCounterMax, exerciseUrl_Str.toStdString().c_str());

        // Command URL: GET https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}
        // done per exercise Url
        QNetworkRequest request(exerciseUrl_Str);
        request.setRawHeader("Authorization", (QString("Bearer %1").arg(token)).toLatin1());
        request.setRawHeader("Accept", "application/json");

        QNetworkReply *reply = nam->get(request);
        printd("response : %s\n", url.toStdString().c_str());

        // blocking request
        QEventLoop loop;
        connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();

        // if successful, lets unpack
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        printd("loop i:%d - errornumber: %d - error text: %s\n", loopCounter, reply->error(), reply->errorString().toStdString().c_str());
        printd("loop i:%d - Exercise HTTP response status code: %d\n", loopCounter, statusCode);

        // Answer-URL https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}
        // Returns JSON
        // https://www.polar.com/accesslink-api/?shell#get-exercise-summary
        QByteArray get_exercise_summary_Ary = reply->readAll();
        printd("summary - loop i:%d - Exercise summary response string: %s \n", loopCounter, get_exercise_summary_Ary.toStdString().c_str());

        //Response string is JSON data
        //https://www.polar.com/accesslink-api/?shell#get-exercise-summary
        QJsonParseError get_exercise_summary_Doc_parseError;
        QJsonDocument get_exercise_summary_Doc;
        get_exercise_summary_Doc = QJsonDocument::fromJson(get_exercise_summary_Ary, &get_exercise_summary_Doc_parseError);
        //qDebug() << "readdir - loop i:"<< loopCounter << " - get_exercise_summary_Document - Error Number: " << get_exercise_summary_Doc_parseError.error << " - Error String: " << get_exercise_summary_Doc_parseError.errorString();
        //qDebug() << "readdir - loop i:"<< loopCounter << " - get_exercise_summary_Document: " << get_exercise_summary_Doc;

        QJsonObject get_exercise_summary_Obj = get_exercise_summary_Doc.object();
        //qDebug() << "readdir - loop i:"<< loopCounter << " - get_exercise_summary_Object: " << get_exercise_summary_Obj;

        CloudServiceEntry *add = newCloudServiceEntry();

        // each item looks like this:
        /*
              {
                "id": 1937529874,                                       // integer(int64)
                "upload-time": "2008-10-13T10:40:02Z",                  // string
                "polar-user": "https://www.polaraccesslink/v3/users/1", // string
                "transaction-id": 179879,                               // integer(int64)
                "device": "Polar M400",                                 // string
                "start-time": "2008-10-13T10:40:02Z",                   // string
                "start-time-utc-offset": 180,                           // integer(int32)
                "duration": "PT2H44M",                                  // string
                "calories": 530,                                        // integer(int32)
                "distance": 1600,                                       // number(float)
                "heart-rate": {
                  "average": 129,                                       // integer(int32)
                  "maximum": 147                                        // integer(int32)
                },
                "training-load": 143.22,                                // number(float)
                "sport": "OTHER",                                       // string
                "has-route": true,                                      // boolean
                "club-id": 999,                                         // integer(int64)
                "club-name": "Polar Club",                              // string
                "detailed-sport-info": "WATERSPORTS_WATERSKI",          // string
                "fat-percentage": 60,                                   // integer(int32)
                "carbohydrate-percentage": 38,                          // integer(int32)
                "protein-percentage": 2                                 // integer(int32)
                "running-index": 51                                     // integer(int32)
                "training-load-pro":
                    {
                      "date": "2023-01-01",                              // string(date)
                      "cardio-load": 1,                                  // number (float)
                      "muscle-load": 1,                                  // number (float)
                      "perceived-load": 1,                               // number (float)
                      "cardio-load-interpretation": "UNKNOWN",           // string
                      "muscle-load-interpretation": "UNKNOWN",           // string
                      "perceived-load-interpretation": "UNKNOWN",        // string
                      "user-rpe": "UNKNOWN"                              // string
                    }
              }
             */

        //Polar reports in local time
        QDateTime startDate = QDateTime::fromString(get_exercise_summary_Obj["start-time"].toString(), Qt::ISODate);
        //startDate.setTimeSpec(Qt::UTC);
        //startDate = startDate.toLocalTime();
        //qDebug() << "readdir - loop i:"<< loopCounter << " - get_exercise_summary_Object - add name: " << startDate;

        //Convert frome ISO to Seconds
        QString duration_str = get_exercise_summary_Obj["duration"].toString(); // "PT4H51M38.512S"
        int duration_time = isoDateToSeconds(duration_str);
        printd("prepareResponse - Duration Int: %d \n", duration_time);

        add->label = QFileInfo(get_exercise_summary_Obj["detailed-sport-info"].toString()).fileName();
        add->id = QString("%1").arg(get_exercise_summary_Obj["id"].toVariant().toULongLong());

        add->isDir = false;
        add->distance = get_exercise_summary_Obj["distance"].toDouble()/1000.0f;
        add->duration = duration_time;
        add->name = startDate.toString("yyyy_MM_dd_HH_mm_ss")+".json";

        //qDebug() << "readdir - loop - get_exercise_summary_Object - add label: " << get_exercise_summary_Obj["detailed-sport-info"].toString();
        //qDebug() << "readdir - loop - get_exercise_summary_Object - add exercise id: " << QString("%1").arg(get_exercise_summary_Obj["id"].toVariant().toULongLong());
        //qDebug() << "readdir - loop - add isDir: " << isDir;
        //qDebug() << "readdir - loop - get_exercise_summary_Object - add distance: " << get_exercise_summary_Obj["distance"].toDouble()/1000.0;
        //qDebug() << "readdir - loop - add duration: " << duration_time;
        //qDebug() << "readdir - loop - get_exercise_summary_Object - add name: " << startDate.toString("yyyy_MM_dd_HH_mm_ss")+".json";

        printd("direntry: %s %s\n", add->id.toStdString().c_str(), add->name.toStdString().c_str());
        printd("number of last loop: %d (max=%d) - last Excercise Id: \n", loopCounter, activityCounterMax);

        // fetch last exercise id
        if(loopCounter == exerciseUrls_Ary.size()) {
            QString lastListedExerciseId = QString::number(get_exercise_summary_Obj["id"].toInt());
            //int lastListedExerciseId = get_exercise_summary_Obj["id"].toInt();
            setSetting(GC_POLARFLOW_LAST_EXERCISE_ID, lastListedExerciseId);
            CloudServiceFactory::instance().saveSettings(this, context);
            printd("number of last loop: %d (max=%d) - last Excercise Id (saved): %s \n", loopCounter, activityCounterMax, lastListedExerciseId.toStdString().c_str());
        }
        returning << add;
    }
    printd("loop end \n");

    // all good ?
    printd("returning count(%d), errors(%s)\n", returning.count(), errors.join(",").toStdString().c_str());
    printd("closed \n");
    return returning;
}

bool
PolarFlow::readFile(QByteArray *data, QString remotename, QString remoteid)
{
    printd("readFile - start\n");
    // create alias
    QString exercise_id_Str = remoteid;
    printd("PolarFlow::readFile remotename: %s - remoteid: %s \n", remotename.toStdString().c_str(), exercise_id_Str.toStdString().c_str());
    printd("readFile - exercise_id_String alias: %s\n", exercise_id_Str.toStdString().c_str());

    // do we have a token ?
    QString token = getSetting(GC_POLARFLOW_TOKEN, "").toString();
    if (token == "") return false;

    // do we have a transactionID
    QString transaction_id = getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString();
    if (transaction_id == "") return false;

    // connect to direct to exercise via remoteid from PolarFlow::readDir
    // https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}
    QString url;
    url = QString("%1/v3/users/%2/exercise-transactions/%3/exercises/%4")
              .arg(getSetting(GC_POLARFLOW_URL, "https://www.polaraccesslink.com").toString())
              .arg(getSetting(GC_POLARFLOW_USER_ID, "").toString())
              .arg(getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString())
              .arg(exercise_id_Str);

    printd("readFile - url:%s\n", url.toStdString().c_str());

    // request using the bearer token
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", (QString("Bearer %1").arg(token)).toLatin1());
    request.setRawHeader("Accept", "application/json");

    // put the file
    QNetworkReply *reply = nam->get(request);

    // remember
    mapReply(reply,remotename);
    buffers.insert(reply,data);

    // catch finished signal
    connect(reply, SIGNAL(finished()), this, SLOT(readFileCompleted()));
    connect(reply, SIGNAL(readyRead()), this, SLOT(readyRead()));
    printd("readFile - end\n\n");
    return true;
}

QByteArray*
PolarFlow::prepareResponse(QByteArray* data, QString &name)
{
    printd("prepareResponse - start \n");
    //qDebug() << "prepareResponse data:" << data;
    //qDebug() << "prepareResponse name:" << name;

    // Save original name
    QString originalName = name;

    // how will the exercise be acquired? via "File"-Import or via "Streams"
    QString typeOfImporting; // "File|Samples"
    typeOfImporting = POLARFLOW_IMPORTTYPE;
    //typeOfImporting = "File";
    //printd("PolarFlow::prepareResponse: %s \n", originalName);

    QJsonParseError parseError;
    QJsonDocument get_exercise_summary_Doc = QJsonDocument::fromJson(data->constData(), &parseError);
    printd("prepareResponse - get_exercise_summary_Doc - Error Number: %d - Error String: %s \n", parseError.error, parseError.errorString().toStdString().c_str());
    //qDebug() << "prepareResponse - get_exercise_summary_Document:" << get_exercise_summary_Doc;

    // which variables are needed overall
    QString exercise_id_Str;
    QDateTime starttime;

    // if path was returned all is good, lets set root
    if (parseError.error == QJsonParseError::NoError) {
        QJsonObject get_exercise_summary_Obj = get_exercise_summary_Doc.object();

        // each item looks like this:
        //https://www.polar.com/accesslink-api/?shell#get-exercise-summary
        //https://www.polar.com/accesslink-api/?shell#tocSexercise
        //https://www.polar.com/accesslink-api/?shell#tocSheartrate
        /*
          {
            "id": 1937529874,                                       integer(int64)
            "upload-time": "2008-10-13T10:40:02Z",                  string
            "polar-user": "https://www.polaraccesslink/v3/users/1", string
            "transaction-id": 179879,                               integer(int64)
            "device": "Polar M400",                                 string
            "device-id": "1111AAAA",                                string
            "start-time": "2008-10-13T10:40:02Z",                   string
            "start-time-utc-offset": 180,                           integer(int32)
            "duration": "PT2H44M",                                  string
            "calories": 530,                                        integer(int32)
            "distance": 1600,                                       number(float)
            "heart-rate": {
              "average": 129,                                       integer(int32)
              "maximum": 147                                        integer(int32)
            },
            "training-load": 143.22,                                number(float)
            "sport": "OTHER",                                       string
            "has-route": true,                                      boolean
            "club-id": 999,                                         integer(int64)
            "club-name": "Polar Club",                              string
            "detailed-sport-info": "WATERSPORTS_WATERSKI",          string
            "fat-percentage": 60,                                   integer(int32)
            "carbohydrate-percentage": 38,                          integer(int32)
            "protein-percentage": 2                                 integer(int32)
            "running-index": 51                                     // integer(int32)
            "training-load-pro":
                {
                 "date": "2023-01-01",                              // string(date)
                 "cardio-load": 1,                                  // float
                 "muscle-load": 1,                                  // float
                 "perceived-load": 1,                               // float
                 "cardio-load-interpretation": "UNKNOWN",           // string
                 "muscle-load-interpretation": "UNKNOWN",           // string
                 "perceived-load-interpretation": "UNKNOWN",        // string
                 "user-rpe": "UNKNOWN"                              // string
               }
          }
         */

        // Get Exercise ID - needed for download file and getting sample-data
        exercise_id_Str = QString::number(get_exercise_summary_Obj["id"].toInt());
        printd("prepareResponse - ExerciseID_String: %s \n", exercise_id_Str.toStdString().c_str());

        // Get start-time
        starttime = QDateTime::fromString(get_exercise_summary_Obj["start-time"].toString(), Qt::ISODate);

        // Acquire Excercise in all available file formats
        // get detailed-sport-info
        // https://www.polar.com/accesslink-api/#detailed-sport-info-values-in-exercise-entity
        // detailed sports info is written in capital letters and all spaces are replaced by an underline by Polar default
        QString detailedSportInfo_Str = "DetailedSportInfoIsEmpty"; // set default to not empty
        if (get_exercise_summary_Obj["detailed-sport-info"].isString()) {
            detailedSportInfo_Str = get_exercise_summary_Obj["detailed-sport-info"].toString();
        }

        // getting all available file-types = tcx, gpx, fitBeta
        // we will download all available filetypes
        QStringList errorsImport;
        QString filetype;         // for downloading

        // download tcx
        filetype = "tcx";
        QString filenameWithPathTCX = getFile(exercise_id_Str, filetype, starttime, detailedSportInfo_Str);

        // download gpx
        filetype = "gpx";
        QString filenameWithPathGPX = getFile(exercise_id_Str, filetype, starttime, detailedSportInfo_Str);

        // download fit
        filetype = "fit";
        QString filenameWithPathFITBeta = getFile(exercise_id_Str, filetype, starttime, detailedSportInfo_Str);

        printd("prepareResponse - filename with Path TCX: %s \n", filenameWithPathTCX.toStdString().c_str());
        printd("prepareResponse - filename with Path GPX: %s \n", filenameWithPathGPX.toStdString().c_str());
        printd("prepareResponse - filename with Path FITBeta: %s \n", filenameWithPathFITBeta.toStdString().c_str());

        // Import Excercise via File-Import
        if (typeOfImporting == "File") {
            //Which type will be imported? "TCX" or "FIT"
            // set file name for import in GC - use TCX - Format because notes are included
            QString filenameForImport; // file for importing

            QString filetypeForImport;
            filetypeForImport = POLARFLOW_IMPORTFILEFORMAT;
            //filetypeForImport = "TCX";

            if (filetypeForImport == "TCX") {
                filenameForImport = filenameWithPathTCX;
            }
            if (filetypeForImport == "GPX") {
                filenameForImport = filenameWithPathGPX;
            }
            if (filetypeForImport == "FIT") {
                filenameForImport = filenameWithPathFITBeta;
            }

            // we need the full Path for importing downloaded file
            QString filenameWithPathSuffix = QFileInfo(filenameForImport).suffix();
            printd("prepareResponse - filename with Path Suffix: %s \n", filenameWithPathSuffix.toStdString().c_str());

            QFile importfile(filenameForImport);
            printd("prepareResponse - filename with Path: %s \n", filenameForImport.toStdString().c_str());

            QFileInfo filenameWithPathComplete(filenameForImport);
            QString filename = filenameWithPathComplete.fileName();
            printd("prepareResponse - filename: %s \n", filename.toStdString().c_str());

            // Import downloaded file and set some extra data from exercise summary to RideFile: rideImport
            RideFile *rideImport = RideFileFactory::instance().openRideFile(context, importfile, errorsImport);
            //qDebug() << "prepareResponse - uncompressRide - errors:" << errorsImport;

            // minimal information added to imported file, so we can see where we got the information from
            // Set Source Filename
            if (filename.length()>0) {
                rideImport->setTag("Source Filename", filename);
            }

            // set polar exercise id in metadata (to show where we got it from - to add View on Polar link in Summary view
            if (!get_exercise_summary_Obj["id"].isNull()) {
                rideImport->setTag("Polar Exercise ID",  exercise_id_Str);
            }

            // add the rest of available excercise informations
            addExerciseSummaryInformation(rideImport, get_exercise_summary_Obj);

            //printd("prepareResponse - fixSmartRecording: %s \n", rideImport);
            fixSmartRecording(rideImport);
            JsonFileReader rideImportReader;
            data->clear();
            //What is included? (Context *context, const RideFile *ride, bool withAlt, bool withWatts, bool withHr, bool withCad)
            data->append(rideImportReader.toByteArray(context, rideImport, true, true, true, true));
            // temp ride not needed anymore
            delete rideImport;
        }

        // Acquire Excercise via Stream-Import
        if (typeOfImporting == "Samples") {
            // Create a new Ridefile for catching all sample data
            // Get Exercise Starttime - needed for RideFile: rideWithSamples
            starttime = QDateTime::fromString(get_exercise_summary_Obj["start-time"].toString(), Qt::ISODate);

            // 1s samples with start time
            RideFile *rideWithSamples = new RideFile(starttime.toUTC(), 1.0f);
            // set polar exercise id in metadata - to show where we got it from - to add View on Polar link in Summary view
            if (!get_exercise_summary_Obj["id"].isNull()) {
                rideWithSamples->setTag("Polar Exercise ID",  exercise_id_Str);
            }

            // add rest of metadata and return start time as value
            addExerciseSummaryInformation(rideWithSamples, get_exercise_summary_Obj);

            // Adding SampleData to Ridefile
            //addSamples(rideWithSamples, exercise_id_Str, filenameWithPathSuffix);
            addSamples(rideWithSamples, exercise_id_Str);

            // swim laps / own laps
            /*
            if (!get_exercise_summary_Obj["laps"].isNull()) {
                QJsonArray laps = get_exercise_summary_Obj["laps"].toArray();

                double last_lap = 0.0;
                foreach (QJsonValue value, laps) {
                    QJsonObject lap = value.toObject();

                    double start = starttime.secsTo(QDateTime::fromString(lap["start-time"].toString(), Qt::ISODate));
                    if (start < last_lap) start = last_lap + 1; // Don't overlap
                    double end = start + duration_time; //lap["elapsed_time"].toDouble() - 1;

                    last_lap = end;

                    rideImport->addInterval(RideFileInterval::USER, start, end, lap["name"].toString());
                } // close foreach - loop
                // Fix distance from laps and fill gaps for pool swims
                fixLapSwim(rideImport, laps);
             } //close laps if
            */

            fixSmartRecording(rideWithSamples);
            //printd("prepareResponse - fixSmartRecording: %s \n", rideWithSamples);
            JsonFileReader rideWithSamplesReader;

            data->clear();
            //What is included? (Context *context, const RideFile *ride, bool withAlt, bool withWatts, bool withHr, bool withCad)
            data->append(rideWithSamplesReader.toByteArray(context, rideWithSamples, true, true, true, true));

            //new MergeActivityWizard(currentAthleteTab->context))->exec()
            //currentAthleteTab->context->ride && currentAthleteTab->context->ride->ride() && currentAthleteTab->context->ride->ride()->dataPoints().count()) (new MergeActivityWizard(currentAthleteTab->context))->exec();
            //MergeActivityWizard *merge = new MergeActivityWizard(rideImport, rideWithSamples);
            //MergeActivityWizard::setRide(QWizard->rideMerge, rideWithSamples)

            // temp ride not needed anymore
            delete rideWithSamples;
        }

        //printd("reply:%s\n", data->toStdString().c_str());
        printd(" end \n");
    } // close lets set root

    // if we have aquired the last exercise from list, so we can commit the transaction
    QStringList emptyerrorlist;
    QString lastListedExerciseId = getSetting(GC_POLARFLOW_LAST_EXERCISE_ID, "").toString();
    QString commitTransactionOverride = POLARFLOW_COMMITTRANSACTIONOVERRIDE; // true/false

    // What will we do? Commit Transaction or not ?
    // first check, if all exercises are aquired
    printd("last exercise id: %s \n", lastListedExerciseId.toStdString().c_str());
    printd("this exercise id: %s \n", exercise_id_Str.toStdString().c_str());

    // if last listed Exercise Id is NOT the same as actual Exercise Id then we can NOT commit the transaction
    if (lastListedExerciseId != exercise_id_Str) {
        printd("not calling commit, because is it not the last exercise\n");
    }
    // else  we can commit the transaction
    else {
        printd("we can commit the transaction, because we fetched the last exercise \n");
        printd("check debug override commit- last exercise id: %s - Override: %s \n ", commitTransactionOverride.toStdString().c_str(), commitTransactionOverride.toStdString().c_str());
        if (commitTransactionOverride == "false") {
            printd("no override, so we commit the transaction with \n");
            printd("we fetched all available exercises: %s = %s \n",exercise_id_Str.toStdString().c_str() ,lastListedExerciseId.toStdString().c_str());
            //commit(emptyerrorlist);
        }

    }
    // at last return aquired data
    return data;
}

// development put on hold - AccessLink API compatibility issues with Desktop applications
//#if 0
static bool addPolarFlow() {
    CloudServiceFactory::instance().addService(new PolarFlow(NULL));
    return true;
}

static bool add = addPolarFlow();
//#endif
