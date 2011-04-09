/***************************************************************************
 *   FreeMedicalForms                                                      *
 *   (C) 2008-2010 by Eric MAEKER, MD                                     **
 *   eric.maeker@free.fr                                                   *
 *   All rights reserved.                                                  *
 *                                                                         *
 *   This program is a free and open source software.                      *
 *   It is released under the terms of the new BSD License.                *
 *                                                                         *
 *   Redistribution and use in source and binary forms, with or without    *
 *   modification, are permitted provided that the following conditions    *
 *   are met:                                                              *
 *   - Redistributions of source code must retain the above copyright      *
 *   notice, this list of conditions and the following disclaimer.         *
 *   - Redistributions in binary form must reproduce the above copyright   *
 *   notice, this list of conditions and the following disclaimer in the   *
 *   documentation and/or other materials provided with the distribution.  *
 *   - Neither the name of the FreeMedForms' organization nor the names of *
 *   its contributors may be used to endorse or promote products derived   *
 *   from this software without specific prior written permission.         *
 *                                                                         *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS   *
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT     *
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS     *
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE        *
 *   COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,  *
 *   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,  *
 *   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;      *
 *   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER      *
 *   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT    *
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN     *
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
 *   POSSIBILITY OF SUCH DAMAGE.                                           *
 ***************************************************************************/
  /***************************************************************************
 *   Main Developper : Eric MAEKER, <eric.maeker@free.fr>                  *
 *   Contributors :                                                        *
 *       Pierre-Marie DESOMBRE <pm.desombre@medsyn.fr>                     *
 *       NAME <MAIL@ADRESS>                                                *
 ***************************************************************************/
#include "assetsratesmodel.h"
#include "accountbase.h"
#include "constants.h"

#include <utils/log.h>
#include <coreplugin/icore.h>
#include <coreplugin/iuser.h>

#include <QSqlTableModel>

using namespace AccountDB;
static inline Core::IUser *user() { return  Core::ICore::instance()->user(); }
namespace AccountDB {
namespace Internal {

class AssetsRatesModelPrivate
{
public:
    AssetsRatesModelPrivate(AssetsRatesModel *parent) : m_SqlTable(0), m_IsDirty(false), q(parent)
    {
        m_SqlTable = new QSqlTableModel(q, QSqlDatabase::database(Constants::DB_ACCOUNTANCY));
        m_SqlTable->setTable(AccountDB::AccountBase::instance()->table(Constants::Table_AssetsRates));
    }
    ~AssetsRatesModelPrivate () {}

public:
    QSqlTableModel *m_SqlTable;
    bool m_IsDirty;

private:
    AssetsRatesModel *q;
};

}  // End namespace Internal
}  // End namespace AccountDB



AssetsRatesModel::AssetsRatesModel(QObject *parent) : QAbstractTableModel(parent), 
                                                      m_UserUid(user()->value(Core::IUser::Uuid).toString()),
                                                      d(new Internal::AssetsRatesModelPrivate(this))
{
    d->m_SqlTable->setEditStrategy(QSqlTableModel::OnFieldChange);
    setUserUuid(m_UserUid);
}

AssetsRatesModel::~AssetsRatesModel()
{
    if (d) {
        delete d;
        d=0;
    }
}


int AssetsRatesModel::rowCount(const QModelIndex &parent) const
{
    return d->m_SqlTable->rowCount(parent);
}

int AssetsRatesModel::columnCount(const QModelIndex &parent) const
{
    return d->m_SqlTable->columnCount(parent);
}

void AssetsRatesModel::setUserUuid(const QString &uuid)
{
    QHash<int, QString> where;
    where.insert(Constants::ASSETSRATES_USER_UID, QString("='%1'").arg(uuid));
    d->m_SqlTable->setFilter(AccountBase::instance()->getWhereClause(Constants::Table_AssetsRates, where));
    d->m_SqlTable->select();
}

QVariant AssetsRatesModel::data(const QModelIndex &index, int role) const
{
    return d->m_SqlTable->data(index, role);
}

bool AssetsRatesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    bool ret = d->m_SqlTable->setData(index, value, role);
    d->m_IsDirty = d->m_SqlTable->isDirty(index);
    return ret;
}


QVariant AssetsRatesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return d->m_SqlTable->headerData(section,orientation,role);
}

bool AssetsRatesModel::insertRows(int row, int count, const QModelIndex &parent)
{
    d->m_IsDirty = true;
    bool ret = d->m_SqlTable->insertRows(row, count, parent);
    return ret;
}

bool AssetsRatesModel::removeRows(int row, int count, const QModelIndex &parent)
{
    d->m_IsDirty = true;
    return d->m_SqlTable->removeRows(row, count, parent);
}

bool AssetsRatesModel::submit()
{
    if (d->m_SqlTable->submitAll()) {
        d->m_IsDirty = false;
        return true;
    }
    return false;
}

void AssetsRatesModel::revert()
{
    d->m_IsDirty = false;
    d->m_SqlTable->revert();
}

bool AssetsRatesModel::isDirty() const
{
    return d->m_IsDirty;
}

void AssetsRatesModel::setFilter(const QString & filter){
    d->m_SqlTable->setFilter(filter);
    d->m_SqlTable->select();
}

QString AssetsRatesModel::filter(){
    return d->m_SqlTable->filter();
}

QSqlError AssetsRatesModel::lastError(){
    return d->m_SqlTable->lastError();
}
