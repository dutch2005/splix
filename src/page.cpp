/*
 * 	    page.cpp                  (C) 2006-2008, Aurélien Croc (AP²C)
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
#include "page.h"
#include "sp_portable.h"
#include <algorithm>
#include <cstring>
#include "band.h"
#include "errlog.h"

Page::Page() = default;
Page::~Page() = default;

/*
 * This magic formula reverse the bit of a byte. ie. the bit 1 becomes the 
 * bit 8, the bit 2 becomes the bit 7 etc.
 */
static constexpr uint8_t REVERSE_BITS(uint8_t n) {
    uint64_t res = (static_cast<uint64_t>(n) * 0x0202020202ULL & 0x010884422010ULL) % 1023;
    return static_cast<uint8_t>(res);
}


/*
 * Enregistrement d'une nouvelle bande
 * Register a new band
 */
void Page::registerBand(std::unique_ptr<Band> band)
{
    band->registerParent(this);
    Band* raw_band = band.get();
    
    if (_lastBand)
        _lastBand->registerSibling(std::move(band));
    else
        _firstBand = std::move(band);
    
    _lastBand = raw_band;
    _bandsNr++;
}

/*
 * Register a new color plane.
 */
SP::Result<> Page::setPlaneBuffer(uint8_t color, std::vector<uint8_t> buffer)
{
    if (color >= 4) {
        return SP::Unexpected(SP::Error::InvalidArgument);
    }

    // Harden: check for excessive memory allocation (safety cap 512MB per plane)
    if (buffer.size() > 512 * 1024 * 1024) {
        ERRORMSG(_("Plane buffer too large: %zu bytes"), buffer.size());
        return SP::Unexpected(SP::Error::MemoryError);
    }

    try {
        _planes[color] = std::move(buffer);
    } catch (const std::bad_alloc&) {
        return SP::Unexpected(SP::Error::MemoryError);
    }
    
    _empty = false;
    return {};
}

/*
 * Rotation des couches
 * Rotate bitmaps planes
 */
void Page::rotate()
{
    if (_width == 0 || _height == 0) return;
    
    size_t size = static_cast<size_t>(_width) * _height / 8;
    size_t midSize = size / 2;

    for (uint8_t i = 0; i < _colors; i++) {
        if (_planes[i].empty()) continue;
        
        auto& plane = _planes[i];
        for (size_t j = 0; j < midSize; j++) {
            uint8_t tmp = plane[j];
            plane[j] = REVERSE_BITS(plane[size - j - 1]);
            plane[size - j - 1] = REVERSE_BITS(tmp);
        }
    }
}

/*
 * Libération de la mémoire utilisée par les couches
 * Flush the planes
 */
void Page::flushPlanes()
{
    for (auto& plane : _planes) {
        plane.clear();
        plane.shrink_to_fit();
    }
    _empty = false;
}



void Page::setBIH(const uint8_t *bih_data, size_t size) {
    _bih.assign(bih_data, bih_data + size);
}

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

