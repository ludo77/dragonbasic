//  MF_mappy.cpp: MF/TIN compiler FMP parser
//
//  Copyright (C) 2015 Ulrich Hecht
//  Derived from FMP2GBA 1.0, Copyright (C) Dave Etherton
//
//  This software is provided 'as-is', without any express or implied
//  warranty.  In no event will the authors be held liable for any damages
//  arising from the use of this software.
//
//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software
//     in a product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//  2. Altered source versions must be plainly marked as such, and must not be
//     misrepresented as being the original software.
//  3. This notice may not be removed or altered from any source distribution.
//
//  Ulrich Hecht
//  ulrich.hecht@gmail.com


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "MF.h"

#define FOURCC(a,b,c,d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

// borrowed from mappy:
typedef struct {	/* Structure for data blocks */
	int bgoff, fgoff;	/* offsets from start of graphic blocks */
	int fgoff2, fgoff3; /* more overlay blocks */
	unsigned int user1, user2;	/* user long data */
	unsigned short int user3, user4;	/* user short data */
	unsigned char user5, user6, user7;	/* user byte data */
	unsigned char tl : 1;	/* bits for collision detection */
	unsigned char tr : 1;
	unsigned char bl : 1;
	unsigned char br : 1;
	unsigned char trigger : 1;	/* bit to trigger an event */
	unsigned char unused1 : 1;
	unsigned char unused2 : 1;
	unsigned char unused3 : 1;
} BLKSTR;

#define AN_END -1			/* Animation types, AN_END = end of anims */
#define AN_NONE 0			/* No anim defined */
#define AN_LOOPF 1		/* Loops from start to end, then jumps to start etc */
#define AN_LOOPR 2		/* As above, but from end to start */
#define AN_ONCE 3			/* Only plays once */
#define AN_ONCEH 4		/* Only plays once, but holds end frame */
#define AN_PPFF 5			/* Ping Pong start-end-start-end-start etc */
#define AN_PPRR 6			/* Ping Pong end-start-end-start-end etc */
#define AN_PPRF 7			/* Used internally by playback */
#define AN_PPFR 8			/* Used internally by playback */
#define AN_ONCES 9		/* Used internally by playback */

typedef struct { /* Animation control structure */
	signed char antype;	/* Type of anim, AN_? */
	signed char andelay;	/* Frames to go before next frame */
	signed char ancount;	/* Counter, decs each frame, till 0, then resets to andelay */
	signed char anuser;	/* User info */
	int ancuroff;	/* Points to current offset in list */
	int anstartoff;	/* Points to start of blkstr offsets list, AFTER ref. blkstr offset */
	int anendoff;	/* Points to end of blkstr offsets list */
} ANISTR;

typedef struct {	/* Map header structure */
	/* char M,P,H,D;	4 byte chunk identification. */
	/* long int mphdsize;	size of map header. */
	char mapverhigh;	/* map version number to left of . (ie X.0). */
	char mapverlow;		/* map version number to right of . (ie 0.X). */
	char lsb;		/* if 1, data stored LSB first, otherwise MSB first. */
	char bodytype;	/* 0 for 32 offset still, -16 offset anim shorts in BODY */
	short int mapwidth;	/* width in blocks. */
	short int mapheight;	/* height in blocks. */
	short int reserved1;
	short int reserved2;
	short int blockwidth;	/* width of a block (tile) in pixels. */
	short int blockheight;	/* height of a block (tile) in pixels. */
	short int blockdepth;	/* depth of a block (tile) in planes (ie. 256 colours is 8) */
	short int blockstrsize;	/* size of a block data structure */
	short int numblockstr;	/* Number of block structures in BKDT */
	short int numblockgfx;	/* Number of 'blocks' in graphics (BODY) */
	unsigned char ckey8bit, ckeyred, ckeygreen, ckeyblue; /* colour key values */
} MPHD;

static MPHD *Header;
static BLKSTR *Blocks;

static void DecodeMPHD(MPHD *block,int tagLen) {
	if (block->mapverhigh != 1 || block->mapverlow != 0)
		GLB_warning("Expecting version 1.0 header, might not work (re-save after editing map properties)\n");
	Header = block;
}

