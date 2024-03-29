/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2019, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     RdCostX86.cpp
    \brief    RD cost computation class, SIMD version
*/

#include <math.h>
#include <limits>
#include "CommonDefX86.h"
#include "../Rom.h"
#include "../RdCost.h"

#ifdef TARGET_SIMD_X86

template< typename Torg, typename Tcur, X86_VEXT vext >
Distortion RdCost::xGetSSE_SIMD( const DistParam &rcDtParam )
{
  if( rcDtParam.bitDepth > 10 )
    return RdCost::xGetSSE( rcDtParam );

  const Torg* pSrc1     = (const Torg*)rcDtParam.org.buf;
  const Tcur* pSrc2     = (const Tcur*)rcDtParam.cur.buf;
  int  iRows            = rcDtParam.org.height;
  int  iCols            = rcDtParam.org.width;
  const int iStrideSrc1 = rcDtParam.org.stride;
  const int iStrideSrc2 = rcDtParam.cur.stride;

  const uint32_t uiShift = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;
  unsigned int uiRet = 0;

  if( vext >= AVX2 && ( iCols & 15 ) == 0 )
  {
#ifdef USE_AVX2
    __m256i Sum = _mm256_setzero_si256();
    for( int iY = 0; iY < iRows; iY++ )
    {
      for( int iX = 0; iX < iCols; iX+=16 )
      {
        __m256i Src1 = ( sizeof( Torg ) > 1 ) ? ( _mm256_lddqu_si256( ( __m256i* )( &pSrc1[iX] ) ) ) : ( _mm256_unpacklo_epi8( _mm256_permute4x64_epi64( _mm256_castsi128_si256( _mm_lddqu_si128( ( __m128i* )( &pSrc1[iX] ) ) ), 0xD8 ), _mm256_setzero_si256() ) );
        __m256i Src2 = ( sizeof( Tcur ) > 1 ) ? ( _mm256_lddqu_si256( ( __m256i* )( &pSrc2[iX] ) ) ) : ( _mm256_unpacklo_epi8( _mm256_permute4x64_epi64( _mm256_castsi128_si256( _mm_lddqu_si128( ( __m128i* )( &pSrc2[iX] ) ) ), 0xD8 ), _mm256_setzero_si256() ) );
        __m256i Diff = _mm256_sub_epi16( Src1, Src2 );
        __m256i Res = _mm256_madd_epi16( Diff, Diff );
        Sum = _mm256_add_epi32( Sum, Res );
      }
      pSrc1   += iStrideSrc1;
      pSrc2   += iStrideSrc2;
    }
    Sum = _mm256_hadd_epi32( Sum, Sum );
    Sum = _mm256_hadd_epi32( Sum, Sum );
    uiRet = ( _mm_cvtsi128_si32( _mm256_castsi256_si128( Sum ) ) + _mm_cvtsi128_si32( _mm256_castsi256_si128( _mm256_permute2x128_si256( Sum, Sum, 0x11 ) ) ) ) >> uiShift;
#endif
  }
  else if( ( iCols & 7 ) == 0 )
  {
    __m128i Sum = _mm_setzero_si128();
    for( int iY = 0; iY < iRows; iY++ )
    {
      for( int iX = 0; iX < iCols; iX += 8 )
      {
        __m128i Src1 = ( sizeof( Torg ) > 1 ) ? ( _mm_loadu_si128 ( ( const __m128i* )( &pSrc1[iX] ) ) ) : ( _mm_unpacklo_epi8( _mm_loadl_epi64( ( const __m128i* )( &pSrc1[iX] ) ), _mm_setzero_si128() ) );
        __m128i Src2 = ( sizeof( Tcur ) > 1 ) ? ( _mm_lddqu_si128( ( const __m128i* )( &pSrc2[iX] ) ) ) : ( _mm_unpacklo_epi8( _mm_loadl_epi64( ( const __m128i* )( &pSrc2[iX] ) ), _mm_setzero_si128() ) );
        __m128i Diff = _mm_sub_epi16( Src1, Src2 );
        __m128i Res = _mm_madd_epi16( Diff, Diff );
        Sum = _mm_add_epi32( Sum, Res );
      }
      pSrc1   += iStrideSrc1;
      pSrc2   += iStrideSrc2;
    }
    Sum = _mm_hadd_epi32( Sum, Sum );
    Sum = _mm_hadd_epi32( Sum, Sum );
    uiRet = _mm_cvtsi128_si32( Sum )>>uiShift;
  }
  else
  {
    __m128i Sum = _mm_setzero_si128();
    for( int iY = 0; iY < iRows; iY++ )
    {
      for( int iX = 0; iX < iCols; iX += 4 )
      {
        __m128i Src1 = ( sizeof( Torg ) > 1 ) ? ( _mm_loadl_epi64( ( const __m128i* )&pSrc1[iX] ) ) : ( _mm_unpacklo_epi8( _mm_cvtsi32_si128( *(const int*)&pSrc1[iX] ), _mm_setzero_si128() ) );
        __m128i Src2 = ( sizeof( Tcur ) > 1 ) ? ( _mm_loadl_epi64( ( const __m128i* )&pSrc2[iX] ) ) : ( _mm_unpacklo_epi8( _mm_cvtsi32_si128( *(const int*)&pSrc2[iX] ), _mm_setzero_si128() ) );
        __m128i Diff = _mm_sub_epi16( Src1, Src2 );
        __m128i Res = _mm_madd_epi16( Diff, Diff );
        Sum = _mm_add_epi32( Sum, Res );
      }
      pSrc1   += iStrideSrc1;
      pSrc2   += iStrideSrc2;
    }
    Sum = _mm_hadd_epi32( Sum, Sum );
    uiRet = _mm_cvtsi128_si32( Sum )>>uiShift;
  }

  return uiRet;
}


template< typename Torg, typename Tcur, int iWidth, X86_VEXT vext >
Distortion RdCost::xGetSSE_NxN_SIMD( const DistParam &rcDtParam )
{
  if( rcDtParam.bitDepth > 10 || rcDtParam.applyWeight )
    return RdCost::xGetSSE( rcDtParam );

  const Torg* pSrc1     = (const Torg*)rcDtParam.org.buf;
  const Tcur* pSrc2     = (const Tcur*)rcDtParam.cur.buf;
  int  iRows            = rcDtParam.org.height;
  const int iStrideSrc1 = rcDtParam.org.stride;
  const int iStrideSrc2 = rcDtParam.cur.stride;

  const uint32_t uiShift = DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth) << 1;
  unsigned int uiRet = 0;

  if( 4 == iWidth )
  {
    __m128i Sum = _mm_setzero_si128();
    for( int iY = 0; iY < iRows; iY++ )
    {
      __m128i Src1 = ( sizeof( Torg ) > 1 ) ? ( _mm_loadl_epi64( ( const __m128i* )pSrc1 ) ) : ( _mm_unpacklo_epi8( _mm_cvtsi32_si128( *(const int*)pSrc1 ), _mm_setzero_si128() ) );
      __m128i Src2 = ( sizeof( Tcur ) > 1 ) ? ( _mm_loadl_epi64( ( const __m128i* )pSrc2 ) ) : ( _mm_unpacklo_epi8( _mm_cvtsi32_si128( *(const int*)pSrc2 ), _mm_setzero_si128() ) );
      pSrc1 += iStrideSrc1;
      pSrc2 += iStrideSrc2;
      __m128i Diff = _mm_sub_epi16( Src1, Src2 );
      __m128i Res  = _mm_madd_epi16( Diff, Diff );
      Sum = _mm_add_epi32( Sum, Res );
    }
    Sum = _mm_hadd_epi32( Sum, Sum );
    uiRet = _mm_cvtsi128_si32( Sum )>>uiShift;
  }
  else
  {
    if( vext >= AVX2 && iWidth >= 16 )
    {
#ifdef USE_AVX2
      __m256i Sum = _mm256_setzero_si256();
      for( int iY = 0; iY < iRows; iY++ )
      {
        for( int iX = 0; iX < iWidth; iX+=16 )
        {
          __m256i Src1 = ( sizeof( Torg ) > 1 ) ? ( _mm256_lddqu_si256( ( __m256i* )( &pSrc1[iX] ) ) ) : ( _mm256_unpacklo_epi8( _mm256_permute4x64_epi64( _mm256_castsi128_si256( _mm_lddqu_si128( ( __m128i* )( &pSrc1[iX] ) ) ), 0xD8 ), _mm256_setzero_si256() ) );
          __m256i Src2 = ( sizeof( Tcur ) > 1 ) ? ( _mm256_lddqu_si256( ( __m256i* )( &pSrc2[iX] ) ) ) : ( _mm256_unpacklo_epi8( _mm256_permute4x64_epi64( _mm256_castsi128_si256( _mm_lddqu_si128( ( __m128i* )( &pSrc2[iX] ) ) ), 0xD8 ), _mm256_setzero_si256() ) );
          __m256i Diff = _mm256_sub_epi16( Src1, Src2 );
          __m256i Res = _mm256_madd_epi16( Diff, Diff );
          Sum = _mm256_add_epi32( Sum, Res );
        }
        pSrc1   += iStrideSrc1;
        pSrc2   += iStrideSrc2;
      }
      Sum = _mm256_hadd_epi32( Sum, Sum );
      Sum = _mm256_hadd_epi32( Sum, Sum );
      uiRet = ( _mm_cvtsi128_si32( _mm256_castsi256_si128( Sum ) ) + _mm_cvtsi128_si32( _mm256_castsi256_si128( _mm256_permute2x128_si256( Sum, Sum, 0x11 ) ) ) ) >> uiShift;
#endif
    }
    else
    {
      __m128i Sum = _mm_setzero_si128();
      for( int iY = 0; iY < iRows; iY++ )
      {
        for( int iX = 0; iX < iWidth; iX+=8 )
        {
          __m128i Src1 = ( sizeof( Torg ) > 1 ) ? ( _mm_loadu_si128( ( const __m128i* )( &pSrc1[iX] ) ) ) : ( _mm_unpacklo_epi8( _mm_loadl_epi64( ( const __m128i* )( &pSrc1[iX] ) ), _mm_setzero_si128() ) );
          __m128i Src2 = ( sizeof( Tcur ) > 1 ) ? ( _mm_lddqu_si128( ( const __m128i* )( &pSrc2[iX] ) ) ) : ( _mm_unpacklo_epi8( _mm_loadl_epi64( ( const __m128i* )( &pSrc2[iX] ) ), _mm_setzero_si128() ) );
          __m128i Diff = _mm_sub_epi16( Src1, Src2 );
          __m128i Res = _mm_madd_epi16( Diff, Diff );
          Sum = _mm_add_epi32( Sum, Res );
        }
        pSrc1 += iStrideSrc1;
        pSrc2 += iStrideSrc2;
      }
      Sum = _mm_hadd_epi32( Sum, Sum );
      Sum = _mm_hadd_epi32( Sum, Sum );
      uiRet = _mm_cvtsi128_si32( Sum )>>uiShift;
    }
  }
  return uiRet;
}

template< X86_VEXT vext >
Distortion RdCost::xGetSAD_SIMD( const DistParam &rcDtParam )
{
  if( rcDtParam.org.width < 4 || rcDtParam.bitDepth > 10 || rcDtParam.applyWeight )
    return RdCost::xGetSAD( rcDtParam );

  const short* pSrc1   = (const short*)rcDtParam.org.buf;
  const short* pSrc2   = (const short*)rcDtParam.cur.buf;
  int  iRows           = rcDtParam.org.height;
  int  iCols           = rcDtParam.org.width;
  int  iSubShift       = rcDtParam.subShift;
  int  iSubStep        = ( 1 << iSubShift );
  const int iStrideSrc1 = rcDtParam.org.stride * iSubStep;
  const int iStrideSrc2 = rcDtParam.cur.stride * iSubStep;

  uint32_t uiSum = 0;
  if( vext >= AVX2 && ( iCols & 15 ) == 0 )
  {
#ifdef USE_AVX2
    // Do for width that multiple of 16
    __m256i vzero = _mm256_setzero_si256();
    __m256i vsum32 = vzero;
    for( int iY = 0; iY < iRows; iY+=iSubStep )
    {
      __m256i vsum16 = vzero;
      for( int iX = 0; iX < iCols; iX+=16 )
      {
        __m256i vsrc1 = _mm256_lddqu_si256( ( __m256i* )( &pSrc1[iX] ) );
        __m256i vsrc2 = _mm256_lddqu_si256( ( __m256i* )( &pSrc2[iX] ) );
        vsum16 = _mm256_add_epi16( vsum16, _mm256_abs_epi16( _mm256_sub_epi16( vsrc1, vsrc2 ) ) );
      }
      __m256i vsumtemp = _mm256_add_epi32( _mm256_unpacklo_epi16( vsum16, vzero ), _mm256_unpackhi_epi16( vsum16, vzero ) );
      vsum32 = _mm256_add_epi32( vsum32, vsumtemp );
      pSrc1   += iStrideSrc1;
      pSrc2   += iStrideSrc2;
    }
    vsum32 = _mm256_hadd_epi32( vsum32, vzero );
    vsum32 = _mm256_hadd_epi32( vsum32, vzero );
    uiSum =  _mm_cvtsi128_si32( _mm256_castsi256_si128( vsum32 ) ) + _mm_cvtsi128_si32( _mm256_castsi256_si128( _mm256_permute2x128_si256( vsum32, vsum32, 0x11 ) ) );
#endif
  }
  else if( ( iCols & 7 ) == 0 )
  {
    // Do with step of 8
    __m128i vzero = _mm_setzero_si128();
    __m128i vsum32 = vzero;
    for( int iY = 0; iY < iRows; iY+=iSubStep )
    {
      __m128i vsum16 = vzero;
      for( int iX = 0; iX < iCols; iX+=8 )
      {
        __m128i vsrc1 = _mm_loadu_si128( ( const __m128i* )( &pSrc1[iX] ) );
        __m128i vsrc2 = _mm_lddqu_si128( ( const __m128i* )( &pSrc2[iX] ) );
        vsum16 = _mm_add_epi16( vsum16, _mm_abs_epi16( _mm_sub_epi16( vsrc1, vsrc2 ) ) );
      }
      __m128i vsumtemp = _mm_add_epi32( _mm_unpacklo_epi16( vsum16, vzero ), _mm_unpackhi_epi16( vsum16, vzero ) );
      vsum32 = _mm_add_epi32( vsum32, vsumtemp );
      pSrc1   += iStrideSrc1;
      pSrc2   += iStrideSrc2;
    }
    vsum32 = _mm_hadd_epi32( vsum32, vzero );
    vsum32 = _mm_hadd_epi32( vsum32, vzero );
    uiSum  =  _mm_cvtsi128_si32( vsum32 );
  }
  else
  {
    // Do with step of 4
    CHECK( ( iCols & 3 ) != 0, "Not divisible by 4: " << iCols );
    __m128i vzero = _mm_setzero_si128();
    __m128i vsum32 = vzero;
    for( int iY = 0; iY < iRows; iY += iSubStep )
    {
      __m128i vsum16 = vzero;
      for( int iX = 0; iX < iCols; iX+=4 )
      {
        __m128i vsrc1 = _mm_loadl_epi64( ( const __m128i* )&pSrc1[iX] );
        __m128i vsrc2 = _mm_loadl_epi64( ( const __m128i* )&pSrc2[iX] );
        vsum16 = _mm_add_epi16( vsum16, _mm_abs_epi16( _mm_sub_epi16( vsrc1, vsrc2 ) ) );
      }
      __m128i vsumtemp = _mm_add_epi32( _mm_unpacklo_epi16( vsum16, vzero ), _mm_unpackhi_epi16( vsum16, vzero ) );
      vsum32 = _mm_add_epi32( vsum32, vsumtemp );
      pSrc1 += iStrideSrc1;
      pSrc2 += iStrideSrc2;
    }
    vsum32 = _mm_hadd_epi32( vsum32, vzero );
    vsum32 = _mm_hadd_epi32( vsum32, vzero );
    uiSum  = _mm_cvtsi128_si32( vsum32 );
  }

  uiSum <<= iSubShift;
  return uiSum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth);
}

template< X86_VEXT vext >
Distortion RdCost::xGetSAD_IBD_SIMD(const DistParam &rcDtParam)
{
  if (rcDtParam.org.width < 4 || rcDtParam.bitDepth > 10 || rcDtParam.applyWeight)
    return RdCost::xGetSAD(rcDtParam);

  const short* src0 = (const short*)rcDtParam.org.buf;
  const short* src1 = (const short*)rcDtParam.cur.buf;
  int  width = rcDtParam.org.height;
  int  height = rcDtParam.org.width;
  int  subShift = rcDtParam.subShift;
  int  subStep = (1 << subShift);
  const int src0Stride = rcDtParam.org.stride * subStep;
  const int src1Stride = rcDtParam.cur.stride * subStep;

  __m128i vtotalsum32 = _mm_setzero_si128();
  __m128i vzero = _mm_setzero_si128();
  for (int y = 0; y < height; y += subStep)
  {
    for (int x = 0; x < width; x += 4)
    {
      __m128i vsrc1 = _mm_loadl_epi64((const __m128i*)(src0 + x));
      __m128i vsrc2 = _mm_loadl_epi64((const __m128i*)(src1 + x));
      vsrc1 = _mm_cvtepi16_epi32(vsrc1);
      vsrc2 = _mm_cvtepi16_epi32(vsrc2);
      vtotalsum32 = _mm_add_epi32(vtotalsum32, _mm_abs_epi32(_mm_sub_epi32(vsrc1, vsrc2)));
    }
    src0 += src0Stride;
    src1 += src1Stride;
  }
  vtotalsum32 = _mm_hadd_epi32(vtotalsum32, vzero);
  vtotalsum32 = _mm_hadd_epi32(vtotalsum32, vzero);
  Distortion uiSum = _mm_cvtsi128_si32(vtotalsum32);

  uiSum <<= subShift;
  return uiSum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth);
}

