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

/*
QImage
PolarFlow::logo() const
{
    printd("logo\n");
    //return QImage(":images/services/polarflow.png");
}
*/

void
PolarFlow::onSslErrors(QNetworkReply *reply, const QList<QSslError>&)
{
    reply->ignoreSslErrors();
}

bool
PolarFlow::close()
{
    printd("End\n");
    // nothing to do for now
    return true;
}

int
PolarFlow::isoDateToSeconds(QString &string)
{
    // https://www.geeksforgeeks.org/return-statement-in-c-cpp-with-examples/?ref=lbp
    // https://doc.qt.io/qt-5/qdate.html#fromString
    printd("start \n");

    QString duration_str = string;
    printd("duration_String ISO: %s\n", duration_str.toStdString().c_str());

    int secondsPerMinute = 60;
    int minutesPerHour = 60;
    int hoursPerDay = 24;
    float secondsAreZero = 0.000;
    int minutesAreZero = 0;
    int hoursAreZero = 0;
    int isoDateStringLength =  0;
    int positionMark = 0;
    int positionCorrector = 1; // char position counter, counts from zero
    int secondsStringLength =  0;

    bool containsPoint = false;
    bool containsSeconds = false;
    bool containsMinutes = false;
    bool containsHours = false;

    // find out if ISODate contains all needed conversation info
    if (duration_str.contains(".")) {
        containsPoint = true;
    }
    if (duration_str.contains("S")) {
        containsSeconds = true;
    }
    if (duration_str.contains("M")) {
        containsMinutes = true;
    }
    if (duration_str.contains("H")) {
        containsHours = true;
    }

    // prepend seconds to ISODate if not provided
    if (containsSeconds == false) {
        duration_str.prepend("0.000S"); // PT4H51M->PT4H51M0.000S
        printd("duration_String ISO after prepend: %s\n", duration_str.toStdString().c_str());
    }

    // filling up ISODate's string for seconds, if not fully provided - missing "."
    if (containsSeconds == true && containsPoint == false) { // PT4H51M38S: 38S->38.000S - PT4M4S->PT4M4.000S
        isoDateStringLength = duration_str.size();
        if (containsMinutes == true) {
            positionMark = duration_str.indexOf("M");
            printd("M is char at position: %d\n", positionMark);
        }
        if (containsMinutes == false) {
            positionMark = duration_str.indexOf("H");
            printd("H is char at position: %d\n", positionMark);
        }
        // lets find out how many chars are here for seconds
        secondsStringLength = isoDateStringLength - positionMark - positionCorrector;
        printd("ISODate is provided with %d chars\n", isoDateStringLength);
        printd("Seconds are provided with %d chars\n", secondsStringLength);

        if (secondsStringLength == 2) { // 4S->4.000S
            duration_str.replace("S", ".000S");
            printd("duration_String ISO after fillup: %s\n", duration_str.toStdString().c_str());
        }
        if (secondsStringLength == 3) { // 38S->38.000S
            duration_str.replace("S", ".000S");
            printd("duration_String ISO after fillup: %s\n", duration_str.toStdString().c_str());
        }
    };

    // check contained letters H, M, S ; Hours, Minutes ? true / false
    QString duration_strtime;

    // Hours, Minutes and Seconds are provided in String
    if (containsHours == true && containsMinutes == true && containsSeconds == true) {
        duration_strtime = duration_str.replace("PT", "").replace("H", ":").replace("M", ":").replace("S", ""); // PT4H51M38.512S->4:51:38.512
    };
    // Minutes are Zero - Hours und Seconds are provided in String
    if (containsHours == true && containsMinutes == false && containsSeconds == true) {
        duration_strtime = duration_str.replace(QString("H"), QString("H0M"));
        duration_strtime = duration_str.replace("PT", "").replace("H", ":").replace("M", ":").replace("S", ""); // PT43H8.512S->PT43H0M8.512S->4:00:38.512
    };
    // Hours are Zero - Minutes and Seconds are provided in String
    if (containsHours == false && containsMinutes == true && containsSeconds == true) {
        duration_strtime = duration_str.replace("PT", "").prepend("0:").replace("M", ":").replace("S", ""); // 51M38.512S->0:51:38.512
    };
    // Hours and Minutes are Zero - Seconds are provided in String
    if (containsHours == false && containsMinutes == false && containsSeconds == true) {
        duration_strtime = duration_str.replace("PT", "").prepend("0:0:").replace("S", ""); //  38.512S->0:0:38.512
    };
    printd("duration prepared for time : %s\n", duration_strtime.toStdString().c_str());

    QTime duration_astime = QTime::fromString(duration_strtime, "h:m:s.z");
    //qDebug() << "isoDateToSeconds - duration as time:" << duration_astime;
    //printd("duration prepared as time : %s\n", duration_astime.toString());

    int durationHours = duration_astime.hour();
    int durationMinutes = duration_astime.minute();
    int durationSeconds = duration_astime.second();

    int durationTimeInSeconds = (durationHours * minutesPerHour * secondsPerMinute) + (durationMinutes * secondsPerMinute) + durationSeconds;

    //qDebug() printd("isoDateToSeconds - add duration_string: %s\n", duration_str.toStdString().c_str());
    //qDebug() printd("isoDateToSeconds - add duration_stringtime: %s\n", duration_strtime.toStdString().c_str());
    //qDebug() printd("isoDateToSeconds - add duration_astime: %s\n", duration_astime);
    printd("add duration_hours: %d\n", durationHours);
    printd("add duration_minute: %d\n", durationMinutes);
    printd("add duration_second: %d\n", durationSeconds);
    printd("duration_time in seconds: %d\n", durationTimeInSeconds);

    printd("end\n");
    return durationTimeInSeconds;
}

bool
PolarFlow::writeFile(QByteArray &data, QString remotename, RideFile *ride)
{
    printd("writeFile - start\n");
    // to do - Polar Flow does not support upload
    return false;
    //return true;
}

void
PolarFlow::writeFileCompleted()
{
    printd("writeFileCompleted - start \n");
    // to do - Polar Flow does not support upload
}

