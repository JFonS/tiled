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

BgSize GbaPlugin::getMapSize(uint w, uint h) const
{
    const uint small = 32*TILE_SIZE;
    const uint big   = 64*TILE_SIZE;

    if (w <= small and h <= small) return BgSize::BG_32x32;
    if (w <= small and h <= big)   return BgSize::BG_32x64;
    if (w <= big and h <= small)   return BgSize::BG_64x32;
    if (w <= big and h <= big)     return BgSize::BG_64x64;
    return BgSize::BG_ERROR;
}

GbaPlugin::GbaPlugin()
{
}

bool GbaPlugin::openFile(QSaveFile &file)
{
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        mError = tr("Could not open file \"");
        mError.append(file.fileName());
        mError.append(tr("\" for writing."));
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

    if (map->tileWidth() % TILE_SIZE != 0 || map->tileHeight() % TILE_SIZE != 0)
    {
        mError = tr("Tile size must be multiple of ");
        mError.append(TILE_SIZE);
        mError.append("x");
        mError.append(TILE_SIZE);
        mError.append(".");
        return false;
    }

    int totalWidth = map->width() * map->tileWidth();
    int totalHeight = map->height() * map->tileHeight();

    BgSize bgsize = getMapSize(totalWidth,totalHeight);

    if (bgsize == BgSize::BG_ERROR)
    {
        mError = tr("Total map size is too big for a GBA map (max 64x64 tiles of 8x8 pixels)");
        return false;
    }

    int dataSize = 32*32 * (bgsize == BG_32x32 ? 1 : bgsize == BG_64x64 ? 4 : 2);

    std::vector<uint16_t> mapData(dataSize, 0);

    uint pitch = (bgsize == BG_32x32 || bgsize == BG_32x64 ? 32 : 64)/32;
    uint tilesetPitch = map->tilesetAt(0)->columnCount();
    uint scaleFactor =  map->tilesetAt(0)->tileWidth()/TILE_SIZE;

    for (Tiled::Layer *anyLayer : map->layers())
    {
        Tiled::TileLayer *layer = anyLayer->asTileLayer();
        if (!layer) continue; //Skip layers that are not tiles


        for (int i = 0; i < layer->height(); ++i)
        {
            for (int j = 0; j < layer->width(); ++j)
            {

                Tiled::Cell cell = layer->cellAt(j,i);
                uint16_t tileId = cell.isEmpty() ? 0 : cell.tile->id();

                uint tileX = (tileId % tilesetPitch) * scaleFactor;
                uint tileY = (tileId / tilesetPitch) * scaleFactor;

                for (uint k = 0; k < scaleFactor; ++k)
                {
                    for (uint l = 0; l < scaleFactor; ++l)
                    {
                        uint my = i*scaleFactor+k, mx = j*scaleFactor+l;
                        uint sbb= (my/32)*pitch + (mx/32);
                        uint p = sbb*1024 + (my%32)*32 + mx%32;

                        uint16_t id = (tileY + (cell.flippedVertically ? scaleFactor - 1 - k : k)) * tilesetPitch * scaleFactor + tileX + (cell.flippedHorizontally ? scaleFactor -1 - l : l);
                        id |= cell.flippedHorizontally << HFLIP_SHIFT;
                        id |= cell.flippedVertically   << VFLIP_SHIFT;
                        mapData[p] = id;
                    }
                }
            }
        }

        QString layerName(layer->name());
        for (QChar &c : layerName)
            if (!c.isLetterOrNumber() && c != '\0') layerName.remove(c);

        data << "const unsigned short " << layerName << "[" << dataSize << "] = {" << endl;

        uint i;
        for (i = 0; i < mapData.size(); ++i)
        {
            data << showbase << hex << qSetFieldWidth(6) << mapData[i] << reset << ",";
            if (i%32 == 31) data << endl;
        }
        if (i%32 != 31) data << endl;
        data << "};";

        header << "#ifndef MAP_" << layerName.toUpper() << "_H" << endl;
        header << "#define MAP_" << layerName.toUpper() << "_H" << endl << endl;

        header << "    #define " << layerName << "Len " << dataSize * 2 << endl; //Size in bytes
        header << "    extern const unsigned short " << layerName << "[" << dataSize << "];" << endl << endl;

        header << "#endif // MAP_" << layerName.toUpper() << "_H";
    }

    if (!saveFile(dataFile)) return false;
    if (!saveFile(headerFile)) return false;

    return true;
}

} // namespace GBA
