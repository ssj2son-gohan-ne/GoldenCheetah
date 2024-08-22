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
*/

#include "PolarFlowByTransaction.h"
#include "PolarFlow.h"
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
#define POLARFLOW_DEBUG false
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

PolarFlowBT::PolarFlowBT(Context *context) : PolarFlow(context), context(context) {
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
    settings.insert(Local2, GC_POLARFLOW_TRANSACTION_ID);
    //settings.insert(Local3, GC_POLARFLOW_EXERCISE_ID); //
    settings.insert(Local4, GC_POLARFLOW_STATUS_CODE);
    settings.insert(Local5, GC_POLARFLOW_ACTIVITY_COUNTER);
    settings.insert(Local6, GC_POLARFLOW_LAST_EXERCISE_ID);

}

PolarFlowBT::~PolarFlowBT() {
    if (context) delete nam;
}

void
PolarFlowBT::onSslErrors(QNetworkReply *reply, const QList<QSslError>&)
{
    reply->ignoreSslErrors();
}

bool
PolarFlowBT::open(QStringList &errors) //open transaction
{
    printd("open - start\n");

    // Check Debug Override for committing Transactions
    QString commitTransactionOverride = POLARFLOW_COMMITTRANSACTIONOVERRIDE; // true/false

    // open transaction and get valid transaction id and save it to GoldenCheetahSetting
    //
    // Training data - create transaction and save temporarily transaction-id
    // https://www.polar.com/accesslink-api/?shell#create-transaction-2
    //
    printd("PolarFlow::open\n");
    // do we have a token
    QString token = getSetting(GC_POLARFLOW_TOKEN, "").toString();
    if (token == "") {
        errors << tr("You must authorise with Polar Flow first");
        return false;
    }

    // Command-URL: POST https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions
    QString url = QString("%1/v3/users/%2/exercise-transactions")
                      .arg(getSetting(GC_POLARFLOW_URL, "https://www.polaraccesslink.com").toString())
                      .arg(getSetting(GC_POLARFLOW_USER_ID, "").toString());

    printd("URL used: %s\n", url.toStdString().c_str());

    // request using the bearer
    QNetworkRequest request(QUrl(url.toStdString().c_str()));
    request.setRawHeader("Authorization", (QString("Bearer %1").arg(token)).toLatin1());
    request.setRawHeader("Accept", "application/json");

    QString data;
    data += "";

    // create transaction
    QNetworkReply* reply = nam->post(request, data.toLatin1());

    // blocking request
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    printd("HTTP response code: %d\n", statusCode);

    if (statusCode == 201) {
        printd("There is new training session data available");

        // Save status code for commit transaction
        //setSetting(GC_POLARFLOW_STATUS_CODE, statusCode);
        //CloudServiceFactory::instance().saveSettings(this, context);

        // oops, no dice
        if (reply->error() != 0) {
            printd("Got error %s\n", reply->errorString().toStdString().c_str());
            errors << reply->errorString();
            return false;
        }

        // Answer-URL: https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}
        // lets extract the transaction id
        QByteArray transactions_Ary = reply->readAll();
        printd("response: %s\n", transactions_Ary.toStdString().c_str());

        //{
        //  "transaction-id": 122,
        //  "resource-uri": "https://polaraccesslink.com/v3/users/21/physical-information-transactions/32"
        //}

        //Response string is JSON data
        QJsonParseError transactions_Doc_parseError;
        QJsonDocument transactions_Doc = QJsonDocument::fromJson(transactions_Ary, &transactions_Doc_parseError);

        // failed to parse result !?
        if (transactions_Doc_parseError.error != QJsonParseError::NoError) {
            printd("Parse error! - StatusCode:%d\n", statusCode);
            errors << tr("JSON parser error \n","StatusCode:%1\n").arg(statusCode) << transactions_Doc_parseError.errorString();
            return false;
        }
        printd("transactions_Doc Error Number: %d Error String: %s \n", transactions_Doc_parseError.error, transactions_Doc_parseError.errorString().toStdString().c_str());
        //qDebug() << "open - transactions_Doc: ", transactions_Doc);

        //The number has to be converted to an int and than to a string
        //https://stackoverflow.com/questions/61075951/minimal-example-on-how-to-read-write-and-print-qjson-code-with-qjsondocument
        QJsonObject transaction_id_Obj = transactions_Doc.object();
        //qDebug() << "open - transaction_id_Object: " << transactions_Doc.object();

        QJsonValue transaction_id_Val = transaction_id_Obj.value("transaction-id");
        //qDebug() << "open - transaction_id_Value: " << transaction_id_Val;

        QString transaction_id = QString::number(transaction_id_Val.toInt());
        printd("transaction_id_String: %s \n", transaction_id.toStdString().c_str());

        // save temporary transactionID to settings
        setSetting(GC_POLARFLOW_TRANSACTION_ID, transaction_id);
        // get the factory to save our settings permanently
        CloudServiceFactory::instance().saveSettings(this, context);
        printd("open - end");
    } //if close

    if (statusCode == 204) {
        //else {
        printd("check override commit - Override: %s \n ", commitTransactionOverride.toStdString().c_str());
        if (commitTransactionOverride == "false") {
            printd("no override, so we commit the transaction \n");
            commit(errors);
        } // if override close
        printd("There is no new training session data available");
    } // if close
    return true;
}