void
PolarFlow::clearJsonObject(QJsonObject &object)
{
    printd("start\n");

    QStringList strList = object.keys();
    //qDebug() << "clearJsonObject - StringList" << strList;

    for(int i = 0; i < strList.size(); ++i)
    {
        object.remove(strList.at(i));
    }

    printd("JasonObject cleared \n end\n");
}

QJsonObject
//PolarFlow::createSampleStream(QJsonDocument &document, int loopnumber)
PolarFlow::createSampleStream(QJsonDocument &sampletype_Doc, int actualSampleLoopNumber, int timeSamplesLoopNumber)
{
    printd("Start \n");

    //int timeSamplesLoopNumber = getSetting(GC_POLARFLOW_TIMESAMPLES_LOOPNUMBER, "").toInt();

    QJsonObject stream_Obj; // returned Object, sample-stream or time-stream

    //each sample-type looks like this - not each type has to be present
    //{
    //  "recording-rate": 5,                                 // integer(int32), in seconds, zero for rr-data
    //  "sample-type": "1",                                  // string(byte)
    //  "data": "0,100,102,97,97,101,103,106,96,89,88,87,98" // string, comma-separated list, zero indicates sensor is offline
    //}

    // new Array should look like Strava
    // https://developers.strava.com/docs/reference/#api-models-DistanceStream
    // [ {
    //  "type" : "distance",                                                        // integer
    //  "data" : [ 2.9, 5.8, 8.5, 11.7, 15, 19, 23.2, 28, 32.8, 38.1, 43.8, 49.5 ], // float
    //  "series_type" : "distance",                                                 // string
    //  "original_size" : 12,                                                       //
    //  "resolution" : "high"                                                       // string
    // } ]

    // new Stream with Polar samples
    // [ {
    //  "type" : "heartrate",                                                       // string
    //  "data" : [ 0,100,102,97,97,101,103,106,96,89,88,87,98 ],                    // converted to float
    //  "series_type" : "0",                                                        // integer
    //  "original_size" : 12,                                                       // integer
    //  "recording_rate" : "5"                                                      // integer
    // } ]

    int loopCounterSampleTypes = actualSampleLoopNumber + 1; // amount of Samples counts from one

    if (actualSampleLoopNumber < timeSamplesLoopNumber) {

        // Get sample-type ID: represents recorded sample
        QString sampletype_id_Str = QString::number(sampletype_Doc["sample-type"].toInt());
        printd("actualSample: %d - sampletype_id_String: %d\n", loopCounterSampleTypes, sampletype_id_Str.toInt());

        /*
      sample types
      ============
      samples are available in 11 different types.
      If the samples are not available for a particular activity it will be left out of the request results.
      https://www.polar.com/accesslink-api/?shell#exercise-sample-types
      sample-type name              unit
      string(byte)
       0   Heart rate               bpm
       1   Speed                    km/h
       2   Cadence 	                rpm
       3   Altitude                 m
       4   Power                    W
       5   Power pedaling index     %     // https://support.polar.com/de/support/pedalling_index
       6   Power left-right balance %
       7   Air pressure 	        hpa
       8   Running cadence 	        spm
       9   Temperaure 	            ºC
      10   Distance 	            m
      11   RR Interval 	            ms
           RR interval samples can contain NULL samples, while other sample types always have a value.
           NULL sample indicates missing data. In order to get RR interval samples,
           training needs to be performed specific heart rate sensor.
      */

        QString samplename_Str;
        if (sampletype_id_Str == "0" ) {
            samplename_Str = "heartrate";
        }
        if (sampletype_id_Str == "1" ) {
            samplename_Str =  "speed";
        }
        if (sampletype_id_Str == "2" ) {
            //stype (sport) endsWith SWIMMING or CYCLING
            samplename_Str = "cadence";
        }
        if (sampletype_id_Str == "3") {
            samplename_Str = "altitude";
        }
        if (sampletype_id_Str == "4") {
            samplename_Str = "power";
        }
        if (sampletype_id_Str == "5") {
            samplename_Str = "powerPedalingIndex";
        }
        if (sampletype_id_Str == "6") {
            samplename_Str = "powerLeftRightBalance";
        }
        if (sampletype_id_Str == "7") {
            samplename_Str =  "airPressure";
        }
        if (sampletype_id_Str == "8") {
            //stype (sport) endsWith RUNNING
            samplename_Str = "runningCadence";
        }
        if (sampletype_id_Str == "9") {
            samplename_Str = "temperature";
        }
        if (sampletype_id_Str == "10") {
            samplename_Str = "distance";
        }
        if (sampletype_id_Str == "11") {
            samplename_Str = "rrInterval";
        }
        printd("actualSampleLoopNumber: %d - Samples %s with id: %s found\n", loopCounterSampleTypes,  samplename_Str.toStdString().c_str(), sampletype_id_Str.toStdString().c_str());

        // Get recording-rate: each recording rate one data_value
        QString recording_rate_Str = QString::number(sampletype_Doc["recording-rate"].toInt());
        printd("actualSampleLoopNumber: %d - recording_rate_String: %s\n", loopCounterSampleTypes, recording_rate_Str.toStdString().c_str());

        // create array from list of comma separeted e.g. heart rate values
        QString datalist_Str = sampletype_Doc["data"].toString();

        // create a list of all the data we will work with
        QStringList datalist_Lst = datalist_Str.split(QLatin1Char(','));
        printd("actualSampleLoopNumber: %d - datalist_List no of elements: %d\n", loopCounterSampleTypes, datalist_Lst.size());
        //printd("actualSampleLoopNumber: %d - datalist_String elements: %s\n", loopCounterSampleTypes, datalist_Str.toStdString().c_str());

        // create a json array and fill it. Change type from String to float number
        QJsonArray datalist_Ary;
        foreach(QString const& data, datalist_Lst)
            datalist_Ary.append(data.toFloat());
        //qDebug() << "createSampleStream - datalist_Ary:" << datalist_Ary;

        // save HR Sample size and recording rate for generating suitable timeStream
        /* if (sampletype_id_Str == "0") {
          setSetting(GC_POLARFLOW_SAMPLE_SIZE, datalist_Lst.size());
          setSetting(GC_POLARFLOW_SAMPLE_RECORDINGRATE, recording_rate_Str.toInt());
      } */

        // save first available Sample size and recording rate for generating suitable timeStream
        // rrIntervall (sample id 11) could not be used ?!?
        if (sampletype_id_Str != "11") {
            if (actualSampleLoopNumber == 0) {
                setSetting(GC_POLARFLOW_SAMPLE_SIZE, datalist_Lst.size());
                setSetting(GC_POLARFLOW_SAMPLE_RECORDINGRATE, recording_rate_Str.toInt());
                setSetting(GC_POLARFLOW_SAMPLE_ID, sampletype_id_Str);
                printd("actualSampleLoopNumber: %d\n", loopCounterSampleTypes);
            }
        }
        // rrIntervall (sample id 11)
        if (sampletype_id_Str == "11") {
            if (actualSampleLoopNumber == 0) {
                setSetting(GC_POLARFLOW_SAMPLE_SIZE, datalist_Lst.size());
                setSetting(GC_POLARFLOW_SAMPLE_RECORDINGRATE, 1);
                printd("actualSampleLoopNumber: %d\n", loopCounterSampleTypes);
            }
        }
        printd("saved Sample Size: %d\n", getSetting(GC_POLARFLOW_SAMPLE_SIZE, "").toInt());
        printd("saved Sample Size Recording Rate: %d\n", getSetting(GC_POLARFLOW_SAMPLE_RECORDINGRATE, "").toInt());
        printd("saved from Sample id: %d\n", getSetting(GC_POLARFLOW_SAMPLE_ID, "").toInt());

        // Build a new Array like Strava Stream to use Syntax
        // https://www.programmerall.com/article/31271073246/
        QJsonDocument sampleStream_Doc;
        QJsonObject sampleStream_Obj;
        sampleStream_Obj.insert("type", samplename_Str);
        sampleStream_Obj.insert("data", datalist_Ary);
        sampleStream_Obj.insert("series_type", sampletype_id_Str.toInt());
        sampleStream_Obj.insert("original_size", datalist_Lst.size());
        sampleStream_Obj.insert("recording_rate", recording_rate_Str.toInt());

        //qDebug() << "createSampleStream - stream_Object:" << sampleStream_Obj;
        //return sampleStream_Obj;
        stream_Obj = sampleStream_Obj;
        clearJsonObject(sampleStream_Obj);

    } // close if loopnumber < timestream

    // create a time stream
    // time stream needs a size of SAMPLE_SIZE * SAMPLE_RECORDINGRATE
    if (actualSampleLoopNumber == timeSamplesLoopNumber) {

        // How many elements / samples are needed for timestream
        // they need to have SAMPLE_SIZE * RECORDING RATE
        // RR-Record could not be used, because RACORDING_RATE=0
        int recordingSize = getSetting(GC_POLARFLOW_SAMPLE_SIZE, "").toInt();
        int recordingRate = getSetting(GC_POLARFLOW_SAMPLE_RECORDINGRATE, "").toInt();
        //int loops = getSetting(GC_POLARFLOW_SAMPLE_SIZE, "").toInt();
        printd("actualSampleLoopNumber: %d - used Sample Size: %d\n", loopCounterSampleTypes, getSetting(GC_POLARFLOW_SAMPLE_SIZE, "").toInt());
        printd("actualSampleLoopNumber: %d - used Sample Size Recording Rate: %d\n", loopCounterSampleTypes, getSetting(GC_POLARFLOW_SAMPLE_RECORDINGRATE, "").toInt());
        printd("actualSampleLoopNumber: %d - used Sample id: %d\n", loopCounterSampleTypes, getSetting(GC_POLARFLOW_SAMPLE_ID, "").toInt());

        int timeSampleSize = (recordingSize * recordingRate);
        printd("actualSampleLoopNumber: %d - TimeSample Size: %d\n", loopCounterSampleTypes, timeSampleSize);

        // convert integer to float
        //https://www.codegrepper.com/code-examples/cpp/int+to+float+c%2B%2B
        //float recording_rate = (float)recordingRate;

        QList<float> timeSampleData_Lst;
            //timeSampleData_Lst.append(0.0); // first member is zero
        //for(int i=0; i<loops; i++) {
        for(int i=0; i<timeSampleSize; i++) {
            float localLoopNumber = (float)i;
            //float time = recording_rate*localLoopNumber;
            float time = localLoopNumber;
            timeSampleData_Lst.append(time);
        }
        printd("actualSampleLoopNumber: %d - timeSampleData_List no of elements: %d \n", loopCounterSampleTypes,  timeSampleData_Lst.size());
        //qDebug() << "createSampleStream - timeSampleData_List:" << timeSampleData_Lst;

        QJsonArray timeData_Ary;
        //foreach(QList<int> data, timeSampleData_Lst)
        for(int i=0; i<timeSampleData_Lst.size(); i++) {
            timeData_Ary.append(timeSampleData_Lst.at(i));
        }
        printd("actualSampleLoopNumber: %d - time_Array no of elements: %d \n", loopCounterSampleTypes,  timeData_Ary.size());
        //qDebug() << "createSampleStream - time_Array:" << timeData_Ary;

        QJsonObject timeStream_Obj;
        timeStream_Obj.insert("type", "time");
        timeStream_Obj.insert("data", timeData_Ary);
        timeStream_Obj.insert("series_type", "");
        timeStream_Obj.insert("original_size", timeData_Ary.size());
        timeStream_Obj.insert("recording_rate", recordingRate);

        //qDebug() << "createSampleStream - time_Object:" << timeStream_Obj;
        //return timeStream_Obj;
        stream_Obj = timeStream_Obj;
        clearJsonObject(timeStream_Obj);
    }
    return stream_Obj;
}

