/***************************************************************************
 *  The FreeMedForms project is a set of free, open source medical         *
 *  applications.                                                          *
 *  (C) 2008-2012 by Eric MAEKER, MD (France) <eric.maeker@gmail.com>      *
 *  All rights reserved.                                                   *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program (COPYING.FREEMEDFORMS file).                   *
 *  If not, see <http://www.gnu.org/licenses/>.                            *
 ***************************************************************************/
/***************************************************************************
 *   Main developers : Eric MAEKER, <eric.maeker@gmail.com>                *
 *   Contributors :                                                        *
 *       NAME <MAIL@ADDRESS.COM>                                           *
 *       NAME <MAIL@ADDRESS.COM>                                           *
 ***************************************************************************/
/**
  \class Patients::PatientModel
  \brief All Patients' accessible data are provided by this model.
  The model creates and manages the Core::IPatient model wrapper.
  \sa Core::IMode, Core::IPatient
*/

#include "patientmodel.h"
#include "patientbase.h"
#include "constants_db.h"
#include "constants_settings.h"
#include "constants_trans.h"

#include <coreplugin/icore.h>
#include <coreplugin/isettings.h>
#include <coreplugin/itheme.h>
#include <coreplugin/ipatient.h>
#include <coreplugin/iuser.h>
#include <coreplugin/constants_icons.h>
#include <coreplugin/ipatientlistener.h>
#include <coreplugin/constants_tokensandsettings.h>

#include <utils/log.h>
#include <utils/global.h>
#include <medicalutils/global.h>
#include <translationutils/constanttranslations.h>
#include <extensionsystem/pluginmanager.h>

#include <QObject>
#include <QSqlTableModel>
#include <QSqlDatabase>
#include <QPixmap>
#include <QBuffer>
#include <QByteArray>

enum { WarnDatabaseFilter = false };

using namespace Patients;
using namespace Trans::ConstantTranslations;

static inline ExtensionSystem::PluginManager *pluginManager() { return ExtensionSystem::PluginManager::instance(); }
static inline Patients::Internal::PatientBase *patientBase() {return Patients::Internal::PatientBase::instance();}
static inline Core::IUser *user() {return Core::ICore::instance()->user();}
static inline Core::ISettings *settings()  { return Core::ICore::instance()->settings(); }
static inline Core::ITheme *theme()  { return Core::ICore::instance()->theme(); }

namespace Patients {
namespace Internal {


class PatientModelPrivate
{
public:
    PatientModelPrivate(PatientModel *parent) :
        m_SqlPatient(0),
        m_SqlPhoto(0),
        m_EmitCreationAtSubmit(false),
        q(parent)
    {
    }

    ~PatientModelPrivate ()
    {
        if (m_SqlPatient) {
            delete m_SqlPatient;
            m_SqlPatient = 0;
        }
        if (m_SqlPhoto) {
            delete m_SqlPhoto;
            m_SqlPhoto = 0;
        }
//        if (m_PatientWrapper) {
//            delete m_PatientWrapper;
//            m_PatientWrapper = 0;
//        }
    }

    void refreshFilter()
    {
        // TODO: filter photo SQL as well

        // WHERE (LK_ID IN (SELECT LK_ID FROM LK_TOPRACT WHERE LK_PRACT_UID='...')) OR
        //       (LK_ID IN (SELECT LK_ID FROM LK_TOPRACT WHERE LK_GROUP_UID='...'))

        // Manage virtual patients
        QHash<int, QString> where;
        if (!settings()->value(Core::Constants::S_ALLOW_VIRTUAL_DATA, true).toBool())
            where.insert(Constants::IDENTITY_ISVIRTUAL, "=0");

        // All users share the same patients
//        where.insert(Constants::IDENTITY_LK_TOPRACT_LKID, QString("IN (%1)").arg(m_LkIds));
        where.insert(Constants::IDENTITY_ISACTIVE, "=1");

        QString filter = patientBase()->getWhereClause(Constants::Table_IDENT, where);

        if (!m_ExtraFilter.isEmpty())
            filter += QString(" AND (%1)").arg(m_ExtraFilter);

        filter += QString(" ORDER BY lower(`%1`) ASC").arg(patientBase()->fieldName(Constants::Table_IDENT, Constants::IDENTITY_BIRTHNAME));

        if (WarnDatabaseFilter)
            LOG_FOR(q, "Filtering patient database with: " + filter);

        m_SqlPatient->setFilter(filter);
        m_SqlPatient->select();

//        qWarning() << m_SqlPatient->query().lastQuery();

//        q->reset();
    }

