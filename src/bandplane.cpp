/*
 * 	    bandplane.cpp             (C) 2006-2008, Aurélien Croc (AP²C)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the
 *  Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *  $Id$
 * 
 */
#include "bandplane.h"
#include <unistd.h>
#include <numeric>

/*
 * Constructeur - Destructeur
 * Init - Uninit 
 */
BandPlane::BandPlane()
{
    _endian = Endian::Dependant;
    _checksum = 0;
    _colorNr = 0;
    _compression = 0;
}

BandPlane::~BandPlane()
{
}


/*
 * Enregistrement des données
 * Set data
 */
void BandPlane::setData(std::vector<uint8_t> data)
{
    _data = std::move(data);
    _checksum = std::accumulate(_data.begin(), _data.end(), 0U);
}



/*
 * Mise sur disque / Rechargement
 * Swapping / restoring
 */
bool BandPlane::swapToDisk(int fd)
{
    size_t size = _data.size();
    if (write(fd, &_colorNr, sizeof(_colorNr)) == -1) return false;
    if (write(fd, &size, sizeof(size)) == -1) return false;
    if (write(fd, _data.data(), size) == -1) return false;
    if (write(fd, &_checksum, sizeof(_checksum)) == -1) return false;
    if (write(fd, &_endian, sizeof(_endian)) == -1) return false;
    if (write(fd, &_compression, sizeof(_compression)) == -1) return false;
    return true;
}

std::unique_ptr<BandPlane> BandPlane::restoreIntoMemory(int fd)
{
    auto plane = std::make_unique<BandPlane>();
    size_t size = 0;

    if (read(fd, &plane->_colorNr, sizeof(plane->_colorNr)) <= 0) return nullptr;
    if (read(fd, &size, sizeof(size)) <= 0) return nullptr;
    
    plane->_data.resize(size);
    if (read(fd, plane->_data.data(), size) <= 0) return nullptr;
    
    if (read(fd, &plane->_checksum, sizeof(plane->_checksum)) <= 0) return nullptr;
    if (read(fd, &plane->_endian, sizeof(plane->_endian)) <= 0) return nullptr;
    if (read(fd, &plane->_compression, sizeof(plane->_compression)) <= 0) return nullptr;

    return plane;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

