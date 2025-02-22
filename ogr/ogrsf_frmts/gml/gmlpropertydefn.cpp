/**********************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLPropertyDefn
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "gmlreader.h"

#include <cstring>

#include "cpl_conv.h"
#include "cpl_string.h"


/************************************************************************/
/*                           GMLPropertyDefn                            */
/************************************************************************/

GMLPropertyDefn::GMLPropertyDefn( const char *pszName,
                                  const char *pszSrcElement ) :
    m_pszName(CPLStrdup(pszName)),
    m_eType(GMLPT_Untyped),
    m_nWidth(0),
    m_nPrecision(0),
    m_pszSrcElement(pszSrcElement ? CPLStrdup(pszSrcElement) : nullptr),
    m_nSrcElementLen(pszSrcElement ? strlen(pszSrcElement) : 0),
    m_pszCondition(nullptr),
    m_bNullable(true)
{}

/************************************************************************/
/*                          ~GMLPropertyDefn()                          */
/************************************************************************/

GMLPropertyDefn::~GMLPropertyDefn()

{
    CPLFree( m_pszName );
    CPLFree( m_pszSrcElement );
    CPLFree( m_pszCondition );
}

/************************************************************************/
/*                           SetSrcElement()                            */
/************************************************************************/

void GMLPropertyDefn::SetSrcElement( const char *pszSrcElement )

{
    CPLFree(m_pszSrcElement);
    if( pszSrcElement != nullptr )
    {
        m_nSrcElementLen = strlen(pszSrcElement);
        m_pszSrcElement = CPLStrdup(pszSrcElement);
    }
    else
    {
        m_nSrcElementLen = 0;
        m_pszSrcElement = nullptr;
    }
}

/************************************************************************/
/*                           SetCondition()                             */
/************************************************************************/

void GMLPropertyDefn::SetCondition( const char *pszCondition )
{
    CPLFree(m_pszCondition);
    m_pszCondition = pszCondition != nullptr ? CPLStrdup(pszCondition) : nullptr;
}

/************************************************************************/
/*                        AnalysePropertyValue()                        */
/*                                                                      */
/*      Examine the passed property value, and see if we need to        */
/*      make the field type more specific, or more general.             */
/************************************************************************/

void GMLPropertyDefn::AnalysePropertyValue( const GMLProperty *psGMLProperty,
                                            bool bSetWidth )

{
/* -------------------------------------------------------------------- */
/*      Does the string consist entirely of numeric values?             */
/* -------------------------------------------------------------------- */
    bool bIsReal = false;

    for( int j = 0; j < psGMLProperty->nSubProperties; j++ )
    {
        if (j > 0)
        {
            if( m_eType == GMLPT_Integer )
            {
                m_eType = GMLPT_IntegerList;
            }
            else if( m_eType == GMLPT_Integer64 )
            {
                m_eType = GMLPT_Integer64List;
            }
            else if( m_eType == GMLPT_Real )
            {
                m_eType = GMLPT_RealList;
            }
            else if( m_eType == GMLPT_String )
            {
                m_eType = GMLPT_StringList;
                m_nWidth = 0;
            }
            else if( m_eType == GMLPT_Boolean )
                m_eType = GMLPT_BooleanList;
        }
        const char* pszValue = psGMLProperty->papszSubProperties[j];
/* -------------------------------------------------------------------- */
/*      If it is a zero length string, just return.  We can't deduce    */
/*      much from this.                                                 */
/* -------------------------------------------------------------------- */
        if( *pszValue == '\0' )
            continue;

        const CPLValueType valueType = CPLGetValueType(pszValue);

        if (valueType == CPL_VALUE_STRING &&
            m_eType != GMLPT_String &&
            m_eType != GMLPT_StringList )
        {
            if( (m_eType == GMLPT_Untyped || m_eType == GMLPT_Boolean) &&
                (strcmp(pszValue, "true") == 0 ||
                 strcmp(pszValue, "false") == 0) )
            {
                m_eType = GMLPT_Boolean;
            }
            else if( m_eType == GMLPT_BooleanList )
            {
                if( !(strcmp(pszValue, "true") == 0 ||
                      strcmp(pszValue, "false") == 0) )
                    m_eType = GMLPT_StringList;
            }
            else if( m_eType == GMLPT_IntegerList ||
                     m_eType == GMLPT_Integer64List ||
                     m_eType == GMLPT_RealList )
            {
                m_eType = GMLPT_StringList;
            }
            else
            {
                m_eType = GMLPT_String;
            }
        }
        else
        {
            bIsReal = valueType == CPL_VALUE_REAL;
        }

        if( m_eType == GMLPT_String )
        {
            if( bSetWidth )
            {
                // Grow the Width to the length of the string passed in.
                const int nWidth = static_cast<int>(strlen(pszValue));
                if ( m_nWidth < nWidth )
                    SetWidth(nWidth);
            }
        }
        else if( m_eType == GMLPT_Untyped || m_eType == GMLPT_Integer ||
                 m_eType == GMLPT_Integer64 )
        {
            if( bIsReal )
                m_eType = GMLPT_Real;
            else if( m_eType != GMLPT_Integer64 )
            {
                const GIntBig nVal = CPLAtoGIntBig(pszValue);
                if( !CPL_INT64_FITS_ON_INT32(nVal) )
                    m_eType = GMLPT_Integer64;
                else
                    m_eType = GMLPT_Integer;
            }
        }
        else if( (m_eType == GMLPT_IntegerList ||
                  m_eType == GMLPT_Integer64List) && bIsReal )
        {
            m_eType = GMLPT_RealList;
        }
        else if( m_eType == GMLPT_IntegerList &&
                 valueType == CPL_VALUE_INTEGER )
        {
            GIntBig nVal = CPLAtoGIntBig(pszValue);
            if( !CPL_INT64_FITS_ON_INT32(nVal) )
                m_eType = GMLPT_Integer64List;
        }
    }
}

/************************************************************************/
/*                       GMLGeometryPropertyDefn                        */
/************************************************************************/

GMLGeometryPropertyDefn::GMLGeometryPropertyDefn( const char *pszName,
                                                  const char *pszSrcElement,
                                                  int nType,
                                                  int nAttributeIndex,
                                                  bool bNullable ) :
    m_pszName((pszName == nullptr || pszName[0] == '\0') ?
              CPLStrdup(pszSrcElement) : CPLStrdup(pszName)),
    m_pszSrcElement(CPLStrdup(pszSrcElement)),
    m_nGeometryType(nType),
    m_nAttributeIndex(nAttributeIndex),
    m_bNullable(bNullable)
{}

/************************************************************************/
/*                       ~GMLGeometryPropertyDefn                       */
/************************************************************************/

GMLGeometryPropertyDefn::~GMLGeometryPropertyDefn()
{
    CPLFree(m_pszName);
    CPLFree(m_pszSrcElement);
}

/************************************************************************/
/*                           MergeSRSName()                             */
/************************************************************************/

void GMLGeometryPropertyDefn::MergeSRSName( const std::string& osSRSName )

{
    if(!m_bSRSNameConsistent)
        return;

    if( m_osSRSName.empty() )
    {
        m_osSRSName = osSRSName;
    }
    else
    {
        m_bSRSNameConsistent = osSRSName == m_osSRSName;
        if (!m_bSRSNameConsistent)
        {
            m_osSRSName.clear();
        }
    }
}
