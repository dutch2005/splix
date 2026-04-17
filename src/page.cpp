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
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include "band.h"
#include "errlog.h"

/*
 * This magic formula reverse the bit of a byte. ie. the bit 1 becomes the 
 * bit 8, the bit 2 becomes the bit 7 etc.
 */
static constexpr uint8_t REVERSE_BITS(uint8_t n) {
    uint64_t res = (static_cast<uint64_t>(n) * 0x0202020202ULL & 0x010884422010ULL) % 1023;
    return static_cast<uint8_t>(res);
}

/*
 * Constructeur - Destructeur
 * Init - Uninit 
 */
Page::Page()
    : _xResolution(0),
      _yResolution(0),
      _width(0),
      _height(0),
      _colors(0),
      _pageNr(0),
      _copiesNr(0),
      _compression(0),
      _empty(true),
      _bandsNr(0),
      _lastBand(nullptr)
{}

Page::~Page() = default;

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
void Page::setPlaneBuffer(uint8_t color, std::vector<uint8_t> buffer)
{
    if (color < 4) {
        _planes[color] = std::move(buffer);
        _empty = false;
    }
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

/*
 * Mise sur disque / Rechargement
 * Swapping / restoring
 */
bool Page::swapToDisk(int fd)
{
    if (!_planes[0].empty() || !_planes[1].empty() || !_planes[2].empty() || !_planes[3].empty()) {
        ERRORMSG(_("Cannot swap page instance which still contains bitmap "
            "representation"));
        return false;
    }
    
    if (write(fd, &_xResolution, sizeof(_xResolution)) == -1) return false;
    if (write(fd, &_yResolution, sizeof(_yResolution)) == -1) return false;
    if (write(fd, &_width, sizeof(_width)) == -1) return false;
    if (write(fd, &_height, sizeof(_height)) == -1) return false;
    if (write(fd, &_colors, sizeof(_colors)) == -1) return false;
    if (write(fd, &_pageNr, sizeof(_pageNr)) == -1) return false;
    if (write(fd, &_copiesNr, sizeof(_copiesNr)) == -1) return false;
    if (write(fd, &_compression, sizeof(_compression)) == -1) return false;
    if (write(fd, &_empty, sizeof(_empty)) == -1) return false;
    if (write(fd, &_bandsNr, sizeof(_bandsNr)) == -1) return false;
    
    if (0x15 == _compression && _bandsNr > 0 && !_bih.empty()) {
        if (write(fd, _bih.data(), 20) == -1) return false;
    }
    
    Band* band = _firstBand.get();
    while (band) {
        if (!band->swapToDisk(fd))
            return false;
        band = band->sibling();
    }

    return true;
}

std::unique_ptr<Page> Page::restoreIntoMemory(int fd)
{
    auto page = std::make_unique<Page>();
    uint32_t bandsCount = 0;

    if (read(fd, &page->_xResolution, sizeof(page->_xResolution)) <= 0) return nullptr;
    if (read(fd, &page->_yResolution, sizeof(page->_yResolution)) <= 0) return nullptr;
    if (read(fd, &page->_width, sizeof(page->_width)) <= 0) return nullptr;
    if (read(fd, &page->_height, sizeof(page->_height)) <= 0) return nullptr;
    if (read(fd, &page->_colors, sizeof(page->_colors)) <= 0) return nullptr;
    if (read(fd, &page->_pageNr, sizeof(page->_pageNr)) <= 0) return nullptr;
    if (read(fd, &page->_copiesNr, sizeof(page->_copiesNr)) <= 0) return nullptr;
    if (read(fd, &page->_compression, sizeof(page->_compression)) <= 0) return nullptr;
    if (read(fd, &page->_empty, sizeof(page->_empty)) <= 0) return nullptr;
    if (read(fd, &bandsCount, sizeof(bandsCount)) <= 0) return nullptr;
    
    if (0x15 == page->_compression && bandsCount > 0) {
        uint8_t bih[20];
        if (read(fd, bih, 20) <= 0) return nullptr;
        page->setBIH(bih, 20);
    }
    
    for (uint32_t i = 0; i < bandsCount; i++) {
        auto band = Band::restoreIntoMemory(fd);
        if (!band) {
            return nullptr;
        }
        page->registerBand(std::move(band));
    }

    return page;
}

void Page::setBIH(const uint8_t *bih_data, size_t size) {
    _bih.assign(bih_data, bih_data + size);
}

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