void
//PolarFlow::addSamples(RideFile* ret, QString exerciseId, QString fileSuffix)
PolarFlow::addSamples(RideFile* ret, QString exerciseId)
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
                sampleStream_Obj = createSampleStream(sampletype_Doc, actualSampleLoopNumber, timeSamplesLoopNumber);
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
        timeStream_Obj = createSampleStream(emptyForTimestream_Doc, timeSamplesLoopNumber, timeSamplesLoopNumber);

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

void
PolarFlow::fixLapSwim(RideFile* ret, QJsonArray laps)
{
    printd("fixLapSwim - start \n");
    /*
    // Lap Swim & SmartRecording enabled: use distance from laps to fix samples
    if (ret->isSwim() && ret->isDataPresent(RideFile::km) && !ret->isDataPresent(RideFile::lat)) {

        int lastSecs = 0;
        double lastDist = 0.0;
        for (int i=0; i<laps.count() && i<ret->intervals().count(); i++) {
            QJsonObject lap = laps[i].toObject();
            double start = ret->intervals()[i]->start;
            double end = ret->intervals()[i]->stop;
            int startIndex = ret->timeIndex(start) ? ret->timeIndex(start)-1 : 0;
            double km = ret->getPointValue(startIndex, RideFile::km);

            // fill gaps and fix distance before the lap
            double deltaDist = (start>lastSecs && km>lastDist+0.001) ? (km-lastDist)/(start-lastSecs) : 0;
            double kph = 3600.0*deltaDist;
            for (int secs=lastSecs; secs<start; secs++) {
                ret->appendOrUpdatePoint(secs, 0.0, 0.0, lastDist+deltaDist,
                        kph, 0.0, 0.0, 0.0,
                        0.0, 0.0, 0.0, 0.0,
                        RideFile::NA, 0.0,
                        0.0, 0.0, 0.0, 0.0,
                        0.0, 0.0,
                        0.0, 0.0, 0.0, 0.0,
                        0.0, 0.0, 0.0, 0.0,
                        0.0, 0.0,
                        0.0, 0.0, 0.0, 0.0,
                        0, false);
                // force update, even when secs or kph are 0
                if (secs == 0 || kph == 0.0)
                    ret->setPointValue(secs, RideFile::kph, kph);
                lastDist += deltaDist;
            }

            // fill gaps and fix distance inside the lap
            deltaDist = 0.001*lap["distance"].toDouble()/(end-start);
            kph = 3600.0*deltaDist;
            for (int secs=start; secs<end; secs++) {
                ret->appendOrUpdatePoint(secs, 0.0, 0.0, lastDist+deltaDist,
                        kph, 0.0, 0.0, 0.0,
                        0.0, 0.0, 0.0, 0.0,
                        RideFile::NA, 0.0,
                        0.0, 0.0, 0.0, 0.0,
                        0.0, 0.0,
                        0.0, 0.0, 0.0, 0.0,
                        0.0, 0.0, 0.0, 0.0,
                        0.0, 0.0,
                        0.0, 0.0, 0.0, 0.0,
                        0, false);
                // force update, even when secs or kph are 0
                if (secs == 0 || kph == 0.0)
                    ret->setPointValue(secs, RideFile::kph, kph);
                lastDist += deltaDist;
            }
            lastSecs = end;
        }
    }
*/
    printd("fixLapSwim - end \n");
}