static void DecodeCMAP(unsigned char *rgb,int tagLen, Output *out) {
	int i;
	for (i=0; i<tagLen; i += 3) {
		int p = (rgb[i] >> 3) | ((rgb[i+1]>>3) << 5) | ((rgb[i+2]>>3) << 10);
		out->emitWord(p);
	}
	free(rgb);
}

static void DecodeBKDT(BLKSTR *str,int tagLen) {
	Blocks = str;
}

static int DecodeBGFX(unsigned char *bits,int tagLen, Output *out) {
	int i, j;
	int area = Header->blockwidth * Header->blockheight;

	if (!Header)
		GLB_error("got BGFX before header!\n");

	tagLen /= area;
	int tiles = tagLen;

	while (tagLen) {
		for (i=0; i<Header->blockheight; i+=8) {
			for (j=0; j<Header->blockwidth; j+=8) {
				unsigned char *base = bits + j + i * Header->blockwidth;
				int m, n;
				for (m=0; m<8; m++,base += Header->blockwidth) {
					for (n=0; n<8; n++)
						out->emitByte(base[n]);
				}
			}
		}
		tagLen--;
		bits += area;
	}
	return tiles;
}

static int first_nonzero(int a,int b) {
	return a? a : b;
}

class AnimBlocks {
public:
        void reset() { aptr = 0; }
        void add(unsigned int _x, unsigned int _y, unsigned int _anim) {
                x[aptr] = _x;
                y[aptr] = _y;
                anim[aptr++] = _anim;
        }

        int aptr;
        unsigned int x[256];
        unsigned int y[256];
        unsigned int anim[256];
};
AnimBlocks anim_blocks;

unsigned int anim_addr = 0;
unsigned int block_idcs_addr = 0;

static void DecodeBODY(short *body,int tagLen, Output *out) {
	int i;
	if (!Header || !Blocks) {
		GLB_warning("got BODY before header and BKDT!\n");
		return;
	}

	anim_blocks.reset();
	tagLen >>= 1;
	for (i=0; i<tagLen; i++) {
		//char x;
		if (body[i] >= 0) {
			out->emitByte(first_nonzero(Blocks[body[i]].bgoff, Blocks[body[i]].fgoff));
		}
		else {
		        anim_blocks.add(i % Header->mapwidth,
		                        i / Header->mapwidth,
                                        -body[i] - 1);
			out->emitByte(0);	// dummy value
		}
	}
}

static void DecodeANDT(void *andt, int tagLen, Output *out)
{
        ANISTR *end_as = ((ANISTR*)((char*)andt + tagLen));
        ANISTR *as;
        unsigned int *anim_block_idcs = (unsigned int*)andt;
        unsigned int *end_anim_block_idcs;

        anim_addr = out->addr;

        as = end_as - 1;
        while (as->antype != AN_END) {
                if (as->antype != AN_LOOPF)
                        GLB_error("unsupported animation type %d\n", as->antype);
                out->emitWord(as->antype);
                out->emitWord(as->andelay);
                out->emitWord(as->anstartoff - 1);
                out->emitWord(as->anendoff - 1);
                as--;
        }
        end_anim_block_idcs = (unsigned int *)as;

        block_idcs_addr = out->addr;
        for (int i = 0; i < end_anim_block_idcs - anim_block_idcs; i++)
                out->emitWord(first_nonzero(Blocks[anim_block_idcs[i]].bgoff,
                                            Blocks[anim_block_idcs[i]].fgoff));
}

static void CodeAnim(Output *out)
{
        for (int i = 0; i < anim_blocks.aptr; i++) {
                out->emitWord(anim_blocks.x[i]);
                out->emitWord(anim_blocks.y[i]);
                // frame countdown and current offset in EWRAM
                out->emitDword(out->vaddr);
                out->vaddr += 4;
                out->emitDword(anim_addr + anim_blocks.anim[i] * 8);
        }
        out->emitDword(-1);
}

static int GetWord(FILE *f) {	// big-endian word
	int w = fgetc(f) << 24;
	w |= fgetc(f) << 16;
	w |= fgetc(f) << 8;
	w |= fgetc(f);
	return w;
}