    QIcon iconizedGender(const QModelIndex &index)
    {
        //TODO: put this in a separate method/class, there is much duplication of gender (de)referencing in FMF
        const QString &g = m_SqlPatient->data(m_SqlPatient->index(index.row(), Constants::IDENTITY_GENDER)).toString();
        if (g=="M") {
            return theme()->icon(Core::Constants::ICONMALE);
        } else if (g=="F") {
            return theme()->icon(Core::Constants::ICONFEMALE);
        } else if (g=="H") {
            return theme()->icon(Core::Constants::ICONHERMAPHRODISM);
        }
        return QIcon();
    }

    bool savePatientPhoto(const QPixmap &pix, const QString &patientUid)
    {
        if (pix.isNull() || patientUid.isEmpty())
            return false;

        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        pix.save(&buffer, "PNG"); // writes image into ba in PNG format {6a247e73-c241-4556-8dc8-c5d532b8457e}

        // need creation or update ?
        QHash<int, QString> where;
        where.insert(Constants::PHOTO_PATIENT_UID, QString("='%1'").arg(patientUid));
        bool create = patientBase()->count(Constants::Table_PATIENT_PHOTO, Constants::PHOTO_PATIENT_UID, patientBase()->getWhereClause(Constants::Table_PATIENT_PHOTO, where)) == 0;
        QSqlDatabase DB = patientBase()->database();
        DB.transaction();
        QSqlQuery query(DB);
        QString req;
        if (create) {
            req = patientBase()->prepareInsertQuery(Constants::Table_PATIENT_PHOTO);
            query.prepare(req);
            query.bindValue(Constants::PHOTO_ID, QVariant());
            query.bindValue(Constants::PHOTO_UID, patientUid);
            query.bindValue(Constants::PHOTO_PATIENT_UID, patientUid);
            query.bindValue(Constants::PHOTO_BLOB, ba);
        } else {
            req = patientBase()->prepareUpdateQuery(Constants::Table_PATIENT_PHOTO, Constants::PHOTO_BLOB, where);
            query.prepare(req);
            query.bindValue(0, ba);
        }
        if (!query.exec()) {
            LOG_QUERY_ERROR_FOR(q, query);
            query.finish();
            DB.rollback();
            return false;
        }
        query.finish();
        DB.commit();
        return true;
    }

    QPixmap getPatientPhoto(const QModelIndex &index)
    {
        QHash<int, QString> where;
        QString patientUid = m_SqlPatient->index(index.row(), Constants::IDENTITY_UID).data().toString();

        where.insert(Constants::PHOTO_PATIENT_UID, QString("='%1'").arg(patientUid));
        if (patientBase()->count(Constants::Table_PATIENT_PHOTO, Constants::PHOTO_PATIENT_UID,
                                 patientBase()->getWhereClause(Constants::Table_PATIENT_PHOTO, where)) == 0) {
            return QPixmap();
        }

        QSqlDatabase DB = patientBase()->database();
        DB.transaction();
        QSqlQuery query(DB);
        QString req = patientBase()->select(Constants::Table_PATIENT_PHOTO, Constants::PHOTO_BLOB, where);
        if (!query.exec(req)) {
            LOG_QUERY_ERROR_FOR(q, query);
            query.finish();
            DB.rollback();
            return QPixmap();
        } else {
            if (query.next()) {
                QPixmap pix;
                pix.loadFromData(query.value(0).toByteArray());
                query.finish();
                DB.commit();
                return pix;
            }
        }
        query.finish();
        DB.commit();
        return QPixmap();
    }


public:
    QSqlTableModel *m_SqlPatient, *m_SqlPhoto;
    QString m_ExtraFilter;
    QString m_LkIds;
    QString m_UserUuid;
    QStringList m_CreatedPatientUid;
    bool m_EmitCreationAtSubmit;

private:
    PatientModel *q;
};

} // end namespace Internal
} // end namespace Patients

PatientModel *PatientModel::m_ActiveModel = 0;

PatientModel::PatientModel(QObject *parent) :
        QAbstractTableModel(parent), d(new Internal::PatientModelPrivate(this))
{
    setObjectName("PatientModel");

    onCoreDatabaseServerChanged();

    connect(Core::ICore::instance(), SIGNAL(databaseServerChanged()), this, SLOT(onCoreDatabaseServerChanged()));
}

PatientModel::~PatientModel()
{
    if (d) {
        delete d;
        d=0;
    }
}

