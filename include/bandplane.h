/*
 * 	    bandplane.h               (C) 2006-2008, Aurélien Croc (AP²C)
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
#ifndef _BANDPLANE_H_
#define _BANDPLANE_H_

#include <vector>
#include <memory>
#include <cstdint>

/**
  * @brief This class contains data related to a band plane.
  *
  */
class BandPlane
{
    public:
        enum class Endian : uint8_t {
            /** Machine dependant */
            Dependant,
            /** Big endian */
            BigEndian,
            /** Little endian */
            LittleEndian,
        };

    protected:
        uint8_t                 _colorNr;
        std::vector<uint8_t>    _data;
        uint32_t                _checksum;
        Endian                  _endian;
        uint8_t                 _compression;

    public:
        /**
          * Initialize the band plane instance.
          */
        BandPlane();
        /**
          * Destroy the instance
          */
        virtual ~BandPlane();

    public:
        /**
          * Set the color number of this plane.
          * @param nr the color number
          */
        void                    setColorNr(uint8_t nr) {_colorNr = nr;}
        /**
          * Set the data buffer.
          * @param data the data buffer
          */
        void                    setData(std::vector<uint8_t> data);
        /**
          * Set the endian to use.
          * @param endian the endian to use.
          */
        void                    setEndian(Endian endian) {_endian = endian;}
        /**
         * Set the compression algorithm used.
         * @param compression the compression algorithm used.
         */
        void                    setCompression(uint8_t compression)
                                    {_compression = compression;}

        /**
          * @return the color number.
          */
        uint8_t                 colorNr() const {return _colorNr;}
        /**
          * @return the data size.
          */
        size_t                  dataSize() const {return _data.size();}
        /**
          * @return the data.
          */
        const uint8_t*          data() const {return _data.data();}
        /**
          * @return the endian to use.
          */
        Endian                  endian() const {return _endian;}
        /**
          * @return the checksum.
          */
        uint32_t                checksum() const {return _checksum;}
        /**
         * @return the compression algorithm used.
         */
        uint8_t                 compression() const {return _compression;}

    public:
        /**
          * Swap this instance on the disk.
          * @param fd the file descriptor where the instance has to be swapped
          * @return TRUE if the instance has been successfully swapped. 
          *         Otherwise it returns FALSE.
          */
        bool                    swapToDisk(int fd);
        /**
          * Restore an instance from the disk into memory.
          * @param fd the file descriptor where the instance has been swapped
          * @return a bandplane instance if it has been successfully restored. 
          *         Otherwise it returns NULL.
          */
        static std::unique_ptr<BandPlane> restoreIntoMemory(int fd);
};

#endif /* _BANDPLANE_H_ */

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