template< int iWidth, X86_VEXT vext >
Distortion RdCost::xGetSAD_NxN_SIMD( const DistParam &rcDtParam )
{
  if( rcDtParam.bitDepth > 10 || rcDtParam.applyWeight )
    return RdCost::xGetSAD( rcDtParam );

  //  assert( rcDtParam.iCols == iWidth);
  const short* pSrc1   = (const short*)rcDtParam.org.buf;
  const short* pSrc2   = (const short*)rcDtParam.cur.buf;
  int  iRows           = rcDtParam.org.height;
  int  iSubShift       = rcDtParam.subShift;
  int  iSubStep        = ( 1 << iSubShift );
  const int iStrideSrc1 = rcDtParam.org.stride * iSubStep;
  const int iStrideSrc2 = rcDtParam.cur.stride * iSubStep;

  uint32_t uiSum = 0;

  if( iWidth == 4 )
  {
    if( iRows == 4 && iSubShift == 0 )
    {
      __m128i vzero = _mm_setzero_si128();
      __m128i vsum = vzero;
      __m128i vsrc1 = _mm_loadl_epi64( ( const __m128i* )pSrc1 );
      vsrc1 =_mm_castps_si128( _mm_loadh_pi( _mm_castsi128_ps( vsrc1 ), ( __m64* )&pSrc1[iStrideSrc1] ) );
      __m128i vsrc2 = _mm_loadl_epi64( ( const __m128i* )pSrc2 );
      vsrc2 =_mm_castps_si128( _mm_loadh_pi( _mm_castsi128_ps( vsrc2 ), ( __m64* )&pSrc2[iStrideSrc2] ) );
      vsum = _mm_abs_epi16( _mm_sub_epi16( vsrc1, vsrc2 ) );

      vsrc1 = _mm_loadl_epi64( ( const __m128i* )&pSrc1[2 * iStrideSrc1] );
      vsrc1 = _mm_castps_si128( _mm_loadh_pi( _mm_castsi128_ps( vsrc1 ), ( __m64* )&pSrc1[3 * iStrideSrc1] ) );
      vsrc2 = _mm_loadl_epi64( ( const __m128i* )&pSrc2[2 * iStrideSrc2] );
      vsrc2 = _mm_castps_si128( _mm_loadh_pi( _mm_castsi128_ps( vsrc2 ), ( __m64* )&pSrc2[3 * iStrideSrc2] ) );
      vsum  = _mm_hadd_epi16( vsum, _mm_abs_epi16( _mm_sub_epi16( vsrc1, vsrc2 ) ) );
      vsum  = _mm_hadd_epi16( vsum, vzero );
      vsum  = _mm_hadd_epi16( vsum, vzero );
      vsum  = _mm_hadd_epi16( vsum, vzero );
      uiSum = _mm_cvtsi128_si32( vsum );
    }
    else
    {
      __m128i vzero = _mm_setzero_si128();
      __m128i vsum32 = vzero;
      for( int iY = 0; iY < iRows; iY += iSubStep )
      {
        __m128i vsum16 = vzero;
        {
          __m128i vsrc1 = _mm_loadl_epi64( ( const __m128i* )pSrc1 );
          __m128i vsrc2 = _mm_loadl_epi64( ( const __m128i* )pSrc2 );
          vsum16 = _mm_add_epi16( vsum16, _mm_abs_epi16( _mm_sub_epi16( vsrc1, vsrc2 ) ) );
        }
        __m128i vsumtemp = _mm_add_epi32( _mm_unpacklo_epi16( vsum16, vzero ), _mm_unpackhi_epi16( vsum16, vzero ) );
        vsum32 = _mm_add_epi32( vsum32, vsumtemp );
        pSrc1 += iStrideSrc1;
        pSrc2 += iStrideSrc2;
      }
      vsum32 = _mm_hadd_epi32( vsum32, vzero );
      vsum32 = _mm_hadd_epi32( vsum32, vzero );
      uiSum = _mm_cvtsi128_si32( vsum32 );
    }
  }
  else
  {
    if( vext >= AVX2 && iWidth >= 16 )
    {
#ifdef USE_AVX2
      // Do for width that multiple of 16
      __m256i vzero = _mm256_setzero_si256();
      __m256i vsum32 = vzero;
      for( int iY = 0; iY < iRows; iY+=iSubStep )
      {
        __m256i vsum16 = vzero;
        for( int iX = 0; iX < iWidth; iX+=16 )
        {
          __m256i vsrc1 = _mm256_lddqu_si256( ( __m256i* )( &pSrc1[iX] ) );
          __m256i vsrc2 = _mm256_lddqu_si256( ( __m256i* )( &pSrc2[iX] ) );
          vsum16 = _mm256_add_epi16( vsum16, _mm256_abs_epi16( _mm256_sub_epi16( vsrc1, vsrc2 ) ) );
        }
        __m256i vsumtemp = _mm256_add_epi32( _mm256_unpacklo_epi16( vsum16, vzero ), _mm256_unpackhi_epi16( vsum16, vzero ) );
        vsum32 = _mm256_add_epi32( vsum32, vsumtemp );
        pSrc1   += iStrideSrc1;
        pSrc2   += iStrideSrc2;
      }
      vsum32 = _mm256_hadd_epi32( vsum32, vzero );
      vsum32 = _mm256_hadd_epi32( vsum32, vzero );
      uiSum =  _mm_cvtsi128_si32( _mm256_castsi256_si128( vsum32 ) ) + _mm_cvtsi128_si32( _mm256_castsi256_si128( _mm256_permute2x128_si256( vsum32, vsum32, 0x11 ) ) );
#endif
    }
    else
    {
      // For width that multiple of 8
      __m128i vzero = _mm_setzero_si128();
      __m128i vsum32 = vzero;
      for( int iY = 0; iY < iRows; iY+=iSubStep )
      {
        __m128i vsum16 = vzero;
        for( int iX = 0; iX < iWidth; iX+=8 )
        {
          __m128i vsrc1 = _mm_loadu_si128( ( const __m128i* )( &pSrc1[iX] ) );
          __m128i vsrc2 = _mm_lddqu_si128( ( const __m128i* )( &pSrc2[iX] ) );
          vsum16 = _mm_add_epi16( vsum16, _mm_abs_epi16( _mm_sub_epi16( vsrc1, vsrc2 ) ) );
        }
        __m128i vsumtemp = _mm_add_epi32( _mm_unpacklo_epi16( vsum16, vzero ), _mm_unpackhi_epi16( vsum16, vzero ) );
        vsum32 = _mm_add_epi32( vsum32, vsumtemp );
        pSrc1   += iStrideSrc1;
        pSrc2   += iStrideSrc2;
      }
      vsum32 = _mm_hadd_epi32( vsum32, vzero );
      vsum32 = _mm_hadd_epi32( vsum32, vzero );
      uiSum =  _mm_cvtsi128_si32( vsum32 );
    }
  }

  uiSum <<= iSubShift;
  return uiSum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth);
}


template< typename Torg, typename Tcur >
static uint32_t xCalcHAD4x4_SSE( const Torg *piOrg, const Tcur *piCur, const int iStrideOrg, const int iStrideCur )
{
  __m128i r0 = ( sizeof( Torg ) > 1 ) ? ( _mm_loadl_epi64( ( const __m128i* )&piOrg[0] ) ) : ( _mm_unpacklo_epi8( _mm_cvtsi32_si128( *(const int*)&piOrg[0] ), _mm_setzero_si128() ) );
  __m128i r1 = ( sizeof( Torg ) > 1 ) ? ( _mm_loadl_epi64( ( const __m128i* )&piOrg[iStrideOrg] ) ) : ( _mm_unpacklo_epi8( _mm_cvtsi32_si128( *(const int*)&piOrg[iStrideOrg] ), _mm_setzero_si128() ) );
  __m128i r2 = ( sizeof( Torg ) > 1 ) ? ( _mm_loadl_epi64( ( const __m128i* )&piOrg[2 * iStrideOrg] ) ) : ( _mm_unpacklo_epi8( _mm_cvtsi32_si128( *(const int*)&piOrg[2 * iStrideOrg] ), _mm_setzero_si128() ) );
  __m128i r3 = ( sizeof( Torg ) > 1 ) ? ( _mm_loadl_epi64( ( const __m128i* )&piOrg[3 * iStrideOrg] ) ) : ( _mm_unpacklo_epi8( _mm_cvtsi32_si128( *(const int*)&piOrg[3 * iStrideOrg] ), _mm_setzero_si128() ) );
  __m128i r4 = ( sizeof( Tcur ) > 1 ) ? ( _mm_loadl_epi64( ( const __m128i* )&piCur[0] ) ) : ( _mm_unpacklo_epi8( _mm_cvtsi32_si128( *(const int*)&piCur[0] ), _mm_setzero_si128() ) );
  __m128i r5 = ( sizeof( Tcur ) > 1 ) ? ( _mm_loadl_epi64( ( const __m128i* )&piCur[iStrideCur] ) ) : ( _mm_unpacklo_epi8( _mm_cvtsi32_si128( *(const int*)&piCur[iStrideCur] ), _mm_setzero_si128() ) );
  __m128i r6 = ( sizeof( Tcur ) > 1 ) ? ( _mm_loadl_epi64( ( const __m128i* )&piCur[2 * iStrideCur] ) ) : ( _mm_unpacklo_epi8( _mm_cvtsi32_si128( *(const int*)&piCur[2 * iStrideCur] ), _mm_setzero_si128() ) );
  __m128i r7 = ( sizeof( Tcur ) > 1 ) ? ( _mm_loadl_epi64( ( const __m128i* )&piCur[3 * iStrideCur] ) ) : ( _mm_unpacklo_epi8( _mm_cvtsi32_si128( *(const int*)&piCur[3 * iStrideCur] ), _mm_setzero_si128() ) );

  r0 = _mm_sub_epi16( r0, r4 );
  r1 = _mm_sub_epi16( r1, r5 );
  r2 = _mm_sub_epi16( r2, r6 );
  r3 = _mm_sub_epi16( r3, r7 );

  // first stage
  r4 = r0;
  r5 = r1;

  r0 = _mm_add_epi16( r0, r3 );
  r1 = _mm_add_epi16( r1, r2 );

  r4 = _mm_sub_epi16( r4, r3 );
  r5 = _mm_sub_epi16( r5, r2 );

  r2 = r0;
  r3 = r4;

  r0 = _mm_add_epi16( r0, r1 );
  r2 = _mm_sub_epi16( r2, r1 );
  r3 = _mm_sub_epi16( r3, r5 );
  r5 = _mm_add_epi16( r5, r4 );

  // shuffle - flip matrix for vertical transform
  r0 = _mm_unpacklo_epi16( r0, r5 );
  r2 = _mm_unpacklo_epi16( r2, r3 );

  r3 = r0;
  r0 = _mm_unpacklo_epi32( r0, r2 );
  r3 = _mm_unpackhi_epi32( r3, r2 );

  r1 = r0;
  r2 = r3;
  r1 = _mm_srli_si128( r1, 8 );
  r3 = _mm_srli_si128( r3, 8 );

  // second stage
  r4 = r0;
  r5 = r1;

  r0 = _mm_add_epi16( r0, r3 );
  r1 = _mm_add_epi16( r1, r2 );

  r4 = _mm_sub_epi16( r4, r3 );
  r5 = _mm_sub_epi16( r5, r2 );

  r2 = r0;
  r3 = r4;

  r0 = _mm_add_epi16( r0, r1 );
  r2 = _mm_sub_epi16( r2, r1 );
  r3 = _mm_sub_epi16( r3, r5 );
  r5 = _mm_add_epi16( r5, r4 );

  // abs
  __m128i Sum = _mm_abs_epi16( r0 );
  Sum = _mm_add_epi16( Sum, _mm_abs_epi16( r2 ) );
  Sum = _mm_add_epi16( Sum, _mm_abs_epi16( r3 ) );
  Sum = _mm_add_epi16( Sum, _mm_abs_epi16( r5 ) );

  __m128i iZero = _mm_set1_epi16( 0 );
  Sum = _mm_unpacklo_epi16( Sum, iZero );
  Sum = _mm_hadd_epi32( Sum, Sum );
  Sum = _mm_hadd_epi32( Sum, Sum );

  uint32_t sad = _mm_cvtsi128_si32( Sum );

  sad = ( ( sad + 1 ) >> 1 );

  return sad;
}

//working up to 12-bit
template< typename Torg, typename Tcur/*, bool bHorDownsampling*/ >
static uint32_t xCalcHAD8x8_SSE( const Torg *piOrg, const Tcur *piCur, const int iStrideOrg, const int iStrideCur, const int iBitDepth )
{
  __m128i m1[8], m2[8];

  for( int k = 0; k < 8; k++ )
  {
    __m128i r0 = ( sizeof( Torg ) > 1 ) ? ( _mm_loadu_si128( ( __m128i* )piOrg ) ) : ( _mm_unpacklo_epi8( _mm_loadl_epi64( ( const __m128i* )piOrg ), _mm_setzero_si128() ) );
    __m128i r1 = ( sizeof( Tcur ) > 1 ) ? ( _mm_lddqu_si128( ( __m128i* )piCur ) ) : ( _mm_unpacklo_epi8( _mm_loadl_epi64( ( const __m128i* )piCur ), _mm_setzero_si128() ) ); // th  _mm_loadu_si128( (__m128i*)piCur )
    m2[k] = _mm_sub_epi16( r0, r1 );
    piCur += iStrideCur;
    piOrg += iStrideOrg;
  }

  //horizontal
  m1[0] = _mm_add_epi16( m2[0], m2[4] );
  m1[1] = _mm_add_epi16( m2[1], m2[5] );
  m1[2] = _mm_add_epi16( m2[2], m2[6] );
  m1[3] = _mm_add_epi16( m2[3], m2[7] );
  m1[4] = _mm_sub_epi16( m2[0], m2[4] );
  m1[5] = _mm_sub_epi16( m2[1], m2[5] );
  m1[6] = _mm_sub_epi16( m2[2], m2[6] );
  m1[7] = _mm_sub_epi16( m2[3], m2[7] );

  m2[0] = _mm_add_epi16( m1[0], m1[2] );
  m2[1] = _mm_add_epi16( m1[1], m1[3] );
  m2[2] = _mm_sub_epi16( m1[0], m1[2] );
  m2[3] = _mm_sub_epi16( m1[1], m1[3] );
  m2[4] = _mm_add_epi16( m1[4], m1[6] );
  m2[5] = _mm_add_epi16( m1[5], m1[7] );
  m2[6] = _mm_sub_epi16( m1[4], m1[6] );
  m2[7] = _mm_sub_epi16( m1[5], m1[7] );

  m1[0] = _mm_add_epi16( m2[0], m2[1] );
  m1[1] = _mm_sub_epi16( m2[0], m2[1] );
  m1[2] = _mm_add_epi16( m2[2], m2[3] );
  m1[3] = _mm_sub_epi16( m2[2], m2[3] );
  m1[4] = _mm_add_epi16( m2[4], m2[5] );
  m1[5] = _mm_sub_epi16( m2[4], m2[5] );
  m1[6] = _mm_add_epi16( m2[6], m2[7] );
  m1[7] = _mm_sub_epi16( m2[6], m2[7] );

  {
    m2[0] = _mm_unpacklo_epi16( m1[0], m1[1] );
    m2[1] = _mm_unpacklo_epi16( m1[2], m1[3] );
    m2[2] = _mm_unpacklo_epi16( m1[4], m1[5] );
    m2[3] = _mm_unpacklo_epi16( m1[6], m1[7] );
    m2[4] = _mm_unpackhi_epi16( m1[0], m1[1] );
    m2[5] = _mm_unpackhi_epi16( m1[2], m1[3] );
    m2[6] = _mm_unpackhi_epi16( m1[4], m1[5] );
    m2[7] = _mm_unpackhi_epi16( m1[6], m1[7] );

    m1[0] = _mm_unpacklo_epi32( m2[0], m2[1] );
    m1[1] = _mm_unpackhi_epi32( m2[0], m2[1] );
    m1[2] = _mm_unpacklo_epi32( m2[2], m2[3] );
    m1[3] = _mm_unpackhi_epi32( m2[2], m2[3] );
    m1[4] = _mm_unpacklo_epi32( m2[4], m2[5] );
    m1[5] = _mm_unpackhi_epi32( m2[4], m2[5] );
    m1[6] = _mm_unpacklo_epi32( m2[6], m2[7] );
    m1[7] = _mm_unpackhi_epi32( m2[6], m2[7] );

    m2[0] = _mm_unpacklo_epi64( m1[0], m1[2] );
    m2[1] = _mm_unpackhi_epi64( m1[0], m1[2] );
    m2[2] = _mm_unpacklo_epi64( m1[1], m1[3] );
    m2[3] = _mm_unpackhi_epi64( m1[1], m1[3] );
    m2[4] = _mm_unpacklo_epi64( m1[4], m1[6] );
    m2[5] = _mm_unpackhi_epi64( m1[4], m1[6] );
    m2[6] = _mm_unpacklo_epi64( m1[5], m1[7] );
    m2[7] = _mm_unpackhi_epi64( m1[5], m1[7] );
  }

  if( iBitDepth >= 10 /*sizeof( Torg ) > 1 || sizeof( Tcur ) > 1*/ )
  {
    //  if (g_bitDepthY >=10){
    __m128i n1[8][2];
    __m128i n2[8][2];

    for( int i = 0; i < 8; i++ )
    {
      n2[i][0] = _mm_cvtepi16_epi32( m2[i] );
      n2[i][1] = _mm_cvtepi16_epi32( _mm_shuffle_epi32( m2[i], 0xEE ) );
    }

    for( int i = 0; i < 2; i++ )
    {
      n1[0][i] = _mm_add_epi32( n2[0][i], n2[4][i] );
      n1[1][i] = _mm_add_epi32( n2[1][i], n2[5][i] );
      n1[2][i] = _mm_add_epi32( n2[2][i], n2[6][i] );
      n1[3][i] = _mm_add_epi32( n2[3][i], n2[7][i] );
      n1[4][i] = _mm_sub_epi32( n2[0][i], n2[4][i] );
      n1[5][i] = _mm_sub_epi32( n2[1][i], n2[5][i] );
      n1[6][i] = _mm_sub_epi32( n2[2][i], n2[6][i] );
      n1[7][i] = _mm_sub_epi32( n2[3][i], n2[7][i] );

      n2[0][i] = _mm_add_epi32( n1[0][i], n1[2][i] );
      n2[1][i] = _mm_add_epi32( n1[1][i], n1[3][i] );
      n2[2][i] = _mm_sub_epi32( n1[0][i], n1[2][i] );
      n2[3][i] = _mm_sub_epi32( n1[1][i], n1[3][i] );
      n2[4][i] = _mm_add_epi32( n1[4][i], n1[6][i] );
      n2[5][i] = _mm_add_epi32( n1[5][i], n1[7][i] );
      n2[6][i] = _mm_sub_epi32( n1[4][i], n1[6][i] );
      n2[7][i] = _mm_sub_epi32( n1[5][i], n1[7][i] );

      n1[0][i] = _mm_abs_epi32( _mm_add_epi32( n2[0][i], n2[1][i] ) );
      n1[1][i] = _mm_abs_epi32( _mm_sub_epi32( n2[0][i], n2[1][i] ) );
      n1[2][i] = _mm_abs_epi32( _mm_add_epi32( n2[2][i], n2[3][i] ) );
      n1[3][i] = _mm_abs_epi32( _mm_sub_epi32( n2[2][i], n2[3][i] ) );
      n1[4][i] = _mm_abs_epi32( _mm_add_epi32( n2[4][i], n2[5][i] ) );
      n1[5][i] = _mm_abs_epi32( _mm_sub_epi32( n2[4][i], n2[5][i] ) );
      n1[6][i] = _mm_abs_epi32( _mm_add_epi32( n2[6][i], n2[7][i] ) );
      n1[7][i] = _mm_abs_epi32( _mm_sub_epi32( n2[6][i], n2[7][i] ) );
    }
    for( int i = 0; i < 8; i++ )
    {
      m1[i] = _mm_add_epi32( n1[i][0], n1[i][1] );
    }
  }
  else
  {
    m1[0] = _mm_add_epi16( m2[0], m2[4] );
    m1[1] = _mm_add_epi16( m2[1], m2[5] );
    m1[2] = _mm_add_epi16( m2[2], m2[6] );
    m1[3] = _mm_add_epi16( m2[3], m2[7] );
    m1[4] = _mm_sub_epi16( m2[0], m2[4] );
    m1[5] = _mm_sub_epi16( m2[1], m2[5] );
    m1[6] = _mm_sub_epi16( m2[2], m2[6] );
    m1[7] = _mm_sub_epi16( m2[3], m2[7] );

    m2[0] = _mm_add_epi16( m1[0], m1[2] );
    m2[1] = _mm_add_epi16( m1[1], m1[3] );
    m2[2] = _mm_sub_epi16( m1[0], m1[2] );
    m2[3] = _mm_sub_epi16( m1[1], m1[3] );
    m2[4] = _mm_add_epi16( m1[4], m1[6] );
    m2[5] = _mm_add_epi16( m1[5], m1[7] );
    m2[6] = _mm_sub_epi16( m1[4], m1[6] );
    m2[7] = _mm_sub_epi16( m1[5], m1[7] );

    m1[0] = _mm_abs_epi16( _mm_add_epi16( m2[0], m2[1] ) );
    m1[1] = _mm_abs_epi16( _mm_sub_epi16( m2[0], m2[1] ) );
    m1[2] = _mm_abs_epi16( _mm_add_epi16( m2[2], m2[3] ) );
    m1[3] = _mm_abs_epi16( _mm_sub_epi16( m2[2], m2[3] ) );
    m1[4] = _mm_abs_epi16( _mm_add_epi16( m2[4], m2[5] ) );
    m1[5] = _mm_abs_epi16( _mm_sub_epi16( m2[4], m2[5] ) );
    m1[6] = _mm_abs_epi16( _mm_add_epi16( m2[6], m2[7] ) );
    m1[7] = _mm_abs_epi16( _mm_sub_epi16( m2[6], m2[7] ) );

    __m128i ma1, ma2;
    __m128i vzero = _mm_setzero_si128();

    for( int i = 0; i < 8; i++ )
    {
      ma1 = _mm_unpacklo_epi16( m1[i], vzero );
      ma2 = _mm_unpackhi_epi16( m1[i], vzero );
      m1[i] = _mm_add_epi32( ma1, ma2 );
    }
  }


  m1[0] = _mm_add_epi32( m1[0], m1[1] );
  m1[2] = _mm_add_epi32( m1[2], m1[3] );
  m1[4] = _mm_add_epi32( m1[4], m1[5] );
  m1[6] = _mm_add_epi32( m1[6], m1[7] );

  m1[0] = _mm_add_epi32( m1[0], m1[2] );
  m1[4] = _mm_add_epi32( m1[4], m1[6] );
  __m128i iSum = _mm_add_epi32( m1[0], m1[4] );

  iSum = _mm_hadd_epi32( iSum, iSum );
  iSum = _mm_hadd_epi32( iSum, iSum );

  uint32_t sad = _mm_cvtsi128_si32( iSum );
  sad = ( ( sad + 2 ) >> 2 );

  return sad;
}