void PatientModel::changeUserUuid()
{
    d->m_UserUuid = user()->uuid();
    QList<int> ids = QList<int>() << user()->value(Core::IUser::PersonalLinkId).toInt();
    d->m_LkIds.clear();
    foreach(int i, ids)
        d->m_LkIds.append(QString::number(i) + ",");
    d->m_LkIds.chop(1);

//    qWarning() << Q_FUNC_INFO << d->m_LkIds << userModel()->practionnerLkIds(uuid);

    d->refreshFilter();
}

void PatientModel::onCoreDatabaseServerChanged()
{
    // Destroy old model and recreate it
    if (d->m_SqlPatient) {
        disconnect(d->m_SqlPatient);
        delete d->m_SqlPatient;
    }
    d->m_SqlPatient = new QSqlTableModel(this, patientBase()->database());
    d->m_SqlPatient->setTable(patientBase()->table(Constants::Table_IDENT));
    Utils::linkSignalsFromFirstModelToSecondModel(d->m_SqlPatient, this, false);

    if (d->m_SqlPhoto)
        delete d->m_SqlPhoto;
    d->m_SqlPhoto = new QSqlTableModel(this, patientBase()->database());
    d->m_SqlPhoto->setTable(patientBase()->table(Constants::Table_PATIENT_PHOTO));
    d->refreshFilter();
}

/**
  * \brief Sets the index to the given patient QModelIndex.
  *
  * Before changing to the new patient, the plugin extension Core::IPatientListener->currentPatientAboutToChange()
  * is called to enable plugins to e.g. save data before changing to the new patient.
  *
  * Two new signals \e currentPatientChanged() and currentPatientChanged(QModelIndex) are emitted when the new current patient
  * is set. If the new patient is the current one, no signals are emitted.
  *
  * \sa Core::IPatient::currentPatientChanged()
 */
void PatientModel::setCurrentPatient(const QModelIndex &index)
{
    if (index == m_CurrentPatient) {
        return;
    }

    // Call all extensions that provide listeners to patient change: the extensions can now do things like
    // save data BEFORE the patient is changed.
    QList<Core::IPatientListener *> listeners = pluginManager()->getObjects<Core::IPatientListener>();
    for(int i = 0; i < listeners.count(); ++i) {
        if (!listeners.at(i)->currentPatientAboutToChange()) {
            return;
        }
    }

    m_CurrentPatient = index;
    LOG("setCurrentPatient: " + this->index(index.row(), Core::IPatient::Uid).data().toString());
    Q_EMIT currentPatientChanged(this->index(index.row(), Core::IPatient::Uid).data().toString());
    Q_EMIT currentPatientChanged(index);
}

int PatientModel::rowCount(const QModelIndex &parent) const
{
    // prevent trees
    if (parent.isValid()) return 0;

    return d->m_SqlPatient->rowCount();
}

int PatientModel::columnCount(const QModelIndex &) const
{
    return Core::IPatient::NumberOfColumns;
}

int PatientModel::numberOfFilteredPatients() const
{
    return patientBase()->count(Constants::Table_IDENT, Constants::IDENTITY_BIRTHNAME, d->m_SqlPatient->filter());
}

bool PatientModel::hasChildren(const QModelIndex &) const
{
    return false;
}

