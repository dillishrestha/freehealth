#!/bin/sh
#/***************************************************************************
# *  The FreeMedForms project is a set of free, open source medical         *
# *  applications.                                                          *
# *  (C) 2008-2013 by Eric MAEKER, MD (France) <eric.maeker@gmail.com>      *
# *  All rights reserved.                                                   *
# *                                                                         *
# *  This program is free software: you can redistribute it and/or modify   *
# *  it under the terms of the GNU General Public License as published by   *
# *  the Free Software Foundation, either version 3 of the License, or      *
# *  (at your option) any later version.                                    *
# *                                                                         *
# *  This program is distributed in the hope that it will be useful,        *
# *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
# *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
# *  GNU General Public License for more details.                           *
# *                                                                         *
# *  You should have received a copy of the GNU General Public License      *
# *  along with this program (COPYING.FREEMEDFORMS file).                   *
# *  If not, see <http://www.gnu.org/licenses/>.                            *
# ***************************************************************************/
#/***************************************************************************
# *   Main developers : Eric MAEKER, <eric.maeker@gmail.com>                *
# *  Contributors:                                                          *
# *       NAME <MAIL@ADDRESS.COM>                                           *
# *       NAME <MAIL@ADDRESS.COM>                                           *
# ***************************************************************************/

BUNDLE_NAME=""
GIT_REVISION=""
PROJECT_VERSION=""
PACKPATH=""
SOURCES_ROOT_PATH=""

# Some path definition
SCRIPT_NAME=`basename $0`
if [ "`echo $0 | cut -c1`" = "/" ]; then
    SCRIPT_PATH=`dirname $0`
else
    SCRIPT_PATH=`pwd`/`echo $0 | sed -e s/$SCRIPT_NAME//`
fi
SOURCES_ROOT_PATH=$SCRIPT_PATH"/../"

# get version number of the project
PROJECT_VERSION=`cat $SOURCES_ROOT_PATH/buildspecs/projectversion.pri | grep "PACKAGE_VERSION" -m 1 | cut -d = -s -f2 | tr -d ' '`

showHelp()
{
    echo "$SCRIPT_NAME builds FreeMedForms source package, GIT branches and tags."
    echo "Project version: $PROJECT_VERSION"
    echo
    echo "Usage: $SCRIPT_NAME -r 123"
    echo "Options:"
    echo "  -d  Include eDRC non-free datapack in datapacks/appinstalled"
    echo "  -h  Show this help"
    echo
}

cd $SCRIPT_PATH"/.."

# remove executable flags to doc files
#echo "Updating files rights : step 1"
#sudo find . -type f -exec chmod -R 666 {} \;
#echo "Updating files rights : step 2"
#sudo find . -type f -name "*.sh" -exec chmod -R 777 {} \;

export COPYFILE_DISABLE=true

