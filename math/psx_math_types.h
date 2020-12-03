#ifndef PSX_MATH_TYPES_H
#define PSX_MATH_TYPES_H

struct VECTOR_NOPAD {
	long vx; // size=0, offset=0
	long vy; // size=0, offset=4
	long vz; // size=0, offset=8
};

struct USVECTOR_NOPAD {
	unsigned short vx; // size=0, offset=0
	unsigned short vy; // size=0, offset=2
	unsigned short vz; // size=0, offset=4
};

struct SVECTOR
{
	short x;
	short y;
	short z;
	short pad;
};

struct CVECTOR
{
	ubyte r;
	ubyte g;
	ubyte b;
	ubyte pad;
};

struct XYPAIR {
	int x; // size=0, offset=0
	int y; // size=0, offset=4
};

struct UV_INFO
{
	unsigned char u;
	unsigned char v;
};

#endif // PSX_MATH_TYPES_H