QVariant PatientModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role==Qt::DecorationRole) {
        using namespace Core;
        if (index.column() == IPatient::BirthName) {
            return d->iconizedGender(index);
        }
    } else if (role==Qt::DisplayRole || role==Qt::ToolTipRole || role==Qt::EditRole) {
        using namespace Core;
        int col = -1;
        switch (index.column()) {
        case IPatient::UsersUidList:  break;
        case IPatient::GroupsUidList: break;
        case IPatient::Id:            col = Constants::IDENTITY_ID;         break;
        case IPatient::Uid:           col = Constants::IDENTITY_UID;        break;
        case IPatient::FamilyUid:     col = Constants::IDENTITY_FAMILY_UID; break;
        case IPatient::BirthName:     col = Constants::IDENTITY_BIRTHNAME;       break;
        case IPatient::SecondName:    col = Constants::IDENTITY_SECONDNAME;        break;
        case IPatient::Firstname:     col = Constants::IDENTITY_FIRSTNAME;           break;
        case IPatient::Gender:        col = Constants::IDENTITY_GENDER;            break;
        case IPatient::GenderIndex:
            {
            // TODO: put this in a separate method/class, there is much duplication of gender (de)referencing in FMF
                const QString &g = d->m_SqlPatient->index(index.row(), Constants::IDENTITY_GENDER).data().toString();
                if (g=="M")
                    return 0;
                if (g=="F")
                    return 1;
                if (g=="H")
                    return 2;
                return -1;
            }
        case IPatient::DateOfBirth:
        {
            QModelIndex idx = d->m_SqlPatient->index(index.row(), Constants::IDENTITY_DOB);
            QDate dob = d->m_SqlPatient->data(idx).toDate();
            if (role==Qt::DisplayRole) {
                return dob;
            } else if (role==Qt::ToolTipRole) {
                return QString("%1; %2").arg(QLocale().toString(dob, QLocale().dateFormat(QLocale::LongFormat))).arg(MedicalUtils::readableAge(dob));
            }
            return dob;
            break;
        }
        case IPatient::MaritalStatus: col = Constants::IDENTITY_MARITAL_STATUS;    break;
        case IPatient::DateOfDeath:   col = Constants::IDENTITY_DATEOFDEATH;       break;
        case IPatient::Profession:    col = Constants::IDENTITY_PROFESSION;        break;
        case IPatient::Street:        col = Constants::IDENTITY_ADDRESS_STREET;    break;
        case IPatient::ZipCode:       col = Constants::IDENTITY_ADDRESS_ZIPCODE;   break;
        case IPatient::City:          col = Constants::IDENTITY_ADRESS_CITY;       break;
        case IPatient::Country:       col = Constants::IDENTITY_ADDRESS_COUNTRY;   break;
        case IPatient::AddressNote:   col = Constants::IDENTITY_ADDRESS_NOTE;      break;
        case IPatient::Mails:         col = Constants::IDENTITY_MAILS;             break;
        case IPatient::Tels:          col = Constants::IDENTITY_TELS;              break;
        case IPatient::Faxes:         col = Constants::IDENTITY_FAXES;             break;
        case IPatient::TitleIndex :   col = Constants::IDENTITY_TITLE;            break;
        case IPatient::Title:
            {
                col = Constants::IDENTITY_TITLE;
                int titleIndex = d->m_SqlPatient->data(d->m_SqlPatient->index(index.row(), col)).toInt();
                switch (titleIndex) {
                case Trans::Constants::Mister:    return tkTr(Trans::Constants::MISTER); break;
                case Trans::Constants::Miss :     return tkTr(Trans::Constants::MISS); break;
                case Trans::Constants::Madam :    return tkTr(Trans::Constants::MADAM); break;
                case Trans::Constants::Doctor :   return tkTr(Trans::Constants::DOCTOR); break;
                case Trans::Constants::Professor: return tkTr(Trans::Constants::PROFESSOR); break;
                case Trans::Constants::Captain :  return tkTr(Trans::Constants::CAPTAIN); break;
                default :       return QString();
                }
                return QString();
            }
        case IPatient::FullName:
            {
                const QString &name = d->m_SqlPatient->data(d->m_SqlPatient->index(index.row(), Constants::IDENTITY_BIRTHNAME)).toString();
                const QString &sec = d->m_SqlPatient->data(d->m_SqlPatient->index(index.row(), Constants::IDENTITY_SECONDNAME)).toString();
                const QString &first = d->m_SqlPatient->data(d->m_SqlPatient->index(index.row(), Constants::IDENTITY_FIRSTNAME)).toString();
                QString title;
                // add title
                int titleIndex = d->m_SqlPatient->data(d->m_SqlPatient->index(index.row(), Constants::IDENTITY_TITLE)).toInt();
                switch (titleIndex) {
                case Trans::Constants::Mister:    title = tkTr(Trans::Constants::MISTER); break;
                case Trans::Constants::Miss :     title = tkTr(Trans::Constants::MISS); break;
                case Trans::Constants::Madam :    title = tkTr(Trans::Constants::MADAM); break;
                case Trans::Constants::Doctor :   title = tkTr(Trans::Constants::DOCTOR); break;
                case Trans::Constants::Professor: title = tkTr(Trans::Constants::PROFESSOR); break;
                case Trans::Constants::Captain :  title = tkTr(Trans::Constants::CAPTAIN); break;
                }
                if (!title.isEmpty())
                    title.append(" ");

                if (!sec.isEmpty()) {
                    return QString("%1 - %2 %3").arg(title+name, sec, first);
                } else {
                    return QString("%1 %2").arg(title+name, first);
                }
                break;
            }
        case IPatient::FullAddress:
            {
                const QString &street = d->m_SqlPatient->data(d->m_SqlPatient->index(index.row(), Constants::IDENTITY_ADDRESS_STREET)).toString();
                const QString &city = d->m_SqlPatient->data(d->m_SqlPatient->index(index.row(), Constants::IDENTITY_ADRESS_CITY)).toString();
                const QString &zip = d->m_SqlPatient->data(d->m_SqlPatient->index(index.row(), Constants::IDENTITY_ADDRESS_ZIPCODE)).toString();
                QString country = d->m_SqlPatient->data(d->m_SqlPatient->index(index.row(), Constants::IDENTITY_ADDRESS_COUNTRY)).toString();
                country = Utils::countryIsoToName(country);
                return QString("%1 %2 %3 %4").arg(street, city, zip, country).simplified();
            }
        case IPatient::Age:
        {
            QModelIndex idx = d->m_SqlPatient->index(index.row(), Constants::IDENTITY_DOB);
            return MedicalUtils::readableAge(d->m_SqlPatient->data(idx).toDate());
        }
        case IPatient::YearsOld:
        {
            QModelIndex idx = d->m_SqlPatient->index(index.row(), Constants::IDENTITY_DOB);
            return MedicalUtils::ageYears(d->m_SqlPatient->data(idx).toDate());
        }
        case IPatient::IconizedGender: return d->iconizedGender(index);
        case IPatient::GenderPixmap: return d->iconizedGender(index).pixmap(16,16);
        case IPatient::Photo_32x32 :
        {
            QPixmap pix = d->getPatientPhoto(index);
            if (pix.isNull())
                return pix;
            if (pix.size()==QSize(32,32)) {
                return pix;
            }
            return pix.scaled(QSize(32,32));
        }
        case IPatient::Photo_64x64 :
        {
            QPixmap pix = d->getPatientPhoto(index);
            if (pix.isNull())
                return pix;
            if (pix.size()==QSize(64,64)) {
                return pix;
            }
            return pix.scaled(QSize(64,64));
        }
        case IPatient::PractitionnerLkID: return d->m_LkIds;
        }
        QVariant r = d->m_SqlPatient->data(d->m_SqlPatient->index(index.row(), col), role);
        switch (index.column()) {
        case Core::IPatient::DateOfBirth:
        case Core::IPatient::DateOfDeath:
            return QLocale().toString(r.toDate(), tkTr(Trans::Constants::DATEFORMAT_FOR_MODEL));
        default:
            return r;
        }
    }
    else if (role==Qt::DecorationRole) {
        switch (index.column()) {
        case Core::IPatient::IconizedGender: return d->iconizedGender(index);
        }
    } else if (role==Qt::BackgroundRole) {
        if (settings()->value(Constants::S_SELECTOR_USEGENDERCOLORS).toBool()) {
            const QString &g = d->m_SqlPatient->data(d->m_SqlPatient->index(index.row(), Constants::IDENTITY_GENDER)).toString();
            if (g=="M") {
                return Constants::maleColor;
            } else if (g=="F") {
                return Constants::femaleColor;
            } else if (g=="H") {
                return Constants::hermaColor;
            }
        }
    }
    return QVariant();
}

