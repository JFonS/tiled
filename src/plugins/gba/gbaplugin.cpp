/*
 * Defold Tiled Plugin
 * Copyright 2016, Nikita Razdobreev <exzo0mex@gmail.com>
 * Copyright 2016, Thorbjørn Lindeijer <bjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
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
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gbaplugin.h"

#include <QSaveFile>
#include <QTextStream>

#include "layer.h"
#include "map.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tile.h"
#include "tilelayer.h"

#include <cmath>

namespace GBA {

QStringList GbaPlugin::outputFiles(const Tiled::Map *, const QString &fileName) const
{
    QString c, h;
    c.append(fileName);

    if (!fileName.endsWith(".c",Qt::CaseInsensitive)) c.append(".c");

    h.append(c);
    h.chop(1);
    h.append("h");

    return QStringList() << c << h;
}

QString GbaPlugin::nameFilter() const
{
    return tr("GBA data file (*.c)");
}

QString GbaPlugin::errorString() const
{
    return mError;
}

GbaPlugin::GbaPlugin()
{
}

bool GbaPlugin::saveFile(const QString &fileName, const QString &content)
{
    QSaveFile mapFile(fileName);
    if (!mapFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        mError = tr("Could not open file for writing.");
        return false;
    }
    QTextStream stream(&mapFile);
    stream << content;

    if (mapFile.error() != QSaveFile::NoError) {
        mError = mapFile.errorString();
        return false;
    }

    if (!mapFile.commit()) {
        mError = mapFile.errorString();
        return false;
    }
    return true;
}

bool GbaPlugin::write(const Tiled::Map *map, const QString &fileName)
{
    QString result("HI :)");

    QStringList files = outputFiles(map, fileName);
    if (!saveFile(files[0],result)) return false;
    if (!saveFile(files[1],result)) return false;

    return true;
}

} // namespace GBA
