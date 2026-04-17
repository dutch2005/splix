/*
 * 	    ppdfile.cpp               (C) 2006-2008, Aurélien Croc (AP²C)
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
#include "ppdfile.h"
#include <cups/ppd.h>
#include "errlog.h"



/*
 * Constructeur - Destructeur
 * Init - Uninit
 */
#include <cups/cups.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "errlog.h"

/*
 * Constructeur - Destructeur
 * Init - Uninit
 */
PPDFile::PPDFile() : _dest(nullptr), _dinfo(nullptr), _ppd(nullptr), 
    _num_options(0), _options(nullptr)
{
}

PPDFile::~PPDFile()
{
    close();
}

/*
 * Ouverture et fermeture du fichier PPD
 * Opening or closing PPD file
 */
bool PPDFile::open(const char *file, const char *version, const char *useropts)
{
    const char *printerName;
    const char *fileVersion;

    // Check if printer information was previously opened
    if (_dest || _dinfo) {
        ERRORMSG(_("Internal warning: printer info was previously opened. Please "
            "close it before opening a new one."));
        close();
    }

    if (file) _ppdPath = file;

    // Get the printer name from the environment
    printerName = getenv("PRINTER");
    if (!printerName)
        printerName = getenv("CUPS_DEST");

    _dest = cupsGetNamedDest(CUPS_HTTP_DEFAULT, printerName, NULL);
    if (!_dest) {
        // Fallback: try to find any destination if PRINTER is not set
        _dest = cupsGetNamedDest(CUPS_HTTP_DEFAULT, NULL, NULL);
    }

    if (!_dest) {
        WARNMSG(_("Cannot find printer destination (PRINTER=%s). Proceeding with defaults."), 
            printerName ? printerName : "NULL");
    } else {
        _dinfo = cupsCopyDestInfo(CUPS_HTTP_DEFAULT, _dest);
        if (!_dinfo) {
            WARNMSG(_("Cannot get destination info for printer %s. Proceeding with defaults."), _dest->name);
        }
    }

    // Direct PPD file loading fallback
    if (file) {
        _ppd = ppdOpenFile(file);
        if (!_ppd) {
            ERRORMSG(_("Cannot open PPD file %s: %s"), file, cupsLastErrorString());
        }
    }

    // Parse user options
    _num_options = cupsParseOptions(useropts, 0, &_options);

    // Check version (we still use the file for this if available, or skip)
    // NOTE: In a pure IPP world, version checking against a local PPD
    // might be obsolete, but we keep the logic if possible.
    fileVersion = get("FileVersion");
    if (fileVersion && strcmp(fileVersion, version)) {
        ERRORMSG(_("Invalid PPD version: %s (Expected: %s)"), fileVersion, version);
        // We continue anyway as IPP info might be compatible
    }

    return true;
}

void PPDFile::close()
{
    if (_dinfo) {
        cupsFreeDestInfo(_dinfo);
        _dinfo = nullptr;
    }
    if (_dest) {
        cupsFreeDests(1, _dest);
        _dest = nullptr;
    }
    if (_ppd) {
        ppdClose(_ppd);
        _ppd = nullptr;
    }
    if (_options) {
        cupsFreeOptions(_num_options, _options);
        _options = nullptr;
        _num_options = 0;
    }
}

/*
 * Obtention d'une donnée
 * Get a value
 */
PPDValue PPDFile::get(const char *name, const char *opt)
{
    const char *valStr = nullptr;
    PPDValue val;

    // 1. Search in job-specific options first
    valStr = cupsGetOption(name, _num_options, _options);

    // 2. Search in destination info (defaults/attributes)
    if (!valStr && _dinfo) {
        if (opt) {
            // It might be a choice for a specific option
            ipp_attribute_t *attr = cupsFindDestSupported(CUPS_HTTP_DEFAULT, 
                                        _dest, _dinfo, name);
            if (attr) {
                // Return default value if present
                valStr = cupsGetOption(name, _dest->num_options, _dest->options);
            }
        } else {
            // Check for general IPP attributes
            valStr = cupsGetOption(name, _dest->num_options, _dest->options);
            if (!valStr) {
                std::string cupsName = std::string("cups-") + name;
                valStr = cupsGetOption(cupsName.c_str(), _dest->num_options, 
                            _dest->options);
            }
        }
    }

    // 3. Fallback to direct PPD file loading
    if (!valStr && _ppd) {
        ppd_choice_t *choice = ppdFindMarkedChoice(_ppd, name);
        if (!choice) {
            ppd_option_t *option = ppdFindOption(_ppd, name);
            if (option) valStr = option->defchoice;
        } else {
            valStr = choice->choice;
        }
    }

    if (valStr)
        val.set(valStr);

    return val;
}