bool PatientModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    if (role == Qt::EditRole) {
        // TODO: manage patient duplicates when modifying names/uuid
        using namespace Core;
        int col = -1;
        QList<int> colsToEmit;
        colsToEmit << index.column();

        switch (index.column()) {
        case IPatient::UsersUidList:  break;
        case IPatient::GroupsUidList: break;
        case IPatient::Id :           col = Constants::IDENTITY_ID;               break;
        case IPatient::Uid:           col = Constants::IDENTITY_UID;              break;
        case IPatient::FamilyUid:     col = Constants::IDENTITY_FAMILY_UID;       break;
        case IPatient::BirthName:
        {
            col = Constants::IDENTITY_BIRTHNAME;
            colsToEmit << Core::IPatient::FullName;
            break;
        }
        case IPatient::SecondName:
        {
            col = Constants::IDENTITY_SECONDNAME;
            colsToEmit << Core::IPatient::FullName;
            break;
        }
        case IPatient::Firstname:
        {
            col = Constants::IDENTITY_FIRSTNAME;
            colsToEmit << Core::IPatient::FullName;
            break;
        }
        case IPatient::GenderIndex:
        {
            col = Constants::IDENTITY_GENDER;
            QString g;
            switch (value.toInt()) {
            case 0: g = "M"; break;
            case 1: g = "F"; break;
            case 2: g = "H"; break;
            }

            // value not changed ? -> return
            if (d->m_SqlPatient->index(index.row(), Constants::IDENTITY_GENDER).data().toString() == g)
                return true;

            d->m_SqlPatient->setData(d->m_SqlPatient->index(index.row(), Constants::IDENTITY_GENDER), g, role);
            col=-1;
            colsToEmit << Core::IPatient::Gender << Core::IPatient::GenderPixmap << Core::IPatient::Photo_32x32 << Core::IPatient::Photo_64x64;
            break;
        }
        case IPatient::Gender:
        {
            const QString &g = value.toString();
            QString toSave;
            switch (genders().indexOf(g)) {
            case 0 : toSave = "M"; break;
            case 1 : toSave = "F"; break;
            case 2:  toSave = "H"; break;
            default: LOG_ERROR("Unknown gender " + g);
            }
            // value not changed ? -> return
            if (d->m_SqlPatient->index(index.row(), Constants::IDENTITY_GENDER).data().toString() == toSave)
                return true;

            d->m_SqlPatient->setData(d->m_SqlPatient->index(index.row(), Constants::IDENTITY_GENDER), toSave, role);
            col = -1;
            colsToEmit << Core::IPatient::GenderIndex << Core::IPatient::GenderPixmap << Core::IPatient::Photo_32x32 << Core::IPatient::Photo_64x64;
            break;
        }
        case IPatient::DateOfBirth:
        {
            col = Constants::IDENTITY_DOB;
            colsToEmit << Core::IPatient::Age << Core::IPatient::YearsOld;
            break;
        }
        case IPatient::MaritalStatus: col = Constants::IDENTITY_MARITAL_STATUS;   break;
        case IPatient::DateOfDeath:   col = Constants::IDENTITY_DATEOFDEATH;      break;
        case IPatient::Profession:    col = Constants::IDENTITY_PROFESSION;       break;
        case IPatient::Street:
        {
            col = Constants::IDENTITY_ADDRESS_STREET;
            colsToEmit << Core::IPatient::FullAddress;
            break;
        }
        case IPatient::ZipCode:
        {
            col = Constants::IDENTITY_ADDRESS_ZIPCODE;
            colsToEmit << Core::IPatient::FullAddress;
            break;
        }
        case IPatient::City:
        {
            col = Constants::IDENTITY_ADRESS_CITY;
            colsToEmit << Core::IPatient::FullAddress;
            break;
        }
        case IPatient::Country:
        {
            col = Constants::IDENTITY_ADDRESS_COUNTRY;
            colsToEmit << Core::IPatient::FullAddress;
            break;
        }
        case IPatient::AddressNote:
        {
            col = Constants::IDENTITY_ADDRESS_NOTE;
            colsToEmit << Core::IPatient::FullAddress;
            break;
        }
        case IPatient::Mails:         col = Constants::IDENTITY_MAILS;            break;
        case IPatient::Tels:          col = Constants::IDENTITY_TELS;             break;
        case IPatient::Faxes:         col = Constants::IDENTITY_FAXES;            break;
        case IPatient::TitleIndex :   col = Constants::IDENTITY_TITLE;            break;
        case IPatient::Title:
            {
                QString t = value.toString();
                int id = -1;
                col = Constants::IDENTITY_TITLE;
                if (t == tkTr(Trans::Constants::MISTER)) {
                    id = Trans::Constants::Mister;
                } else if (t == tkTr(Trans::Constants::MISS)) {
                    id = Trans::Constants::Miss;
                } else if (t == tkTr(Trans::Constants::MADAM)) {
                    id = Trans::Constants::Madam;
                } else if (t == tkTr(Trans::Constants::DOCTOR)) {
                    id = Trans::Constants::Doctor;
                } else if (t == tkTr(Trans::Constants::PROFESSOR)) {
                    id = Trans::Constants::Professor;
                } else if (t == tkTr(Trans::Constants::CAPTAIN)) {
                    id = Trans::Constants::Captain;
                }
                if (id != -1) {
                    if (d->m_SqlPatient->data(d->m_SqlPatient->index(index.row(), col)).toInt() == id)
                        return true;

                    d->m_SqlPatient->setData(d->m_SqlPatient->index(index.row(), col), id, role);
                    colsToEmit << Core::IPatient::TitleIndex << Core::IPatient::FullName;
                }
                col = -1;
                break;
            }
        case IPatient::Photo_32x32:
        case IPatient::Photo_64x64:
            {
                QPixmap pix = value.value<QPixmap>();
                QString patientUid = d->m_SqlPatient->index(index.row(), Constants::IDENTITY_UID).data().toString();
                d->savePatientPhoto(pix, patientUid);
                col = -1;
                colsToEmit << Core::IPatient::Photo_32x32 << Core::IPatient::Photo_64x64;
                break;
            }
        }

        if (col != -1) {
            // value not changed ? -> return
            if (d->m_SqlPatient->index(index.row(), col).data() == value)
                return true;
            if (value.isNull() && d->m_SqlPatient->index(index.row(), col).data().toString().isEmpty())
                return true;

            // value changed -> save to database
            const bool ok = d->m_SqlPatient->setData(d->m_SqlPatient->index(index.row(), col), value, role);
            if (!ok)
                LOG_QUERY_ERROR(d->m_SqlPatient->query());
        }

        // Emit data changed signals
        for(int i = 0; i < colsToEmit.count(); ++i) {
            QModelIndex idx = this->index(index.row(), colsToEmit.at(i), index.parent());
            Q_EMIT dataChanged(idx, idx);
        }
        return true;
    }
    return true;
}