void
PolarFlow::fixSmartRecording(RideFile* ret)
{
    printd("fixSmartRecording - start \n");
    QVariant isGarminSmartRecording = appsettings->value(NULL, GC_GARMIN_SMARTRECORD,Qt::Checked);
    // do nothing if disabled
    if (isGarminSmartRecording.toInt() == 0) return;
    //qDebug() << "fixSmartRecording - isGarminSmartRecording" << isGarminSmartRecording;


    QVariant GarminHWM = appsettings->value(NULL, GC_GARMIN_HWMARK);
    //qDebug() << "fixSmartRecording - GarminHWM" << GarminHWM;
    // default to 30 seconds
    if (GarminHWM.isNull() || GarminHWM.toInt() == 0) GarminHWM.setValue(30);
    // minimum 90 seconds for swims
    if (ret->isSwim() && GarminHWM.toInt()<90) GarminHWM.setValue(90);

    // The following fragment was adapted from FixGaps

    // If there are less than 2 dataPoints then there
    // is no way of post processing anyway (e.g. manual workouts)
    //qDebug() << "fixSmartRecording - ret" << ret;
    if (ret->dataPoints().count() < 2) return;

    // OK, so there are probably some gaps, lets post process them
    RideFilePoint *last = NULL;
    int dropouts = 0;
    double dropouttime = 0.0;
    printd("fixSmartRecording - dropoutime: %f \n", dropouttime);

    for (int position = 0; position < ret->dataPoints().count(); position++) {
        RideFilePoint *point = ret->dataPoints()[position];
        //printd("fixSmartRecording - loop position: %i \n", position);

        if (NULL != last) {
            double gap = point->secs - last->secs - ret->recIntSecs();

            // if we have gps and we moved, then this isn't a stop
            bool stationary = ((last->lat || last->lon) && (point->lat || point->lon)) // gps is present
                              && last->lat == point->lat && last->lon == point->lon;

            // moved for less than GarminHWM seconds ... interpolate
            if (!stationary && gap >= 1 && gap <= GarminHWM.toInt()) {

                // what's needed?
                dropouts++;
                dropouttime += gap;

                int count = gap/ret->recIntSecs();
                double hrdelta = (point->hr - last->hr) / (double) count;
                double pwrdelta = (point->watts - last->watts) / (double) count;
                double kphdelta = (point->kph - last->kph) / (double) count;
                double kmdelta = (point->km - last->km) / (double) count;
                double caddelta = (point->cad - last->cad) / (double) count;
                double altdelta = (point->alt - last->alt) / (double) count;
                double nmdelta = (point->nm - last->nm) / (double) count;
                double londelta = (point->lon - last->lon) / (double) count;
                double latdelta = (point->lat - last->lat) / (double) count;
                double hwdelta = (point->headwind - last->headwind) / (double) count;
                double slopedelta = (point->slope - last->slope) / (double) count;
                double temperaturedelta = (point->temp - last->temp) / (double) count;
                double lrbalancedelta = (point->lrbalance - last->lrbalance) / (double) count;
                double ltedelta = (point->lte - last->lte) / (double) count;
                double rtedelta = (point->rte - last->rte) / (double) count;
                double lpsdelta = (point->lps - last->lps) / (double) count;
                double rpsdelta = (point->rps - last->rps) / (double) count;
                double lpcodelta = (point->lpco - last->lpco) / (double) count;
                double rpcodelta = (point->rpco - last->rpco) / (double) count;
                double lppbdelta = (point->lppb - last->lppb) / (double) count;
                double rppbdelta = (point->rppb - last->rppb) / (double) count;
                double lppedelta = (point->lppe - last->lppe) / (double) count;
                double rppedelta = (point->rppe - last->rppe) / (double) count;
                double lpppbdelta = (point->lpppb - last->lpppb) / (double) count;
                double rpppbdelta = (point->rpppb - last->rpppb) / (double) count;
                double lpppedelta = (point->lpppe - last->lpppe) / (double) count;
                double rpppedelta = (point->rpppe - last->rpppe) / (double) count;
                double smo2delta = (point->smo2 - last->smo2) / (double) count;
                double thbdelta = (point->thb - last->thb) / (double) count;
                double rcontactdelta = (point->rcontact - last->rcontact) / (double) count;
                double rcaddelta = (point->rcad - last->rcad) / (double) count;
                double rvertdelta = (point->rvert - last->rvert) / (double) count;
                double tcoredelta = (point->tcore - last->tcore) / (double) count;

                // add the points
                for(int i=0; i<count; i++) {
                    printd("fixSmartRecording - loop i: %i \n", i);
                    RideFilePoint *add = new RideFilePoint(last->secs+((i+1)*ret->recIntSecs()),
                                                           last->cad+((i+1)*caddelta),
                                                           last->hr + ((i+1)*hrdelta),
                                                           last->km + ((i+1)*kmdelta),
                                                           last->kph + ((i+1)*kphdelta),
                                                           last->nm + ((i+1)*nmdelta),
                                                           last->watts + ((i+1)*pwrdelta),
                                                           last->alt + ((i+1)*altdelta),
                                                           last->lon + ((i+1)*londelta),
                                                           last->lat + ((i+1)*latdelta),
                                                           last->headwind + ((i+1)*hwdelta),
                                                           last->slope + ((i+1)*slopedelta),
                                                           last->temp + ((i+1)*temperaturedelta),
                                                           last->lrbalance + ((i+1)*lrbalancedelta),
                                                           last->lte + ((i+1)*ltedelta),
                                                           last->rte + ((i+1)*rtedelta),
                                                           last->lps + ((i+1)*lpsdelta),
                                                           last->rps + ((i+1)*rpsdelta),
                                                           last->lpco + ((i+1)*lpcodelta),
                                                           last->rpco + ((i+1)*rpcodelta),
                                                           last->lppb + ((i+1)*lppbdelta),
                                                           last->rppb + ((i+1)*rppbdelta),
                                                           last->lppe + ((i+1)*lppedelta),
                                                           last->rppe + ((i+1)*rppedelta),
                                                           last->lpppb + ((i+1)*lpppbdelta),
                                                           last->rpppb + ((i+1)*rpppbdelta),
                                                           last->lpppe + ((i+1)*lpppedelta),
                                                           last->rpppe + ((i+1)*rpppedelta),
                                                           last->smo2 + ((i+1)*smo2delta),
                                                           last->thb + ((i+1)*thbdelta),
                                                           last->rvert + ((i+1)*rvertdelta),
                                                           last->rcad + ((i+1)*rcaddelta),
                                                           last->rcontact + ((i+1)*rcontactdelta),
                                                           last->tcore + ((i+1)*tcoredelta),
                                                           last->interval);

                    ret->insertPoint(position++, add);
                }

            }
        }
        last = point;
    }
}