PPDValue PPDFile::getPageSize(const char *name)
{
    PPDValue val;
    cups_size_t size;

    if (!_dinfo) return val;

    if (cupsGetDestMediaByName(CUPS_HTTP_DEFAULT, _dest, _dinfo, name, 
            CUPS_MEDIA_FLAGS_DEFAULT, &size)) {
        // CUPS size is in 100ths of a millimeter. Convert to points (1/72 inch).
        float w = (static_cast<float>(size.width) / 2540.0f) * 72.0f;
        float h = (static_cast<float>(size.length) / 2540.0f) * 72.0f;
        float lm = (static_cast<float>(size.left) / 2540.0f) * 72.0f;
        float bm = (static_cast<float>(size.bottom) / 2540.0f) * 72.0f;
        val.set(w, h, lm, bm);
    } else if (_ppd) {
        // Fallback to direct PPD page size resolution
        ppd_size_t *psize = ppdPageSize(_ppd, name);
        if (psize) {
            val.set(psize->width, psize->length, psize->left, psize->bottom);
        }
    }

    return val;
}



/*
 * Gestion des valeurs du PPD
 * PPD values management
 */
PPDFile::Value::Value()
{
    _value = NULL;
    _out = NULL;
    _width = 0.;
    _height = 0.;
    _marginX = 0.;
    _marginY = 0.;
}

PPDFile::Value::Value(const char *value)
{
    _value = value;
    _out = value;
    _width = 0.;
    _height = 0.;
    _marginX = 0.;
    _marginY = 0.;
}

PPDFile::Value::~Value()
{
}

PPDFile::Value& PPDFile::Value::set(const char *value)
{
    _preformatted.clear();
    _value = value;
    _out = _value;

    return *this;
}

PPDFile::Value& PPDFile::Value::set(float width, float height, float marginX,
    float marginY)
{
    _width = width;
    _height = height;
    _marginX = marginX;
    _marginY = marginY;

    return *this;
}

PPDFile::Value& PPDFile::Value::setPreformatted()
{
    const char *str = _value;

    if (!str)
        return *this;

    _preformatted.clear();
    _preformatted.reserve(strlen(str));

    while (*str) {
        if (*str == '<' && strlen(str) >= 3 && isxdigit(*(str+1))) {
            char temp[3] = {0, 0, 0};

            str++;
            temp[0] = *str;
            str++;
            if (*str != '>' && (!isxdigit(*str) || *(str + 1) != '>')) {
                _preformatted += '<';
                _preformatted += temp[0];
                continue;
            }
            if (*str != '>') {
                temp[1] = *str;
                str++;
            }
            _preformatted += static_cast<char>(strtol(temp, nullptr, 16));
            if (*str == '>')
                str++;
            continue;
        }
        _preformatted += *str;
        str++;
    }
    _out = _preformatted.c_str();

    return *this;
}

std::string PPDFile::Value::deepCopy() const
{
    if (!_out)
        return "";
    return std::string(_out);
}

bool PPDFile::Value::isTrue() const
{
    if (!_out)
        return false;
    if (!strcmp(_out, "1") || !strcasecmp(_out, "true") || 
        !strcasecmp(_out, "enable") || !strcasecmp(_out, "enabled") || 
        !strcasecmp(_out, "yes") || !strcasecmp(_out, "on"))
        return true;
    return false;
}

bool PPDFile::Value::operator == (const char* val) const
{
    if (!_out)
        return false;
    return !strcasecmp(_out, val);
}

bool PPDFile::Value::operator != (const char* val) const
{
    if (!_out)
        return true;
    return strcasecmp(_out, val);
}



/*
 * Copy constructor
 */
PPDFile::Value::Value(const PPDFile::Value &val)
{
    _value = val._value;
    _preformatted = val._preformatted;
    _out = _preformatted.empty() ? _value : _preformatted.c_str();
    _width = val._width;
    _height = val._height;
    _marginX = val._marginX;
    _marginY = val._marginY;
}

/*
 * Opérateur d'assignation
 * Assignment operator
 */
PPDFile::Value& PPDFile::Value::operator = (const PPDFile::Value &val)
{
    if (this == &val)
        return *this;
    _value = val._value;
    _preformatted = val._preformatted;
    _out = _preformatted.empty() ? _value : _preformatted.c_str();
    _width = val._width;
    _height = val._height;
    _marginX = val._marginX;
    _marginY = val._marginY;
    return *this;
}


/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

