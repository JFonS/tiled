/*
 * GBA Tiled Plugin
 * Copyright 2011, JFonS <joan.fonssanchez@gmail.com>
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

#ifndef GBAPLUGIN_H
#define GBAPLUGIN_H

#include "gbaplugin_global.h"
#include "tiled.h"

#include "mapformat.h"

namespace GBA {

class GBAPLUGINSHARED_EXPORT GbaPlugin : public Tiled::WritableMapFormat
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.mapeditor.MapFormat" FILE "plugin.json")

public:
    GbaPlugin();

    bool write(const Tiled::Map *map, const QString &fileName) override;
    QString errorString() const override;
    QStringList outputFiles(const Tiled::Map *, const QString &fileName) const override;

protected:
    QString nameFilter() const override;

private:
    bool saveFile(const QString &fileName, const QString &content);
    QString mError;
};

} // namespace Defold

#endif