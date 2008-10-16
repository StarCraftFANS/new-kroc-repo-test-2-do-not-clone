/*
 *	video-v4l.h -- private definitions for occam-pi V4L2 library
 *	Copyright (C) 2008 Fred Barnes <frmb@kent.ac.uk>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef __VIDEO_V4L_H
#define __VIDEO_V4L_H

#define OPI_VIDEO_DEVICE_FNAMEMAX 128

typedef struct opi_video_device {
	char fname[OPI_VIDEO_DEVICE_FNAMEMAX];
	int fnamelen;
	int fd;
	int api;
} opi_video_device_t;

#define OPI_VIDEO_IDENTITY_NAMEMAX 64

typedef struct opi_video_identity {
	char name[OPI_VIDEO_IDENTITY_NAMEMAX];
	int namelen;
} opi_video_identity_t;

#define OPI_VIDEO_CAMINPUT_NAMEMAX 64

typedef struct opi_video_caminput {
	int id;
	char name[OPI_VIDEO_CAMINPUT_NAMEMAX];
	int namelen;
	int minw;
	int minh;
	int maxw;
	int maxh;
} opi_video_caminput_t;

typedef struct opi_video_iodata {
	int use_mmap;
	int mmap_addr;
	int mmap_size;
	int width;
	int height;
	int format;
	int isize;
} opi_video_iodata_t;

typedef struct opi_video_frameinfo {
	int width;
	int height;
	int format;
	int isize;
} opi_video_frameinfo_t;

#endif	/* !__VIDEO_V4L_H */