//working up to 12-bit
template< typename Torg, typename Tcur/*, bool bHorDownsampling*/ >
static uint32_t xCalcHAD16x8_SSE( const Torg *piOrg, const Tcur *piCur, const int iStrideOrg, const int iStrideCur, const int iBitDepth )
{
  __m128i m1[16][2], m2[16][2];
  __m128i iSum = _mm_setzero_si128();

  for( int l = 0; l < 2; l++ )
  {
    const Torg *piOrgPtr = piOrg + l*8;
    const Tcur *piCurPtr = piCur + l*8;
    for( int k = 0; k < 8; k++ )
    {
      __m128i r0 = (sizeof( Torg ) > 1) ? (_mm_loadu_si128( (__m128i*)piOrgPtr )) : (_mm_unpacklo_epi8( _mm_loadl_epi64( (const __m128i*)piOrgPtr ), _mm_setzero_si128() ));
      __m128i r1 = (sizeof( Tcur ) > 1) ? (_mm_lddqu_si128( (__m128i*)piCurPtr )) : (_mm_unpacklo_epi8( _mm_loadl_epi64( (const __m128i*)piCurPtr ), _mm_setzero_si128() )); // th  _mm_loadu_si128( (__m128i*)piCurPtr )
      m2[k][l] = _mm_sub_epi16( r0, r1 );
      piCurPtr += iStrideCur;
      piOrgPtr += iStrideOrg;
    }

    //vertical
    m1[0][l] = _mm_add_epi16( m2[0][l], m2[4][l] );
    m1[1][l] = _mm_add_epi16( m2[1][l], m2[5][l] );
    m1[2][l] = _mm_add_epi16( m2[2][l], m2[6][l] );
    m1[3][l] = _mm_add_epi16( m2[3][l], m2[7][l] );
    m1[4][l] = _mm_sub_epi16( m2[0][l], m2[4][l] );
    m1[5][l] = _mm_sub_epi16( m2[1][l], m2[5][l] );
    m1[6][l] = _mm_sub_epi16( m2[2][l], m2[6][l] );
    m1[7][l] = _mm_sub_epi16( m2[3][l], m2[7][l] );

    m2[0][l] = _mm_add_epi16( m1[0][l], m1[2][l] );
    m2[1][l] = _mm_add_epi16( m1[1][l], m1[3][l] );
    m2[2][l] = _mm_sub_epi16( m1[0][l], m1[2][l] );
    m2[3][l] = _mm_sub_epi16( m1[1][l], m1[3][l] );
    m2[4][l] = _mm_add_epi16( m1[4][l], m1[6][l] );
    m2[5][l] = _mm_add_epi16( m1[5][l], m1[7][l] );
    m2[6][l] = _mm_sub_epi16( m1[4][l], m1[6][l] );
    m2[7][l] = _mm_sub_epi16( m1[5][l], m1[7][l] );

    m1[0][l] = _mm_add_epi16( m2[0][l], m2[1][l] );
    m1[1][l] = _mm_sub_epi16( m2[0][l], m2[1][l] );
    m1[2][l] = _mm_add_epi16( m2[2][l], m2[3][l] );
    m1[3][l] = _mm_sub_epi16( m2[2][l], m2[3][l] );
    m1[4][l] = _mm_add_epi16( m2[4][l], m2[5][l] );
    m1[5][l] = _mm_sub_epi16( m2[4][l], m2[5][l] );
    m1[6][l] = _mm_add_epi16( m2[6][l], m2[7][l] );
    m1[7][l] = _mm_sub_epi16( m2[6][l], m2[7][l] );
  }

  __m128i vzero = _mm_setzero_si128();

  // 4 x 8x4 blocks
  // 0 1
  // 2 3

  if( iBitDepth >= 10 )
  {
    // transpose and do horizontal in two steps
    for( int l = 0; l < 2; l++ )
    {
      int off = l * 4;

      // transpose 8x4 -> 4x8
      // 0 1  ->  0   and        ->  2
      //          1         2 3      3

      // transpose 8x4 -> 4x8, block 0(2)
      m2[0][0] = _mm_unpacklo_epi16( m1[0 + off][0], m1[1 + off][0] );
      m2[1][0] = _mm_unpacklo_epi16( m1[2 + off][0], m1[3 + off][0] );
      m2[2][0] = _mm_unpackhi_epi16( m1[0 + off][0], m1[1 + off][0] );
      m2[3][0] = _mm_unpackhi_epi16( m1[2 + off][0], m1[3 + off][0] );

      m1[0][0] = _mm_unpacklo_epi32( m2[0][0], m2[1][0] );
      m1[1][0] = _mm_unpackhi_epi32( m2[0][0], m2[1][0] );
      m1[2][0] = _mm_unpacklo_epi32( m2[2][0], m2[3][0] );
      m1[3][0] = _mm_unpackhi_epi32( m2[2][0], m2[3][0] );

      m2[0][0] = _mm_unpacklo_epi64( m1[0][0], vzero );
      m2[1][0] = _mm_unpackhi_epi64( m1[0][0], vzero );
      m2[2][0] = _mm_unpacklo_epi64( m1[1][0], vzero );
      m2[3][0] = _mm_unpackhi_epi64( m1[1][0], vzero );
      m2[4][0] = _mm_unpacklo_epi64( m1[2][0], vzero );
      m2[5][0] = _mm_unpackhi_epi64( m1[2][0], vzero );
      m2[6][0] = _mm_unpacklo_epi64( m1[3][0], vzero );
      m2[7][0] = _mm_unpackhi_epi64( m1[3][0], vzero );

      // transpose 8x4 -> 4x8, block 1(3)
      m2[0 + 8][0] = _mm_unpacklo_epi16( m1[0 + off][1], m1[1 + off][1] );
      m2[1 + 8][0] = _mm_unpacklo_epi16( m1[2 + off][1], m1[3 + off][1] );
      m2[2 + 8][0] = _mm_unpackhi_epi16( m1[0 + off][1], m1[1 + off][1] );
      m2[3 + 8][0] = _mm_unpackhi_epi16( m1[2 + off][1], m1[3 + off][1] );

      m1[0 + 8][0] = _mm_unpacklo_epi32( m2[0 + 8][0], m2[1 + 8][0] );
      m1[1 + 8][0] = _mm_unpackhi_epi32( m2[0 + 8][0], m2[1 + 8][0] );
      m1[2 + 8][0] = _mm_unpacklo_epi32( m2[2 + 8][0], m2[3 + 8][0] );
      m1[3 + 8][0] = _mm_unpackhi_epi32( m2[2 + 8][0], m2[3 + 8][0] );

      m2[0 + 8][0] = _mm_unpacklo_epi64( m1[0 + 8][0], vzero );
      m2[1 + 8][0] = _mm_unpackhi_epi64( m1[0 + 8][0], vzero );
      m2[2 + 8][0] = _mm_unpacklo_epi64( m1[1 + 8][0], vzero );
      m2[3 + 8][0] = _mm_unpackhi_epi64( m1[1 + 8][0], vzero );
      m2[4 + 8][0] = _mm_unpacklo_epi64( m1[2 + 8][0], vzero );
      m2[5 + 8][0] = _mm_unpackhi_epi64( m1[2 + 8][0], vzero );
      m2[6 + 8][0] = _mm_unpacklo_epi64( m1[3 + 8][0], vzero );
      m2[7 + 8][0] = _mm_unpackhi_epi64( m1[3 + 8][0], vzero );

      // horizontal
      //if( iBitDepth >= 10 )
      {
        __m128i n1[16];
        __m128i n2[16];

        for( int i = 0; i < 16; i++ )
        {
          n1[i] = _mm_cvtepi16_epi32( m2[i][0] );
        }

        n2[0] = _mm_add_epi32( n1[0], n1[8] );
        n2[1] = _mm_add_epi32( n1[1], n1[9] );
        n2[2] = _mm_add_epi32( n1[2], n1[10] );
        n2[3] = _mm_add_epi32( n1[3], n1[11] );
        n2[4] = _mm_add_epi32( n1[4], n1[12] );
        n2[5] = _mm_add_epi32( n1[5], n1[13] );
        n2[6] = _mm_add_epi32( n1[6], n1[14] );
        n2[7] = _mm_add_epi32( n1[7], n1[15] );
        n2[8] = _mm_sub_epi32( n1[0], n1[8] );
        n2[9] = _mm_sub_epi32( n1[1], n1[9] );
        n2[10] = _mm_sub_epi32( n1[2], n1[10] );
        n2[11] = _mm_sub_epi32( n1[3], n1[11] );
        n2[12] = _mm_sub_epi32( n1[4], n1[12] );
        n2[13] = _mm_sub_epi32( n1[5], n1[13] );
        n2[14] = _mm_sub_epi32( n1[6], n1[14] );
        n2[15] = _mm_sub_epi32( n1[7], n1[15] );

        n1[0] = _mm_add_epi32( n2[0], n2[4] );
        n1[1] = _mm_add_epi32( n2[1], n2[5] );
        n1[2] = _mm_add_epi32( n2[2], n2[6] );
        n1[3] = _mm_add_epi32( n2[3], n2[7] );
        n1[4] = _mm_sub_epi32( n2[0], n2[4] );
        n1[5] = _mm_sub_epi32( n2[1], n2[5] );
        n1[6] = _mm_sub_epi32( n2[2], n2[6] );
        n1[7] = _mm_sub_epi32( n2[3], n2[7] );
        n1[8] = _mm_add_epi32( n2[8], n2[12] );
        n1[9] = _mm_add_epi32( n2[9], n2[13] );
        n1[10] = _mm_add_epi32( n2[10], n2[14] );
        n1[11] = _mm_add_epi32( n2[11], n2[15] );
        n1[12] = _mm_sub_epi32( n2[8], n2[12] );
        n1[13] = _mm_sub_epi32( n2[9], n2[13] );
        n1[14] = _mm_sub_epi32( n2[10], n2[14] );
        n1[15] = _mm_sub_epi32( n2[11], n2[15] );

        n2[0] = _mm_add_epi32( n1[0], n1[2] );
        n2[1] = _mm_add_epi32( n1[1], n1[3] );
        n2[2] = _mm_sub_epi32( n1[0], n1[2] );
        n2[3] = _mm_sub_epi32( n1[1], n1[3] );
        n2[4] = _mm_add_epi32( n1[4], n1[6] );
        n2[5] = _mm_add_epi32( n1[5], n1[7] );
        n2[6] = _mm_sub_epi32( n1[4], n1[6] );
        n2[7] = _mm_sub_epi32( n1[5], n1[7] );
        n2[8] = _mm_add_epi32( n1[8], n1[10] );
        n2[9] = _mm_add_epi32( n1[9], n1[11] );
        n2[10] = _mm_sub_epi32( n1[8], n1[10] );
        n2[11] = _mm_sub_epi32( n1[9], n1[11] );
        n2[12] = _mm_add_epi32( n1[12], n1[14] );
        n2[13] = _mm_add_epi32( n1[13], n1[15] );
        n2[14] = _mm_sub_epi32( n1[12], n1[14] );
        n2[15] = _mm_sub_epi32( n1[13], n1[15] );

        n1[0] = _mm_abs_epi32( _mm_add_epi32( n2[0], n2[1] ) );
        n1[1] = _mm_abs_epi32( _mm_sub_epi32( n2[0], n2[1] ) );
        n1[2] = _mm_abs_epi32( _mm_add_epi32( n2[2], n2[3] ) );
        n1[3] = _mm_abs_epi32( _mm_sub_epi32( n2[2], n2[3] ) );
        n1[4] = _mm_abs_epi32( _mm_add_epi32( n2[4], n2[5] ) );
        n1[5] = _mm_abs_epi32( _mm_sub_epi32( n2[4], n2[5] ) );
        n1[6] = _mm_abs_epi32( _mm_add_epi32( n2[6], n2[7] ) );
        n1[7] = _mm_abs_epi32( _mm_sub_epi32( n2[6], n2[7] ) );
        n1[8] = _mm_abs_epi32( _mm_add_epi32( n2[8], n2[9] ) );
        n1[9] = _mm_abs_epi32( _mm_sub_epi32( n2[8], n2[9] ) );
        n1[10] = _mm_abs_epi32( _mm_add_epi32( n2[10], n2[11] ) );
        n1[11] = _mm_abs_epi32( _mm_sub_epi32( n2[10], n2[11] ) );
        n1[12] = _mm_abs_epi32( _mm_add_epi32( n2[12], n2[13] ) );
        n1[13] = _mm_abs_epi32( _mm_sub_epi32( n2[12], n2[13] ) );
        n1[14] = _mm_abs_epi32( _mm_add_epi32( n2[14], n2[15] ) );
        n1[15] = _mm_abs_epi32( _mm_sub_epi32( n2[14], n2[15] ) );

        // sum up
        n1[0] = _mm_add_epi32( n1[0], n1[1] );
        n1[2] = _mm_add_epi32( n1[2], n1[3] );
        n1[4] = _mm_add_epi32( n1[4], n1[5] );
        n1[6] = _mm_add_epi32( n1[6], n1[7] );
        n1[8] = _mm_add_epi32( n1[8], n1[9] );
        n1[10] = _mm_add_epi32( n1[10], n1[11] );
        n1[12] = _mm_add_epi32( n1[12], n1[13] );
        n1[14] = _mm_add_epi32( n1[14], n1[15] );

        n1[0] = _mm_add_epi32( n1[0], n1[2] );
        n1[4] = _mm_add_epi32( n1[4], n1[6] );
        n1[8] = _mm_add_epi32( n1[8], n1[10] );
        n1[12] = _mm_add_epi32( n1[12], n1[14] );

        n1[0] = _mm_add_epi32( n1[0], n1[4] );
        n1[8] = _mm_add_epi32( n1[8], n1[12] );

        n1[0] = _mm_add_epi32( n1[0], n1[8] );
        iSum = _mm_add_epi32( iSum, n1[0] );
      }
    }
  }
  else
  {
    const int off = 4;
    // transpose 8x8
    // block 0
    m2[0][0] = _mm_unpacklo_epi16( m1[0][0], m1[1][0] );
    m2[1][0] = _mm_unpacklo_epi16( m1[2][0], m1[3][0] );
    m2[2][0] = _mm_unpackhi_epi16( m1[0][0], m1[1][0] );
    m2[3][0] = _mm_unpackhi_epi16( m1[2][0], m1[3][0] );

    m1[0][0] = _mm_unpacklo_epi32( m2[0][0], m2[1][0] );
    m1[1][0] = _mm_unpackhi_epi32( m2[0][0], m2[1][0] );
    m1[2][0] = _mm_unpacklo_epi32( m2[2][0], m2[3][0] );
    m1[3][0] = _mm_unpackhi_epi32( m2[2][0], m2[3][0] );

    // block 2
    m2[0 + off][0] = _mm_unpacklo_epi16( m1[0 + off][0], m1[1 + off][0] );
    m2[1 + off][0] = _mm_unpacklo_epi16( m1[2 + off][0], m1[3 + off][0] );
    m2[2 + off][0] = _mm_unpackhi_epi16( m1[0 + off][0], m1[1 + off][0] );
    m2[3 + off][0] = _mm_unpackhi_epi16( m1[2 + off][0], m1[3 + off][0] );

    m1[0 + off][0] = _mm_unpacklo_epi32( m2[0 + off][0], m2[1 + off][0] );
    m1[1 + off][0] = _mm_unpackhi_epi32( m2[0 + off][0], m2[1 + off][0] );
    m1[2 + off][0] = _mm_unpacklo_epi32( m2[2 + off][0], m2[3 + off][0] );
    m1[3 + off][0] = _mm_unpackhi_epi32( m2[2 + off][0], m2[3 + off][0] );

    m2[0][0] = _mm_unpacklo_epi64( m1[0][0], m1[0 + off][0] );
    m2[1][0] = _mm_unpackhi_epi64( m1[0][0], m1[0 + off][0] );
    m2[2][0] = _mm_unpacklo_epi64( m1[1][0], m1[1 + off][0] );
    m2[3][0] = _mm_unpackhi_epi64( m1[1][0], m1[1 + off][0] );
    m2[4][0] = _mm_unpacklo_epi64( m1[2][0], m1[2 + off][0] );
    m2[5][0] = _mm_unpackhi_epi64( m1[2][0], m1[2 + off][0] );
    m2[6][0] = _mm_unpacklo_epi64( m1[3][0], m1[3 + off][0] );
    m2[7][0] = _mm_unpackhi_epi64( m1[3][0], m1[3 + off][0] );

    // transpose 8x8
    // block 1
    m2[0][1] = _mm_unpacklo_epi16( m1[0][1], m1[1][1] );
    m2[1][1] = _mm_unpacklo_epi16( m1[2][1], m1[3][1] );
    m2[2][1] = _mm_unpackhi_epi16( m1[0][1], m1[1][1] );
    m2[3][1] = _mm_unpackhi_epi16( m1[2][1], m1[3][1] );

    m1[0][1] = _mm_unpacklo_epi32( m2[0][1], m2[1][1] );
    m1[1][1] = _mm_unpackhi_epi32( m2[0][1], m2[1][1] );
    m1[2][1] = _mm_unpacklo_epi32( m2[2][1], m2[3][1] );
    m1[3][1] = _mm_unpackhi_epi32( m2[2][1], m2[3][1] );

    // block 3
    m2[0 + off][1] = _mm_unpacklo_epi16( m1[0 + off][1], m1[1 + off][1] );
    m2[1 + off][1] = _mm_unpacklo_epi16( m1[2 + off][1], m1[3 + off][1] );
    m2[2 + off][1] = _mm_unpackhi_epi16( m1[0 + off][1], m1[1 + off][1] );
    m2[3 + off][1] = _mm_unpackhi_epi16( m1[2 + off][1], m1[3 + off][1] );

    m1[0 + off][1] = _mm_unpacklo_epi32( m2[0 + off][1], m2[1 + off][1] );
    m1[1 + off][1] = _mm_unpackhi_epi32( m2[0 + off][1], m2[1 + off][1] );
    m1[2 + off][1] = _mm_unpacklo_epi32( m2[2 + off][1], m2[3 + off][1] );
    m1[3 + off][1] = _mm_unpackhi_epi32( m2[2 + off][1], m2[3 + off][1] );

    m2[0 + 8][0] = _mm_unpacklo_epi64( m1[0][1], m1[0 + off][1] );
    m2[1 + 8][0] = _mm_unpackhi_epi64( m1[0][1], m1[0 + off][1] );
    m2[2 + 8][0] = _mm_unpacklo_epi64( m1[1][1], m1[1 + off][1] );
    m2[3 + 8][0] = _mm_unpackhi_epi64( m1[1][1], m1[1 + off][1] );
    m2[4 + 8][0] = _mm_unpacklo_epi64( m1[2][1], m1[2 + off][1] );
    m2[5 + 8][0] = _mm_unpackhi_epi64( m1[2][1], m1[2 + off][1] );
    m2[6 + 8][0] = _mm_unpacklo_epi64( m1[3][1], m1[3 + off][1] );
    m2[7 + 8][0] = _mm_unpackhi_epi64( m1[3][1], m1[3 + off][1] );

    // horizontal
    m1[0][0] = _mm_add_epi16( m2[0][0], m2[8][0] );
    m1[1][0] = _mm_add_epi16( m2[1][0], m2[9][0] );
    m1[2][0] = _mm_add_epi16( m2[2][0], m2[10][0] );
    m1[3][0] = _mm_add_epi16( m2[3][0], m2[11][0] );
    m1[4][0] = _mm_add_epi16( m2[4][0], m2[12][0] );
    m1[5][0] = _mm_add_epi16( m2[5][0], m2[13][0] );
    m1[6][0] = _mm_add_epi16( m2[6][0], m2[14][0] );
    m1[7][0] = _mm_add_epi16( m2[7][0], m2[15][0] );
    m1[8][0] = _mm_sub_epi16( m2[0][0], m2[8][0] );
    m1[9][0] = _mm_sub_epi16( m2[1][0], m2[9][0] );
    m1[10][0] = _mm_sub_epi16( m2[2][0], m2[10][0] );
    m1[11][0] = _mm_sub_epi16( m2[3][0], m2[11][0] );
    m1[12][0] = _mm_sub_epi16( m2[4][0], m2[12][0] );
    m1[13][0] = _mm_sub_epi16( m2[5][0], m2[13][0] );
    m1[14][0] = _mm_sub_epi16( m2[6][0], m2[14][0] );
    m1[15][0] = _mm_sub_epi16( m2[7][0], m2[15][0] );

    m2[0][0] = _mm_add_epi16( m1[0][0], m1[4][0] );
    m2[1][0] = _mm_add_epi16( m1[1][0], m1[5][0] );
    m2[2][0] = _mm_add_epi16( m1[2][0], m1[6][0] );
    m2[3][0] = _mm_add_epi16( m1[3][0], m1[7][0] );
    m2[4][0] = _mm_sub_epi16( m1[0][0], m1[4][0] );
    m2[5][0] = _mm_sub_epi16( m1[1][0], m1[5][0] );
    m2[6][0] = _mm_sub_epi16( m1[2][0], m1[6][0] );
    m2[7][0] = _mm_sub_epi16( m1[3][0], m1[7][0] );
    m2[8][0] = _mm_add_epi16( m1[8][0], m1[12][0] );
    m2[9][0] = _mm_add_epi16( m1[9][0], m1[13][0] );
    m2[10][0] = _mm_add_epi16( m1[10][0], m1[14][0] );
    m2[11][0] = _mm_add_epi16( m1[11][0], m1[15][0] );
    m2[12][0] = _mm_sub_epi16( m1[8][0], m1[12][0] );
    m2[13][0] = _mm_sub_epi16( m1[9][0], m1[13][0] );
    m2[14][0] = _mm_sub_epi16( m1[10][0], m1[14][0] );
    m2[15][0] = _mm_sub_epi16( m1[11][0], m1[15][0] );

    m1[0][0] = _mm_add_epi16( m2[0][0], m2[2][0] );
    m1[1][0] = _mm_add_epi16( m2[1][0], m2[3][0] );
    m1[2][0] = _mm_sub_epi16( m2[0][0], m2[2][0] );
    m1[3][0] = _mm_sub_epi16( m2[1][0], m2[3][0] );
    m1[4][0] = _mm_add_epi16( m2[4][0], m2[6][0] );
    m1[5][0] = _mm_add_epi16( m2[5][0], m2[7][0] );
    m1[6][0] = _mm_sub_epi16( m2[4][0], m2[6][0] );
    m1[7][0] = _mm_sub_epi16( m2[5][0], m2[7][0] );
    m1[8][0] = _mm_add_epi16( m2[8][0], m2[10][0] );
    m1[9][0] = _mm_add_epi16( m2[9][0], m2[11][0] );
    m1[10][0] = _mm_sub_epi16( m2[8][0], m2[10][0] );
    m1[11][0] = _mm_sub_epi16( m2[9][0], m2[11][0] );
    m1[12][0] = _mm_add_epi16( m2[12][0], m2[14][0] );
    m1[13][0] = _mm_add_epi16( m2[13][0], m2[15][0] );
    m1[14][0] = _mm_sub_epi16( m2[12][0], m2[14][0] );
    m1[15][0] = _mm_sub_epi16( m2[13][0], m2[15][0] );

    m2[0][0] = _mm_abs_epi16( _mm_add_epi16( m1[0][0], m1[1][0] ) );
    m2[1][0] = _mm_abs_epi16( _mm_sub_epi16( m1[0][0], m1[1][0] ) );
    m2[2][0] = _mm_abs_epi16( _mm_add_epi16( m1[2][0], m1[3][0] ) );
    m2[3][0] = _mm_abs_epi16( _mm_sub_epi16( m1[2][0], m1[3][0] ) );
    m2[4][0] = _mm_abs_epi16( _mm_add_epi16( m1[4][0], m1[5][0] ) );
    m2[5][0] = _mm_abs_epi16( _mm_sub_epi16( m1[4][0], m1[5][0] ) );
    m2[6][0] = _mm_abs_epi16( _mm_add_epi16( m1[6][0], m1[7][0] ) );
    m2[7][0] = _mm_abs_epi16( _mm_sub_epi16( m1[6][0], m1[7][0] ) );
    m2[8][0] = _mm_abs_epi16( _mm_add_epi16( m1[8][0], m1[9][0] ) );
    m2[9][0] = _mm_abs_epi16( _mm_sub_epi16( m1[8][0], m1[9][0] ) );
    m2[10][0] = _mm_abs_epi16( _mm_add_epi16( m1[10][0], m1[11][0] ) );
    m2[11][0] = _mm_abs_epi16( _mm_sub_epi16( m1[10][0], m1[11][0] ) );
    m2[12][0] = _mm_abs_epi16( _mm_add_epi16( m1[12][0], m1[13][0] ) );
    m2[13][0] = _mm_abs_epi16( _mm_sub_epi16( m1[12][0], m1[13][0] ) );
    m2[14][0] = _mm_abs_epi16( _mm_add_epi16( m1[14][0], m1[15][0] ) );
    m2[15][0] = _mm_abs_epi16( _mm_sub_epi16( m1[14][0], m1[15][0] ) );

    __m128i ma1, ma2;

    for( int i = 0; i < 16; i++ )
    {
      ma1 = _mm_unpacklo_epi16( m2[i][0], vzero );
      ma2 = _mm_unpackhi_epi16( m2[i][0], vzero );
      iSum = _mm_add_epi32( iSum, _mm_add_epi32( ma1, ma2 ) );
    }
  }

  iSum = _mm_hadd_epi32( iSum, iSum );
  iSum = _mm_hadd_epi32( iSum, iSum );

  uint32_t sad = _mm_cvtsi128_si32( iSum );

  sad = (uint32_t)(sad / sqrt(16.0 * 8) * 2);

  return sad;
}


