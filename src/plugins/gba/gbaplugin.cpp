#include "gbaplugin.h"

#include "layer.h"
#include "map.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tile.h"
#include "tilelayer.h"

#include <cmath>
#include <QFileInfo>

namespace GBA {

  QStringList GbaPlugin::outputFiles(const Tiled::Map *, const QString &fileName) const
  {
    QString c, h;
    c.append(fileName);

    if (!fileName.endsWith(".s",Qt::CaseInsensitive)) c.append(".s");

    h.append(c);
    h.chop(1);
    h.append("h");

    return QStringList() << c << h;
  }

  QString GbaPlugin::nameFilter() const
  {
    return tr("GBA assembler data file (*.s)");
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


  bool GbaPlugin::checkParameters(const Tiled::Map *map)
  {
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

    return true;
  }

  void fixString(QString &str) {
    for (QChar &c : str)
      if (!c.isLetterOrNumber() && c != '\0') str.remove(c);
  }

  void GbaPlugin::writeAsmHeader(QString name, QTextStream &file)
  {
    file << "@{{BLOCK(" << name << ")" << endl;
    file << ".section .rodata" << endl;
    file << ".align	2"  << endl;
    file << ".global " << name  << endl;
    file << ".hidden " << name  << endl;
    file << name  << ":" << endl;
  }

  bool GbaPlugin::write(const Tiled::Map *map, const QString &fileName)
  {
    if (!checkParameters(map)) return false;

    QStringList files = outputFiles(map, fileName);

    QSaveFile dataFile(files[0]);
    QSaveFile headerFile(files[1]);

    if (!openFile(dataFile)) return false;
    if (!openFile(headerFile)) return false;

    QTextStream data(&dataFile), header(&headerFile);

    uint emptyTileId = map->tilesetAt(0)->tileCount();
    uint scaleFactor =  map->tilesetAt(0)->tileWidth()/TILE_SIZE;

    int blocksX = (map->width()*scaleFactor)/32 + (map->width()*scaleFactor%32 != 0);
    int blocksY = (map->height()*scaleFactor)/32 + (map->height()*scaleFactor%32 != 0);

    MapData mapData(blocksY, std::vector< std::vector<std::vector<uint16_t>> >
                    (blocksX, std::vector<std::vector<uint16_t>>
                     (32,std::vector<uint16_t>(32,0))));

    QString defName(QFileInfo(dataFile.fileName()).baseName());
    fixString(defName);

    header << "#ifndef MAP_" << defName.toUpper() << "_H" << endl;
    header << "#define MAP_" << defName.toUpper() << "_H" << endl << endl;

    for (Tiled::Layer *anyLayer : map->layers())
    {
      Tiled::TileLayer *layer = anyLayer->asTileLayer();
      if (!layer) continue; //Skip layers that are not tiles

      for (int i = 0; i < layer->height(); ++i)
      {
        for (int j = 0; j < layer->width(); ++j)
        {
          Tiled::Cell cell = layer->cellAt(j,i);
          uint16_t tileId = (cell.isEmpty() ? emptyTileId : cell.tile->id()) * scaleFactor * scaleFactor;

          for (uint k = 0; k < scaleFactor; ++k)
          {
            for (uint l = 0; l < scaleFactor; ++l)
            {
              uint my = i*scaleFactor+k, mx = j*scaleFactor+l;

              uint blockX = mx/32;
              uint blockY = my/32;

              mx = mx%32;
              my = my%32;

              uint kk = cell.flippedVertically ? scaleFactor - 1 - k : k;
              uint ll = cell.flippedHorizontally ? scaleFactor - 1 - l : l;

              uint16_t id = tileId + scaleFactor*kk + ll;

              id |= cell.flippedHorizontally << HFLIP_SHIFT;
              id |= cell.flippedVertically   << VFLIP_SHIFT;

              mapData[blockY][blockX][my][mx] = id;
            }
          }
        }
      }

      QString layerName(layer->name());
      fixString(layerName);
      QString logicLayerName(layerName + "_logic");

      // Write tile data
      writeAsmHeader(layerName, data);

      for (int i = 0; i < blocksY; ++i)
      {
        for (int j = 0; j < blocksX; ++j)
        {
          data << ".hword ";
          for (int k = 0; k < 32; ++k)
          {
            for (int l = 0; l < 32; ++l)
            {
              data << "0x" << QString("%1").arg(mapData[i][j][k][l], 4, 16, QLatin1Char( '0' ));
              if (k < 31 || l < 31) data << ",";
            }
          }
          data << endl;
        }
      }

      data << "@}}BLOCK(" << layerName << ")" << endl << endl;

      // Write logic data
      writeAsmHeader(logicLayerName, data);


      for (int i = 0; i < layer->height(); ++i)
      {
        data << ".hword ";
        for (int j = 0; j < layer->width(); ++j)
        {
          Tiled::Cell cell = layer->cellAt(j,i);
          uint16_t tileId = cell.isEmpty() ? emptyTileId : cell.tile->id();
          data << "0x" << QString("%1").arg(tileId, 4, 16, QLatin1Char( '0' ));
          if (j < layer->width()-1) data << ",";
        }
        data << endl;
      }

      data << "@}}BLOCK(" << logicLayerName << ")" << endl << endl;

      header << "    extern const unsigned short " << layerName << "[" << blocksY << "][" << blocksX << "][32][32];" << endl;
      header << "    extern const unsigned short " << logicLayerName << "[" << layer->width() << "][" << layer->height() << "];" << endl << endl;
    }

    header << "#endif // MAP_" << defName.toUpper() << "_H";

    if (!saveFile(dataFile)) return false;
    if (!saveFile(headerFile)) return false;

    return true;
  }

} // namespace GBA
