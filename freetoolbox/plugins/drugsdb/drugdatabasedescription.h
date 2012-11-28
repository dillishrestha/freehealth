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
 *   Main Developper : Eric MAEKER, <eric.maeker@gmail.com>                *
 *   Contributors :                                                        *
 *       NAME <MAIL@ADDRESS.COM>                                           *
 ***************************************************************************/
#ifndef DRUGSDB_DRUGDATABASEDESCRIPTION_H
#define DRUGSDB_DRUGDATABASEDESCRIPTION_H

#include <utils/genericdescription.h>
#include <drugsdb/drugsdb_exporter.h>

namespace DrugsDB {

class DRUGSDB_EXPORT DrugDatabaseDescription : public Utils::GenericDescription
{
public:
    enum Data {
        Language = Utils::GenericDescription::NonTranslatableExtraData + 1,
        Vendor,
        Copyright,
        DrugUidName,
        IsAtcValid,
        IsDDIValid,
        ComplementaryWebLink,
        PackUidName,
        DrugNameConstructor
    };

    DrugDatabaseDescription();
};

}

#endif // DRUGSDB_DRUGDATABASEDESCRIPTION_H