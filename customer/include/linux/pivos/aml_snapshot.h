/*
 * include/linux/pivos/aml_snapshot.h
 *
 * video scaler for thumbnails/snapshots
 *
 * Copyright (C) 2013 PivosGroup
 *
 * Written by Scott Davilla <scott.davilla@pivosgroup.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the BSD Licence or GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define AMSNAPSHOT_IOC_MAGIC 'T'
#define AMSNAPSHOT_IOC_GET_FRAME   _IOW(AMSNAPSHOT_IOC_MAGIC, 0x04, unsigned long)

/*  Four-character-code (FOURCC) */
#define AMSNAPSHOT_FOURCC(a, b, c, d)\
	((__u32)(a) | ((__u32)(b) << 8) | ((__u32)(c) << 16) | ((__u32)(d) << 24))

#define AMSNAPSHOT_FMT_S24_BGR   AMSNAPSHOT_FOURCC('B', 'G', 'R', '3') /* 24  BGR-8-8-8     */
#define AMSNAPSHOT_FMT_S24_RGB   AMSNAPSHOT_FOURCC('R', 'G', 'B', '3') /* 24  RGB-8-8-8     */
#define AMSNAPSHOT_FMT_S32_RGBA  AMSNAPSHOT_FOURCC('R', 'G', 'B', 'A') /* 32  BGR-8-8-8-8   */
#define AMSNAPSHOT_FMT_S32_BGRA  AMSNAPSHOT_FOURCC('B', 'G', 'R', 'A') /* 32  BGR-8-8-8-8   */
#define AMSNAPSHOT_FMT_S32_ABGR  AMSNAPSHOT_FOURCC('A', 'B', 'G', 'R') /* 32  BGR-8-8-8-8   */
#define AMSNAPSHOT_FMT_S32_ARGB  AMSNAPSHOT_FOURCC('A', 'R', 'G', 'B') /* 32  BGR-8-8-8-8   */

struct aml_snapshot_t {
  unsigned int  src_x;
  unsigned int  src_y;
  unsigned int  src_width;
  unsigned int  src_height;
  unsigned int  dst_width;
  unsigned int  dst_height;
  unsigned int  dst_stride;
  unsigned int  dst_format;
  unsigned int  dst_size;
  unsigned long dst_vaddr;
};
