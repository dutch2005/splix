/*
 * 	    band.cpp                  (C) 2006-2008, Aurélien Croc (AP²C)
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
#include "band.h"
#include <unistd.h>
#include "errlog.h"
#include "bandplane.h"
#include "page.h"

/*
 * Constructeur - Destructeur
 * Init - Uninit 
 */
Band::Band()
{
    _bandNr = 0;
    _parent = nullptr;
    _width = 0;
    _height = 0;
}

Band::Band(uint32_t nr, uint32_t width, uint32_t height)
{
    _parent = nullptr;
    _bandNr = nr;
    _width = width;
    _height = height;
}

Band::~Band()
{
}


void Band::registerPlane(std::unique_ptr<BandPlane> plane)
{
    if (_planes.size() < 4) {
        _planes.push_back(std::move(plane));
    }
}


/*
 * Mise sur disque / Rechargement
 * Swapping / restoring
 */
bool Band::swapToDisk(int fd)
{
    uint32_t colors = static_cast<uint32_t>(_planes.size());
    if (write(fd, &_bandNr, sizeof(_bandNr)) == -1) return false;
    if (write(fd, &colors, sizeof(colors)) == -1) return false;
    if (write(fd, &_width, sizeof(_width)) == -1) return false;
    if (write(fd, &_height, sizeof(_height)) == -1) return false;
    
    for (auto &plane : _planes) {
        if (!plane->swapToDisk(fd))
            return false;
    }
    return true;
}

std::unique_ptr<Band> Band::restoreIntoMemory(int fd)
{
    uint32_t colors = 0;
    auto band = std::make_unique<Band>();

    if (read(fd, &band->_bandNr, sizeof(band->_bandNr)) <= 0) return nullptr;
    if (read(fd, &colors, sizeof(colors)) <= 0) return nullptr;
    if (read(fd, &band->_width, sizeof(band->_width)) <= 0) return nullptr;
    if (read(fd, &band->_height, sizeof(band->_height)) <= 0) return nullptr;

    for (uint32_t i = 0; i < colors; i++) {
        auto plane = BandPlane::restoreIntoMemory(fd);
        if (!plane) {
            return nullptr;
        }
        band->registerPlane(std::move(plane));
    }

    return band;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