//working up to 12-bit
template< typename Torg, typename Tcur/*, bool bHorDownsampling*/ >
static uint32_t xCalcHAD8x16_SSE( const Torg *piOrg, const Tcur *piCur, const int iStrideOrg, const int iStrideCur, const int iBitDepth )
{
  __m128i m1[16], m2[16];
  __m128i iSum = _mm_setzero_si128();

  for( int k = 0; k < 16; k++ )
  {
    __m128i r0 = (sizeof( Torg ) > 1) ? (_mm_loadu_si128( (__m128i*)piOrg )) : (_mm_unpacklo_epi8( _mm_loadl_epi64( (const __m128i*)piOrg ), _mm_setzero_si128() ));
    __m128i r1 = (sizeof( Tcur ) > 1) ? (_mm_lddqu_si128( (__m128i*)piCur )) : (_mm_unpacklo_epi8( _mm_loadl_epi64( (const __m128i*)piCur ), _mm_setzero_si128() )); // th  _mm_loadu_si128( (__m128i*)piCur )
    m1[k] = _mm_sub_epi16( r0, r1 );
    piCur += iStrideCur;
    piOrg += iStrideOrg;
  }

  // vertical
  m2[0] = _mm_add_epi16( m1[0], m1[8] );
  m2[1] = _mm_add_epi16( m1[1], m1[9] );
  m2[2] = _mm_add_epi16( m1[2], m1[10] );
  m2[3] = _mm_add_epi16( m1[3], m1[11] );
  m2[4] = _mm_add_epi16( m1[4], m1[12] );
  m2[5] = _mm_add_epi16( m1[5], m1[13] );
  m2[6] = _mm_add_epi16( m1[6], m1[14] );
  m2[7] = _mm_add_epi16( m1[7], m1[15] );
  m2[8] = _mm_sub_epi16( m1[0], m1[8] );
  m2[9] = _mm_sub_epi16( m1[1], m1[9] );
  m2[10] = _mm_sub_epi16( m1[2], m1[10] );
  m2[11] = _mm_sub_epi16( m1[3], m1[11] );
  m2[12] = _mm_sub_epi16( m1[4], m1[12] );
  m2[13] = _mm_sub_epi16( m1[5], m1[13] );
  m2[14] = _mm_sub_epi16( m1[6], m1[14] );
  m2[15] = _mm_sub_epi16( m1[7], m1[15] );

  m1[0] = _mm_add_epi16( m2[0], m2[4] );
  m1[1] = _mm_add_epi16( m2[1], m2[5] );
  m1[2] = _mm_add_epi16( m2[2], m2[6] );
  m1[3] = _mm_add_epi16( m2[3], m2[7] );
  m1[4] = _mm_sub_epi16( m2[0], m2[4] );
  m1[5] = _mm_sub_epi16( m2[1], m2[5] );
  m1[6] = _mm_sub_epi16( m2[2], m2[6] );
  m1[7] = _mm_sub_epi16( m2[3], m2[7] );
  m1[8] = _mm_add_epi16( m2[8], m2[12] );
  m1[9] = _mm_add_epi16( m2[9], m2[13] );
  m1[10] = _mm_add_epi16( m2[10], m2[14] );
  m1[11] = _mm_add_epi16( m2[11], m2[15] );
  m1[12] = _mm_sub_epi16( m2[8], m2[12] );
  m1[13] = _mm_sub_epi16( m2[9], m2[13] );
  m1[14] = _mm_sub_epi16( m2[10], m2[14] );
  m1[15] = _mm_sub_epi16( m2[11], m2[15] );

  m2[0] = _mm_add_epi16( m1[0], m1[2] );
  m2[1] = _mm_add_epi16( m1[1], m1[3] );
  m2[2] = _mm_sub_epi16( m1[0], m1[2] );
  m2[3] = _mm_sub_epi16( m1[1], m1[3] );
  m2[4] = _mm_add_epi16( m1[4], m1[6] );
  m2[5] = _mm_add_epi16( m1[5], m1[7] );
  m2[6] = _mm_sub_epi16( m1[4], m1[6] );
  m2[7] = _mm_sub_epi16( m1[5], m1[7] );
  m2[8] = _mm_add_epi16( m1[8], m1[10] );
  m2[9] = _mm_add_epi16( m1[9], m1[11] );
  m2[10] = _mm_sub_epi16( m1[8], m1[10] );
  m2[11] = _mm_sub_epi16( m1[9], m1[11] );
  m2[12] = _mm_add_epi16( m1[12], m1[14] );
  m2[13] = _mm_add_epi16( m1[13], m1[15] );
  m2[14] = _mm_sub_epi16( m1[12], m1[14] );
  m2[15] = _mm_sub_epi16( m1[13], m1[15] );

  m1[ 0] = _mm_add_epi16( m2[0], m2[1] );
  m1[ 1] = _mm_sub_epi16( m2[0], m2[1] );
  m1[ 2] = _mm_add_epi16( m2[2], m2[3] );
  m1[ 3] = _mm_sub_epi16( m2[2], m2[3] );
  m1[ 4] = _mm_add_epi16( m2[4], m2[5] );
  m1[ 5] = _mm_sub_epi16( m2[4], m2[5] );
  m1[ 6] = _mm_add_epi16( m2[6], m2[7] );
  m1[ 7] = _mm_sub_epi16( m2[6], m2[7] );
  m1[ 8] = _mm_add_epi16( m2[8], m2[9] );
  m1[ 9] = _mm_sub_epi16( m2[8], m2[9] );
  m1[10] = _mm_add_epi16( m2[10], m2[11] );
  m1[11] = _mm_sub_epi16( m2[10], m2[11] );
  m1[12] = _mm_add_epi16( m2[12], m2[13] );
  m1[13] = _mm_sub_epi16( m2[12], m2[13] );
  m1[14] = _mm_add_epi16( m2[14], m2[15] );
  m1[15] = _mm_sub_epi16( m2[14], m2[15] );


  // process horizontal in two steps ( 2 x 8x8 blocks )

  for( int l = 0; l < 2; l++ )
  {
    int off = l * 8;

    // transpose 8x8
    // blocks 0,1  and 2,3
    m2[0] = _mm_unpacklo_epi16( m1[0 + off], m1[1 + off] );
    m2[1] = _mm_unpacklo_epi16( m1[2 + off], m1[3 + off] );
    m2[2] = _mm_unpacklo_epi16( m1[4 + off], m1[5 + off] );
    m2[3] = _mm_unpacklo_epi16( m1[6 + off], m1[7 + off] );

    m2[0 + 4] = _mm_unpackhi_epi16( m1[0 + off], m1[1 + off] );
    m2[1 + 4] = _mm_unpackhi_epi16( m1[2 + off], m1[3 + off] );
    m2[2 + 4] = _mm_unpackhi_epi16( m1[4 + off], m1[5 + off] );
    m2[3 + 4] = _mm_unpackhi_epi16( m1[6 + off], m1[7 + off] );

    m1[0] = _mm_unpacklo_epi32( m2[0], m2[1] );
    m1[1] = _mm_unpackhi_epi32( m2[0], m2[1] );
    m1[2] = _mm_unpacklo_epi32( m2[2], m2[3] );
    m1[3] = _mm_unpackhi_epi32( m2[2], m2[3] );

    m2[0] = _mm_unpacklo_epi64( m1[0], m1[2] );
    m2[1] = _mm_unpackhi_epi64( m1[0], m1[2] );
    m2[2] = _mm_unpacklo_epi64( m1[1], m1[3] );
    m2[3] = _mm_unpackhi_epi64( m1[1], m1[3] );

    m1[0 + 4] = _mm_unpacklo_epi32( m2[0 + 4], m2[1 + 4] );
    m1[1 + 4] = _mm_unpackhi_epi32( m2[0 + 4], m2[1 + 4] );
    m1[2 + 4] = _mm_unpacklo_epi32( m2[2 + 4], m2[3 + 4] );
    m1[3 + 4] = _mm_unpackhi_epi32( m2[2 + 4], m2[3 + 4] );

    m2[0 + 4] = _mm_unpacklo_epi64( m1[0 + 4], m1[2 + 4] );
    m2[1 + 4] = _mm_unpackhi_epi64( m1[0 + 4], m1[2 + 4] );
    m2[2 + 4] = _mm_unpacklo_epi64( m1[1 + 4], m1[3 + 4] );
    m2[3 + 4] = _mm_unpackhi_epi64( m1[1 + 4], m1[3 + 4] );

    // horizontal calculation
    if( iBitDepth >= 10 )
    {
      __m128i n1[8][2];
      __m128i n2[8][2];

      for( int i = 0; i < 8; i++ )
      {
        n2[i][0] = _mm_cvtepi16_epi32( m2[i] );
        n2[i][1] = _mm_cvtepi16_epi32( _mm_shuffle_epi32( m2[i], 0xEE ) );
      }

      for( int i = 0; i < 2; i++ )
      {
        n1[0][i] = _mm_add_epi32( n2[0][i], n2[4][i] );
        n1[1][i] = _mm_add_epi32( n2[1][i], n2[5][i] );
        n1[2][i] = _mm_add_epi32( n2[2][i], n2[6][i] );
        n1[3][i] = _mm_add_epi32( n2[3][i], n2[7][i] );
        n1[4][i] = _mm_sub_epi32( n2[0][i], n2[4][i] );
        n1[5][i] = _mm_sub_epi32( n2[1][i], n2[5][i] );
        n1[6][i] = _mm_sub_epi32( n2[2][i], n2[6][i] );
        n1[7][i] = _mm_sub_epi32( n2[3][i], n2[7][i] );

        n2[0][i] = _mm_add_epi32( n1[0][i], n1[2][i] );
        n2[1][i] = _mm_add_epi32( n1[1][i], n1[3][i] );
        n2[2][i] = _mm_sub_epi32( n1[0][i], n1[2][i] );
        n2[3][i] = _mm_sub_epi32( n1[1][i], n1[3][i] );
        n2[4][i] = _mm_add_epi32( n1[4][i], n1[6][i] );
        n2[5][i] = _mm_add_epi32( n1[5][i], n1[7][i] );
        n2[6][i] = _mm_sub_epi32( n1[4][i], n1[6][i] );
        n2[7][i] = _mm_sub_epi32( n1[5][i], n1[7][i] );

        n1[0][i] = _mm_abs_epi32( _mm_add_epi32( n2[0][i], n2[1][i] ) );
        n1[1][i] = _mm_abs_epi32( _mm_sub_epi32( n2[0][i], n2[1][i] ) );
        n1[2][i] = _mm_abs_epi32( _mm_add_epi32( n2[2][i], n2[3][i] ) );
        n1[3][i] = _mm_abs_epi32( _mm_sub_epi32( n2[2][i], n2[3][i] ) );
        n1[4][i] = _mm_abs_epi32( _mm_add_epi32( n2[4][i], n2[5][i] ) );
        n1[5][i] = _mm_abs_epi32( _mm_sub_epi32( n2[4][i], n2[5][i] ) );
        n1[6][i] = _mm_abs_epi32( _mm_add_epi32( n2[6][i], n2[7][i] ) );
        n1[7][i] = _mm_abs_epi32( _mm_sub_epi32( n2[6][i], n2[7][i] ) );
      }
      for( int i = 0; i < 8; i++ )
      {
        m1[i] = _mm_add_epi32( n1[i][0], n1[i][1] );
      }
    }
    else
    {
      m1[0] = _mm_add_epi16( m2[0], m2[4] );
      m1[1] = _mm_add_epi16( m2[1], m2[5] );
      m1[2] = _mm_add_epi16( m2[2], m2[6] );
      m1[3] = _mm_add_epi16( m2[3], m2[7] );
      m1[4] = _mm_sub_epi16( m2[0], m2[4] );
      m1[5] = _mm_sub_epi16( m2[1], m2[5] );
      m1[6] = _mm_sub_epi16( m2[2], m2[6] );
      m1[7] = _mm_sub_epi16( m2[3], m2[7] );

      m2[0] = _mm_add_epi16( m1[0], m1[2] );
      m2[1] = _mm_add_epi16( m1[1], m1[3] );
      m2[2] = _mm_sub_epi16( m1[0], m1[2] );
      m2[3] = _mm_sub_epi16( m1[1], m1[3] );
      m2[4] = _mm_add_epi16( m1[4], m1[6] );
      m2[5] = _mm_add_epi16( m1[5], m1[7] );
      m2[6] = _mm_sub_epi16( m1[4], m1[6] );
      m2[7] = _mm_sub_epi16( m1[5], m1[7] );

      m1[0] = _mm_abs_epi16( _mm_add_epi16( m2[0], m2[1] ) );
      m1[1] = _mm_abs_epi16( _mm_sub_epi16( m2[0], m2[1] ) );
      m1[2] = _mm_abs_epi16( _mm_add_epi16( m2[2], m2[3] ) );
      m1[3] = _mm_abs_epi16( _mm_sub_epi16( m2[2], m2[3] ) );
      m1[4] = _mm_abs_epi16( _mm_add_epi16( m2[4], m2[5] ) );
      m1[5] = _mm_abs_epi16( _mm_sub_epi16( m2[4], m2[5] ) );
      m1[6] = _mm_abs_epi16( _mm_add_epi16( m2[6], m2[7] ) );
      m1[7] = _mm_abs_epi16( _mm_sub_epi16( m2[6], m2[7] ) );

      __m128i ma1, ma2;
      __m128i vzero = _mm_setzero_si128();

      for( int i = 0; i < 8; i++ )
      {
        ma1 = _mm_unpacklo_epi16( m1[i], vzero );
        ma2 = _mm_unpackhi_epi16( m1[i], vzero );
        m1[i] = _mm_add_epi32( ma1, ma2 );
      }
    }

    m1[0] = _mm_add_epi32( m1[0], m1[1] );
    m1[2] = _mm_add_epi32( m1[2], m1[3] );
    m1[4] = _mm_add_epi32( m1[4], m1[5] );
    m1[6] = _mm_add_epi32( m1[6], m1[7] );

    m1[0] = _mm_add_epi32( m1[0], m1[2] );
    m1[4] = _mm_add_epi32( m1[4], m1[6] );
    iSum = _mm_add_epi32( iSum, _mm_add_epi32( m1[0], m1[4] ) );
  }

  iSum = _mm_hadd_epi32( iSum, iSum );
  iSum = _mm_hadd_epi32( iSum, iSum );

  uint32_t sad = _mm_cvtsi128_si32( iSum );

  sad = (uint32_t)(sad / sqrt(16.0 * 8) * 2);

  return sad;
}