# prepare the source tar.gz file
prepareFileSelection()
{
echo "**** PREPARE SOURCES PACKAGE ****"

SCRIPT_SOURCE="\
scripts/mac/*.sh \
scripts/Linux/* \
scripts/source.sh \
scripts/win/* \
scripts/debug/* \
"

RESOURCES="\
global_resources/datapacks/appinstalled/defaultservers.txt \
global_resources/datapacks/appinstalled/drugs/master.db \
global_resources/datapacks/appinstalled/drugs/readme.txt \
global_resources/datapacks/appinstalled/account/readme.txt \
global_resources/doc/freeaccount \
global_resources/doc/freediams \
global_resources/doc/freedrc \
global_resources/doc/freeicd \
global_resources/doc/freemedforms \
global_resources/doc/freepad \
global_resources/doc/freetoolbox \
global_resources/forms \
global_resources/package_helpers/freeaccount* \
global_resources/package_helpers/freediams* \
global_resources/package_helpers/freedrc* \
global_resources/package_helpers/freeicd* \
global_resources/package_helpers/freemedforms* \
global_resources/package_helpers/freepad* \
global_resources/package_helpers/freetool* \
global_resources/pixmap/16x16 \
global_resources/pixmap/32x32 \
global_resources/pixmap/48x48 \
global_resources/pixmap/64x64 \
global_resources/pixmap/others \
global_resources/pixmap/splashscreens \
global_resources/pixmap/svg/*.svg \
global_resources/pixmap/svg/*.icns \
global_resources/pixmap/svg/*.ico \
global_resources/pixmap/svg/*.bmp \
global_resources/profiles \
global_resources/sql/account \
global_resources/sql/drugdb \
global_resources/sql/druginfodb \
global_resources/sql/icd10 \
global_resources/sql/server_config \
global_resources/sql/zipcodes \
global_resources/textfiles/boys_surnames.csv \
global_resources/textfiles/default_user_footer.htm \
global_resources/textfiles/default_user_header.htm \
global_resources/textfiles/girls_surnames.csv \
global_resources/textfiles/haarcascade_frontalface_alt2.xml \
global_resources/textfiles/listemotsfr.txt \
global_resources/textfiles/surnames.txt \
global_resources/textfiles/*.db \
global_resources/textfiles/freediamstest \
global_resources/textfiles/oldprescriptionsfiles \
global_resources/textfiles/edrc \
global_resources/textfiles/prescription \
global_resources/translations/*.ts \
global_resources/translations/qt*.qm \
"

BUILDSPEC_SOURCES="\
README.txt COPYING.txt INSTALL \
updatetranslations.sh \
buildspecs/*.pri \
buildspecs/*.in \
doc \
"

LIBS_SOURCES="\
libs/aggregation \
libs/calendar \
libs/datapackutils \
libs/extensionsystem \
libs/medicalutils \
libs/medintuxutils \
libs/translationutils \
libs/utils \
libs/*.pri \
contrib \
"

APP_SOURCES="\
freeaccount.pro \
freeaccount \
freediams.pro \
freediams \
freedrc.pro \
freedrc \
freeicd.pro \
freeicd \
freemedforms.pro \
freemedforms \
freepad.pro \
freepad \
freetoolbox.pro \
freetoolbox \
"

PLUGINS_SOURCES="\
plugins/fmf_plugins.pri \
plugins/pluginjsonmetadata.xsl \
plugins/accountbaseplugin \
plugins/accountplugin \
plugins/account2plugin \
plugins/agendaplugin \
plugins/aggirplugin \
plugins/alertplugin \
plugins/basewidgetsplugin \
plugins/categoryplugin \
plugins/coreplugin \
plugins/datapackplugin \
plugins/druginteractionsplugin \
plugins/drugsbaseplugin \
plugins/drugsplugin \
plugins/edrcplugin \
plugins/emptyplugin \
plugins/feedbackplugin \
plugins/fmfcoreplugin \
plugins/fmfmainwindowplugin \
plugins/formmanagerplugin \
plugins/icdplugin \
plugins/identityplugin \
plugins/listviewplugin \
plugins/padtoolsplugin \
plugins/patientbaseplugin
plugins/pmhplugin \
plugins/printerplugin \
plugins/saverestoreplugin \
plugins/scriptplugin \
plugins/templatesplugin \
plugins/texteditorplugin \
plugins/toolsplugin \
plugins/usermanagerplugin \
plugins/webcamplugin \
plugins/xmlioplugin \
plugins/zipcodesplugin \
"

TEST_SOURCES="\
tests \
"

SELECTED_SOURCES=$SCRIPT_SOURCE$RESOURCES$BUILDSPEC_SOURCES$LIBS_SOURCES$APP_SOURCES$PLUGINS_SOURCES$TEST_SOURCES
}

includeEdrcFiles()
{
SELECTED_SOURCES=$SELECTED_SOURCES" \
global_resources/datapacks/appinstalled/edrc_ro/edrc.db \
global_resources/datapacks/appinstalled/edrc_ro/readme.txt
"
}

