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

bool GbaPlugin::openFile(QSaveFile &file)
{
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    mError = tr("Could not open file \"");
    mError.append(file.fileName());
    mError.append("\" for writing.");
    return false;
  }
  return true;
}

bool GbaPlugin::saveFile(QSaveFile &file)
{
    if (file.error() != QSaveFile::NoError) {
        mError = file.errorString();
        return false;
    }

    if (!file.commit()) {
        mError = file.errorString();
        return false;
    }

    return true;
}

bool GbaPlugin::write(const Tiled::Map *map, const QString &fileName)
{
    QStringList files = outputFiles(map, fileName);

    QSaveFile dataFile(files[0]);
    QSaveFile headerFile(files[1]);

    if (!openFile(dataFile)) return false;
    if (!openFile(headerFile)) return false;

    QTextStream data(&dataFile), header(&headerFile);

    if (map->orientation() != Tiled::Map::Orthogonal)
    {
      mError = tr("Only orthogonal maps are supported.");
      return false;
    }

    int totalWidth = map->width() * map->tileWidth();
    int totalHeight = map->height() * map->tileHeight();

    if (totalWidth > 64*8)
    {
      mError = tr("Total width (map witdth * tile width) is too big for a GBA map");
      return false;
    }

    if (totalHeight > 64*8)
    {
      mError = tr("Total width (map witdth * tile width) is too big for a GBA map");
      return false;
    }

    for (Tiled::Layer *anyLayer : map->layers())
    {
      Tiled::TileLayer *layer = anyLayer->asTileLayer();
      if (!layer) continue; //Skip layers that are not tiles

      QString layerName(layer->name());
      for (QChar &c : layerName)
        if (!c.isLetterOrNumber()) c = '_';

      data << "const unsigned short " << layerName << "[1024] = {" << endl;

      //data.setFieldWidth(4);
      for (int i = 0; i < layer->height(); ++i)
      {
        for (int j = 0; j < layer->width(); ++j)
        {
          Tiled::Cell cell = layer->cellAt(j,i);

          if (cell.isEmpty())
          {
              mError = QString("Cell at (%1,%2) is empty, no empty cells allowed.").arg(j).arg(i);
              return false;
          }

          uint16_t id = cell.tile->id() & TILE_ID_MASK;
          id |= cell.flippedHorizontally << HFLIP_SHIFT;
          id |= cell.flippedVertically   << VFLIP_SHIFT;

          data << showbase << hex << qSetFieldWidth(6) << id << reset << ", ";
        }
        data << endl;
      }
      data << "};";

      header << "#ifndef MAP_" << layerName.toUpper() << "_H" << endl;
      header << "#define MAP_" << layerName.toUpper() << "_H" << endl << endl;

      header << "    #define " << layerName << "Len " << layer->width() * layer->height() * 2 << endl;
      header << "    extern const unsigned short " << layerName << "[1024];" << endl << endl;

      header << "#endif // MAP_" << layerName.toUpper() << "_H";
    }

    if (!saveFile(dataFile)) return false;
    if (!saveFile(headerFile)) return false;

    return true;
}

} // namespace GBA