template< typename Torg, typename Tcur/*, bool bHorDownsampling*/ >
static uint32_t xCalcHAD8x4_SSE( const Torg *piOrg, const Tcur *piCur, const int iStrideOrg, const int iStrideCur, const int iBitDepth )
{
  __m128i m1[8], m2[8];
  __m128i vzero = _mm_setzero_si128();

  for( int k = 0; k < 4; k++ )
  {
    __m128i r0 = (sizeof( Torg ) > 1) ? (_mm_loadu_si128 ( (__m128i*)piOrg )) : (_mm_unpacklo_epi8( _mm_loadl_epi64( (const __m128i*)piOrg ), _mm_setzero_si128() ));
    __m128i r1 = (sizeof( Tcur ) > 1) ? (_mm_lddqu_si128( (__m128i*)piCur )) : (_mm_unpacklo_epi8( _mm_loadl_epi64( (const __m128i*)piCur ), _mm_setzero_si128() )); // th  _mm_loadu_si128( (__m128i*)piCur )
    m1[k] = _mm_sub_epi16( r0, r1 );
    piCur += iStrideCur;
    piOrg += iStrideOrg;
  }

  //vertical
  m2[0] = _mm_add_epi16( m1[0], m1[2] );
  m2[1] = _mm_add_epi16( m1[1], m1[3] );
  m2[2] = _mm_sub_epi16( m1[0], m1[2] );
  m2[3] = _mm_sub_epi16( m1[1], m1[3] );

  m1[0] = _mm_add_epi16( m2[0], m2[1] );
  m1[1] = _mm_sub_epi16( m2[0], m2[1] );
  m1[2] = _mm_add_epi16( m2[2], m2[3] );
  m1[3] = _mm_sub_epi16( m2[2], m2[3] );

  // transpose, partially
  {
    m2[0] = _mm_unpacklo_epi16( m1[0], m1[1] );
    m2[1] = _mm_unpacklo_epi16( m1[2], m1[3] );
    m2[2] = _mm_unpackhi_epi16( m1[0], m1[1] );
    m2[3] = _mm_unpackhi_epi16( m1[2], m1[3] );

    m1[0] = _mm_unpacklo_epi32( m2[0], m2[1] );
    m1[1] = _mm_unpackhi_epi32( m2[0], m2[1] );
    m1[2] = _mm_unpacklo_epi32( m2[2], m2[3] );
    m1[3] = _mm_unpackhi_epi32( m2[2], m2[3] );
  }

  // horizontal
  if( iBitDepth >= 10 /*sizeof( Torg ) > 1 || sizeof( Tcur ) > 1*/ )
  {
    // finish transpose
    m2[0] = _mm_unpacklo_epi64( m1[0], vzero );
    m2[1] = _mm_unpackhi_epi64( m1[0], vzero );
    m2[2] = _mm_unpacklo_epi64( m1[1], vzero );
    m2[3] = _mm_unpackhi_epi64( m1[1], vzero );
    m2[4] = _mm_unpacklo_epi64( m1[2], vzero );
    m2[5] = _mm_unpackhi_epi64( m1[2], vzero );
    m2[6] = _mm_unpacklo_epi64( m1[3], vzero );
    m2[7] = _mm_unpackhi_epi64( m1[3], vzero );

    for( int i = 0; i < 8; i++ )
    {
      m2[i] = _mm_cvtepi16_epi32( m2[i] );
    }

    m1[0] = _mm_add_epi32( m2[0], m2[4] );
    m1[1] = _mm_add_epi32( m2[1], m2[5] );
    m1[2] = _mm_add_epi32( m2[2], m2[6] );
    m1[3] = _mm_add_epi32( m2[3], m2[7] );
    m1[4] = _mm_sub_epi32( m2[0], m2[4] );
    m1[5] = _mm_sub_epi32( m2[1], m2[5] );
    m1[6] = _mm_sub_epi32( m2[2], m2[6] );
    m1[7] = _mm_sub_epi32( m2[3], m2[7] );

    m2[0] = _mm_add_epi32( m1[0], m1[2] );
    m2[1] = _mm_add_epi32( m1[1], m1[3] );
    m2[2] = _mm_sub_epi32( m1[0], m1[2] );
    m2[3] = _mm_sub_epi32( m1[1], m1[3] );
    m2[4] = _mm_add_epi32( m1[4], m1[6] );
    m2[5] = _mm_add_epi32( m1[5], m1[7] );
    m2[6] = _mm_sub_epi32( m1[4], m1[6] );
    m2[7] = _mm_sub_epi32( m1[5], m1[7] );

    m1[0] = _mm_abs_epi32( _mm_add_epi32( m2[0], m2[1] ) );
    m1[1] = _mm_abs_epi32( _mm_sub_epi32( m2[0], m2[1] ) );
    m1[2] = _mm_abs_epi32( _mm_add_epi32( m2[2], m2[3] ) );
    m1[3] = _mm_abs_epi32( _mm_sub_epi32( m2[2], m2[3] ) );
    m1[4] = _mm_abs_epi32( _mm_add_epi32( m2[4], m2[5] ) );
    m1[5] = _mm_abs_epi32( _mm_sub_epi32( m2[4], m2[5] ) );
    m1[6] = _mm_abs_epi32( _mm_add_epi32( m2[6], m2[7] ) );
    m1[7] = _mm_abs_epi32( _mm_sub_epi32( m2[6], m2[7] ) );
  }
  else
  {
    m2[0] = _mm_add_epi16( m1[0], m1[2] );
    m2[1] = _mm_add_epi16( m1[1], m1[3] );
    m2[2] = _mm_sub_epi16( m1[0], m1[2] );
    m2[3] = _mm_sub_epi16( m1[1], m1[3] );

    m1[0] = _mm_add_epi16( m2[0], m2[1] );
    m1[1] = _mm_sub_epi16( m2[0], m2[1] );
    m1[2] = _mm_add_epi16( m2[2], m2[3] );
    m1[3] = _mm_sub_epi16( m2[2], m2[3] );

    // finish transpose
    m2[0] = _mm_unpacklo_epi64( m1[0], vzero );
    m2[1] = _mm_unpackhi_epi64( m1[0], vzero );
    m2[2] = _mm_unpacklo_epi64( m1[1], vzero );
    m2[3] = _mm_unpackhi_epi64( m1[1], vzero );
    m2[4] = _mm_unpacklo_epi64( m1[2], vzero );
    m2[5] = _mm_unpackhi_epi64( m1[2], vzero );
    m2[6] = _mm_unpacklo_epi64( m1[3], vzero );
    m2[7] = _mm_unpackhi_epi64( m1[3], vzero );

    m1[0] = _mm_abs_epi16( _mm_add_epi16( m2[0], m2[1] ) );
    m1[1] = _mm_abs_epi16( _mm_sub_epi16( m2[0], m2[1] ) );
    m1[2] = _mm_abs_epi16( _mm_add_epi16( m2[2], m2[3] ) );
    m1[3] = _mm_abs_epi16( _mm_sub_epi16( m2[2], m2[3] ) );
    m1[4] = _mm_abs_epi16( _mm_add_epi16( m2[4], m2[5] ) );
    m1[5] = _mm_abs_epi16( _mm_sub_epi16( m2[4], m2[5] ) );
    m1[6] = _mm_abs_epi16( _mm_add_epi16( m2[6], m2[7] ) );
    m1[7] = _mm_abs_epi16( _mm_sub_epi16( m2[6], m2[7] ) );

    for( int i = 0; i < 8; i++ )
    {
      m1[i] = _mm_unpacklo_epi16( m1[i], vzero );
    }
  }


  m1[0] = _mm_add_epi32( m1[0], m1[1] );
  m1[1] = _mm_add_epi32( m1[2], m1[3] );
  m1[2] = _mm_add_epi32( m1[4], m1[5] );
  m1[3] = _mm_add_epi32( m1[6], m1[7] );

  m1[0] = _mm_add_epi32( m1[0], m1[1] );
  m1[1] = _mm_add_epi32( m1[2], m1[3] );

  __m128i iSum = _mm_add_epi32( m1[0], m1[1] );

  iSum = _mm_hadd_epi32( iSum, iSum );
  iSum = _mm_hadd_epi32( iSum, iSum );

  uint32_t sad = _mm_cvtsi128_si32( iSum );
  //sad = ((sad + 2) >> 2);
  sad = (uint32_t)(sad / sqrt(4.0 * 8) * 2);
  return sad;
}


template< typename Torg, typename Tcur/*, bool bHorDownsampling*/ >
static uint32_t xCalcHAD4x8_SSE( const Torg *piOrg, const Tcur *piCur, const int iStrideOrg, const int iStrideCur, const int iBitDepth )
{
  __m128i m1[8], m2[8];

  for( int k = 0; k < 8; k++ )
  {
    __m128i r0 = (sizeof( Torg ) > 1) ? (_mm_loadl_epi64( (__m128i*)piOrg )) : (_mm_cvtsi32_si128( *(const int*)piOrg ));
    __m128i r1 = (sizeof( Tcur ) > 1) ? (_mm_loadl_epi64( (__m128i*)piCur )) : (_mm_cvtsi32_si128( *(const int*)piCur ));
    m2[k] = _mm_sub_epi16( r0, r1 );
    piCur += iStrideCur;
    piOrg += iStrideOrg;
  }


  // vertical

  m1[0] = _mm_add_epi16( m2[0], m2[4] );
  m1[1] = _mm_add_epi16( m2[1], m2[5] );
  m1[2] = _mm_add_epi16( m2[2], m2[6] );
  m1[3] = _mm_add_epi16( m2[3], m2[7] );
  m1[4] = _mm_sub_epi16( m2[0], m2[4] );
  m1[5] = _mm_sub_epi16( m2[1], m2[5] );
  m1[6] = _mm_sub_epi16( m2[2], m2[6] );
  m1[7] = _mm_sub_epi16( m2[3], m2[7] );

  m2[0] = _mm_add_epi16( m1[0], m1[2] );
  m2[1] = _mm_add_epi16( m1[1], m1[3] );
  m2[2] = _mm_sub_epi16( m1[0], m1[2] );
  m2[3] = _mm_sub_epi16( m1[1], m1[3] );
  m2[4] = _mm_add_epi16( m1[4], m1[6] );
  m2[5] = _mm_add_epi16( m1[5], m1[7] );
  m2[6] = _mm_sub_epi16( m1[4], m1[6] );
  m2[7] = _mm_sub_epi16( m1[5], m1[7] );

  m1[0] = _mm_add_epi16( m2[0], m2[1] );
  m1[1] = _mm_sub_epi16( m2[0], m2[1] );
  m1[2] = _mm_add_epi16( m2[2], m2[3] );
  m1[3] = _mm_sub_epi16( m2[2], m2[3] );
  m1[4] = _mm_add_epi16( m2[4], m2[5] );
  m1[5] = _mm_sub_epi16( m2[4], m2[5] );
  m1[6] = _mm_add_epi16( m2[6], m2[7] );
  m1[7] = _mm_sub_epi16( m2[6], m2[7] );


  // horizontal
  // transpose
  {
    m2[0] = _mm_unpacklo_epi16( m1[0], m1[1] );
    m2[1] = _mm_unpacklo_epi16( m1[2], m1[3] );
    m2[2] = _mm_unpacklo_epi16( m1[4], m1[5] );
    m2[3] = _mm_unpacklo_epi16( m1[6], m1[7] );

    m1[0] = _mm_unpacklo_epi32( m2[0], m2[1] );
    m1[1] = _mm_unpackhi_epi32( m2[0], m2[1] );
    m1[2] = _mm_unpacklo_epi32( m2[2], m2[3] );
    m1[3] = _mm_unpackhi_epi32( m2[2], m2[3] );

    m2[0] = _mm_unpacklo_epi64( m1[0], m1[2] );
    m2[1] = _mm_unpackhi_epi64( m1[0], m1[2] );
    m2[2] = _mm_unpacklo_epi64( m1[1], m1[3] );
    m2[3] = _mm_unpackhi_epi64( m1[1], m1[3] );
  }

  if( iBitDepth >= 10 /*sizeof( Torg ) > 1 || sizeof( Tcur ) > 1*/ )
  {
    __m128i n1[4][2];
    __m128i n2[4][2];

    for( int i = 0; i < 4; i++ )
    {
      n1[i][0] = _mm_cvtepi16_epi32( m2[i] );
      n1[i][1] = _mm_cvtepi16_epi32( _mm_shuffle_epi32( m2[i], 0xEE ) );
    }

    for( int i = 0; i < 2; i++ )
    {
      n2[0][i] = _mm_add_epi32( n1[0][i], n1[2][i] );
      n2[1][i] = _mm_add_epi32( n1[1][i], n1[3][i] );
      n2[2][i] = _mm_sub_epi32( n1[0][i], n1[2][i] );
      n2[3][i] = _mm_sub_epi32( n1[1][i], n1[3][i] );

      n1[0][i] = _mm_abs_epi32( _mm_add_epi32( n2[0][i], n2[1][i] ) );
      n1[1][i] = _mm_abs_epi32( _mm_sub_epi32( n2[0][i], n2[1][i] ) );
      n1[2][i] = _mm_abs_epi32( _mm_add_epi32( n2[2][i], n2[3][i] ) );
      n1[3][i] = _mm_abs_epi32( _mm_sub_epi32( n2[2][i], n2[3][i] ) );
    }
    for( int i = 0; i < 4; i++ )
    {
      m1[i] = _mm_add_epi32( n1[i][0], n1[i][1] );
    }
  }
  else
  {
    m1[0] = _mm_add_epi16( m2[0], m2[2] );
    m1[1] = _mm_add_epi16( m2[1], m2[3] );
    m1[2] = _mm_sub_epi16( m2[0], m2[2] );
    m1[3] = _mm_sub_epi16( m2[1], m2[3] );

    m2[0] = _mm_abs_epi16( _mm_add_epi16( m1[0], m1[1] ) );
    m2[1] = _mm_abs_epi16( _mm_sub_epi16( m1[0], m1[1] ) );
    m2[2] = _mm_abs_epi16( _mm_add_epi16( m1[2], m1[3] ) );
    m2[3] = _mm_abs_epi16( _mm_sub_epi16( m1[2], m1[3] ) );

    __m128i ma1, ma2;
    __m128i vzero = _mm_setzero_si128();

    for( int i = 0; i < 4; i++ )
    {
      ma1 = _mm_unpacklo_epi16( m2[i], vzero );
      ma2 = _mm_unpackhi_epi16( m2[i], vzero );
      m1[i] = _mm_add_epi32( ma1, ma2 );
    }
  }

  m1[0] = _mm_add_epi32( m1[0], m1[1] );
  m1[2] = _mm_add_epi32( m1[2], m1[3] );

  __m128i iSum = _mm_add_epi32( m1[0], m1[2] );

  iSum = _mm_hadd_epi32( iSum, iSum );
  iSum = _mm_hadd_epi32( iSum, iSum );

  uint32_t sad = _mm_cvtsi128_si32( iSum );

  //sad = ((sad + 2) >> 2);
  sad = (uint32_t)(sad / sqrt(4.0 * 8) * 2);

  return sad;
}