int Decode(const char *filename,Output *out) {
	FILE *f = fopen(filename,"rb");
	int totalSize;
	int tag;

	anim_blocks.reset();

	if (!f)
		GLB_error("'%s' - file not found\n",filename);

	if (GetWord(f) != FOURCC('F','O','R','M')) {
		fclose(f);
		GLB_error("'%s' - not a IFF format file\n",filename);
	}

	totalSize = GetWord(f) - 4;	// form type is counted here

	if (GetWord(f) != FOURCC('F','M','A','P')) {
		fclose(f);
		GLB_error("'%s' - not an FMAP file\n",filename);
	}

	unsigned int cmap_ptr = out->addr;
	out->emitDword(0);
	unsigned int gfx_ptr = out->addr;
	out->emitDword(0);
	unsigned int map_ptr = out->addr;
	out->emitDword(0);
	unsigned int dim = out->addr;
	out->emitDword(0);
	unsigned int tiles_addr = out->addr;
	out->emitWord(0);
	unsigned int layers_addr = out->addr;
	out->emitWord(0);
	int layers = 0;
	unsigned int anim_ptr = 0;
	anim_addr = 0;

	while ((tag = GetWord(f)) != -1) {
		int tagLen = GetWord(f);
		void *block = malloc(tagLen);
		fread(block,tagLen,1,f);

		totalSize -= 8 + tagLen;
		if (tag == FOURCC('M','P','H','D')) {
			DecodeMPHD((MPHD *)block,tagLen);
			DEBUG("map w %d h %d block w %d h %d\n",
				Header->mapwidth,
				Header->mapheight,
				Header->blockwidth,
				Header->blockheight);
                        if (Header->mapwidth & 1)
                                GLB_warning("%s: odd horizontal map size", filename);
                        if (Header->blockwidth != Header->blockheight)
                                GLB_warning("%s: non-square blocks", filename);
			// XXX: endianness...
			out->patch32(dim,
				(Header->mapheight    << 24) |
				(Header->mapwidth   << 16) |
				(Header->blockheight  <<  8) |
				(Header->blockwidth <<  0));
		} else if (tag == FOURCC('C','M','A','P')) {
			out->patch32(cmap_ptr, out->addr);
			DecodeCMAP((unsigned char *)block,tagLen,out);
		}
		else if (tag == FOURCC('B','K','D','T'))
			DecodeBKDT((BLKSTR *)block,tagLen);
		else if (tag == FOURCC('B','G','F','X')) {
			out->patch32(gfx_ptr, out->addr);
			int tiles = DecodeBGFX((unsigned char *)block,tagLen,out) *
			        Header->blockwidth / 8 *
			        Header->blockheight / 8;
			out->patch16(tiles_addr, tiles);
		}
		else if (tag == FOURCC('B','O','D','Y') ||
			 (tag & 0xffffff00) == FOURCC('L','Y','R',0)) {
			++layers;
			out->patch32(map_ptr, out->addr);
			map_ptr = out->addr;
			out->emitDword(0);
			anim_ptr = out->addr;
			out->emitDword(0);
			DEBUG("layer at 0x%x\n", out->addr);
			DecodeBODY((short *)block,tagLen,out);
                        if (anim_addr) {
                                out->alignDword();
                                out->patch32(anim_ptr, out->addr);
                                assert(block_idcs_addr);
                                out->emitDword(block_idcs_addr);
                                CodeAnim(out);
                        }
		}
		else if (tag == FOURCC('A','N','D','T')) {
		        out->alignDword();
		        DecodeANDT(block, tagLen, out);
		}
		else {
			GLB_warning("code %c%c%c%c (%d bytes) skipped\n",
				    tag >> 24, tag << 8 >> 16,
				    tag << 16 >> 16, tag & 0xff, tagLen);
			free(block);
		}
	}

	out->patch16(layers_addr, layers);

	if (totalSize)
		GLB_error("'%s' - warning - %d bytes after IFF\n",filename,totalSize);

	fclose(f);
	return 0;
}