bool
PolarFlowBT::commit(QStringList &errors) //commit transaction
{
    // commit transaction and delete transaction id
    // https://www.polar.com/accesslink-api/?shell#commit-transaction-2
    // prerquisitory: there has to be an open transaction (Token, TransactionID)
    printd("PolarFlow::commit - start\n");

    // do we have a token
    QString token = getSetting(GC_POLARFLOW_TOKEN, "").toString();
    if (token == "") {
        errors << tr("There is no Token");
        return false;
    }

    // do we have a transactionID
    QString transaction_id = getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString();
    if (transaction_id == "") {
        errors << tr("There is no TransactionID");
        return false;
    }

    // Command-URL: PUT https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}
    QString url = QString("%1/v3/users/%2/exercise-transactions/%3")
                      .arg(getSetting(GC_POLARFLOW_URL, "https://www.polaraccesslink.com").toString())
                      .arg(getSetting(GC_POLARFLOW_USER_ID, "").toString())
                      .arg(getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString());

    printd("URL used: %s\n", url.toStdString().c_str());

    // request using the bearer
    QNetworkRequest request(QUrl(url.toStdString().c_str()));
    request.setRawHeader("Authorization", (QString("Bearer %1").arg(token)).toLatin1());

    QString data;
    data += "";

    // commit transaction
    QNetworkReply* reply = nam->put(request, data.toLatin1());

    // blocking request
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    printd("HTTP response code: %d\n", statusCode);

    // oops, no dice
    if (reply->error() != 0) {
        printd("Got error %s\n", reply->errorString().toStdString().c_str());
        errors << reply->errorString();
        return false;
    }

    // delete temporary transactionID / StatusCode / Activities Counter / Last Exercise ID from settings
    setSetting(GC_POLARFLOW_TRANSACTION_ID, "0");
    CloudServiceFactory::instance().saveSettings(this, context);
    setSetting(GC_POLARFLOW_STATUS_CODE, "0");
    CloudServiceFactory::instance().saveSettings(this, context);
    setSetting(GC_POLARFLOW_ACTIVITY_COUNTER, "0");
    CloudServiceFactory::instance().saveSettings(this, context);
    setSetting(GC_POLARFLOW_LAST_EXERCISE_ID, "0");
    CloudServiceFactory::instance().saveSettings(this, context);

    printd("Commit close - end\n");
    return true;
}

bool
PolarFlowBT::close()
{
    printd("End\n");
    // nothing to do for now
    return true;
}

QList<CloudServiceEntry*>
PolarFlowBT::readdir(QString path, QStringList &errors)
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

    //Command URL: GET https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}
    QString url = QString("%1/v3/users/%2/exercise-transactions/%3")
                      .arg(getSetting(GC_POLARFLOW_URL, "https://www.polaraccesslink.com").toString())
                      .arg(getSetting(GC_POLARFLOW_USER_ID, "").toString())
                      .arg(getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString());

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
    printd("readdir - HTTP response: %d: %s\n", reply->error(), reply->errorString().toStdString().c_str());
    printd("readdir - HTTP response status code: %d\n", statusCode);

    // Save statusCode 200=OK, 204=No Content
    setSetting(GC_POLARFLOW_STATUS_CODE, statusCode);
    CloudServiceFactory::instance().saveSettings(this, context);

    // Answer-URL https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}
    // Returns a list of hyperlinks to available exercises
    // https://www.polar.com/accesslink-api/?shell#list-exercises
    /*
    {
      "exercises": [
        "https://www.polaraccesslink.com/v3/users/12/exercise-transactions/34/exercises/56",
        "https://www.polaraccesslink.com/v3/users/12/exercise-transactions/34/exercises/120"
      ]
    }
    */
    QByteArray listExercises_Ary = reply->readAll();

    printd("readdir - listExercises_Array: %s\n", listExercises_Ary.toStdString().c_str());

    // parse JSON payload
    QJsonParseError listExercises_Doc_parseError;
    QJsonDocument listExercises_Doc;
    listExercises_Doc = QJsonDocument::fromJson(listExercises_Ary, &listExercises_Doc_parseError);
    printd("readdir - listExercises_Document - Error Number: %d - Error String: %s \n", listExercises_Doc_parseError.error, listExercises_Doc_parseError.errorString().toStdString().c_str());
    //qDebug() << "readdir - listExercises_Document - Url-List to Execerises: " << listExercises_Doc;

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

        exercisesUrlList_Obj = listExercises_Doc.object();
        //qDebug() << "readdir - listExercises_Object - Url-List to Execerises: " << exercisesUrlList_Obj;

        exerciseUrls_Ary = exercisesUrlList_Obj["exercises"].toArray();
        //qDebug() << "readdir - exerciseUrls_Ary - exerciseUrls: " << exerciseUrls_Ary;

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
        int duration_time = PolarFlow::isoDateToSeconds(duration_str);
        printd("Duration Int: %d \n", duration_time);

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
PolarFlowBT::readFile(QByteArray *data, QString remotename, QString remoteid)
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