template< typename Torg, typename Tcur/*, bool bHorDownsampling*/ >
static uint32_t xCalcHAD16x16_AVX2( const Torg *piOrg, const Tcur *piCur, const int iStrideOrg, const int iStrideCur, const int iBitDepth )
{
  uint32_t sad = 0;

#ifdef USE_AVX2
  // const int iLoops = ( bHorDownsampling && HAD_DOWNSAMPLING_HOR ) ? ( 1 ) : ( 2 );
  const int iLoops = 2;
  __m256i m1[8], m2[8];

  for( int l = 0; l < iLoops; l++ )
  {
    {
      for( int k = 0; k < 8; k++ )
      {
        __m256i r0 = ( sizeof( Torg ) > 1 ) ? ( _mm256_lddqu_si256( ( __m256i* )piOrg ) ) : ( _mm256_unpacklo_epi8( _mm256_permute4x64_epi64( _mm256_castsi128_si256( _mm_lddqu_si128( ( __m128i* )piOrg ) ), 0xD8 ), _mm256_setzero_si256() ) );
        __m256i r1 = ( sizeof( Tcur ) > 1 ) ? ( _mm256_lddqu_si256( ( __m256i* )piCur ) ) : ( _mm256_unpacklo_epi8( _mm256_permute4x64_epi64( _mm256_castsi128_si256( _mm_lddqu_si128( ( __m128i* )piCur ) ), 0xD8 ), _mm256_setzero_si256() ) );
        m2[k] = _mm256_sub_epi16( r0, r1 );
        piCur += iStrideCur;
        piOrg += iStrideOrg;
      }
    }

    // horizontal

    m1[0] = _mm256_add_epi16( m2[0], m2[4] );
    m1[1] = _mm256_add_epi16( m2[1], m2[5] );
    m1[2] = _mm256_add_epi16( m2[2], m2[6] );
    m1[3] = _mm256_add_epi16( m2[3], m2[7] );
    m1[4] = _mm256_sub_epi16( m2[0], m2[4] );
    m1[5] = _mm256_sub_epi16( m2[1], m2[5] );
    m1[6] = _mm256_sub_epi16( m2[2], m2[6] );
    m1[7] = _mm256_sub_epi16( m2[3], m2[7] );

    m2[0] = _mm256_add_epi16( m1[0], m1[2] );
    m2[1] = _mm256_add_epi16( m1[1], m1[3] );
    m2[2] = _mm256_sub_epi16( m1[0], m1[2] );
    m2[3] = _mm256_sub_epi16( m1[1], m1[3] );
    m2[4] = _mm256_add_epi16( m1[4], m1[6] );
    m2[5] = _mm256_add_epi16( m1[5], m1[7] );
    m2[6] = _mm256_sub_epi16( m1[4], m1[6] );
    m2[7] = _mm256_sub_epi16( m1[5], m1[7] );

    m1[0] = _mm256_add_epi16( m2[0], m2[1] );
    m1[1] = _mm256_sub_epi16( m2[0], m2[1] );
    m1[2] = _mm256_add_epi16( m2[2], m2[3] );
    m1[3] = _mm256_sub_epi16( m2[2], m2[3] );
    m1[4] = _mm256_add_epi16( m2[4], m2[5] );
    m1[5] = _mm256_sub_epi16( m2[4], m2[5] );
    m1[6] = _mm256_add_epi16( m2[6], m2[7] );
    m1[7] = _mm256_sub_epi16( m2[6], m2[7] );

    // transpose 2 8x8 blocks in parallel

    m2[0] = _mm256_unpacklo_epi16( m1[0], m1[1] );
    m2[1] = _mm256_unpacklo_epi16( m1[2], m1[3] );
    m2[2] = _mm256_unpacklo_epi16( m1[4], m1[5] );
    m2[3] = _mm256_unpacklo_epi16( m1[6], m1[7] );
    m2[4] = _mm256_unpackhi_epi16( m1[0], m1[1] );
    m2[5] = _mm256_unpackhi_epi16( m1[2], m1[3] );
    m2[6] = _mm256_unpackhi_epi16( m1[4], m1[5] );
    m2[7] = _mm256_unpackhi_epi16( m1[6], m1[7] );

    m1[0] = _mm256_unpacklo_epi32( m2[0], m2[1] );
    m1[1] = _mm256_unpackhi_epi32( m2[0], m2[1] );
    m1[2] = _mm256_unpacklo_epi32( m2[2], m2[3] );
    m1[3] = _mm256_unpackhi_epi32( m2[2], m2[3] );
    m1[4] = _mm256_unpacklo_epi32( m2[4], m2[5] );
    m1[5] = _mm256_unpackhi_epi32( m2[4], m2[5] );
    m1[6] = _mm256_unpacklo_epi32( m2[6], m2[7] );
    m1[7] = _mm256_unpackhi_epi32( m2[6], m2[7] );

    m2[0] = _mm256_unpacklo_epi64( m1[0], m1[2] );
    m2[1] = _mm256_unpackhi_epi64( m1[0], m1[2] );
    m2[2] = _mm256_unpacklo_epi64( m1[1], m1[3] );
    m2[3] = _mm256_unpackhi_epi64( m1[1], m1[3] );
    m2[4] = _mm256_unpacklo_epi64( m1[4], m1[6] );
    m2[5] = _mm256_unpackhi_epi64( m1[4], m1[6] );
    m2[6] = _mm256_unpacklo_epi64( m1[5], m1[7] );
    m2[7] = _mm256_unpackhi_epi64( m1[5], m1[7] );

    // vertical
    if( iBitDepth >= 10 )
    {
      __m256i n1[8][2];
      __m256i n2[8][2];

      for( int i = 0; i < 8; i++ )
      {
        n2[i][0] = _mm256_cvtepi16_epi32( _mm256_castsi256_si128( _mm256_permute4x64_epi64( m2[i], 0xD8 ) ) );
        n2[i][1] = _mm256_cvtepi16_epi32( _mm256_castsi256_si128( _mm256_permute4x64_epi64( m2[i], 0x8D ) ) );
      }

      for( int i = 0; i < 2; i++ )
      {
        n1[0][i] = _mm256_add_epi32( n2[0][i], n2[4][i] );
        n1[1][i] = _mm256_add_epi32( n2[1][i], n2[5][i] );
        n1[2][i] = _mm256_add_epi32( n2[2][i], n2[6][i] );
        n1[3][i] = _mm256_add_epi32( n2[3][i], n2[7][i] );
        n1[4][i] = _mm256_sub_epi32( n2[0][i], n2[4][i] );
        n1[5][i] = _mm256_sub_epi32( n2[1][i], n2[5][i] );
        n1[6][i] = _mm256_sub_epi32( n2[2][i], n2[6][i] );
        n1[7][i] = _mm256_sub_epi32( n2[3][i], n2[7][i] );

        n2[0][i] = _mm256_add_epi32( n1[0][i], n1[2][i] );
        n2[1][i] = _mm256_add_epi32( n1[1][i], n1[3][i] );
        n2[2][i] = _mm256_sub_epi32( n1[0][i], n1[2][i] );
        n2[3][i] = _mm256_sub_epi32( n1[1][i], n1[3][i] );
        n2[4][i] = _mm256_add_epi32( n1[4][i], n1[6][i] );
        n2[5][i] = _mm256_add_epi32( n1[5][i], n1[7][i] );
        n2[6][i] = _mm256_sub_epi32( n1[4][i], n1[6][i] );
        n2[7][i] = _mm256_sub_epi32( n1[5][i], n1[7][i] );

        n1[0][i] = _mm256_abs_epi32( _mm256_add_epi32( n2[0][i], n2[1][i] ) );
        n1[1][i] = _mm256_abs_epi32( _mm256_sub_epi32( n2[0][i], n2[1][i] ) );
        n1[2][i] = _mm256_abs_epi32( _mm256_add_epi32( n2[2][i], n2[3][i] ) );
        n1[3][i] = _mm256_abs_epi32( _mm256_sub_epi32( n2[2][i], n2[3][i] ) );
        n1[4][i] = _mm256_abs_epi32( _mm256_add_epi32( n2[4][i], n2[5][i] ) );
        n1[5][i] = _mm256_abs_epi32( _mm256_sub_epi32( n2[4][i], n2[5][i] ) );
        n1[6][i] = _mm256_abs_epi32( _mm256_add_epi32( n2[6][i], n2[7][i] ) );
        n1[7][i] = _mm256_abs_epi32( _mm256_sub_epi32( n2[6][i], n2[7][i] ) );
      }
      for( int i = 0; i < 8; i++ )
      {
        m1[i] = _mm256_add_epi32( n1[i][0], n1[i][1] );
      }
    }
    else
    {
      m1[0] = _mm256_add_epi16( m2[0], m2[4] );
      m1[1] = _mm256_add_epi16( m2[1], m2[5] );
      m1[2] = _mm256_add_epi16( m2[2], m2[6] );
      m1[3] = _mm256_add_epi16( m2[3], m2[7] );
      m1[4] = _mm256_sub_epi16( m2[0], m2[4] );
      m1[5] = _mm256_sub_epi16( m2[1], m2[5] );
      m1[6] = _mm256_sub_epi16( m2[2], m2[6] );
      m1[7] = _mm256_sub_epi16( m2[3], m2[7] );

      m2[0] = _mm256_add_epi16( m1[0], m1[2] );
      m2[1] = _mm256_add_epi16( m1[1], m1[3] );
      m2[2] = _mm256_sub_epi16( m1[0], m1[2] );
      m2[3] = _mm256_sub_epi16( m1[1], m1[3] );
      m2[4] = _mm256_add_epi16( m1[4], m1[6] );
      m2[5] = _mm256_add_epi16( m1[5], m1[7] );
      m2[6] = _mm256_sub_epi16( m1[4], m1[6] );
      m2[7] = _mm256_sub_epi16( m1[5], m1[7] );

      m1[0] = _mm256_abs_epi16( _mm256_add_epi16( m2[0], m2[1] ) );
      m1[1] = _mm256_abs_epi16( _mm256_sub_epi16( m2[0], m2[1] ) );
      m1[2] = _mm256_abs_epi16( _mm256_add_epi16( m2[2], m2[3] ) );
      m1[3] = _mm256_abs_epi16( _mm256_sub_epi16( m2[2], m2[3] ) );
      m1[4] = _mm256_abs_epi16( _mm256_add_epi16( m2[4], m2[5] ) );
      m1[5] = _mm256_abs_epi16( _mm256_sub_epi16( m2[4], m2[5] ) );
      m1[6] = _mm256_abs_epi16( _mm256_add_epi16( m2[6], m2[7] ) );
      m1[7] = _mm256_abs_epi16( _mm256_sub_epi16( m2[6], m2[7] ) );

      __m256i ma1, ma2;
      __m256i vzero = _mm256_setzero_si256();

      for( int i = 0; i < 8; i++ )
      {
        ma1 = _mm256_unpacklo_epi16( m1[i], vzero );
        ma2 = _mm256_unpackhi_epi16( m1[i], vzero );
        m1[i] = _mm256_add_epi32( ma1, ma2 );
      }
    }

    m1[0] = _mm256_add_epi32( m1[0], m1[1] );
    m1[2] = _mm256_add_epi32( m1[2], m1[3] );
    m1[4] = _mm256_add_epi32( m1[4], m1[5] );
    m1[6] = _mm256_add_epi32( m1[6], m1[7] );

    m1[0] = _mm256_add_epi32( m1[0], m1[2] );
    m1[4] = _mm256_add_epi32( m1[4], m1[6] );

    __m256i iSum = _mm256_add_epi32( m1[0], m1[4] );
    iSum = _mm256_hadd_epi32( iSum, iSum );
    iSum = _mm256_hadd_epi32( iSum, iSum );

    uint32_t tmp;
    tmp = _mm_cvtsi128_si32( _mm256_castsi256_si128( iSum ) );
    tmp = ( ( tmp + 2 ) >> 2 );
    sad += tmp;

    tmp = _mm_cvtsi128_si32( _mm256_castsi256_si128( _mm256_permute2x128_si256( iSum, iSum, 0x11 ) ) );
    tmp = ( ( tmp + 2 ) >> 2 );
    sad += tmp;
  }

#endif
  return ( sad );
}