createSource()
{
    # create sources tmp path
    PACKPATH=$SCRIPT_PATH/freemedforms-$PROJECT_VERSION
    if [ -e $PACKPATH ]; then
        rm -R $PACKPATH
    fi
    mkdir $PACKPATH

    tar -cf $PACKPATH/sources.tar \
    --exclude '.git' --exclude '.svn' --exclude '.cvsignore' --exclude 'qtc-gdbmacros' \
    --exclude '_protected' --exclude '__nonfree__' --exclude 'nonfree' \
    --exclude 'build' --exclude 'bin' --exclude 'packages' --exclude 'zlib-1.2.3' \
    --exclude 'rushes' --exclude 'doxygen' \
    --exclude 'Makefile*' --exclude '*.pro.user*' --exclude '*bkup' --exclude '*autosave' \
    --exclude 'dosages.db' --exclude 'users.db' --exclude '*.mdb' --exclude '.*' --exclude '._*' \
    --exclude '*.app' --exclude '*.zip' --exclude '*.a' \
    --exclude '*.o' --exclude 'moc_*' --exclude 'ui_*.h' --exclude '*.dylib' \
    --exclude 'global_resources/databases' \
    --exclude 'sources.tar' \
    $EXCLUSIONS \
    $SELECTED_SOURCES

    echo "**** UNPACK SOURCES PACKAGE TO CREATED DIR ****"
    tar xf $PACKPATH/sources.tar -C $PACKPATH
    rm $PACKPATH/sources.tar
    find $PACKPATH -type f -exec chmod -R 666 {} \;

    echo "   * DEFINING *.ISS FILES APP VERSION"
    cd $PACKPATH/global_resources/package_helpers
    FILES=`find ./ -type f -name '*.iss'`
    for f in $FILES; do
        sed -i "bkup" 's#__version__#'$PROJECT_VERSION'#' $f
    done
    rm *.*bkup
    echo "   * DEFINING *.BAT FILES APP VERSION"
    cd $PACKPATH/scripts
    FILES=`find ./ -type f -name '*.bat'`
    for f in $FILES; do
        sed -i "bkup" 's#__version__#'$PROJECT_VERSION'#' $f
    done
    rm *.*bkup

    echo "   * DEFINING *.PLUGINSPEC FILES APP VERSION"
    cd $PACKPATH
    FILES=`find ./ -type f -name '*.pluginspec'`
    NON_ALPHABETA_PROJECT_VERSION=`echo $PROJECT_VERSION | tr '~' '.' | tr '-' '.' | cut -d"." -f1,2,3`
    for f in $FILES; do
        # compatVersion="0.6.0"
        sed -i "bkup" 's#compatVersion=\".*\"#compatVersion=\"'$NON_ALPHABETA_PROJECT_VERSION'\"#' $f
        rm $f"bkup"
        # version="0.6.0"
        sed -i "bkup" 's#version=\".*\" #version=\"'$NON_ALPHABETA_PROJECT_VERSION'\" #' $f
        rm $f"bkup"
        sed -i "bkup" 's#version=\".*\"/>#version=\"'$NON_ALPHABETA_PROJECT_VERSION'\"/>#' $f
        rm $f"bkup"
    done

    echo "   * ADDING LIBRARY VERSION NUMBER"
    cd $PACKPATH/libs
    find . -type f -name '*.pro' -exec sed -i bkup 's/# VERSION=1.0.0/!win32:{VERSION='$NON_ALPHABETA_PROJECT_VERSION'}/' {} \;
    find . -type f -name '*.probkup' -exec rm {} \;

    echo "   * REMOVING TEST VERSION IN FORMS"
    cd $PACKPATH/global_resources/forms
    find . -type f -name '*.xml' -exec sed -i bkup 's#<version>test</version>#<version>'$NON_ALPHABETA_PROJECT_VERSION'</version>#' {} \;
    find . -type f -name '*.xmlbkup' -exec rm {} \;

    # git version is computed in the buildspecs/githash.pri
    # but the source package needs a static reference
    # while source package does not include the git logs
    GITHASH=`git rev-parse HEAD`
    echo "   * SETTING GIT revision hash to " $GITHASH
    sed -i bkup 's/GIT_HASH=.*/GIT_HASH='$GITHASH'/' $PACKPATH/buildspecs/githash.pri
    rm $PACKPATH/buildspecs/githash.pribkup

    echo "**** REPACK SOURCES PACKAGE FROM CREATED DIR ****"
    cd $SCRIPT_PATH
    tar czf ../freemedformsfullsources-$PROJECT_VERSION.tgz  ./freemedforms-$PROJECT_VERSION

    echo "**** CLEANING TMP SOURCES PATH ****"
    rm -R $PACKPATH

    PWD=`pwd`

    echo "*** Source package successfully created at `pwd`./freemedforms-$PROJECT_VERSION"
}

#########################################################################################
## Analyse options
#########################################################################################

prepareFileSelection

while getopts "hd" option
do
  case $option in
    h) showHelp
      exit 0
    ;;
    d) includeEdrcFiles
    ;;
  esac
done

createSource

exit 0