void
PolarFlowBT::readyRead()
{
    printd("start\n");
    QNetworkReply *reply = static_cast<QNetworkReply*>(QObject::sender());
    buffers.value(reply)->append(reply->readAll());
    //qDebug() << "readyRead - reply: " << reply;
    printd("end\n\n");
}

void
PolarFlowBT::readFileCompleted()
{
    printd("start\n");

    QNetworkReply *reply = static_cast<QNetworkReply*>(QObject::sender());
    //qDebug() << "readFileCompleted - reply:" << reply;

    printd("reply:%s\n", buffers.value(reply)->toStdString().c_str());

    QString rename = replyName(reply);
    QByteArray* data = prepareResponse(buffers.value(reply), rename);
    //qDebug() << "readFileCompleted - data:" << data;

    notifyReadComplete(data, replyName(reply), tr("Completed."));

    printd("end\n\n");

}

void
PolarFlowBT::addSamples(RideFile* ret, QString exerciseId)
{
    printd("start\n");
    // Get infos from requsted JSON
    // https://www.polar.com/accesslink-api/?shell#get-exercise-summary
    // https://www.polar.com/accesslink-api/?shell#get-available-samples
    // https://www.polar.com/accesslink-api/?shell#get-samples
    // https://www.polar.com/accesslink-api/?shell#exercise-sample-types
    //printd("addSamples\n");

    QString token = getSetting(GC_POLARFLOW_TOKEN, "").toString();

    QString transaction_id_Str = getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString();
    //printd("transaction_id_String: %s\n", transaction_id_Str.toStdString().c_str());

    //printd("fileSuffix: %s\n", fileSuffix.toStdString().c_str());

    // RequestURL: GET https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}/samples
    QString getAvailableSampleTypesUrls_Str = QString("%1/v3/users/%2/exercise-transactions/%3/exercises/%4/samples")
                                                  .arg(getSetting(GC_POLARFLOW_URL, "https://www.polaraccesslink.com").toString())
                                                  .arg(getSetting(GC_POLARFLOW_USER_ID, "").toString())
                                                  .arg(getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString())
                                                  .arg(exerciseId);
    printd("URL-List Sample Types: %s\n", getAvailableSampleTypesUrls_Str.toStdString().c_str());

    // request using the bearer token
    QNetworkRequest request(getAvailableSampleTypesUrls_Str);
    request.setRawHeader("Authorization", (QString("Bearer %1").arg(token)).toLatin1());
    request.setRawHeader("Accept", "application/json");

    // make request
    QNetworkReply *reply = nam->get(request);
    printd("response : %s\n", getAvailableSampleTypesUrls_Str.toStdString().c_str());
    //Example: Reply List of URLs
    //Example: https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}/samples/{type-id}

    // blocking request
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    // if successful, lets unpack
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    printd("fetch response: %d: %s \n", reply->error(), reply->errorString().toStdString().c_str());
    printd("HTTP response status code: %d \n", statusCode);

    // Example: https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}/samples/{type-id}
    // Returns a list of hyperlinks to available exercise sample types
    QByteArray getAvailableSampleTypesUrls_Ary = reply->readAll();
    printd("samples url array : %s \n", getAvailableSampleTypesUrls_Ary.toStdString().c_str());
    printd("amount of sample types: %d \n", getAvailableSampleTypesUrls_Ary.size()); // size is nonsense, or amount signs in string
    //printd("QByteArray of sample type urls from exercise: %s \n", getAvailableSampleTypesUrls_Ary.toStdString().c_str());

    /* Example Array
    {
      "samples": [
        "https://www.polaraccesslink.com/v3/users/12/exercise-transactions/34/exercises/56/samples/0",
        "https://www.polaraccesslink.com/v3/users/12/exercise-transactions/34/exercises/56/samples/3"
      ]
    }
    */

    // parse JSON payload
    QJsonParseError getAvailableSampleTypesUrls_Doc_parseError;
    QJsonDocument getAvailableSampleTypesUrls_Doc = QJsonDocument::fromJson(getAvailableSampleTypesUrls_Ary, &getAvailableSampleTypesUrls_Doc_parseError);
    //qDebug() << "addSamples - Document Error Number: " << getAvailableSampleTypesUrls_Doc_parseError.error << " - Document Error String: " << getAvailableSampleTypesUrls_Doc_parseError.errorString();
    //qDebug() << "addSamples - Document list of urls: " << getAvailableSampleTypesUrls_Doc;

    // https://www.polar.com/accesslink-api/?shell#exercise-sample-types
    // List of urls to available samples -> array

    // QJsonArrays declared before if-statments, usage see down below
    // https://stackoverflow.com/questions/13337529/using-variables-outside-of-an-if-statement/13337565
    QJsonArray combinedStream_Ary; // fetches all provided streams and adds time stream
    QJsonObject sampleStream_Obj; // filled with all available samples besides not provided  and added to QJsonArray streams
    QJsonObject timeStream_Obj; // timeStream_Obj will be created seperatly, but gets number of datapoints from heartrate sample, with is always provided and added to QJsonArray streams
    int numberOfAvailableSampleTyps = 0; // amount of available Sample Types, default is set to zero
    int timeSamplesLoopNumber = 0; // is set in after available sample Types are figured out, default is set to zero

    if (getAvailableSampleTypesUrls_Doc_parseError.error == QJsonParseError::NoError) {

        QJsonObject getAvailableSampleTypesUrls_Obj = getAvailableSampleTypesUrls_Doc.object();
        //qDebug() << "addSamples - Samples Exercise - Object url sample exercises: " << getAvailableSampleTypesUrls_Obj;

        QJsonArray sampleTypesfromExercise_Ary = getAvailableSampleTypesUrls_Obj["samples"].toArray();
        //qDebug() << "addSamples - Samples Exercise - Array of Sample Urls: " << sampleTypesfromExercise_Ary;

        // How many Sample Types are available?
        numberOfAvailableSampleTyps = sampleTypesfromExercise_Ary.size();
        printd("sampleTypes - available Amount: %d\n", numberOfAvailableSampleTyps);

        // set timeStream_Obj Loop number
        timeSamplesLoopNumber = numberOfAvailableSampleTyps + 1;
        printd("timeStream - loopnumber: %d\n", timeSamplesLoopNumber);

        // getting sample type
        // https://www.polar.com/accesslink-api/?shell#exercise-sample-types
        // seperate sample urls and get samples data and additional information
        QString sampleUrl_Str;

        // loop number of of actually aquired sample = actualSampleLoopNumber
        for(int actualSampleLoopNumber = 0; actualSampleLoopNumber < numberOfAvailableSampleTyps; actualSampleLoopNumber++) {
            int loopCounterSampleTypes = actualSampleLoopNumber + 1; // amount of Samples counts from one
            sampleUrl_Str = sampleTypesfromExercise_Ary[actualSampleLoopNumber].toString();
            printd("actualSampleLoopNumber: %d / %d - sampleUrl_String - Url to sample-series: %s \n",loopCounterSampleTypes, numberOfAvailableSampleTyps, sampleUrl_Str.toStdString().c_str());

            // Sample URL is provided in
            // Example: GET https://www.polaraccesslink.com//v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}/samples/{type-id}

            // request using the bearer token
            QNetworkRequest request(sampleUrl_Str);
            request.setRawHeader("Authorization", (QString("Bearer %1").arg(token)).toLatin1());
            request.setRawHeader("Accept", "application/json");

            // make request
            QNetworkReply *reply = nam->get(request);
            printd("response : %s\n", getAvailableSampleTypesUrls_Str.toStdString().c_str());

            // blocking request
            QEventLoop loop;
            connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
            loop.exec();

            // if successful, lets unpack
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            printd("fetch response: %d: %s\n", reply->error(), reply->errorString().toStdString().c_str());
            printd("actualSampleLoopNumber: %d / %d \n",loopCounterSampleTypes, numberOfAvailableSampleTyps);
            printd("HTTP response status code: %d \n", statusCode);

            // no bracktes []
            QByteArray sampletype_Ary = reply->readAll();
            //printd("actualSampleLoopNumber: %d / %d - sampletype_Array: %s \n",loopCounterSampleTypes, numberOfAvailableSampleTyps, sampletype_Ary.toStdString().c_str());

            // parse JSON payload
            QJsonParseError sampletype_Doc_parseError;
            QJsonDocument sampletype_Doc;
            sampletype_Doc = QJsonDocument::fromJson(sampletype_Ary, &sampletype_Doc_parseError);
            //qDebug() << "addSamples - loop actualSampleLoopNumber:"<< loopCounterSampleTypes <<"/"<< numberOfAvailableSampleTyps << " - sampletype_Document - Error Number:" << sampletype_Doc_parseError.error << " - sampletype_Document - Error String: " << sampletype_Doc_parseError.errorString();
            //qDebug() << "addSamples - loop actualSampleLoopNumber:"<< loopCounterSampleTypes <<"/"<< numberOfAvailableSampleTyps << " - sampletype_Document:" << sampletype_Doc;
            //printd("sampletype_Doc: %s \n", sampletype_Doc.toJson().toStdString().c_str());
            printd("sampletype_Doc - Error Number: %d - Error String: %s \n", sampletype_Doc_parseError.error, sampletype_Doc_parseError.errorString().toStdString().c_str());

            if (sampletype_Doc_parseError.error == QJsonParseError::NoError) {
                // we first collect all provided samples
                sampleStream_Obj = PolarFlow::createSampleStream(sampletype_Doc, actualSampleLoopNumber, timeSamplesLoopNumber);
                //qDebug() << "addSamples - loop loopCounterSampleTypes:" << loopCounterSampleTypes <<"/"<< numberOfAvailableSampleTyps <<" - sampleStream_Obj:" << sampleStream_Obj;
                //printd("creating  Stream-Object without TimeSampleStream - actual loopnumber: %d / %d\n", loopCounterSampleTypes, sampleStream_Obj.size());
                printd("creating Stream-Object without TimeSampleStream - actual loopnumber: %d / %d\n", loopCounterSampleTypes, numberOfAvailableSampleTyps);

                // and save them into one Array
                combinedStream_Ary.append(sampleStream_Obj);
                //qDebug() << "addSamples - loop loopCounterSampleTypes:" << loopCounterSampleTypes <<"/"<< numberOfAvailableSampleTyps <<" - streams:" << combinedStream_Ary;
                //printd("appending to combined Stream (endsize: %d) without timeStream - actual loopnumber: %d\n", combinedStream_Ary.size(), loopCounterSampleTypes);
                printd("appending to combined Stream (endsize: %d) without timeStream - actual loopnumber: %d\n", numberOfAvailableSampleTyps, loopCounterSampleTypes);
            } // close doc parser error

        } // close for actualSampleLoopNumber
        printd("combined Stream (size) without timeStream %d \n", combinedStream_Ary.size());

        // for correct importing we need a time stream
        // define an empty Json Document for not-provided time stream
        QJsonDocument emptyForTimestream_Doc;

        // fill QJsonObject with time data -> RideFile::secs
        printd("creating TimeStream\n");
        timeStream_Obj = PolarFlow::createSampleStream(emptyForTimestream_Doc, timeSamplesLoopNumber, timeSamplesLoopNumber);

        // and prepend it as first stream in QJsonArray streams
        printd("adding TimeStream to existing combined Stream\n");
        combinedStream_Ary.prepend(timeStream_Obj);
        //printd("streams now with time: %s", combinedStream_Ary);
        //printd("combined Stream (endsize %d) now with timeStream - actual loopnumber: %d\n", combinedStream_Ary.size(), timeSamplesLoopNumber);

        //printd("combinedStream_Array: % \n", combinedStream_Ary.toVariantList());
        //qDebug() << "combinedStream_Ary - Array of Sample Urls: " << combinedStream_Ary;


        // after having created a STRAVA like Stream we work with it
        // mapping names used in the Polar Flow json response
        // to the series names we use in GC
        //printd("creating static struct for available RideFile seriestyps\n");
        static struct {
            RideFile::seriestype type;
            const char *polarsampletypename; // polarsampletypename
            double factor; // to convert from PolarFlow units to GC units
        }
        seriesnames[] = {
                // seriestype,         polarsampletypename,     conversion factor -> RideFile.h/.cpp Line 350ff
                { RideFile::secs,      "time",                  1.0f   }, // s (not provided by polar, needs to be created seperatly)
                { RideFile::hr,        "heartrate",             1.0f   }, // bpm
                { RideFile::kph,       "speed",                 1.0f   }, // km/h
                { RideFile::cad,       "cadence",               1.0f   }, // rpm
                { RideFile::alt,       "altitude",              1.0f   }, // m
                { RideFile::watts,     "power",                 1.0f   }, // W
                //{ RideFile::none,      "powerPedalingIndex",    0.0f   }, // %
                { RideFile::lrbalance, "powerLeftRightBalance", 1.0f   }, // %
                //{ RideFile::none,      "airPressure",           0.0f   }, // hpa
                { RideFile::rcad,       "runningCadence",       1.0f   }, // spm // conversation factor "2" same Value as Strava
                { RideFile::temp,       "temperature",          1.0f   }, // °C
                { RideFile::km,         "distance",             0.001f }, // m
                //{ RideFile::none,       "rrInterval",           0.001f }, // ms
                { RideFile::none,       "",                     0.0f   } // must be last in list, stop condition for loop
            };

        // data to combine with ride
        // define QList<polarSamples_Class> syntax
        printd("creating class  for data\n");
        class polarSamples_Class { // polarSamples_Class = strava_stream
        public:
            double factor; // for converting
            RideFile::seriestype type;
            QJsonArray samples;
        };
        // class example: 1.0f; RideFile::hr; 0,100,102,97,97,101,103,106,96,89,88,87

        printd("creating a QList for each series");
        // create a list of all the data we will work with
        QList<polarSamples_Class> polarSampleClassData_QLst;
        //qDebug() << "addSamples - Samples - loop i:" << loopCounterSampleTypes <<"- data: " << polarSampleClassData_QLst;

        // running through combined stream and fill the QLists
        for(int fillQListsLoop=0; fillQListsLoop<combinedStream_Ary.size(); fillQListsLoop++) {
            QJsonObject combinedStream_Obj = combinedStream_Ary.at(fillQListsLoop).toObject();
            QString type = combinedStream_Obj["type"].toString();
            //printd("filling QList with combinedStream data (Loop: %d / %d) - exercise sample type type: %s\n", fillQListsLoop + 1, combinedStream_Ary.size(), type.toStdString().c_str());

            for(int seriesLoop=0; seriesnames[seriesLoop].type != RideFile::none; seriesLoop++) {
                QString name = seriesnames[seriesLoop].polarsampletypename;
                printd("series name: %s - seriesLoop: %d - fillQListsLoop: %d\n", name.toStdString().c_str(), seriesLoop, fillQListsLoop);
                polarSamples_Class add;

                // running through series
                if (type == name) {
                    add.factor = seriesnames[seriesLoop].factor;
                    add.type = seriesnames[seriesLoop].type;
                    add.samples = combinedStream_Obj["data"].toArray();
                    printd("running through known series %s - seriesloop %d - fillQListloop %d\n", name.toStdString().c_str(), seriesLoop, fillQListsLoop);
                    printd("and fill in data from commbinedStream data-Array - %d\n", combinedStream_Obj["data"].toArray().size());
                    //qDebug() << "addSamples - seriesLoop: " << seriesLoop << "- streams factor:" << seriesnames[seriesLoop].factor;
                    //qDebug() << "addSamples - seriesLoop: " << seriesLoop << "- streams type:" << seriesnames[seriesLoop].type;
                    //qDebug() << "addSamples - seriesLoop: " << seriesLoop << "- streams samples size:" << combinedStream_Obj["data"].toArray().size();
                    //qDebug() << "addSamples - seriesLoop: " << seriesLoop << "- streams samples:" << combinedStream_Obj["data"].toArray();

                    polarSampleClassData_QLst << add;
                    //printd("running through known series %", polarSampleClassData_QLst);
                    //qDebug() << "addSamples - seriesLoop: " << seriesLoop << "- polarSampleClassData_QLst : " << polarSampleClassData_QLst;
                    //printd("running through known series %s done - seriesloop %d - fillQListloop %d\n", name.toStdString().c_str(), seriesLoop, fillQListsLoop);
                    break; // breaks to seriesloop loop
                }
                // close running through series if type name
            } // close for seriesLoop
            printd("combining samples  ended\n\n");

        }
        // close if loop fillQListsLoop

        bool end = false;
        int index = 0;

        //printd("adding data to Ridefile from each Polar Sample Class (amount classes: %d)\n", polarSampleClassData_QLst.size());
        do {
            RideFilePoint add;
            //printd("filling Ridefile - do-while: %d - list size: %d \n", index, polarSampleClassData_QLst.size());

            // move through streams if they're waiting for this point
            foreach(polarSamples_Class polarSampleClass_Element, polarSampleClassData_QLst) {

                // if this stream still has List of data to consume
                if (index < polarSampleClass_Element.samples.count()) {
                    //printd("there are more elements in series list - list index number: (%d / %d)\n", index + 1, polarSampleClass_Element.samples.count());
                    // latitude and longitude is provided in gpx file
                    //printd("add all other besides latitude and longitude - list index number %d\n", index+1);
                    // hr, distance et al
                    double value = polarSampleClass_Element.factor * polarSampleClass_Element.samples.at(index).toDouble();
                    // this is one for us, update and move on
                    add.setValue(polarSampleClass_Element.type, value);
                    //qDebug() << "addSamples - do-while"<< index << "/" << polarSampleClass_Element.samples.count()  << "- else add polarSampleClass_Element.type, value:" << polarSampleClass_Element.type << ", " << value;
                }
                // close if polarSampleClass_Element
                else {
                    printd("no more elements found in series list - list index number %d\n", index);
                    end = true;
                }
                // close else
                }
            // close foreach

            //printd("adding element to Ridefile:Series - loop %d\n", index); //%s", polarSampleClassData_QLst.samples.at(index));
            ret->appendPoint(add); // example: ret->appendPoint(RideFile::hr, 182)
            //printd("do-while streamSize %d \n", polarSampleClass_Element.size());
            //qDebug() << "addSamples - do-while" << index << "- ret:" << ret;
            index++;

        }
        while (!end);
        printd("do-while - ended with index: %d\n", index); // << "- ret:" << ret;
        printd("all samples added - end\n");

    }
    // close if getAvailableSampleTypesUrls_Doc_parseError
}

