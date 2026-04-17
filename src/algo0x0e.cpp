/*
 *         algo0x0e.cpp SpliX is Copyright 2006-2008 by Aurélien Croc
 *                      This file is a SpliX derivative work
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
 * $Id$
 *
 * --
 * This code is written by Leonardo Hamada
 */
#include "algo0x0e.h"
#include <vector>
#include <span>
#include <memory>
#include <cstdint>
#include <algorithm>
#include "errlog.h"
#include "request.h"
#include "printer.h"
#include "bandplane.h"

#define getData(x) data[(x)]

#define setOutputData(y,z) output[(y)]=(z)

#define codecR(R, V, n)                         \
    addReplicativeRun(output, R, V)

#define codecL(X, L, B, n)                                      \
    addLiteralSequence(output, data, X, L, B)



/*
 * Constructor
 */
Algo0x0E::Algo0x0E()
{
}

Algo0x0E::~Algo0x0E()
{
}

inline void Algo0x0E::addLiteralSequence(
                                        std::vector<uint8_t> &output,
                                        std::span<const uint8_t> data,
                                        uint32_t position,
                                        uint32_t length,
                                        uint32_t blanks )
{
    /* Set control value for the literal chunk length. */
    uint32_t tmp = length + blanks - 1;

    output.push_back(0x80 | static_cast<uint8_t>(tmp >> 8));
    output.push_back(static_cast<uint8_t>(tmp));

    /* Copy literal data chunk. */
    for(uint32_t w=0; w<length; ++w){
        output.push_back(data[position + w]);
    }

    /* Pad with required blanks. */
    for(uint32_t w=0; w<blanks; ++w){
        output.push_back(0xff);
    }
}

inline void Algo0x0E::addReplicativeRun(
                                       std::vector<uint8_t> &output,
                                       uint32_t runs,
                                       uint8_t value )
{
    /* Verify run numbers to choose appropriate control header. */
    if ( 65 >= runs ) {
        output.push_back(0x7f & static_cast<uint8_t>(1L - runs));
    } else {
        uint32_t t = 0xffff - runs + 2;

        output.push_back(static_cast<uint8_t>(t >> 8));
        output.push_back(static_cast<uint8_t>(t));
    }

    /* Set value to be replicated. */
    output.push_back(value);
}



/* Check if segment at 'e' position and forward can be encoded as 
   consecutive runs. 'L' limits the width of the seek. */
uint32_t Algo0x0E::verifyGain(uint32_t e, 
                                  uint32_t L,
                                  std::span<const uint8_t> data)
{
    uint32_t g=0, u=1;
    while(e+u<L){
    gain_seek:
        if(getData(e+u-1)==getData(e+u)){
            if(++u==4){
                break;
            }
        }else{
            break;
        };
    }
    if(u>=2){
        g+=(u<=65)?(u-2):(u-3);
        if(g<2){
            e+=u;
            if(e+1<L){
                u=1;
                goto gain_seek;
            }
        }
    }
    return g;
}

uint32_t Algo0x0E::encodeReplications(uint32_t q,
                                          uint32_t L,
                                          std::span<const uint8_t> data,
                                          std::vector<uint8_t> &output)
{
    uint32_t r;
 runs_enc: r=1;
    while(q+r<L){
        if(getData(q+r-1)==getData(q+r)){
            r++;
        }else{
            break;
        }
    }
    if(r>=2){
        codecR(r,getData(q),0);
        q+=r;
        goto runs_enc;
    }
    return q;
}