QString
PolarFlow::saveFile(QByteArray &array, QString exerciseId, QString filetype, QDateTime starttime, QString detailedSportInfo_Str)
{
    printd("start \n");
    printd("used exerciseId: %s \n", exerciseId.toStdString().c_str());
    printd("used filetype / fileSuffix: %s \n", filetype.toStdString().c_str());


    QString starttime_format;
    starttime_format = "yyyy_MM_dd_HH_mm_ss";
    QString starttime_str;
    starttime_str = starttime.toString(starttime_format);

    QString filename;
    filename = context->athlete->home->tmpActivities().canonicalPath() + "/" + starttime_str + "_" + detailedSportInfo_Str + "_"+ exerciseId  + "." + filetype;
    printd("filename - %s \n", filename.toStdString().c_str());

    //https://stackoverflow.com/questions/12988131/qt4-write-qbytearray-to-file-with-filename
    QSaveFile tempFile(filename);
    tempFile.open(QIODevice::WriteOnly);
    tempFile.write(array);
    tempFile.commit();

    printd("returnd filename %s \n", filename.toStdString().c_str());
    printd("end \n");
    return filename;
}

void
PolarFlow::ImportFiles(QJsonObject &object, QString exerciseId)
{
    /*
    printd("acquireAndImportFiles - Remotestring: %d \n", remoteid);
    //qDebug() << "acquireAndImportFiles - data:" << object;

    QList<QString> fileName_Lst; // for RideImportWizard
    QString fileName_Str;
    QString stype = object["sport"].toString();
    if (stype.endsWith("SWIMMING")) {
        fileName_Str = getFITBeta(exerciseId);
        getTCX(exerciseId);
        getGPX(exerciseId);
    }
    else {
        fileName_Str = saveTCX(exerciseId);
        getFITBeta(exerciseId);
        getGPX(exerciseId);
    }
        fileName_Lst.append(fileName_Str);

    printd("acquireAndImportFiles - filename_String: %s \n" << fileName_Str.toStdString().c_str());
    //qDebug() << "acquireAndImportFiles - filename_String-List: " << fileName_Lst;

// First: Take Infos from tcx-File
    // Start with Importfile - set isAutoImport
    //RideImportWizard *rideImportFile = new RideImportWizard(fileName_Lst, context);
                      //rideImportFile->process();
                      //rideImportFile->isAutoImport();
                      //rideImportFile->activateSave(); // does not work this way
                      //rideImportFile->autoImportMode; // does not work this way
                      //rideImportFile->autoImportStealth; // does not work this way
// Import of Ride is completed
    //delete rideImportFile;
    */
}