template< typename Torg, typename Tcur/*, bool bHorDownsampling*/ >
static uint32_t xCalcHAD16x8_AVX2( const Torg *piOrg, const Tcur *piCur, const int iStrideOrg, const int iStrideCur, const int iBitDepth )
{
  uint32_t sad = 0;

#ifdef USE_AVX2
  // const int iLoops = ( bHorDownsampling && HAD_DOWNSAMPLING_HOR ) ? ( 1 ) : ( 2 );
  //const int iLoops = 1;
  __m256i m1[16], m2[16];
  __m256i vzero = _mm256_setzero_si256();


  //for( int l = 0; l < iLoops; l++ )
  {
    {
      for( int k = 0; k < 8; k++ )
      {
        __m256i r0 = (sizeof( Torg ) > 1) ? (_mm256_lddqu_si256( (__m256i*)piOrg )) : (_mm256_unpacklo_epi8( _mm256_castsi128_si256( _mm_lddqu_si128( (__m128i*)piOrg ) ), vzero ));
        __m256i r1 = (sizeof( Tcur ) > 1) ? (_mm256_lddqu_si256( (__m256i*)piCur )) : (_mm256_unpacklo_epi8( _mm256_castsi128_si256( _mm_lddqu_si128( (__m128i*)piCur ) ), vzero ));
        m1[k] = _mm256_sub_epi16( r0, r1 );
        piCur += iStrideCur;
        piOrg += iStrideOrg;
      }
    }

    // vertical
    m2[0] = _mm256_add_epi16( m1[0], m1[4] );
    m2[1] = _mm256_add_epi16( m1[1], m1[5] );
    m2[2] = _mm256_add_epi16( m1[2], m1[6] );
    m2[3] = _mm256_add_epi16( m1[3], m1[7] );
    m2[4] = _mm256_sub_epi16( m1[0], m1[4] );
    m2[5] = _mm256_sub_epi16( m1[1], m1[5] );
    m2[6] = _mm256_sub_epi16( m1[2], m1[6] );
    m2[7] = _mm256_sub_epi16( m1[3], m1[7] );

    m1[0] = _mm256_add_epi16( m2[0], m2[2] );
    m1[1] = _mm256_add_epi16( m2[1], m2[3] );
    m1[2] = _mm256_sub_epi16( m2[0], m2[2] );
    m1[3] = _mm256_sub_epi16( m2[1], m2[3] );
    m1[4] = _mm256_add_epi16( m2[4], m2[6] );
    m1[5] = _mm256_add_epi16( m2[5], m2[7] );
    m1[6] = _mm256_sub_epi16( m2[4], m2[6] );
    m1[7] = _mm256_sub_epi16( m2[5], m2[7] );

    m2[0] = _mm256_add_epi16( m1[0], m1[1] );
    m2[1] = _mm256_sub_epi16( m1[0], m1[1] );
    m2[2] = _mm256_add_epi16( m1[2], m1[3] );
    m2[3] = _mm256_sub_epi16( m1[2], m1[3] );
    m2[4] = _mm256_add_epi16( m1[4], m1[5] );
    m2[5] = _mm256_sub_epi16( m1[4], m1[5] );
    m2[6] = _mm256_add_epi16( m1[6], m1[7] );
    m2[7] = _mm256_sub_epi16( m1[6], m1[7] );

    // transpose
    m1[0] = _mm256_unpacklo_epi16( m2[0], m2[1] );
    m1[1] = _mm256_unpacklo_epi16( m2[2], m2[3] );
    m1[2] = _mm256_unpacklo_epi16( m2[4], m2[5] );
    m1[3] = _mm256_unpacklo_epi16( m2[6], m2[7] );
    m1[4] = _mm256_unpackhi_epi16( m2[0], m2[1] );
    m1[5] = _mm256_unpackhi_epi16( m2[2], m2[3] );
    m1[6] = _mm256_unpackhi_epi16( m2[4], m2[5] );
    m1[7] = _mm256_unpackhi_epi16( m2[6], m2[7] );

    m2[0] = _mm256_unpacklo_epi32( m1[0], m1[1] );
    m2[1] = _mm256_unpackhi_epi32( m1[0], m1[1] );
    m2[2] = _mm256_unpacklo_epi32( m1[2], m1[3] );
    m2[3] = _mm256_unpackhi_epi32( m1[2], m1[3] );
    m2[4] = _mm256_unpacklo_epi32( m1[4], m1[5] );
    m2[5] = _mm256_unpackhi_epi32( m1[4], m1[5] );
    m2[6] = _mm256_unpacklo_epi32( m1[6], m1[7] );
    m2[7] = _mm256_unpackhi_epi32( m1[6], m1[7] );

    m1[0] = _mm256_unpacklo_epi64( m2[0], m2[2] );
    m1[1] = _mm256_unpackhi_epi64( m2[0], m2[2] );
    m1[2] = _mm256_unpacklo_epi64( m2[1], m2[3] );
    m1[3] = _mm256_unpackhi_epi64( m2[1], m2[3] );
    m1[4] = _mm256_unpacklo_epi64( m2[4], m2[6] );
    m1[5] = _mm256_unpackhi_epi64( m2[4], m2[6] );
    m1[6] = _mm256_unpacklo_epi64( m2[5], m2[7] );
    m1[7] = _mm256_unpackhi_epi64( m2[5], m2[7] );
    m1[8] = _mm256_permute2x128_si256( m1[0], vzero, 0x31 );
    m1[9] = _mm256_permute2x128_si256( m1[1], vzero, 0x31 );
    m1[10] = _mm256_permute2x128_si256( m1[2], vzero, 0x31 );
    m1[11] = _mm256_permute2x128_si256( m1[3], vzero, 0x31 );
    m1[12] = _mm256_permute2x128_si256( m1[4], vzero, 0x31 );
    m1[13] = _mm256_permute2x128_si256( m1[5], vzero, 0x31 );
    m1[14] = _mm256_permute2x128_si256( m1[6], vzero, 0x31 );
    m1[15] = _mm256_permute2x128_si256( m1[7], vzero, 0x31 );

    // horizontal
    {
      // extend to 32bit
      for( int i = 0; i < 16; i++ )
      {
        m1[i] = _mm256_cvtepi16_epi32( _mm256_castsi256_si128( m1[i] ) );
      }

      m2[ 0] = _mm256_add_epi32( m1[0], m1[ 8] );
      m2[ 1] = _mm256_add_epi32( m1[1], m1[ 9] );
      m2[ 2] = _mm256_add_epi32( m1[2], m1[10] );
      m2[ 3] = _mm256_add_epi32( m1[3], m1[11] );
      m2[ 4] = _mm256_add_epi32( m1[4], m1[12] );
      m2[ 5] = _mm256_add_epi32( m1[5], m1[13] );
      m2[ 6] = _mm256_add_epi32( m1[6], m1[14] );
      m2[ 7] = _mm256_add_epi32( m1[7], m1[15] );
      m2[ 8] = _mm256_sub_epi32( m1[0], m1[ 8] );
      m2[ 9] = _mm256_sub_epi32( m1[1], m1[ 9] );
      m2[10] = _mm256_sub_epi32( m1[2], m1[10] );
      m2[11] = _mm256_sub_epi32( m1[3], m1[11] );
      m2[12] = _mm256_sub_epi32( m1[4], m1[12] );
      m2[13] = _mm256_sub_epi32( m1[5], m1[13] );
      m2[14] = _mm256_sub_epi32( m1[6], m1[14] );
      m2[15] = _mm256_sub_epi32( m1[7], m1[15] );

      m1[ 0] = _mm256_add_epi32( m2[ 0], m2[ 4] );
      m1[ 1] = _mm256_add_epi32( m2[ 1], m2[ 5] );
      m1[ 2] = _mm256_add_epi32( m2[ 2], m2[ 6] );
      m1[ 3] = _mm256_add_epi32( m2[ 3], m2[ 7] );
      m1[ 4] = _mm256_sub_epi32( m2[ 0], m2[ 4] );
      m1[ 5] = _mm256_sub_epi32( m2[ 1], m2[ 5] );
      m1[ 6] = _mm256_sub_epi32( m2[ 2], m2[ 6] );
      m1[ 7] = _mm256_sub_epi32( m2[ 3], m2[ 7] );
      m1[ 8] = _mm256_add_epi32( m2[ 8], m2[12] );
      m1[ 9] = _mm256_add_epi32( m2[ 9], m2[13] );
      m1[10] = _mm256_add_epi32( m2[10], m2[14] );
      m1[11] = _mm256_add_epi32( m2[11], m2[15] );
      m1[12] = _mm256_sub_epi32( m2[ 8], m2[12] );
      m1[13] = _mm256_sub_epi32( m2[ 9], m2[13] );
      m1[14] = _mm256_sub_epi32( m2[10], m2[14] );
      m1[15] = _mm256_sub_epi32( m2[11], m2[15] );

      m2[ 0] = _mm256_add_epi32( m1[ 0], m1[ 2] );
      m2[ 1] = _mm256_add_epi32( m1[ 1], m1[ 3] );
      m2[ 2] = _mm256_sub_epi32( m1[ 0], m1[ 2] );
      m2[ 3] = _mm256_sub_epi32( m1[ 1], m1[ 3] );
      m2[ 4] = _mm256_add_epi32( m1[ 4], m1[ 6] );
      m2[ 5] = _mm256_add_epi32( m1[ 5], m1[ 7] );
      m2[ 6] = _mm256_sub_epi32( m1[ 4], m1[ 6] );
      m2[ 7] = _mm256_sub_epi32( m1[ 5], m1[ 7] );
      m2[ 8] = _mm256_add_epi32( m1[ 8], m1[10] );
      m2[ 9] = _mm256_add_epi32( m1[ 9], m1[11] );
      m2[10] = _mm256_sub_epi32( m1[ 8], m1[10] );
      m2[11] = _mm256_sub_epi32( m1[ 9], m1[11] );
      m2[12] = _mm256_add_epi32( m1[12], m1[14] );
      m2[13] = _mm256_add_epi32( m1[13], m1[15] );
      m2[14] = _mm256_sub_epi32( m1[12], m1[14] );
      m2[15] = _mm256_sub_epi32( m1[13], m1[15] );

      m1[ 0] = _mm256_abs_epi32( _mm256_add_epi32( m2[ 0], m2[ 1] ) );
      m1[ 1] = _mm256_abs_epi32( _mm256_sub_epi32( m2[ 0], m2[ 1] ) );
      m1[ 2] = _mm256_abs_epi32( _mm256_add_epi32( m2[ 2], m2[ 3] ) );
      m1[ 3] = _mm256_abs_epi32( _mm256_sub_epi32( m2[ 2], m2[ 3] ) );
      m1[ 4] = _mm256_abs_epi32( _mm256_add_epi32( m2[ 4], m2[ 5] ) );
      m1[ 5] = _mm256_abs_epi32( _mm256_sub_epi32( m2[ 4], m2[ 5] ) );
      m1[ 6] = _mm256_abs_epi32( _mm256_add_epi32( m2[ 6], m2[ 7] ) );
      m1[ 7] = _mm256_abs_epi32( _mm256_sub_epi32( m2[ 6], m2[ 7] ) );
      m1[ 8] = _mm256_abs_epi32( _mm256_add_epi32( m2[ 8], m2[ 9] ) );
      m1[ 9] = _mm256_abs_epi32( _mm256_sub_epi32( m2[ 8], m2[ 9] ) );
      m1[10] = _mm256_abs_epi32( _mm256_add_epi32( m2[10], m2[11] ) );
      m1[11] = _mm256_abs_epi32( _mm256_sub_epi32( m2[10], m2[11] ) );
      m1[12] = _mm256_abs_epi32( _mm256_add_epi32( m2[12], m2[13] ) );
      m1[13] = _mm256_abs_epi32( _mm256_sub_epi32( m2[12], m2[13] ) );
      m1[14] = _mm256_abs_epi32( _mm256_add_epi32( m2[14], m2[15] ) );
      m1[15] = _mm256_abs_epi32( _mm256_sub_epi32( m2[14], m2[15] ) );
    }

    // sum up
    m1[ 0] = _mm256_add_epi32( m1[ 0], m1[ 1] );
    m1[ 2] = _mm256_add_epi32( m1[ 2], m1[ 3] );
    m1[ 4] = _mm256_add_epi32( m1[ 4], m1[ 5] );
    m1[ 6] = _mm256_add_epi32( m1[ 6], m1[ 7] );
    m1[ 8] = _mm256_add_epi32( m1[ 8], m1[ 9] );
    m1[10] = _mm256_add_epi32( m1[10], m1[11] );
    m1[12] = _mm256_add_epi32( m1[12], m1[13] );
    m1[14] = _mm256_add_epi32( m1[14], m1[15] );

    m1[ 0] = _mm256_add_epi32( m1[ 0], m1[ 2] );
    m1[ 4] = _mm256_add_epi32( m1[ 4], m1[ 6] );
    m1[ 8] = _mm256_add_epi32( m1[ 8], m1[10] );
    m1[12] = _mm256_add_epi32( m1[12], m1[14] );

    m1[0] = _mm256_add_epi32( m1[0], m1[ 4] );
    m1[8] = _mm256_add_epi32( m1[8], m1[12] );

    __m256i iSum = _mm256_add_epi32( m1[0], m1[8] );
    iSum = _mm256_hadd_epi32( iSum, iSum );
    iSum = _mm256_hadd_epi32( iSum, iSum );
    iSum = _mm256_add_epi32( iSum, _mm256_permute2x128_si256( iSum, iSum, 0x11 ) );

    sad = _mm_cvtsi128_si32( _mm256_castsi256_si128( iSum ) );

    sad = (uint32_t)(sad / sqrt(16.0 * 8) * 2);
  }

#endif //USE_AVX2

  return (sad);
}


template< typename Torg, typename Tcur/*, bool bHorDownsampling*/ >
static uint32_t xCalcHAD8x16_AVX2( const Torg *piOrg, const Tcur *piCur, const int iStrideOrg, const int iStrideCur, const int iBitDepth )
{
  uint32_t sad = 0;

#ifdef USE_AVX2
  // const int iLoops = ( bHorDownsampling && HAD_DOWNSAMPLING_HOR ) ? ( 1 ) : ( 2 );
  //const int iLoops = 1;
  __m256i m1[16], m2[16];
  __m256i vzero = _mm256_setzero_si256();

  //for( int l = 0; l < iLoops; l++ )
  {
    {
      for( int k = 0; k < 16; k++ )
      {
        __m256i r0 = (sizeof( Torg ) > 1) ? ( _mm256_castsi128_si256( _mm_lddqu_si128( (__m128i*)piOrg ) ) ): (_mm256_unpacklo_epi8( _mm256_castsi128_si256( _mm_loadl_epi64( (__m128i*)piOrg ) ), vzero ));
        __m256i r1 = (sizeof( Tcur ) > 1) ? ( _mm256_castsi128_si256( _mm_lddqu_si128( (__m128i*)piCur ) ) ): (_mm256_unpacklo_epi8( _mm256_castsi128_si256( _mm_loadl_epi64( (__m128i*)piCur ) ), vzero ));
        m1[k] = _mm256_sub_epi16( r0, r1 );
        piCur += iStrideCur;
        piOrg += iStrideOrg;
      }
    }

    // vertical

    m2[ 0] = _mm256_add_epi16( m1[0], m1[ 8] );
    m2[ 1] = _mm256_add_epi16( m1[1], m1[ 9] );
    m2[ 2] = _mm256_add_epi16( m1[2], m1[10] );
    m2[ 3] = _mm256_add_epi16( m1[3], m1[11] );
    m2[ 4] = _mm256_add_epi16( m1[4], m1[12] );
    m2[ 5] = _mm256_add_epi16( m1[5], m1[13] );
    m2[ 6] = _mm256_add_epi16( m1[6], m1[14] );
    m2[ 7] = _mm256_add_epi16( m1[7], m1[15] );
    m2[ 8] = _mm256_sub_epi16( m1[0], m1[ 8] );
    m2[ 9] = _mm256_sub_epi16( m1[1], m1[ 9] );
    m2[10] = _mm256_sub_epi16( m1[2], m1[10] );
    m2[11] = _mm256_sub_epi16( m1[3], m1[11] );
    m2[12] = _mm256_sub_epi16( m1[4], m1[12] );
    m2[13] = _mm256_sub_epi16( m1[5], m1[13] );
    m2[14] = _mm256_sub_epi16( m1[6], m1[14] );
    m2[15] = _mm256_sub_epi16( m1[7], m1[15] );

    m1[ 0] = _mm256_add_epi16( m2[ 0], m2[ 4] );
    m1[ 1] = _mm256_add_epi16( m2[ 1], m2[ 5] );
    m1[ 2] = _mm256_add_epi16( m2[ 2], m2[ 6] );
    m1[ 3] = _mm256_add_epi16( m2[ 3], m2[ 7] );
    m1[ 4] = _mm256_sub_epi16( m2[ 0], m2[ 4] );
    m1[ 5] = _mm256_sub_epi16( m2[ 1], m2[ 5] );
    m1[ 6] = _mm256_sub_epi16( m2[ 2], m2[ 6] );
    m1[ 7] = _mm256_sub_epi16( m2[ 3], m2[ 7] );
    m1[ 8] = _mm256_add_epi16( m2[ 8], m2[12] );
    m1[ 9] = _mm256_add_epi16( m2[ 9], m2[13] );
    m1[10] = _mm256_add_epi16( m2[10], m2[14] );
    m1[11] = _mm256_add_epi16( m2[11], m2[15] );
    m1[12] = _mm256_sub_epi16( m2[ 8], m2[12] );
    m1[13] = _mm256_sub_epi16( m2[ 9], m2[13] );
    m1[14] = _mm256_sub_epi16( m2[10], m2[14] );
    m1[15] = _mm256_sub_epi16( m2[11], m2[15] );

    m2[ 0] = _mm256_add_epi16( m1[ 0], m1[ 2] );
    m2[ 1] = _mm256_add_epi16( m1[ 1], m1[ 3] );
    m2[ 2] = _mm256_sub_epi16( m1[ 0], m1[ 2] );
    m2[ 3] = _mm256_sub_epi16( m1[ 1], m1[ 3] );
    m2[ 4] = _mm256_add_epi16( m1[ 4], m1[ 6] );
    m2[ 5] = _mm256_add_epi16( m1[ 5], m1[ 7] );
    m2[ 6] = _mm256_sub_epi16( m1[ 4], m1[ 6] );
    m2[ 7] = _mm256_sub_epi16( m1[ 5], m1[ 7] );
    m2[ 8] = _mm256_add_epi16( m1[ 8], m1[10] );
    m2[ 9] = _mm256_add_epi16( m1[ 9], m1[11] );
    m2[10] = _mm256_sub_epi16( m1[ 8], m1[10] );
    m2[11] = _mm256_sub_epi16( m1[ 9], m1[11] );
    m2[12] = _mm256_add_epi16( m1[12], m1[14] );
    m2[13] = _mm256_add_epi16( m1[13], m1[15] );
    m2[14] = _mm256_sub_epi16( m1[12], m1[14] );
    m2[15] = _mm256_sub_epi16( m1[13], m1[15] );

    m1[ 0] = _mm256_add_epi16( m2[ 0], m2[ 1] );
    m1[ 1] = _mm256_sub_epi16( m2[ 0], m2[ 1] );
    m1[ 2] = _mm256_add_epi16( m2[ 2], m2[ 3] );
    m1[ 3] = _mm256_sub_epi16( m2[ 2], m2[ 3] );
    m1[ 4] = _mm256_add_epi16( m2[ 4], m2[ 5] );
    m1[ 5] = _mm256_sub_epi16( m2[ 4], m2[ 5] );
    m1[ 6] = _mm256_add_epi16( m2[ 6], m2[ 7] );
    m1[ 7] = _mm256_sub_epi16( m2[ 6], m2[ 7] );
    m1[ 8] = _mm256_add_epi16( m2[ 8], m2[ 9] );
    m1[ 9] = _mm256_sub_epi16( m2[ 8], m2[ 9] );
    m1[10] = _mm256_add_epi16( m2[10], m2[11] );
    m1[11] = _mm256_sub_epi16( m2[10], m2[11] );
    m1[12] = _mm256_add_epi16( m2[12], m2[13] );
    m1[13] = _mm256_sub_epi16( m2[12], m2[13] );
    m1[14] = _mm256_add_epi16( m2[14], m2[15] );
    m1[15] = _mm256_sub_epi16( m2[14], m2[15] );


    // transpose
    // 1. 8x8
    m2[0] = _mm256_unpacklo_epi16( m1[0], m1[1] );
    m2[1] = _mm256_unpacklo_epi16( m1[2], m1[3] );
    m2[2] = _mm256_unpacklo_epi16( m1[4], m1[5] );
    m2[3] = _mm256_unpacklo_epi16( m1[6], m1[7] );
    m2[4] = _mm256_unpackhi_epi16( m1[0], m1[1] );
    m2[5] = _mm256_unpackhi_epi16( m1[2], m1[3] );
    m2[6] = _mm256_unpackhi_epi16( m1[4], m1[5] );
    m2[7] = _mm256_unpackhi_epi16( m1[6], m1[7] );

    m1[0] = _mm256_unpacklo_epi32( m2[0], m2[1] );
    m1[1] = _mm256_unpackhi_epi32( m2[0], m2[1] );
    m1[2] = _mm256_unpacklo_epi32( m2[2], m2[3] );
    m1[3] = _mm256_unpackhi_epi32( m2[2], m2[3] );
    m1[4] = _mm256_unpacklo_epi32( m2[4], m2[5] );
    m1[5] = _mm256_unpackhi_epi32( m2[4], m2[5] );
    m1[6] = _mm256_unpacklo_epi32( m2[6], m2[7] );
    m1[7] = _mm256_unpackhi_epi32( m2[6], m2[7] );

    m2[0] = _mm256_unpacklo_epi64( m1[0], m1[2] );
    m2[1] = _mm256_unpackhi_epi64( m1[0], m1[2] );
    m2[2] = _mm256_unpacklo_epi64( m1[1], m1[3] );
    m2[3] = _mm256_unpackhi_epi64( m1[1], m1[3] );
    m2[4] = _mm256_unpacklo_epi64( m1[4], m1[6] );
    m2[5] = _mm256_unpackhi_epi64( m1[4], m1[6] );
    m2[6] = _mm256_unpacklo_epi64( m1[5], m1[7] );
    m2[7] = _mm256_unpackhi_epi64( m1[5], m1[7] );

    // 2. 8x8
    m2[0+8] = _mm256_unpacklo_epi16( m1[0+8], m1[1+8] );
    m2[1+8] = _mm256_unpacklo_epi16( m1[2+8], m1[3+8] );
    m2[2+8] = _mm256_unpacklo_epi16( m1[4+8], m1[5+8] );
    m2[3+8] = _mm256_unpacklo_epi16( m1[6+8], m1[7+8] );
    m2[4+8] = _mm256_unpackhi_epi16( m1[0+8], m1[1+8] );
    m2[5+8] = _mm256_unpackhi_epi16( m1[2+8], m1[3+8] );
    m2[6+8] = _mm256_unpackhi_epi16( m1[4+8], m1[5+8] );
    m2[7+8] = _mm256_unpackhi_epi16( m1[6+8], m1[7+8] );

    m1[0+8] = _mm256_unpacklo_epi32( m2[0+8], m2[1+8] );
    m1[1+8] = _mm256_unpackhi_epi32( m2[0+8], m2[1+8] );
    m1[2+8] = _mm256_unpacklo_epi32( m2[2+8], m2[3+8] );
    m1[3+8] = _mm256_unpackhi_epi32( m2[2+8], m2[3+8] );
    m1[4+8] = _mm256_unpacklo_epi32( m2[4+8], m2[5+8] );
    m1[5+8] = _mm256_unpackhi_epi32( m2[4+8], m2[5+8] );
    m1[6+8] = _mm256_unpacklo_epi32( m2[6+8], m2[7+8] );
    m1[7+8] = _mm256_unpackhi_epi32( m2[6+8], m2[7+8] );

    m2[0+8] = _mm256_unpacklo_epi64( m1[0+8], m1[2+8] );
    m2[1+8] = _mm256_unpackhi_epi64( m1[0+8], m1[2+8] );
    m2[2+8] = _mm256_unpacklo_epi64( m1[1+8], m1[3+8] );
    m2[3+8] = _mm256_unpackhi_epi64( m1[1+8], m1[3+8] );
    m2[4+8] = _mm256_unpacklo_epi64( m1[4+8], m1[6+8] );
    m2[5+8] = _mm256_unpackhi_epi64( m1[4+8], m1[6+8] );
    m2[6+8] = _mm256_unpacklo_epi64( m1[5+8], m1[7+8] );
    m2[7+8] = _mm256_unpackhi_epi64( m1[5+8], m1[7+8] );


    // horizontal
    if( iBitDepth >= 10 )
    {
      // extend to 32bit
      //for( int j = 0; j < 16; j+=8 )
      {
        for( int i = 0; i < 8; i++ )
        {
          m2[i] = _mm256_cvtepi16_epi32( _mm256_castsi256_si128( m2[i] ) );
        }

        m1[0] = _mm256_add_epi32( m2[0], m2[4] );
        m1[1] = _mm256_add_epi32( m2[1], m2[5] );
        m1[2] = _mm256_add_epi32( m2[2], m2[6] );
        m1[3] = _mm256_add_epi32( m2[3], m2[7] );
        m1[4] = _mm256_sub_epi32( m2[0], m2[4] );
        m1[5] = _mm256_sub_epi32( m2[1], m2[5] );
        m1[6] = _mm256_sub_epi32( m2[2], m2[6] );
        m1[7] = _mm256_sub_epi32( m2[3], m2[7] );

        m2[0] = _mm256_add_epi32( m1[0], m1[2] );
        m2[1] = _mm256_add_epi32( m1[1], m1[3] );
        m2[2] = _mm256_sub_epi32( m1[0], m1[2] );
        m2[3] = _mm256_sub_epi32( m1[1], m1[3] );
        m2[4] = _mm256_add_epi32( m1[4], m1[6] );
        m2[5] = _mm256_add_epi32( m1[5], m1[7] );
        m2[6] = _mm256_sub_epi32( m1[4], m1[6] );
        m2[7] = _mm256_sub_epi32( m1[5], m1[7] );

        m1[0] = _mm256_abs_epi32( _mm256_add_epi32( m2[0], m2[1] ) );
        m1[1] = _mm256_abs_epi32( _mm256_sub_epi32( m2[0], m2[1] ) );
        m1[2] = _mm256_abs_epi32( _mm256_add_epi32( m2[2], m2[3] ) );
        m1[3] = _mm256_abs_epi32( _mm256_sub_epi32( m2[2], m2[3] ) );
        m1[4] = _mm256_abs_epi32( _mm256_add_epi32( m2[4], m2[5] ) );
        m1[5] = _mm256_abs_epi32( _mm256_sub_epi32( m2[4], m2[5] ) );
        m1[6] = _mm256_abs_epi32( _mm256_add_epi32( m2[6], m2[7] ) );
        m1[7] = _mm256_abs_epi32( _mm256_sub_epi32( m2[6], m2[7] ) );

        for( int i = 0; i < 8; i++ )
        {
          m2[i+8] = _mm256_cvtepi16_epi32( _mm256_castsi256_si128( m2[i+8] ) );
        }

        m1[0 + 8] = _mm256_add_epi32( m2[0 + 8], m2[4 + 8] );
        m1[1 + 8] = _mm256_add_epi32( m2[1 + 8], m2[5 + 8] );
        m1[2 + 8] = _mm256_add_epi32( m2[2 + 8], m2[6 + 8] );
        m1[3 + 8] = _mm256_add_epi32( m2[3 + 8], m2[7 + 8] );
        m1[4 + 8] = _mm256_sub_epi32( m2[0 + 8], m2[4 + 8] );
        m1[5 + 8] = _mm256_sub_epi32( m2[1 + 8], m2[5 + 8] );
        m1[6 + 8] = _mm256_sub_epi32( m2[2 + 8], m2[6 + 8] );
        m1[7 + 8] = _mm256_sub_epi32( m2[3 + 8], m2[7 + 8] );

        m2[0 + 8] = _mm256_add_epi32( m1[0 + 8], m1[2 + 8] );
        m2[1 + 8] = _mm256_add_epi32( m1[1 + 8], m1[3 + 8] );
        m2[2 + 8] = _mm256_sub_epi32( m1[0 + 8], m1[2 + 8] );
        m2[3 + 8] = _mm256_sub_epi32( m1[1 + 8], m1[3 + 8] );
        m2[4 + 8] = _mm256_add_epi32( m1[4 + 8], m1[6 + 8] );
        m2[5 + 8] = _mm256_add_epi32( m1[5 + 8], m1[7 + 8] );
        m2[6 + 8] = _mm256_sub_epi32( m1[4 + 8], m1[6 + 8] );
        m2[7 + 8] = _mm256_sub_epi32( m1[5 + 8], m1[7 + 8] );

        m1[0 + 8] = _mm256_abs_epi32( _mm256_add_epi32( m2[0 + 8], m2[1 + 8] ) );
        m1[1 + 8] = _mm256_abs_epi32( _mm256_sub_epi32( m2[0 + 8], m2[1 + 8] ) );
        m1[2 + 8] = _mm256_abs_epi32( _mm256_add_epi32( m2[2 + 8], m2[3 + 8] ) );
        m1[3 + 8] = _mm256_abs_epi32( _mm256_sub_epi32( m2[2 + 8], m2[3 + 8] ) );
        m1[4 + 8] = _mm256_abs_epi32( _mm256_add_epi32( m2[4 + 8], m2[5 + 8] ) );
        m1[5 + 8] = _mm256_abs_epi32( _mm256_sub_epi32( m2[4 + 8], m2[5 + 8] ) );
        m1[6 + 8] = _mm256_abs_epi32( _mm256_add_epi32( m2[6 + 8], m2[7 + 8] ) );
        m1[7 + 8] = _mm256_abs_epi32( _mm256_sub_epi32( m2[6 + 8], m2[7 + 8] ) );
      }
      // sum up
      m1[0] = _mm256_add_epi32( m1[0], m1[1] );
      m1[1] = _mm256_add_epi32( m1[2], m1[3] );
      m1[2] = _mm256_add_epi32( m1[4], m1[5] );
      m1[3] = _mm256_add_epi32( m1[6], m1[7] );
      m1[4] = _mm256_add_epi32( m1[8], m1[9] );
      m1[5] = _mm256_add_epi32( m1[10], m1[11] );
      m1[6] = _mm256_add_epi32( m1[12], m1[13] );
      m1[7] = _mm256_add_epi32( m1[14], m1[15] );
    }
    else
    {
      // 16x8
      m2[0] = _mm256_permute2x128_si256( m2[0], m2[0 + 8], 0x20 );
      m2[1] = _mm256_permute2x128_si256( m2[1], m2[1 + 8], 0x20 );
      m2[2] = _mm256_permute2x128_si256( m2[2], m2[2 + 8], 0x20 );
      m2[3] = _mm256_permute2x128_si256( m2[3], m2[3 + 8], 0x20 );
      m2[4] = _mm256_permute2x128_si256( m2[4], m2[4 + 8], 0x20 );
      m2[5] = _mm256_permute2x128_si256( m2[5], m2[5 + 8], 0x20 );
      m2[6] = _mm256_permute2x128_si256( m2[6], m2[6 + 8], 0x20 );
      m2[7] = _mm256_permute2x128_si256( m2[7], m2[7 + 8], 0x20 );

      m1[0] = _mm256_add_epi16( m2[0], m2[4] );
      m1[1] = _mm256_add_epi16( m2[1], m2[5] );
      m1[2] = _mm256_add_epi16( m2[2], m2[6] );
      m1[3] = _mm256_add_epi16( m2[3], m2[7] );
      m1[4] = _mm256_sub_epi16( m2[0], m2[4] );
      m1[5] = _mm256_sub_epi16( m2[1], m2[5] );
      m1[6] = _mm256_sub_epi16( m2[2], m2[6] );
      m1[7] = _mm256_sub_epi16( m2[3], m2[7] );

      m2[0] = _mm256_add_epi16( m1[0], m1[2] );
      m2[1] = _mm256_add_epi16( m1[1], m1[3] );
      m2[2] = _mm256_sub_epi16( m1[0], m1[2] );
      m2[3] = _mm256_sub_epi16( m1[1], m1[3] );
      m2[4] = _mm256_add_epi16( m1[4], m1[6] );
      m2[5] = _mm256_add_epi16( m1[5], m1[7] );
      m2[6] = _mm256_sub_epi16( m1[4], m1[6] );
      m2[7] = _mm256_sub_epi16( m1[5], m1[7] );

      m1[0] = _mm256_abs_epi16( _mm256_add_epi16( m2[0], m2[1] ) );
      m1[1] = _mm256_abs_epi16( _mm256_sub_epi16( m2[0], m2[1] ) );
      m1[2] = _mm256_abs_epi16( _mm256_add_epi16( m2[2], m2[3] ) );
      m1[3] = _mm256_abs_epi16( _mm256_sub_epi16( m2[2], m2[3] ) );
      m1[4] = _mm256_abs_epi16( _mm256_add_epi16( m2[4], m2[5] ) );
      m1[5] = _mm256_abs_epi16( _mm256_sub_epi16( m2[4], m2[5] ) );
      m1[6] = _mm256_abs_epi16( _mm256_add_epi16( m2[6], m2[7] ) );
      m1[7] = _mm256_abs_epi16( _mm256_sub_epi16( m2[6], m2[7] ) );

      __m256i ma1, ma2;

      for( int i = 0; i < 8; i++ )
      {
        ma1 = _mm256_unpacklo_epi16( m1[i], vzero );
        ma2 = _mm256_unpackhi_epi16( m1[i], vzero );
        m1[i] = _mm256_add_epi32( ma1, ma2 );
      }
    }

    // sum up
    m1[ 0] = _mm256_add_epi32( m1[ 0], m1[ 1] );
    m1[ 1] = _mm256_add_epi32( m1[ 2], m1[ 3] );
    m1[ 2] = _mm256_add_epi32( m1[ 4], m1[ 5] );
    m1[ 3] = _mm256_add_epi32( m1[ 6], m1[ 7] );

    m1[ 0] = _mm256_add_epi32( m1[ 0], m1[ 1] );
    m1[ 1] = _mm256_add_epi32( m1[ 2], m1[ 3] );

    __m256i iSum = _mm256_add_epi32( m1[0], m1[1] );

    iSum = _mm256_hadd_epi32( iSum, iSum );
    iSum = _mm256_hadd_epi32( iSum, iSum );
    iSum = _mm256_add_epi32( iSum, _mm256_permute2x128_si256( iSum, iSum, 0x11 ) );

    int sad2 = _mm_cvtsi128_si32( _mm256_castsi256_si128( iSum ) );

    sad = (uint32_t)(sad2 / sqrt(16.0 * 8) * 2);
  }

#endif //USE_AVX2

  return (sad);
}