void PatientModel::setFilter(const QString &name, const QString &firstname, const QString &uuid, const FilterOn on)
{
    // Calculate new filter
    switch (on) {
    case FilterOnFullName :
        {
            // WHERE (NAME || SECONDNAME || SURNAME LIKE '%') OR (NAME LIKE '%')
            const QString &nameField = patientBase()->fieldName(Constants::Table_IDENT, Constants::IDENTITY_BIRTHNAME);
            const QString &secondField = patientBase()->fieldName(Constants::Table_IDENT, Constants::IDENTITY_SECONDNAME);
            const QString &surField = patientBase()->fieldName(Constants::Table_IDENT, Constants::IDENTITY_FIRSTNAME);
            d->m_ExtraFilter.clear();
//            d->m_ExtraFilter =  name + " || ";
//            d->m_ExtraFilter += second + " || ";
//            d->m_ExtraFilter += sur + " ";
            d->m_ExtraFilter += QString("((%1 LIKE '%2%' ").arg(nameField, name);
            d->m_ExtraFilter += QString("OR %1 LIKE '%2%') ").arg(secondField, name);
            if (!firstname.isEmpty())
                d->m_ExtraFilter += QString("AND %1 LIKE '%2%')").arg(surField, firstname);
            else
                d->m_ExtraFilter += ")";

            // OR ...
//            QStringList splitters;
//            if (filter.contains(" ")) {
//                splitters << " ";
//            }
//            if (filter.contains(";")) {
//                splitters << ";";
//            }
//            if (filter.contains(",")) {
//                splitters << ",";
//            }
//            foreach(const QString &s, splitters) {
//                foreach(const QString &f, filter.split(s, QString::SkipEmptyParts)) {
//                    QString like = QString(" LIKE '%1%'").arg(f);
//                    d->m_ExtraFilter += QString(" OR (%1)").arg(name + like);
//                    d->m_ExtraFilter += QString(" OR (%1)").arg(second + like);
//                    d->m_ExtraFilter += QString(" OR (%1)").arg(sur + like);
//                }
//            }
            break;
        }
    case FilterOnName :
        {
            // WHERE NAME LIKE '%'
            d->m_ExtraFilter.clear();
            d->m_ExtraFilter = patientBase()->fieldName(Constants::Table_IDENT, Constants::IDENTITY_BIRTHNAME) + " ";
            d->m_ExtraFilter += QString("LIKE '%%1%'").arg(name);
            break;
        }
    case FilterOnCity:
        {
//            // WHERE CITY LIKE '%'
//            d->m_ExtraFilter.clear();
//            d->m_ExtraFilter = patientBase()->fieldName(Constants::Table_IDENT, Constants::IDENTITY_ADRESS_CITY) + " ";
//            d->m_ExtraFilter += QString("LIKE '%%1%'").arg(filter);
            break;
        }
    case FilterOnUuid:
        {
            // WHERE PATIENT_UID='xxxx'
            d->m_ExtraFilter.clear();
            d->m_ExtraFilter = patientBase()->fieldName(Constants::Table_IDENT, Constants::IDENTITY_UID) + " ";
            d->m_ExtraFilter += QString("='%1'").arg(uuid);
            break;
        }
    }

    d->refreshFilter();
}

