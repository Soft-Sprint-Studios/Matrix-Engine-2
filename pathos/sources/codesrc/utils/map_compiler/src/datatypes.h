/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#include <cstdint>
#include <cassert>
#include <cstring>

#ifdef M_PI
#undef M_PI
#endif
#define M_PI 3.14159265358979323846f

#ifndef DATATYPES_H
#define DATATYPES_H
typedef char                Char;
typedef unsigned char       Uchar;
typedef uint8_t             byte;
typedef int32_t             Int32;
typedef uint32_t            Uint32;
typedef int16_t             Int16;
typedef uint16_t            Uint16;
typedef int64_t             Int64;
typedef uint64_t            Uint64;
typedef float               Float;
typedef double              Double;
typedef Uint32              string_t;
typedef Int32               entindex_t;

struct vec4_t
{
	inline vec4_t()
	{
		for(Uint32 i = 0; i < 4; i++)
			v[i] = 0;
	}

	inline vec4_t(const vec4_t& src)
	{
		for(Uint32 i = 0; i < 4; i++)
			v[i] = src.v[i];
	}

	inline vec4_t( Float coords[4] )
	{
		for(Uint32 i = 0; i < 4; i++)
			v[i] = coords[i];
	}

	inline vec4_t& operator=( const vec4_t& src )
	{
		for(Uint32 i = 0; i < 4; i++)
			v[i] = src.v[i];

		return *this;
	}

	inline Float& operator[]( Uint32 n )
	{
		assert(n <= 4);
		return v[n];
	}

	inline Float operator[]( Uint32 n ) const
	{
		assert(n <= 4);
		return v[n];
	}

	inline Float& operator[]( Int32 n )
	{
		assert(n <= 4);
		return v[n];
	}

	inline Float operator[]( Int32 n ) const
	{
		assert(n <= 4);
		return v[n];
	}

	inline operator Float*( void )
	{
		return v;
	}

	inline operator const Float*( void ) const
	{
		return v;
	}

	Float v[4];
};

struct color32_t
{
	color32_t():
		r(0),
		g(0),
		b(0),
		a(255)
	{}
	color32_t( byte _r, byte _g, byte _b, byte _a )
	{
		r = _r;
		g = _g;
		b = _b;
		a = _a;
	}
	byte& operator[]( Uint32 n )
	{
		assert(n <= 3);

		if(n == 0)
			return r;
		else if(n == 1)
			return g;
		else if(n == 2)
			return b;
		else
			return a;
	}
	const byte& operator[]( Uint32 n ) const
	{
		assert(n <= 3);

		if(n == 0)
			return r;
		else if(n == 1)
			return g;
		else if(n == 2)
			return b;
		else
			return a;
	}

	byte r;
	byte g;
	byte b;
	byte a;
};

struct color24_t
{
	color24_t():
		r(0),
		g(0),
		b(0)
	{}
	color24_t( byte _r, byte _g, byte _b )
	{
		r = _r;
		g = _g;
		b = _b;
	}
	byte& operator[]( Uint32 n )
	{
		assert(n <= 2);

		if(n == 0)
			return r;
		else if(n == 1)
			return g;
		else
			return b;
	}
	const byte& operator[]( Uint32 n ) const
	{
		assert(n <= 2);

		if(n == 0)
			return r;
		else if(n == 1)
			return g;
		else
			return b;
	}

	byte r;
	byte g;
	byte b;
};

struct pmatrix3x4_t
{
	pmatrix3x4_t()
	{
		memset(matrix, 0, sizeof(matrix));
	}

	Float matrix[3][4];
};
#endif
