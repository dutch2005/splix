/*
 * 	    algo0x13.cpp              (C) 2006-2008, Aurélien Croc (AP²C)
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
#include "algo0x13.h"
#include <cstring>
#include <vector>
#include <memory>
#include <deque>
#include <span>
#include <cstdint>
#include <algorithm>
#include "errlog.h"
#include "request.h"
#include "printer.h"
#include "bandplane.h"

#ifndef DISABLE_JBIG

/*
 * Fonction de rappel
 * Callback
 */
void Algo0x13::_callback(unsigned char *data, size_t len, void *arg)
{
    info_t* info = static_cast<info_t *>(arg);

    if (!len)
        return;

    // It's the first BIH
    if (info->list->empty()) {
        std::vector<uint8_t> bih(data, data + len);
        auto plane = std::make_unique<BandPlane>();
        plane->setData(std::move(bih));
        plane->setEndian(BandPlane::Endian::BigEndian);
        plane->setCompression(0x13);
        info->list->push_back(std::move(plane));
        if (len != 20)
            ERRORMSG(_("the first BIH *MUST* be 20 bytes long (currently=%zu"), len);
    } else {
        while (len > 0) {
            // Full band: register it
            if (!info->currentData.empty() && info->currentData.size() == info->maxSize) {
                auto plane = std::make_unique<BandPlane>();
                plane->setData(std::move(info->currentData));
                plane->setEndian(BandPlane::Endian::BigEndian);
                plane->setCompression(0x13);
                info->list->push_back(std::move(plane));
                info->currentData.clear();
            }

            // Collect data
            uint32_t freeSpace = info->maxSize - static_cast<uint32_t>(info->currentData.size());
            uint32_t toCopy = (freeSpace < static_cast<uint32_t>(len)) ? freeSpace : static_cast<uint32_t>(len);
            
            size_t oldSize = info->currentData.size();
            info->currentData.resize(oldSize + toCopy);
            std::memcpy(info->currentData.data() + oldSize, data, toCopy);
            
            data += toCopy;
            len -= toCopy;
        }
    }
}



/*
 * Constructeur - Destructeur
 * Init - Uninit
 */
Algo0x13::Algo0x13()
{
    _compressed = false;
}

Algo0x13::~Algo0x13()
{
}



/*
 * Routine de compression
 * Compression routine
 */
std::unique_ptr<BandPlane> Algo0x13::compress(const Request& request, std::span<const uint8_t> data, 
        uint32_t width, uint32_t height)
{
    jbg85_enc_state state;
    uint32_t i, wbytes;
    info_t info = {&_list, {}, 0};

    if (data.empty() || !width || !height) {
        ERRORMSG(_("Invalid given data for compression (0x13)"));
        return nullptr;
    }

    // Compress if it's the first time
    if (!_compressed) {
        info.maxSize = static_cast<uint32_t>(request.printer()->packetSize());
        if (!info.maxSize) {
            ERRORMSG(_("PacketSize is set to 0!"));
            info.maxSize = 512*1024;
        }
        wbytes = (width + 7) / 8;
        jbg85_enc_init(&state, width, height, _callback, &info);
        jbg85_enc_options(&state, JBG_LRLTWO | JBG_TPBON, height, 0);
        for (i = 0; i < height; i++) {
            const uint8_t* curr = &data[i * wbytes];
            const uint8_t* prev = (i > 0) ? &data[(i - 1) * wbytes] : nullptr;
            const uint8_t* prev2 = (i > 1) ? &data[(i - 2) * wbytes] : nullptr;

            jbg85_enc_lineout(&state,
                              const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(curr)),
                              const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(prev)),
                              const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(prev2)));
        }

        // Register the last band
        if (!info.currentData.empty()) {
            auto plane = std::make_unique<BandPlane>();
            plane->setData(std::move(info.currentData));
            plane->setEndian(BandPlane::Endian::BigEndian);
            plane->setCompression(0x13);
            _list.push_back(std::move(plane));
        }
        _compressed = true;
    }

    if (_list.empty())
        return nullptr;

    auto plane = std::move(_list.front());
    _list.pop_front();
    return plane;
}

#endif /* DISABLE_JBIG */

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