QString PatientModel::filter() const
{
    if (d->m_SqlPatient)
        return d->m_SqlPatient->filter();
    return QString();
}

void PatientModel::emitPatientCreationOnSubmit(bool state)
{
    d->m_EmitCreationAtSubmit = state;
    if (!state) {
        // emit created uids
        for(int i = 0; i < d->m_CreatedPatientUid.count(); ++i) {
            Q_EMIT patientCreated(d->m_CreatedPatientUid.at(i));
        }
        d->m_CreatedPatientUid.clear();
    }
}

bool PatientModel::insertRows(int row, int count, const QModelIndex &parent)
{
//    qWarning() << "PatientModel::insertRows" << row << count << parent;
    bool ok = true;
    beginInsertRows(parent, row, row+count);
    for(int i=0; i < count; ++i) {
        if (!d->m_SqlPatient->insertRow(row+i, parent)) {
            ok = false;
            Utils::Log::addError(this, "Unable to create a new patient. PatientModel::insertRows()");
            continue;
        }
        // find an unused uuid
        bool findUuid = false;
        QString uuid;
        while (!findUuid) {
            // TODO: Take care to inifinite looping...
            uuid = Utils::Database::createUid();
            QString f = QString("%1='%2'").arg(patientBase()->fieldName(Constants::Table_IDENT, Constants::IDENTITY_UID), uuid);
            findUuid = (patientBase()->count(Constants::Table_IDENT, Constants::IDENTITY_UID, f) == 0);
        }
        if (!d->m_SqlPatient->setData(d->m_SqlPatient->index(row+i, Constants::IDENTITY_UID), uuid)) {
            ok = false;
            LOG_ERROR("Unable to setData to newly created patient.");
        }
        if (!d->m_SqlPatient->setData(d->m_SqlPatient->index(row+i, Constants::IDENTITY_LK_TOPRACT_LKID), user()->value(Core::IUser::PersonalLinkId))) { // linkIds
            ok = false;
            LOG_ERROR("Unable to setData to newly created patient.");
        }
        if (!d->m_EmitCreationAtSubmit) {
            Q_EMIT patientCreated(uuid);
        } else {
            d->m_CreatedPatientUid << uuid;
        }
    }
    endInsertRows();
    return ok;
}