void
PolarFlow::addExerciseSummaryInformation(RideFile* ret, QJsonObject &object)
{
    printd("start \n");

    // Get start-time
    QDateTime starttime = QDateTime::fromString(object["start-time"].toString(), Qt::ISODate);

    // Get upload-time
    QDateTime uploadtime = QDateTime::fromString(object["upload-time"].toString(), Qt::ISODate);
        //rideImport->setTag("Upload Time", uploadtime);

    // Get device
    if (object["device"].toString().length()>0)
        ret->setDeviceType(object["device"].toString());
    else
        ret->setDeviceType("Polar"); // The device type is unknown

    if (object["device-id"].toString().length()>0)
        ret->setTag("Polar Device id", object["device-id"].toString());

    // Get start-time-utc-offset
    if (!object["start-time-utc-offset"].isNull()) {
        QJsonValue startTimeUtcOffset_Val = object.value("start-time-utc-offset");
        QString startTimeUtcOffset_Str = QString::number(startTimeUtcOffset_Val.toInt());
        ret->setTag("Polar Start Time Utc Offset", startTimeUtcOffset_Str);
    }

    // Get duration
    int duration_time = 0; // start value
    if (object["duration"].toString().length()>0) {
        QString duration_str = object["duration"].toString();        // "PT4H51M38.512S"
        duration_time = isoDateToSeconds(duration_str);
    }

    if (object["moving_time"].toDouble()>0) {
        QMap<QString,QString> map;
        map.insert("value", QString("%1").arg(object["moving_time"].toDouble()));
        ret->metricOverrides.insert("time_riding", map);
    }
    if (duration_time>0) { // elapsed time = Polar Duration
        QMap<QString,QString> map;
        map.insert("value", QString("%1").arg(duration_time));
        ret->metricOverrides.insert("workout_time", map);
    }

    // Get calories
    if (!object["calories"].isNull()) {
        QJsonValue calories_Val = object.value("calories");
        QString calories_Str = QString::number(calories_Val.toInt());
        //int calories = calories_Str.toInt();
        ret->setTag("Polar Calories", calories_Str);
    }

    // Get distance
    if (object["distance"].toDouble()>0) {
        QMap<QString,QString> map;
        map.insert("value", QString("%1").arg(object["distance"].toDouble()/1000.0));
        ret->metricOverrides.insert("total_distance", map);
    }

    // Get heart-rate average and maximum
    if (!object["heart-rate"].isNull()) {
        QJsonValue heartrate_Val = object.value(QString("heart-rate"));
        QJsonObject heartrate_Obj = heartrate_Val.toObject();
        //qDebug() << "trainingLoadPro: " << object["heart-rate"];
        //qDebug() << "heartrate_Value:" << heartrate_Val;
        //qDebug() << "heartrate_Obj:" << heartrate_Obj;

        if (!heartrate_Obj["average"].isNull()) {
            QJsonValue averageHr_Val = heartrate_Obj.value("average");
            QString averageHr = QString::number(averageHr_Val.toInt());
            //qDebug() << "averageHr_Val:" << averageHr_Val;
            //qDebug() << "averageHr:" << averageHr;
            QMap<QString,QString> map;
            map.insert("value", averageHr);
            //qDebug() << "mapHr:" << map;
            ret->metricOverrides.insert("Average Heart Rate", map);
        }
        if (!heartrate_Obj["maximum"].isNull()) {
            QJsonValue maximumHr_Val = heartrate_Obj.value("maximum");
            QString maximumHr = QString::number(maximumHr_Val.toInt());
            QMap<QString,QString> map;
            map.insert("value", maximumHr);
            ret->metricOverrides.insert("Maximum Heart Rate", map);
        }
    }

    // get training-load
    if (object["training-load"].toDouble()>0) {
        QJsonValue trainingLoad_Val = object.value("training-load");
        QString trainingLoad_Str = QString::number(trainingLoad_Val.toDouble());
        ret->setTag("Polar Training Load", trainingLoad_Str);
    }

    // Get sport - main
    if (!object["sport"].isNull()) {
        QString stype = object["sport"].toString();
        if (stype.endsWith("CYCLING")) ret->setTag("Sport", "Bike");
        else if (stype.endsWith("RUNNING")) ret->setTag("Sport", "Run");
        else if (stype.endsWith("SWIMMING")) ret->setTag("Sport", "Swim");
        else if (stype.endsWith("OTHER")) ret->setTag("Sport", "");
        else ret->setTag("Sport", stype);
        // Set SubSport to preserve the original when Sport was mapped
        if (stype != ret->getTag("Sport", "")) ret->setTag("SubSport", stype);
        //qDebug() << "prepareResponse - stype: " << stype;
    }

    // has route
    if (!object["has-route"].isNull()) {
        ret->setTag("Polar has Route", object["has-route"].toBool() ? "1" : "0");
    }

    // get club-id
    if (!object["club-id"].isNull()) {
        QJsonValue club_id_Val = object.value("club-id");
        QString club_id = QString::number(club_id_Val.toInt());
        ret->setTag("Polar Club Id", club_id);
    }

    // get club-name
    if (object["club-name"].isString()) {
        ret->setTag("Polar Club Name", object["club-name"].toString());
    }

    // get detailed-sport-info
    // https://www.polar.com/accesslink-api/?shell#detailed-sport-info-values-in-exercise-entity
    if (object["detailed-sport-info"].isString()) {
        QString detailedSportInfo = object["detailed-sport-info"].toString();
        QString seperated1; // first string
        QString seperated2;
        QString seperated3;
        QString seperated4;
        QString seperated5;
        QString seperated1FC; // first charater from first string
        QString seperated2FC;
        QString seperated3FC;
        QString seperated4FC;
        QString seperated5FC;
        QString seperated1UC; // first character uppercase
        QString seperated2UC;
        QString seperated3UC;
        QString seperated4UC;
        QString seperated5UC;

        QString searchFor = "_";
        QString replaceWith = " ";
        QString detailedSportInfoToLower;
        QString detailedSportInfoToLowerFCUC; // detailed sports info with first charater uppercase
        /*
        // this version separates the "sections" of the string
        int j = 0; // counting variable
        int stringsize = detailedSportInfo.size();
        int before = 0;
        int after = 0;
        // search for divider "_", we assume there is only one "_"
        // this solution works for one underline in the string

        while ((j = detailedSportInfo.indexOf(searchFor, j)) != -1) {
            before = j - 0;
            after = stringsize - (j + 1);
            printd("Found _ at index position: %d \n", j);
            printd("before: %d \n", before);
            printd("after: %d \n", after);
            ++j;
        }
        QString detailedSportInfoLeft = detailedSportInfo.left(before).toLower();
        QString detailedSportInfoRight = detailedSportInfo.right(after).toLower();
        if (before <= 0) {
                detailedSportInfoToLower = detailedSportInfo.toLower();
        }
        if ( before > 0) {
                detailedSportInfoToLower = detailedSportInfoLeft+" "+detailedSportInfoRight;
        }
        */
        detailedSportInfoToLower = detailedSportInfo.replace(searchFor,replaceWith).toLower();
        printd("detailed sport info original: %s \n", detailedSportInfo.toStdString().c_str());

        // First Character should be Uppercase
        QString::SectionFlag emptyParts = QString::SectionSkipEmpty; //skip empty parts
        seperated1 = detailedSportInfoToLower.section(' ', 0, 0, emptyParts);
        seperated1FC = seperated1.left(1);
        seperated2 = detailedSportInfoToLower.section(' ', 1, 1, emptyParts);
        seperated2FC = seperated2.left(1);
        seperated3 = detailedSportInfoToLower.section(' ', 2, 2, emptyParts);
        seperated3FC = seperated3.left(1);
        seperated4 = detailedSportInfoToLower.section(' ', 3, 3, emptyParts);
        seperated4FC = seperated4.left(1);
        seperated5 = detailedSportInfoToLower.section(' ', 4, 4, emptyParts);
        seperated5FC = seperated5.left(1);
        //printd("detailed sport info seperated first string: %s \n", seperated1.toStdString().c_str());
        //printd("detailed sport info seperated first character from first string: %s \n", seperated1FC.toStdString().c_str());
        //printd("detailed sport info seperated second string: %s \n", seperated2.toStdString().c_str());
        //printd("detailed sport info seperated second character from first string: %s \n", seperated2FC.toStdString().c_str());
        //printd("detailed sport info seperated third string: %s \n", seperated3.toStdString().c_str());
        //printd("detailed sport info seperated third character from first string: %s \n", seperated3FC.toStdString().c_str());
        //printd("detailed sport info seperated fourth string: %s \n", seperated4.toStdString().c_str());
        //printd("detailed sport info seperated fourth character from first string: %s \n", seperated4FC.toStdString().c_str());
        //printd("detailed sport info seperated fith string: %s \n", seperated5.toStdString().c_str());
        //printd("detailed sport info seperated fith character from first string: %s \n", seperated5FC.toStdString().c_str());

        seperated1UC = seperated1.replace(0, 1, seperated1FC.toUpper());
        seperated2UC = seperated2.replace(0, 1, seperated2FC.toUpper());
        seperated3UC = seperated3.replace(0, 1, seperated3FC.toUpper());
        seperated4UC = seperated4.replace(0, 1, seperated4FC.toUpper());
        seperated5UC = seperated5.replace(0, 1, seperated5FC.toUpper());
        //printd("detailed sport info seperated first character from first string uppercase: %s \n", seperated1UC.toStdString().c_str());
        //printd("detailed sport info seperated second character from first string uppercase: %s \n", seperated2UC.toStdString().c_str());
        //printd("detailed sport info seperated third character from first string uppercase: %s \n", seperated3UC.toStdString().c_str());
        //printd("detailed sport info seperated fourth character from first string uppercase: %s \n", seperated4UC.toStdString().c_str());
        //printd("detailed sport info seperated fith character from first string uppercase: %s \n", seperated5UC.toStdString().c_str());

        // create one String without whitespaces
        detailedSportInfoToLowerFCUC = (seperated1UC + " " + seperated2UC + " " + seperated3UC + " " + seperated4UC + " " + seperated5UC).simplified();
        //printd("detailed sport info FCUC: %s \n", detailedSportInfoToLowerFCUC.toStdString().c_str());

        //printd("detailed sport info number of chars divider: %d \n", j);
        //printd("detailed sport info number of chars before: %d \n", before);
        //printd("detailed sport info number of chars after: %d \n", after);
        //printd("detailed sport info left: %s \n", detailedSportInfoLeft.toStdString().c_str());
        //printd("detailed sport info right: %s \n", detailedSportInfoRight.toStdString().c_str());
        //printd("detailed sport info: %s \n", detailedSportInfoToLower.toStdString().c_str());
        //ret->setTag("SubSport", detailedSportInfoToLower);
        ret->setTag("SubSport", detailedSportInfoToLowerFCUC);
    }

    // get fat-percentage
    if (!object["fat-percentage"].isNull()) {
        QJsonValue fatPercentage_Val = object.value("fat-percentage");
        QString fatPercentage = QString::number(fatPercentage_Val.toInt());
        ret->setTag("Polar Fat (%)", fatPercentage);
    }

    // get carbohydrate-percentage
    if (!object["carbohydrate-percentage"].isNull()) {
        QJsonValue carbohydratePercentage_Val = object.value("carbohydrate-percentage");
        QString carbohydratePercentage = QString::number(carbohydratePercentage_Val.toInt());
        ret->setTag("Polar Carbohydrate (%)", carbohydratePercentage);
    }

    // get protein-percentage
    if (!object["protein-percentage"].isNull()) {
        QJsonValue proteinPercentage_Val = object.value("protein-percentage");
        QString proteinPercentage = QString::number(proteinPercentage_Val.toInt());
        ret->setTag("Polar Protein (%)", proteinPercentage);
    }

    // get running-index
    if (!object["running-index"].isNull()) {
        QJsonValue runningIndex_Val = object.value("running-index");
        QString runningIndex = QString::number(runningIndex_Val.toInt());
        ret->setTag("Polar Running Index", runningIndex);
    }

    // Get training-load-pro 19.03.2023
    if (!object["training-load-pro"].isNull()) {
        QJsonValue trainingLoadPro_Val = object.value(QString("training-load-pro"));
        QJsonObject trainingLoadPro_Obj = trainingLoadPro_Val.toObject();
        //qDebug() << "trainingLoadPro: " << object["training-load-pro"];
        //qDebug() << "trainingLoadPro_Val: " << trainingLoadPro_Val;
        //qDebug() << "trainingLoadPro_Obj: " << trainingLoadPro_Obj;

        if (trainingLoadPro_Obj["date"].isString()) {
            QJsonValue date_Val = trainingLoadPro_Obj.value("date");
            QString datestring = date_Val.toString();
            QString dateformat = "yyyy-MM-dd";
            QDate tlpdate = QDate::fromString(datestring, dateformat);
            ret->setTag("Polar TLP Date", tlpdate.toString());
            //printd("TLP %s\n", datestring);
            //qDebug() << "date_Val " << date_Val;
            //qDebug() << "datestring " << datestring;
            //qDebug() << "dateformat " << dateformat;
            //qDebug() << "tlpdate " << tlpdate;
        }
        if (trainingLoadPro_Obj["cardio-load"].toDouble()>0) {
            QJsonValue cardioLoad_Val = trainingLoadPro_Obj.value("cardio-load");
            QString cardioLoad = QString::number(cardioLoad_Val.toDouble());
            ret->setTag("Polar TLP Cardio Load", cardioLoad);
            //printd("TLP %s\n", cardioLoad_Val);
            //qDebug() << "cardioLoad " << cardioLoad_Val;
            //qDebug() << "cardioLoad " << cardioLoad;
        }
        if (trainingLoadPro_Obj["muscle-load"].toDouble()>0) {
            QJsonValue muscleLoad_Val = trainingLoadPro_Obj.value("muscle-load");
            QString muscleLoad = QString::number(muscleLoad_Val.toDouble());
            ret->setTag("Polar TLP Muscle Load", muscleLoad);
        }
        if (trainingLoadPro_Obj["perceived-load"].toDouble()>0) {
            QJsonValue perceivedLoad_Val = trainingLoadPro_Obj.value("perceived-load");
            QString perceivedLoad = QString::number(perceivedLoad_Val.toDouble());
            ret->setTag("Polar TLP Perceived Load", perceivedLoad);
        }
        /*
        Training Load Pro Interpretation Posibilities
        Property  Value            GC
        anonymous UNKNOWN           0?
        anonymous VERY_LOW          1?
        anonymous LOW               2?
        anonymous MEDIUM            3?
        anonymous HIGH              4?
        anonymous VERY_HIGH         5?
        anonymous NOT_AVAILABLE    -1?!?
        */
        if (trainingLoadPro_Obj["cardio-load-interpretation"].isString()) {
            QJsonValue cardioLoad_Val = trainingLoadPro_Obj.value("cardio-load-interpretation");
            QString cardioLoad = cardioLoad_Val.toString();
            ret->setTag("Polar TLP Cardio Load Interpretation", cardioLoad);
            qDebug() << "cardioLoad Int" << cardioLoad_Val;
            qDebug() << "cardioLoad Int" << cardioLoad;
        }
        if (trainingLoadPro_Obj["muscle-load-interpretation"].isString()) {
            QJsonValue muscleLoad_Val = trainingLoadPro_Obj.value("muscle-load-interpretation");
            QString muscleLoad = muscleLoad_Val.toString();
            ret->setTag("Polar TLP Muscle Load Interpretation", muscleLoad);
        }
        if (trainingLoadPro_Obj["perceived-load-interpretation"].isString()) {
            QJsonValue perceivedLoad_Val = trainingLoadPro_Obj.value("perceived-load-interpretation");
            QString perceivedLoad = perceivedLoad_Val.toString();
            ret->setTag("Polar TLP Perceived Load Interpretation", perceivedLoad);
        }
        /*
         Training Load Pro User RPE Posibilities
         Property  Value                GC RPE
         anonymous UNKNOWN               0
         anonymous RPE_NONE              1
         anonymous RPE_EASY              2
         anonymous RPE_LIGHT             3
         anonymous RPE_FAIRLY_BRISK      4
         anonymous RPE_BRISK             5
         anonymous RPE_MODERATE          6
         anonymous RPE_FAIRLY_HARD       7
         anonymous RPE_HARD              8
         anonymous RPE_EXHAUSTING        9
         anonymous RPE_EXTREME          10
        */
        if (trainingLoadPro_Obj["user-rpe"].isString()) {
            QJsonValue tlpUserRpe_Val = trainingLoadPro_Obj.value("user-rpe");
            QString tlpUserRpe = tlpUserRpe_Val.toString();
            ret->setTag("Polar TLP User RPE", tlpUserRpe);
            if (tlpUserRpe == "RPE_NONE") {
                ret->setTag("RPE", "1");
            }
            if (tlpUserRpe == "RPE_EASY") {
                ret->setTag("RPE", "2");
            }
            if (tlpUserRpe == "RPE_LIGHT") {
                ret->setTag("RPE", "3");
            }
            if (tlpUserRpe == "RPE_FAIRLY_BRISK") {
                ret->setTag("RPE", "4");
            }
            if (tlpUserRpe == "RPE_BRISK") {
                ret->setTag("RPE", "5");
            }
            if (tlpUserRpe == "RPE_MODERATE") {
                ret->setTag("RPE", "6");
            }
            if (tlpUserRpe == "RPE_FAIRLY_HARD") {
                ret->setTag("RPE", "7");
            }
            if (tlpUserRpe == "RPE_HARD") {
                ret->setTag("RPE", "8");
            }
            if (tlpUserRpe == "RPE_EXHAUSTING") {
                ret->setTag("RPE", "9");
            }
            if (tlpUserRpe == "RPE_EXTREME") {
                ret->setTag("RPE", "10");
            }
            //if (tlpUserRpe == "UNKNOWN") {
            else {
                ret->setTag("RPE", "0");
            }
        }
    }
    printd("end \n");
}


// development put on hold - AccessLink API compatibility issues with Desktop applications
#if 0
static bool addPolarFlow() {
    CloudServiceFactory::instance().addService(new PolarFlow(NULL));
    return true;
}

static bool add = addPolarFlow();
#endif
