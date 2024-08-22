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

#ifndef GC_PolarFlowByTransaction_h
#define GC_PolarFlowByTransaction_h

#include "CloudService.h"
#include "PolarFlow.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QImage>

// OAuth domain (requires client id)
//#define GC_POLARFLOW_OAUTH_URL "https://flow.polar.com/oauth2/authorization"

// Access token (requires client id and secret)
//#define GC_POLARFLOW_TOKEN_URL "https://polarremote.com/v2/oauth2/token"

// Request URL (requires access token)
//#define GC_POLARFLOW_URL "https://www.polaraccesslink.com"

class PolarFlowBT : public PolarFlow {
    Q_OBJECT

public:
    PolarFlowBT(Context *context);
    CloudService *clone(Context *context) { return new PolarFlowBT(context); }
    ~PolarFlowBT();

    QString id() const { return "Polar Flow BT"; }
    QString uiName() const { return tr("Polar Flow By Transaction"); }
    QString description() const { return (tr("Download from the popular Polar website.")); }
    QImage logo() const { return QImage(":images/services/polarflow.png"); }

    // open/connect and close/disconnect transaction
    bool open(QStringList &errors); // open transaction
    bool commit(QStringList &errors); // commit transaction
    bool close();

    // polar has more types of service - NOT Default
    //virtual int type() { return Activities | Measures; }

    // polar capabilities of service - NOT Default
    virtual int capabilities() const { return OAuth | Download | Query; }

    // read a file
    bool readFile(QByteArray *data, QString remotename, QString remoteid); //

    // write a file
    //bool writeFile(QByteArray &data, QString remotename, RideFile *ride); //

    void addSamples(RideFile* ret, QString exerciseId); // Get available samples and Get Samples

    QString getFile(QString exerciseId, QString filetype, QDateTime starttime, QString detailedSportInfo_Str); // Get Data File

    // populate exercise list
    QList<CloudServiceEntry*> readdir(QString path, QStringList &errors); // CloudService.h 171 -> not implemented
    //QList<CloudServiceEntry*> readdir(QString path, QStringList &errors, QDateTime from, QDateTime to);

public slots:

    // getting data
    void readyRead(); //
    void readFileCompleted(); //

    // sending data - NOT for now
    //void writeFileCompleted(); //

private:
    Context *context;
    QNetworkAccessManager *nam;
    QNetworkReply *reply;

    QMap<QNetworkReply*, QByteArray*> buffers;

    QByteArray* prepareResponse(QByteArray* data, QString &name); // Get exercise summary

private slots:
    void onSslErrors(QNetworkReply *reply, const QList<QSslError>&error);

};
#endif
