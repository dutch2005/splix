/*
 * 	    ppdfile.h                 (C) 2006-2008, Aurélien Croc (AP²C)
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
#ifndef _PPDFILE_H_
#define _PPDFILE_H_

#include <cups/cups.h>
#include <cups/ppd.h>
#include <stdlib.h>
#include <string>
#include <memory>
#include <vector>

/**
  * @class PPDFile
  * @brief This class provides an easy method to manage printer capabilities.
  *
  * This class provides methods to access printer capabilities and job options
  * using the modern CUPS Destination Information API.
  */
class PPDFile
{
    public:
        /**
          * @brief This class manages a PPD value.
          *
          * Use the defined type PPDValue to use this class.
          */
        class Value {
            protected:
                const char*         _value;
                std::string         _preformatted;
                const char*         _out;
                float               _width;
                float               _height;
                float               _marginX;
                float               _marginY;

            public:
                Value();
                Value(const char *value);
                virtual ~Value();

            public:
                PPDFile::Value&     set(const char *value);
                PPDFile::Value&     set(float width, float height, float
                                        marginX, float marginY);
                PPDFile::Value&     setPreformatted();

            public:
                float               width() const {return _width;}
                float               height() const {return _height;}
                float               marginX() const {return _marginX;}
                float               marginY() const {return _marginY;}
                bool                isNull() const {return _out ? false : true;}
                std::string         deepCopy() const;
                bool                isTrue() const;
                bool                isFalse() const {return !isTrue();}
                operator bool() const {return isTrue();}
                operator const char*() const {return _out;}
                operator unsigned long() const 
                        {return _out ? strtol(_out, (char**)NULL, 10) : 0;}
                operator long() const 
                        {return _out ? strtol(_out, (char**)NULL, 10) : 0;}
                operator float() const 
                        {return _out ? strtof(_out, (char**)NULL) : 0;}
                operator double() const 
                        {return _out ? strtod(_out, (char**)NULL) : 0;}
                operator long double() const 
                        {return _out ? strtold(_out, (char**)NULL) : 0;}
                bool    operator == (const char* val) const;
                bool    operator != (const char* val) const;
            public:
                Value(const Value& other);
                Value&  operator = (const Value &val);
        };

    protected:
        cups_dest_t     *_dest;
        cups_dinfo_t    *_dinfo;
        ppd_file_t      *_ppd;
        int              _num_options;
        cups_option_t   *_options;
        std::string      _ppdPath;

    public:
        PPDFile();
        /**
         * Destroy the instance.
         * the PPD file will be closed if it was opened.
         */
        virtual~ PPDFile();

    public:
        /**
          * Open a PPD file and check its integrity.
          * @param file the file path and name
          * @param version the current SpliX version
          * @param useropts the user options
          * @return TRUE if the PPD has been successfully opened. Otherwise it
          *         returns false.
          */
        bool                    open(const char *file, const char *version, 
                                    const char *useropts = "");
        /**
          * Close a previously opened PPD file.
          */
        void                    close();

    public:
        /**
          * Get the string associated to a key or a key and a group.
          * @param name the key name
          * @param opt the name of the group if the key is in the group.
          *            Otherwise it must be set to NULL
          * @return a PPDValue instance containing the string or NULL if the key
          *         or the group/key doesn't exists or if there is no data 
          *         associated.
          */
        Value                   get(const char *name, const char *opt=NULL);
        /**
          * Get the page size information.
          * @param name the page format name
          * @return a PPDValue instance containing the width and the height of
          *         the page format requested.
          */
        Value                   getPageSize(const char *name);
};

/**
 * Represent a PPD value
 */
typedef PPDFile::Value PPDValue;


#endif /* _PPDFILE_H_ */

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

