/*
 * This file is part of libhildondesktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Authored By Gordon Williams <gordon.williams@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef PVRTEXTURE_H_
#define PVRTEXTURE_H_
/* handles compression + decompression of PVRTC4 texture files */

#include <glib.h>

/* Header for PVR texture files */
typedef struct PVR_TEXTURE_HEADER_TAG {
    unsigned int dwHeaderSize;     /* size of the structure */
    unsigned int dwHeight;         /* height of surface to be created */
    unsigned int dwWidth;          /* width of input surface */
    unsigned int dwMipMapCount;    /* number of MIP-map levels requested */
    unsigned int dwpfFlags;        /* pixel format flags */
    unsigned int dwDataSize;       /* Size of the compress data */
    unsigned int dwBitCount;       /* number of bits per pixel */
    unsigned int dwRBitMask;       /* mask for red bit */
    unsigned int dwGBitMask;       /* mask for green bits */
    unsigned int dwBBitMask;       /* mask for blue bits */
    unsigned int dwAlphaBitMask;   /* mask for alpha channel */
    unsigned int dwPVR;            /* should be 'P' 'V' 'R' '!' */
    unsigned int dwNumSurfs;       /* number of slices for volume textures or skyboxes */
} PVR_TEXTURE_HEADER;

/* Contents of PVR_TEXTURE_HEADER.dwpfFlags */
#define MGLPT_PVRTC2 (0x18)
#define MGLPT_PVRTC4 (0x19)
#define ETC_RGB_4BPP (0x36)
#define PVR_FLAG_TWIDDLED (0x00000200)
#define PVR_FLAG_ALPHA    (0x00008000)

gboolean pvr_texture_save_pvrtc4(
                        const gchar *filename,
                        const guchar *data,
                        guint data_size,
                        gint width, gint height);

guchar *pvr_texture_compress_pvrtc4(
                const guchar *uncompressed_data,
                gint width,
                gint height,
                guint *compressed_size);

guchar *pvr_texture_decompress_pvrtc4(
                const guchar *compressed_data,
                gint width,
                gint height);

gboolean pvr_texture_save_pvrtc4_atomically (const gchar   *filename,
                                             const guchar  *data,
                                             guint          data_size,
                                             gint           width,
                                             gint           height,
                                             GError       **error);
#endif /*PVRTEXTURE_H_*/