template< typename Torg, typename Tcur, X86_VEXT vext >
Distortion RdCost::xGetHADs_SIMD( const DistParam &rcDtParam )
{
  if( rcDtParam.bitDepth > 10 || rcDtParam.applyWeight )
  {
    return RdCost::xGetHADs( rcDtParam );
  }

  const Torg*  piOrg = (const Torg*)rcDtParam.org.buf;
  const Tcur*  piCur = (const Tcur*)rcDtParam.cur.buf;
  const int iRows = rcDtParam.org.height;
  const int iCols = rcDtParam.org.width;
  const int iStrideCur = rcDtParam.cur.stride;
  const int iStrideOrg = rcDtParam.org.stride;
  const int iBitDepth  = rcDtParam.bitDepth;

  int  x, y;
  Distortion uiSum = 0;

  if( iCols > iRows && ( iCols & 15 ) == 0 && ( iRows & 7 ) == 0 )
  {
    for( y = 0; y < iRows; y += 8 )
    {
      for( x = 0; x < iCols; x += 16 )
      {
        if( vext >= AVX2 )
          uiSum += xCalcHAD16x8_AVX2<Torg, Tcur>( &piOrg[x], &piCur[x], iStrideOrg, iStrideCur, iBitDepth );
        else
          uiSum += xCalcHAD16x8_SSE<Torg, Tcur>( &piOrg[x], &piCur[x], iStrideOrg, iStrideCur, iBitDepth );
      }
      piOrg += iStrideOrg * 8;
      piCur += iStrideCur * 8;
    }
  }
  else if( iCols < iRows && ( iRows & 15 ) == 0 && ( iCols & 7 ) == 0 )
  {
    for( y = 0; y < iRows; y += 16 )
    {
      for( x = 0; x < iCols; x += 8 )
      {
        if( vext >= AVX2 )
          uiSum += xCalcHAD8x16_AVX2<Torg, Tcur>( &piOrg[x], &piCur[x], iStrideOrg, iStrideCur, iBitDepth );
        else
          uiSum += xCalcHAD8x16_SSE<Torg, Tcur>( &piOrg[x], &piCur[x], iStrideOrg, iStrideCur, iBitDepth );
      }
      piOrg += iStrideOrg * 16;
      piCur += iStrideCur * 16;
    }
  }
  else if( iCols > iRows && ( iCols & 7 ) == 0 && ( iRows & 3 ) == 0 )
  {
    for( y = 0; y < iRows; y += 4 )
    {
      for( x = 0; x < iCols; x += 8 )
      {
        uiSum += xCalcHAD8x4_SSE<Torg, Tcur>( &piOrg[x], &piCur[x], iStrideOrg, iStrideCur, iBitDepth );
      }
      piOrg += iStrideOrg * 4;
      piCur += iStrideCur * 4;
    }
  }
  else if( iCols < iRows && ( iRows & 7 ) == 0 && ( iCols & 3 ) == 0 )
  {
    for( y = 0; y < iRows; y += 8 )
    {
      for( x = 0; x < iCols; x += 4 )
      {
        uiSum += xCalcHAD4x8_SSE( &piOrg[x], &piCur[x], iStrideOrg, iStrideCur, iBitDepth );
      }
      piOrg += iStrideOrg * 8;
      piCur += iStrideCur * 8;
    }
  }
  else if( vext >= AVX2 && ( ( ( iRows | iCols ) & 15 ) == 0 ) && ( iRows == iCols ) )
  {
    int  iOffsetOrg = iStrideOrg << 4;
    int  iOffsetCur = iStrideCur << 4;
    for( y = 0; y < iRows; y += 16 )
    {
      for( x = 0; x < iCols; x += 16 )
      {
        uiSum += xCalcHAD16x16_AVX2<Torg, Tcur>( &piOrg[x], &piCur[x], iStrideOrg, iStrideCur, iBitDepth );
      }
      piOrg += iOffsetOrg;
      piCur += iOffsetCur;
    }
  }
  else if( ( ( ( iRows | iCols ) & 7 ) == 0 ) && ( iRows == iCols ) )
  {
    int  iOffsetOrg = iStrideOrg << 3;
    int  iOffsetCur = iStrideCur << 3;
    for( y = 0; y<iRows; y += 8 )
    {
      for( x = 0; x < iCols; x += 8 )
      {
        uiSum += xCalcHAD8x8_SSE<Torg, Tcur>( &piOrg[x], &piCur[x], iStrideOrg, iStrideCur, iBitDepth );
      }
      piOrg += iOffsetOrg;
      piCur += iOffsetCur;
    }
  }
  else if( ( iRows % 4 == 0 ) && ( iCols % 4 == 0 ) )
  {
    int  iOffsetOrg = iStrideOrg << 2;
    int  iOffsetCur = iStrideCur << 2;

    for( y = 0; y < iRows; y += 4 )
    {
      for( x = 0; x < iCols; x += 4 )
      {
        uiSum += xCalcHAD4x4_SSE( &piOrg[x], &piCur[x], iStrideOrg, iStrideCur );
      }
      piOrg += iOffsetOrg;
      piCur += iOffsetCur;
    }
  }
  else if( ( iRows % 2 == 0 ) && ( iCols % 2 == 0 ) )
  {
    int  iOffsetOrg = iStrideOrg << 1;
    int  iOffsetCur = iStrideCur << 1;
    for( y = 0; y < iRows; y += 2 )
    {
      for( x = 0; x < iCols; x += 2 )
      {
        uiSum += xCalcHADs2x2( (Torg*)&piOrg[x], (Tcur*)&piCur[x*rcDtParam.step], iStrideOrg, iStrideCur, rcDtParam.step );
      }
      piOrg += iOffsetOrg;
      piCur += iOffsetCur;
    }
  }
  else
  {
    THROW( "Unsupported size" );
  }

  return uiSum >> DISTORTION_PRECISION_ADJUSTMENT(rcDtParam.bitDepth);
}

template <X86_VEXT vext>
void RdCost::_initRdCostX86()
{
  /* SIMD SSE implementation shifts the final sum instead of every addend
   * resulting in slightly different result compared to the scalar impl. */
  //m_afpDistortFunc[DF_SSE    ] = xGetSSE_SIMD<Pel, Pel, vext>;
  //m_afpDistortFunc[DF_SSE2   ] = xGetSSE_SIMD<Pel, Pel, vext>;
  //m_afpDistortFunc[DF_SSE4   ] = xGetSSE_NxN_SIMD<Pel, Pel, 4,  vext>;
  //m_afpDistortFunc[DF_SSE8   ] = xGetSSE_NxN_SIMD<Pel, Pel, 8,  vext>;
  //m_afpDistortFunc[DF_SSE16  ] = xGetSSE_NxN_SIMD<Pel, Pel, 16, vext>;
  //m_afpDistortFunc[DF_SSE32  ] = xGetSSE_NxN_SIMD<Pel, Pel, 32, vext>;
  //m_afpDistortFunc[DF_SSE64  ] = xGetSSE_NxN_SIMD<Pel, Pel, 64, vext>;
  //m_afpDistortFunc[DF_SSE16N ] = xGetSSE_SIMD<Pel, Pel, vext>;

  m_afpDistortFunc[DF_SAD    ] = xGetSAD_SIMD<vext>;
  m_afpDistortFunc[DF_SAD2   ] = xGetSAD_SIMD<vext>;
  m_afpDistortFunc[DF_SAD4   ] = xGetSAD_NxN_SIMD<4,  vext>;
  m_afpDistortFunc[DF_SAD8   ] = xGetSAD_NxN_SIMD<8,  vext>;
  m_afpDistortFunc[DF_SAD16  ] = xGetSAD_NxN_SIMD<16, vext>;
  m_afpDistortFunc[DF_SAD32  ] = xGetSAD_NxN_SIMD<32, vext>;
  m_afpDistortFunc[DF_SAD64  ] = xGetSAD_NxN_SIMD<64, vext>;
  m_afpDistortFunc[DF_SAD16N]  = xGetSAD_SIMD<vext>;

  m_afpDistortFunc[DF_SAD12  ] = RdCost::xGetSAD_SIMD<vext>;
  m_afpDistortFunc[DF_SAD24  ] = RdCost::xGetSAD_SIMD<vext>;
  m_afpDistortFunc[DF_SAD48  ] = RdCost::xGetSAD_SIMD<vext>;

  m_afpDistortFunc[DF_HAD]     = RdCost::xGetHADs_SIMD<Pel, Pel, vext>;
  m_afpDistortFunc[DF_HAD2]    = RdCost::xGetHADs_SIMD<Pel, Pel, vext>;
  m_afpDistortFunc[DF_HAD4]    = RdCost::xGetHADs_SIMD<Pel, Pel, vext>;
  m_afpDistortFunc[DF_HAD8]    = RdCost::xGetHADs_SIMD<Pel, Pel, vext>;
  m_afpDistortFunc[DF_HAD16]   = RdCost::xGetHADs_SIMD<Pel, Pel, vext>;
  m_afpDistortFunc[DF_HAD32]   = RdCost::xGetHADs_SIMD<Pel, Pel, vext>;
  m_afpDistortFunc[DF_HAD64]   = RdCost::xGetHADs_SIMD<Pel, Pel, vext>;
  m_afpDistortFunc[DF_HAD16N]  = RdCost::xGetHADs_SIMD<Pel, Pel, vext>;

  m_afpDistortFunc[DF_SAD_INTERMEDIATE_BITDEPTH] = RdCost::xGetSAD_IBD_SIMD<vext>;
}

template void RdCost::_initRdCostX86<SIMDX86>();

#endif //#if TARGET_SIMD_X86
//! \}