QString
PolarFlowBT::getFile(QString exerciseId, QString filetype, QDateTime starttime, QString detailedSportInfo_Str)
{
    // https://www.polar.com/accesslink-api/?shell#get-gpx
    // returns training session in GPX (GPS Exchange format)
    // https://www.polar.com/accesslink-api/?shell#get-tcx
    // returns gzipped TCX
    // https://www.polar.com/accesslink-api/?shell#get-fit-beta
    // returns FIT file

    // Status Codes for available Data
    int dataAvailable = 200;

    printd("GPX / TCX / Fit beta - start \n");

    printd("PolarFlow::getFile \n");

    printd("ExerciseID: %s \n", exerciseId.toStdString().c_str());

    QString token = getSetting(GC_POLARFLOW_TOKEN, "").toString();

    QString transaction_id_Str = getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString();
    printd("transaction_id_String: %s \n", transaction_id_Str.toStdString().c_str());

    // RequestURL: GET https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}/gpx
    // RequestURL: GET https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}/tcx
    // RequestURL: GET https://www.polaraccesslink.com/v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}/fit

    QString filename;

    // first filetype GPX
    if (filetype == "gpx") {

        QString get_GPX_Str;
        get_GPX_Str = QString("%1/v3/users/%2/exercise-transactions/%3/exercises/%4/gpx")
                          .arg(getSetting(GC_POLARFLOW_URL, "https://www.polaraccesslink.com").toString())
                          .arg(getSetting(GC_POLARFLOW_USER_ID, "").toString())
                          .arg(getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString())
                          .arg(exerciseId);
        printd("GPX - URL: %s \n", get_GPX_Str.toStdString().c_str());

        // request using the bearer token, application/gpx+xml
        QNetworkRequest requestGPX(get_GPX_Str);
        requestGPX.setRawHeader("Authorization", (QString("Bearer %1").arg(token)).toLatin1());
        requestGPX.setRawHeader("Accept", "application/gpx+xml");

        // make request
        QNetworkReply *replyGPX = nam->get(requestGPX);
        printd("response : %s\n", get_GPX_Str.toStdString().c_str());
        // Example: Reply List of URLs /v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}/gpx

        // blocking request
        QEventLoop loopGPX;
        connect(replyGPX, SIGNAL(finished()), &loopGPX, SLOT(quit()));
        loopGPX.exec();

        // if successful, lets unpack
        int statusCodeGPX = replyGPX->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        printd("fetch response: %d: %s\n", replyGPX->error(), replyGPX->errorString().toStdString().c_str());
        printd("GPX - Samples - HTTP response status code: %d \n", statusCodeGPX);

        QByteArray get_GPX_Ary = replyGPX->readAll();
        //qDebug() << "get File - GPX - reply:" << reply;
        //printd("GPX - QByteArray: %s \n", get_GPX_Ary.toStdString().c_str());

        if (statusCodeGPX == dataAvailable) {
            QString filenameGPX;
            //PolarFlow pf_instance;
            //filenameGPX = saveFileBT(get_GPX_Ary, exerciseId, filetype, starttime, detailedSportInfo_Str);
            filenameGPX = saveFile(get_GPX_Ary, exerciseId, filetype, starttime, detailedSportInfo_Str);
            //filenameGPX = pf_instance.saveFile(get_GPX_Ary, exerciseId, filetype, starttime, detailedSportInfo_Str);
            printd("GPX - filename: %s \n", filenameGPX.toStdString().c_str());
            printd("GPX end \n");
            filename = filenameGPX;
        } else filename = "noContent";
    } // close filetype gpx

    // second file TCX
    if (filetype == "tcx") {
        QString get_TCX_Str;
        get_TCX_Str = QString("%1/v3/users/%2/exercise-transactions/%3/exercises/%4/tcx")
                          .arg(getSetting(GC_POLARFLOW_URL, "https://www.polaraccesslink.com").toString())
                          .arg(getSetting(GC_POLARFLOW_USER_ID, "").toString())
                          .arg(getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString())
                          .arg(exerciseId);
        printd("TCX - URL: %s \n", get_TCX_Str.toStdString().c_str());

        // request using the bearer token, application/vnd.garmin.tcx+xml
        QNetworkRequest requestTCX(get_TCX_Str);
        requestTCX.setRawHeader("Authorization", (QString("Bearer %1").arg(token)).toLatin1());
        requestTCX.setRawHeader("Accept", "application/vnd.garmin.tcx+xml");

        // make request
        QNetworkReply *replyTCX = nam->get(requestTCX);
        printd("response : %s\n", get_TCX_Str.toStdString().c_str());
        // Example: Reply List of URLs /v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}/tcx

        // blocking request
        QEventLoop loopTCX;
        connect(replyTCX, SIGNAL(finished()), &loopTCX, SLOT(quit()));
        loopTCX.exec();

        // if successful, lets unpack
        int statusCodeTCX = replyTCX->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        printd("fetch response: %d: %s\n", replyTCX->error(), replyTCX->errorString().toStdString().c_str());
        printd("TCX - HTTP response status code: %d \n", statusCodeTCX);


        QByteArray get_TCX_Ary = replyTCX->readAll();
        //printd("page : %s\n", get_TCX_Ary.toStdString().c_str());

        if (statusCodeTCX == dataAvailable) {
            QString filenameTCX;
            filenameTCX = PolarFlow::saveFile(get_TCX_Ary, exerciseId, filetype, starttime, detailedSportInfo_Str);

            printd("TCX - filename: %s \n", filenameTCX.toStdString().c_str());
            printd("TCX - end \n");
            filename = filenameTCX;
        } else filename = "noContent";
    } // close filetype tcx

    // third file FIT Beta
    if (filetype == "fit") {
        QString get_FIT_Str;
        get_FIT_Str = QString("%1/v3/users/%2/exercise-transactions/%3/exercises/%4/fit")
                          .arg(getSetting(GC_POLARFLOW_URL, "https://www.polaraccesslink.com").toString())
                          .arg(getSetting(GC_POLARFLOW_USER_ID, "").toString())
                          .arg(getSetting(GC_POLARFLOW_TRANSACTION_ID, "").toString())
                          .arg(exerciseId);
        printd("FIT Beta - URL-List Samples: %s \n", get_FIT_Str.toStdString().c_str());

        // request using the bearer token, */*
        QNetworkRequest requestFIT(get_FIT_Str);
        requestFIT.setRawHeader("Authorization", (QString("Bearer %1").arg(token)).toLatin1());
        requestFIT.setRawHeader("Accept", "*/*");

        // make request
        QNetworkReply *replyFIT = nam->get(requestFIT);
        printd("response : %s\n", get_FIT_Str.toStdString().c_str());
        // Example: Reply List of URLs /v3/users/{user-id}/exercise-transactions/{transaction-id}/exercises/{exercise-id}/fit

        // blocking request
        QEventLoop loopFIT;
        connect(replyFIT, SIGNAL(finished()), &loopFIT, SLOT(quit()));
        loopFIT.exec();

        // if successful, lets unpack
        int statusCodeFIT = replyFIT->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        printd("fetch response: %d: %s\n", replyFIT->error(), replyFIT->errorString().toStdString().c_str());
        printd("FIT Beta - Samples - HTTP response status code: %d \n", statusCodeFIT);

        QByteArray get_FIT_Ary = replyFIT->readAll();
        //printd("FIT Beta - QByteArray: %s \n", getAvailableSampleTypesUrls_Ary.toStdString().c_str());

        if (statusCodeFIT == dataAvailable) {
            QString filenameFIT;
            filenameFIT = PolarFlow::saveFile(get_FIT_Ary, exerciseId, filetype, starttime, detailedSportInfo_Str);

            printd("FIT Beta - filename: %s \n", filenameFIT.toStdString().c_str());
            printd("FIT Beta - close \n");
            filename = filenameFIT;
        } else filename = "noContent";
    } // close filetype fit beta

    // filename to return
    printd("returned filename: %s \n", filename.toStdString().c_str());
    printd("File - end \n");
    return filename;
}

QByteArray*
PolarFlowBT::prepareResponse(QByteArray* data, QString &name)
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
            commit(emptyerrorlist);
        }

    }
    // at last return aquired data
    return data;
}

// development put on hold - AccessLink API compatibility issues with Desktop applications
//#if 0
static bool addPolarFlowBT() {
    CloudServiceFactory::instance().addService(new PolarFlowBT(NULL));
    return true;
}

static bool add = addPolarFlowBT();
//#endif