uint32_t Algo0x0E::locateBackwardReplications(uint32_t L,
                                                  std::span<const uint8_t> data)
{
    /* This must be signed.*/
    int32_t i=static_cast<int32_t>(L)-1, r;     
 seek_literal2: r=1;
    while(i-r>=0){
        if(data[i-r+1]==data[i-r]){
            r++;
        }else{
            break;
        }
    }
    if(r>1){    
        i=i-r;
        goto seek_literal2;
    }
    return static_cast<uint32_t>(i+1);
}
std::unique_ptr<BandPlane> Algo0x0E::compress([[maybe_unused]] const Request & request, std::span<const uint8_t> data,
                               uint32_t width, uint32_t height)
{
    /* Basic parameters validation. */
    if ( data.empty() || !height || !width ) {
        ERRORMSG(_("Invalid given data for compression: 0xe"));
        return nullptr;
    }

    /* We will interpret the band heigth of 128 pixels as 600 DPI printing
       request. Likewise, height of 64 pixels as 300 DPI printing. */
    if ( ! ( 128 == height || 64 == height ) ) {
        ERRORMSG(_("Invalid band height for compression: 0xe"));
        return nullptr;
    }

    /* The row-bytes of the bitmap. */
    const uint32_t rowBytes = ( width + 7 ) / 8;

    /* This is the allowed raw data size per scan-line. */
    const uint32_t maxWorkRb = ( 0x40 == height ) ?
        0x09A0 / 8 : 0x1360 / 8;

    /* If rowBytes is larger than allowed size, print will be cropped. */
    const uint32_t workRb = ( rowBytes > maxWorkRb ) ?
        maxWorkRb : rowBytes;

    std::vector<uint8_t> output;
    try {
        /* Estimate a buffer size equal to 2-byte control header overhead +
           maxWorkRb, times the bitmap height + up to 3-byte padding at end. */
        output.reserve(( 2 + maxWorkRb ) * height + 3);
    } catch( const std::bad_alloc & ){
        /* Catch error if buffer creation fails. */
        ERRORMSG(_("Could not allocate work buffer for encoding: 0xe"));
        return nullptr;
    }

    /* Main encoding loop for each scan-line.
       Top to bottom scan-line processing. */
    for (uint32_t h = height; h > 0; --h) {
        /*
          i: index into data.
          F: last replication encodable marker.
          E: resized WorkRb per scan-line.
          B: blank paddings.
        */
        uint32_t i, F, E, B;

        // Current scanline view
        std::span<const uint8_t> currentData = data.subspan(0, std::min<size_t>(workRb, data.size()));

        /* Adjust this working scan-line size
           up to where there is no blank bytes on the right end. */
        for(E=workRb;(E>0)&&(currentData[E-1]==0xff);E--)
        /* Empty statement. */
        ;

        /* Determine the number of padding blank bytes to the right
           end of the scan-line relative to constant max. width. */ 
        B=maxWorkRb-E;
        
        /* If scan-line is not blank and the number of blank padding is 
           not 1, seek the beginning position of the last scan-line segment,
           when it can be encoded as contiguous replication runs. */
        F=((E>0)&&(B!=1))?locateBackwardReplications(E,currentData):E;
  
        /* Try to encode the first segment as replication runs. */
        i=(F>0)?encodeReplications(0,F,currentData,output):0;

        /* Continue to encode the rest of data as replication or literal
           segment chunks as appropriate. */
        /* l: length of cumulative literal segment.*/
        uint32_t l=0;
        while(i+l+1<F){
            if(currentData[i+l+1]!=currentData[i+l]){
                l++;
            }else{
                if(verifyGain(i+l,F,currentData)>=2){
                    addLiteralSequence(output, currentData, i, l, 0);
                    i=encodeReplications(i+l,F,currentData,output);
                    l=0;                                
                }else{
                    l++;
                }
            }
        }
        
        /* Encode last remaining segments and white paddings. */  
        if(F<E){
            /* Encode previous literal segment that remains unencoded. */
            if(i<F){
                addLiteralSequence(output, currentData, i, F-i, 0);
            }
            /* Encode the last non blank replication runs. */
            i=encodeReplications(F,E,currentData,output);
            /* Ends a scan-line with required white paddings */
            if(B>=2){
                addReplicativeRun(output, B, 0xff);
            }
        }else{
            /* When the end of non blank data of a scan-line does not 
               end with replication runs, encode with literal sequence. */  
            if(B>=2){
                /* Encode previous literal segment that remains unencoded. */
                if(i<E){
                    addLiteralSequence(output, currentData, i, E-i, 0);
                }
                /* Ends a scan-line with required white paddings. */
                addReplicativeRun(output, B, 0xff);
            }else if((B>0)||(i<E)){
                /* Ends a scan-line encoding with a 
                   literal sequence with blanks if any. */
                addLiteralSequence(output, currentData, i, E-i, B);
            }
        }

        if( h > 1 ){
            /* Proceed to the next scan-line. */
            data = data.subspan(std::min<size_t>(rowBytes, data.size()));
        }
    }

    /* Zero value byte padding for data size alignment to 4-bytes bounds. */
    uint32_t zerosPad = 4 - ( static_cast<uint32_t>(output.size()) % 4 );

    /* Check if it is already aligned. */
    if ( 4 > zerosPad ){
        while ( zerosPad-- ){
            output.push_back(0);
        }
    }

    /* Prepare to return data encoded by algorithm 0xe. */
    auto plane = std::make_unique<BandPlane>();

    /* Regsiter data and its size. */
    uint32_t finalSize = static_cast<uint32_t>(output.size());
    plane->setData( std::move(output) );
    plane->setEndian( BandPlane::Endian::Dependant );

    /* Set this band encoding type. */
    plane->setCompression( 0xe );
    DEBUGMSG(_("Finished band encoding: type=0xe, size=%u"), finalSize);
    /* Bye-bye. */
    return plane;
}