bool PatientModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(row);
    Q_UNUSED(count);
    Q_UNUSED(parent);
//    qWarning() << "PatientModel::removeRows" << row << count << parent;
    return true;
}

void PatientModel::fetchMore(const QModelIndex &parent)
{
    if (d->m_SqlPatient)
        d->m_SqlPatient->fetchMore(parent);
}

bool PatientModel::canFetchMore(const QModelIndex &parent) const
{
    if (d->m_SqlPatient)
        return d->m_SqlPatient->canFetchMore(parent);
    return false;
}

bool PatientModel::submit()
{
    bool ok = true;
    if (!d->m_SqlPatient->submitAll()) {
        ok = false;
    } else {
        // emit created uids
        for(int i = 0; i < d->m_CreatedPatientUid.count(); ++i) {
            Q_EMIT patientCreated(d->m_CreatedPatientUid.at(i));
        }
        d->m_CreatedPatientUid.clear();
    }
    return ok;
}

bool PatientModel::refreshModel()
{
    d->refreshFilter();
    d->m_SqlPatient->select();
    reset();
    return true;
}

/** Return the patient name corresponding to the \e uuids. Hash key represents the uuid while value is the full name. */
QHash<QString, QString> PatientModel::patientName(const QList<QString> &uuids)
{
    QHash<QString, QString> names;
    QSqlDatabase DB = patientBase()->database();
    DB.transaction();
    QSqlQuery query(DB);
    const QStringList &titles = Trans::ConstantTranslations::titles();

    foreach(const QString &u, uuids) {
        if (u.isEmpty())
            continue;
        QHash<int, QString> where;
        where.insert(Constants::IDENTITY_UID, QString("='%1'").arg(u));
        QString req = patientBase()->select(Constants::Table_IDENT, QList<int>() << Constants::IDENTITY_TITLE << Constants::IDENTITY_BIRTHNAME << Constants::IDENTITY_SECONDNAME << Constants::IDENTITY_FIRSTNAME, where);
        if (query.exec(req)) {
            if (query.next()) {
                if (!query.value(1).toString().isEmpty())
                    names.insert(u, QString("%1 %2 - %3 %4").arg(titles.at(query.value(0).toInt()), query.value(1).toString(), query.value(2).toString(), query.value(3).toString()).simplified());
                else
                    names.insert(u, QString("%1 %3").arg(titles.at(query.value(0).toInt()), query.value(2).toString(), query.value(3).toString()).simplified());
            }
        } else {
            LOG_QUERY_ERROR_FOR("PatientModel", query);
            query.finish();
            DB.rollback();
            return names;
        }
        query.finish();
    }
    DB.commit();
    return names;
}
