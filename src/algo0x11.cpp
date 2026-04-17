/*
 * 	    algo0x11.cpp              (C) 2006-2008, Aurélien Croc (AP²C)
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
#include "algo0x11.h"
#include <cstring>
#include <cstdlib>
#include <vector>
#include <span>
#include <memory>
#include <cstdint>
#include <algorithm>
#include "bandplane.h"
#include "errlog.h"



/*
 * Fonctions locales
 * Local functions
 */
int Algo0x11::__compare(const void *n1, const void *n2)
{
    // n2 and n1 has been exchanged since the first
    // element of the array MUST be the biggest
    return static_cast<int>(*static_cast<const uint32_t *>(n2)) - 
           static_cast<int>(*static_cast<const uint32_t *>(n1));
}

bool Algo0x11::_lookupBestOccurs(std::span<const uint8_t> data)
{
    uint32_t occurs[COMPRESS_SAMPLE_RATE][2];
    bool oneIsPresent = false;
    uint8_t b;
    uint32_t i;
    uint32_t size = static_cast<uint32_t>(data.size());

    // Initialize the table
    // occurs[i][0] = number of occurences of the offset
    // occurs[i][1] = offset
    for (i=0; i < COMPRESS_SAMPLE_RATE; i++) {
        occurs[i][0] = 0;
        occurs[i][1] = i;
    }

    // Calculate the byte occurrence
    for (i=COMPRESS_SAMPLE_RATE; i < size; i += COMPRESS_SAMPLE_RATE) {
        b = data[i];
        for (uint32_t j=1; j < COMPRESS_SAMPLE_RATE; j++)
            if (data[i - j] == b)
                occurs[(j - 1)][0]++;
    }

    // Order the array
    qsort(occurs, COMPRESS_SAMPLE_RATE, sizeof(uint32_t)*2, __compare);

    // Set the pointer table to use for compression
    for (i=0; i < TABLE_PTR_SIZE; i++) {
        _ptrArray[i] = occurs[i][1] + 1;
        if (_ptrArray[i] == 1)
            oneIsPresent = true;
    }
    // Append the value 1 which improves the compression of multiple same bytes
    if (!oneIsPresent)
        _ptrArray[TABLE_PTR_SIZE-1] = 1;

    return true;
}

bool Algo0x11::_compress(std::span<const uint8_t> data, std::vector<uint8_t> &output)
{
    uint32_t r, uncompSize=0, maxCompSize, bestCompCounter, bestPtr;
    [[maybe_unused]] uint32_t w=4;
    uint32_t rawDataCounter = 0, rawDataCounterPtr=0;
    uint32_t size = static_cast<uint32_t>(data.size());

    // Create the output buffer with estimated size
    output.clear();
    output.resize(4 + TABLE_PTR_SIZE * 2); 

    // Print the table
    for (uint32_t i=0; i < TABLE_PTR_SIZE; i++) {
        uint16_t ptrVal = static_cast<uint16_t>(_ptrArray[i]);
        output.push_back(static_cast<uint8_t>(ptrVal & 0xFF));
        output.push_back(static_cast<uint8_t>((ptrVal >> 8) & 0xFF));
        if (_ptrArray[i] > uncompSize)
            uncompSize = _ptrArray[i];
    }
    // Update w manually to match logic or just use output.size()
    w = static_cast<uint32_t>(output.size());

    // Print the first uncompressed bytes
    if (uncompSize > MAX_UNCOMPRESSED_BYTES)
        uncompSize = MAX_UNCOMPRESSED_BYTES;
    
    std::memcpy(output.data(), &uncompSize, 4);
    
    for (r=0; r < uncompSize; r++) {
        output.push_back(data[r]);
    }
    w = static_cast<uint32_t>(output.size());

    //
    // Compress the data
    //
    do {
        maxCompSize = size - r > MAX_COMPRESSED_BYTES ? MAX_COMPRESSED_BYTES :
            size - r;

        // End of the compression
        if (!maxCompSize) {
            if (rawDataCounter)
                output[rawDataCounterPtr] = static_cast<uint8_t>(rawDataCounter - 1);
            break;

        // Try to compress the next piece of data
        } else if (maxCompSize >= 2) {
            bestCompCounter = 0;
            bestPtr = 0;

            // Check the best similar piece of data
            for (uint32_t i=0; i < TABLE_PTR_SIZE; i++) {
                uint32_t rTmp, counter;
               
                if (_ptrArray[i] > r)
                    continue;
                rTmp = r - _ptrArray[i];
                for (counter = 0; counter < maxCompSize; counter++)
                    if (data[r + counter] != data[rTmp + counter])
                        break;
                if (counter > bestCompCounter) {
                    bestCompCounter = counter;
                    bestPtr = i;
                }
            }

            // If the reproduced piece is large enough, use it!
            if (bestCompCounter > MIN_COMPRESSED_BYTES) {
                r += bestCompCounter;
                bestCompCounter -= 3;
                output.push_back(COMPRESSION_FLAG | (bestCompCounter & 0x7F));
                output.push_back(((bestCompCounter >> 1) & 0xC0) | (bestPtr & 0x3F));
                if (rawDataCounter) {
                    output[rawDataCounterPtr] = static_cast<uint8_t>(rawDataCounter - 1);
                    rawDataCounter = 0;
                }
                continue;
            }
        }

        // Else write the uncompressed data
        rawDataCounter++;
        if (rawDataCounter == 1) {
            rawDataCounterPtr = static_cast<uint32_t>(output.size());
            output.push_back(0); // placeholder
        } else if (rawDataCounter == MAX_UNCOMPRESSED_BYTES) {
            output[rawDataCounterPtr] = 0x7F;
            rawDataCounter = 0;
        }
        output.push_back(data[r]);
        r++;
    } while (r < size);

    return true;
}




/*
 * Constructeur - Destructeur
 * Init - Uninit
 */
Algo0x11::Algo0x11()
{
}

Algo0x11::~Algo0x11()
{
}



/*
 * Routine de compression
 * Compression routine
 */
std::unique_ptr<BandPlane> Algo0x11::compress([[maybe_unused]] const Request& request, std::span<const uint8_t> data, 
        uint32_t width, uint32_t height)
{
    uint32_t size = width * height / 8;
    std::vector<uint8_t> output;

    if (data.empty() || !size) {
        ERRORMSG(_("Invalid given data for compression (0x11)"));
        return nullptr;
    }

    // Lookup for the best occurs
    if (!_lookupBestOccurs(data) || 
        !_compress(data, output)) {
        return nullptr;
    }

    // Register the result into a band plane
    auto plane = std::make_unique<BandPlane>();
    uint32_t outputSize = static_cast<uint32_t>(output.size());
    plane->setData(std::move(output));
    plane->setEndian(BandPlane::Endian::Dependant);
    plane->setCompression(0x11);

    DEBUGMSG(_("Finished band encoding: type=0x11, size=%u"), outputSize);

    return plane;
}

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */
