/**
 * @file ECC.cpp
 * @brief Unified Ed25519 and X25519 implementation for Ksee.
 */

#include "../../../include/Ksee/Crypto/ECC.hpp"
#include "../../../include/Ksee/Crypto/SHA.hpp"
#include <string.h>
#include <cstdio>

namespace Ksee {

#define FOR(i, start, end) for (usz i = (start); i < (end); i++)
#define ZERO(p, n) do { for (usz _i = 0; _i < (n); _i++) (p)[_i] = 0; } while (0)
#define COPY(d, s, n) do { for (usz _i = 0; _i < (n); _i++) (d)[_i] = (s)[_i]; } while (0)
#define WIPE_BUFFER(b) do { volatile u8 *_p = (volatile u8*)(b); for (usz _i = 0; _i < sizeof(b); _i++) _p[_i] = 0; } while (0)

typedef i32 fe[10];

static const fe fe_one  = {1};
static const fe sqrtm1 = {
	34513072, 25610706, 9377949, 3500415, 12389472,
	33281959, 41962654, 31548777, 326685, 11406482
};
static const fe d_const = {
	56195235, 13857412, 51736253, 6949390, 114729,
	24766616, 60832955, 30306712, 48412415, 21499315
};
static const fe D2 = {
	45281625, 27714825, 36363642, 13898781, 229458,
	15978800, 54557047, 27058993, 29715967, 9444199
};

static void fe_0(fe h) {           ZERO(h  , 10); }
static void fe_1(fe h) { h[0] = 1; ZERO(h+1,  9); }

static void fe_copy(fe h,const fe f           ){FOR(i,0,10) h[i] =  f[i];      }
static void fe_neg (fe h,const fe f           ){FOR(i,0,10) h[i] = -f[i];      }
static void fe_add (fe h,const fe f,const fe g){FOR(i,0,10) h[i] = f[i] + g[i];}
static void fe_sub (fe h,const fe f,const fe g){FOR(i,0,10) h[i] = f[i] - g[i];}

static void fe_cswap(fe f, fe g, int b) {
	i32 mask = -b;
	FOR (i, 0, 10) {
		i32 x = (f[i] ^ g[i]) & mask;
		f[i] = f[i] ^ x;
		g[i] = g[i] ^ x;
	}
}

static void fe_ccopy(fe f, const fe g, int b) {
	i32 mask = -b;
	FOR (i, 0, 10) {
		i32 x = (f[i] ^ g[i]) & mask;
		f[i] = f[i] ^ x;
	}
}

#define FE_CARRY	\
	i64 c; \
	c = (t0 + ((i64)1<<25)) >> 26;  t0 -= c * ((i64)1 << 26);  t1 += c; \
	c = (t4 + ((i64)1<<25)) >> 26;  t4 -= c * ((i64)1 << 26);  t5 += c; \
	c = (t1 + ((i64)1<<24)) >> 25;  t1 -= c * ((i64)1 << 25);  t2 += c; \
	c = (t5 + ((i64)1<<24)) >> 25;  t5 -= c * ((i64)1 << 25);  t6 += c; \
	c = (t2 + ((i64)1<<25)) >> 26;  t2 -= c * ((i64)1 << 26);  t3 += c; \
	c = (t6 + ((i64)1<<25)) >> 26;  t6 -= c * ((i64)1 << 26);  t7 += c; \
	c = (t3 + ((i64)1<<24)) >> 25;  t3 -= c * ((i64)1 << 25);  t4 += c; \
	c = (t7 + ((i64)1<<24)) >> 25;  t7 -= c * ((i64)1 << 25);  t8 += c; \
	c = (t4 + ((i64)1<<25)) >> 26;  t4 -= c * ((i64)1 << 26);  t5 += c; \
	c = (t8 + ((i64)1<<25)) >> 26;  t8 -= c * ((i64)1 << 26);  t9 += c; \
	c = (t9 + ((i64)1<<24)) >> 25;  t9 -= c * ((i64)1 << 25);  t0 += c * 19; \
	c = (t0 + ((i64)1<<25)) >> 26;  t0 -= c * ((i64)1 << 26);  t1 += c; \
	h[0] = (i32)t0;  h[1] = (i32)t1;  h[2] = (i32)t2;  h[3] = (i32)t3;  h[4] = (i32)t4; \
	h[5] = (i32)t5;  h[6] = (i32)t6;  h[7] = (i32)t7;  h[8] = (i32)t8;  h[9] = (i32)t9;

static void fe_mul(fe h, const fe f, const fe g)
{
	i32 f0 = f[0]; i32 f1 = f[1]; i32 f2 = f[2]; i32 f3 = f[3]; i32 f4 = f[4];
	i32 f5 = f[5]; i32 f6 = f[6]; i32 f7 = f[7]; i32 f8 = f[8]; i32 f9 = f[9];
	i32 g0 = g[0]; i32 g1 = g[1]; i32 g2 = g[2]; i32 g3 = g[3]; i32 g4 = g[4];
	i32 g5 = g[5]; i32 g6 = g[6]; i32 g7 = g[7]; i32 g8 = g[8]; i32 g9 = g[9];
	i64 F1 = (i64)f1*2; i64 F3 = (i64)f3*2; i64 F5 = (i64)f5*2; i64 F7 = (i64)f7*2; i64 F9 = (i64)f9*2;
	i64 G1 = (i64)g1*19;  i64 G2 = (i64)g2*19;  i64 G3 = (i64)g3*19;
	i64 G4 = (i64)g4*19;  i64 G5 = (i64)g5*19;  i64 G6 = (i64)g6*19;
	i64 G7 = (i64)g7*19;  i64 G8 = (i64)g8*19;  i64 G9 = (i64)g9*19;

	i64 t0 = f0*(i64)g0 + F1*G9 + f2*G8 + F3*G7 + f4*G6
	       + F5*G5 + f6*G4 + F7*G3 + f8*G2 + F9*G1;
	i64 t1 = f0*(i64)g1 + f1*(i64)g0 + f2*G9 + f3*G8 + f4*G7
	       + f5*G6 + f6*G5 + f7*G4 + f8*G3 + f9*G2;
	i64 t2 = f0*(i64)g2 + F1*(i64)g1 + f2*(i64)g0 + F3*G9 + f4*G8
	       + F5*G7 + f6*G6 + F7*G5 + f8*G4 + F9*G3;
	i64 t3 = f0*(i64)g3 + f1*(i64)g2 + f2*(i64)g1 + f3*(i64)g0 + f4*G9
	       + f5*G8 + f6*G7 + f7*G6 + f8*G5 + f9*G4;
	i64 t4 = f0*(i64)g4 + F1*(i64)g3 + f2*(i64)g2 + F3*(i64)g1 + f4*(i64)g0
	       + F5*G9 + f6*G8 + F7*G7 + f8*G6 + F9*G5;
	i64 t5 = f0*(i64)g5 + f1*(i64)g4 + f2*(i64)g3 + f3*(i64)g2 + f4*(i64)g1
	       + f5*(i64)g0 + f6*G9 + f7*G8 + f8*G7 + f9*G6;
	i64 t6 = f0*(i64)g6 + F1*(i64)g5 + f2*(i64)g4 + F3*(i64)g3 + f4*(i64)g2
	       + F5*(i64)g1 + f6*(i64)g0 + F7*G9 + f8*G8 + F9*G7;
	i64 t7 = f0*(i64)g7 + f1*(i64)g6 + f2*(i64)g5 + f3*(i64)g4 + f4*(i64)g3
	       + f5*(i64)g2 + f6*(i64)g1 + f7*(i64)g0 + f8*G9 + f9*G8;
	i64 t8 = f0*(i64)g8 + F1*(i64)g7 + f2*(i64)g6 + F3*(i64)g5 + f4*(i64)g4
	       + F5*(i64)g3 + f6*(i64)g2 + F7*(i64)g1 + f8*(i64)g0 + F9*G9;
	i64 t9 = f0*(i64)g9 + f1*(i64)g8 + f2*(i64)g7 + f3*(i64)g6 + f4*(i64)g5
	       + f5*(i64)g4 + f6*(i64)g3 + f7*(i64)g2 + f8*(i64)g1 + f9*(i64)g0;

	FE_CARRY;
}

static void fe_sq(fe h, const fe f)
{
	i32 f0 = f[0]; i32 f1 = f[1]; i32 f2 = f[2]; i32 f3 = f[3]; i32 f4 = f[4];
	i32 f5 = f[5]; i32 f6 = f[6]; i32 f7 = f[7]; i32 f8 = f[8]; i32 f9 = f[9];
	i64 f0_2  = (i64)f0*2;   i64 f1_2  = (i64)f1*2;   i64 f2_2  = (i64)f2*2;   i64 f3_2 = (i64)f3*2;
	i64 f4_2  = (i64)f4*2;   i64 f5_2  = (i64)f5*2;   i64 f6_2  = (i64)f6*2;   i64 f7_2 = (i64)f7*2;
	i64 f5_38 = (i64)f5*38;  i64 f6_19 = (i64)f6*19;  i64 f7_38 = (i64)f7*38;
	i64 f8_19 = (i64)f8*19;  i64 f9_38 = (i64)f9*38;

	i64 t0 = f0  *(i64)f0    + f1_2*f9_38 + f2_2*f8_19
	       + f3_2*f7_38 + f4_2*f6_19 + f5  *f5_38;
	i64 t1 = f0_2*(i64)f1    + f2  *f9_38 + f3_2*f8_19
	       + f4  *f7_38 + f5_2*f6_19;
	i64 t2 = f0_2*(i64)f2    + f1_2*(i64)f1    + f3_2*f9_38
	       + f4_2*f8_19 + f5_2*f7_38 + f6  *f6_19;
	i64 t3 = f0_2*(i64)f3    + f1_2*(i64)f2    + f4  *f9_38
	       + f5_2*f8_19 + f6  *f7_38;
	i64 t4 = f0_2*(i64)f4    + f1_2*f3_2  + f2  *(i64)f2
	       + f5_2*f9_38 + f6_2*f8_19 + f7  *f7_38;
	i64 t5 = f0_2*(i64)f5    + f1_2*(i64)f4    + f2_2*(i64)f3
	       + f6  *f9_38 + f7_2*f8_19;
	i64 t6 = f0_2*(i64)f6    + f1_2*f5_2  + f2_2*(i64)f4
	       + f3_2*(i64)f3    + f7_2*f9_38 + f8  *f8_19;
	i64 t7 = f0_2*(i64)f7    + f1_2*(i64)f6    + f2_2*(i64)f5
	       + f3_2*(i64)f4    + f8  *f9_38;
	i64 t8 = f0_2*(i64)f8    + f1_2*f7_2  + f2_2*(i64)f6
	       + f3_2*f5_2  + f4  *(i64)f4    + f9  *f9_38;
	i64 t9 = f0_2*(i64)f9    + f1_2*(i64)f8    + f2_2*(i64)f7
	       + f3_2*(i64)f6    + f4  *f5_2;

	FE_CARRY;
}

static void fe_tobytes(u8 s[32], const fe h);

static int fe_isequal(const fe f, const fe g)
{
	u8 fs[32], gs[32];
	fe_tobytes(fs, f);
	fe_tobytes(gs, g);
	u32 diff = 0;
	for (int i = 0; i < 32; ++i) diff |= (u32)(fs[i] ^ gs[i]);
	WIPE_BUFFER(fs);
	WIPE_BUFFER(gs);
	return (diff == 0) ? 1 : 0;
}

static void fe_pow22523(fe out, const fe x)
{
	fe t0, t1, t2;
	fe_sq(t0, x);
	fe_sq(t1,t0);                     fe_sq(t1, t1);    fe_mul(t1, x, t1);
	fe_mul(t0, t0, t1);
	fe_sq(t0, t0);                                      fe_mul(t0, t1, t0);
	fe_sq(t1, t0);  FOR (i, 1,   5) { fe_sq(t1, t1); }  fe_mul(t0, t1, t0);
	fe_sq(t1, t0);  FOR (i, 1,  10) { fe_sq(t1, t1); }  fe_mul(t1, t1, t0);
	fe_sq(t2, t1);  FOR (i, 1,  20) { fe_sq(t2, t2); }  fe_mul(t1, t2, t1);
	fe_sq(t1, t1);  FOR (i, 1,  10) { fe_sq(t1, t1); }  fe_mul(t0, t1, t0);
	fe_sq(t1, t0);  FOR (i, 1,  50) { fe_sq(t1, t1); }  fe_mul(t1, t1, t0);
	fe_sq(t2, t1);  FOR (i, 1, 100) { fe_sq(t2, t2); }  fe_mul(t1, t2, t1);
	fe_sq(t1, t1);  FOR (i, 1,  50) { fe_sq(t1, t1); }  fe_mul(t0, t1, t0);
	fe_sq(t0, t0);  FOR (i, 1,   2) { fe_sq(t0, t0); }  fe_mul(out, t0, x);
	WIPE_BUFFER(t0);
	WIPE_BUFFER(t1);
	WIPE_BUFFER(t2);
}

static void fe_invert(fe out, const fe x)
{
	fe t0, t1, t2;
	fe_pow22523(t0, x);
	fe_sq(t1, t0);
	fe_sq(t1, t1);
	fe_sq(t1, t1);
	fe_sq(t2, x);
	fe_mul(t2, t2, x);
	fe_mul(out, t1, t2);
	WIPE_BUFFER(t0);
	WIPE_BUFFER(t1);
	WIPE_BUFFER(t2);
}

static inline u32 load32_le(const u8 s[4])
{
	return (u32)s[0]
		| ((u32)s[1] <<  8)
		| ((u32)s[2] << 16)
		| ((u32)s[3] << 24);
}

static inline u64 load24_le(const u8 s[3])
{
	return (u64)s[0]
		| ((u64)s[1] <<  8)
		| ((u64)s[2] << 16);
}

static inline void store32_le(u8 s[4], u32 v)
{
	s[0] = (u8)v;
	s[1] = (u8)(v >>  8);
	s[2] = (u8)(v >> 16);
	s[3] = (u8)(v >> 24);
}

static inline u64 load64_le(const u8 *p) {
	return (u64)p[0] | ((u64)p[1] << 8) | ((u64)p[2] << 16) | ((u64)p[3] << 24)
		| ((u64)p[4] << 32) | ((u64)p[5] << 40) | ((u64)p[6] << 48) | ((u64)p[7] << 56);
}

static inline void store64_le(u8 *p, u64 v) {
	for (int i = 0; i < 8; ++i) p[i] = (u8)(v >> (i * 8));
}

static void fe_frombytes(fe h, const u8 s[32])
{
	u32 mask = 0xffffff >> 1;
	i64 t0 =  load32_le(s);
	i64 t1 =  load24_le(s +  4) << 6;
	i64 t2 =  load24_le(s +  7) << 5;
	i64 t3 =  load24_le(s + 10) << 3;
	i64 t4 =  load24_le(s + 13) << 2;
	i64 t5 =  load32_le(s + 16);
	i64 t6 =  load24_le(s + 20) << 7;
	i64 t7 =  load24_le(s + 23) << 5;
	i64 t8 =  load24_le(s + 26) << 4;
	i64 t9 = (load24_le(s + 29) & mask) << 2;
	FE_CARRY
}

static void fe_tobytes(u8 s[32], const fe h)
{
	i32 t[10];
	COPY(t, h, 10);
	i32 q = (19 * t[9] + (((i32) 1) << 24)) >> 25;
	FOR (i, 0, 5) {
		q += t[2*i  ]; q >>= 26;
		q += t[2*i+1]; q >>= 25;
	}
	q *= 19;
	FOR (i, 0, 5) {
		t[i*2  ] += q;  q = t[i*2  ] >> 26;  t[i*2  ] -= q * ((i32)1 << 26);
		t[i*2+1] += q;  q = t[i*2+1] >> 25;  t[i*2+1] -= q * ((i32)1 << 25);
	}

	store32_le(s +  0, ((u32)t[0] >>  0) | ((u32)t[1] << 26));
	store32_le(s +  4, ((u32)t[1] >>  6) | ((u32)t[2] << 19));
	store32_le(s +  8, ((u32)t[2] >> 13) | ((u32)t[3] << 13));
	store32_le(s + 12, ((u32)t[3] >> 19) | ((u32)t[4] <<  6));
	store32_le(s + 16, ((u32)t[5] >>  0) | ((u32)t[6] << 25));
	store32_le(s + 20, ((u32)t[6] >>  7) | ((u32)t[7] << 19));
	store32_le(s + 24, ((u32)t[7] >> 13) | ((u32)t[8] << 12));
	store32_le(s + 28, ((u32)t[8] >> 20) | ((u32)t[9] <<  6));

	WIPE_BUFFER(t);
}

// ---------------------------------------------------------------------------
static void fe_mul_small(fe h, const fe f, i32 g)
{
	i64 t0 = f[0] * (i64) g;  i64 t1 = f[1] * (i64) g;
	i64 t2 = f[2] * (i64) g;  i64 t3 = f[3] * (i64) g;
	i64 t4 = f[4] * (i64) g;  i64 t5 = f[5] * (i64) g;
	i64 t6 = f[6] * (i64) g;  i64 t7 = f[7] * (i64) g;
	i64 t8 = f[8] * (i64) g;  i64 t9 = f[9] * (i64) g;
	FE_CARRY;
}

// ---------------------------------------------------------------------------
// X25519 core
// ---------------------------------------------------------------------------

static void x25519_scalarmult(u8 *out, const u8 *scalar, const u8 *point) {
	fe x1, x2, z2, x3, z3, t0, t1;
	fe_frombytes(x1, point);
	fe_1(x2);
	fe_0(z2);
	fe_copy(x3, x1);
	fe_1(z3);

	u8 e[32];
	COPY(e, scalar, 32);
	e[0] &= 248;
	e[31] &= 127;
	e[31] |= 64;

	int swap = 0;
	for (int pos = 254; pos >= 0; --pos) {
		int b = (e[pos / 8] >> (pos & 7)) & 1;
		swap ^= b;
		fe_cswap(x2, x3, swap);
		fe_cswap(z2, z3, swap);
		swap = b;

		fe_sub(t0, x3, z3);
		fe_sub(t1, x2, z2);
		fe_add(x2, x2, z2);
		fe_add(z2, x3, z3);
		fe_mul(z3, t0, x2);
		fe_mul(z2, z2, t1);
		fe_sq (t0, t1    );
		fe_sq (t1, x2    );
		fe_add(x3, z3, z2);
		fe_sub(z2, z3, z2);
		fe_mul(x2, t1, t0);
		fe_sub(t1, t1, t0);
		fe_sq (z2, z2    );
		fe_mul_small(z3, t1, 121666);
		fe_sq (x3, x3    );
		fe_add(t0, t0, z3);
		fe_mul(z3, x1, z2);
		fe_mul(z2, t1, t0);
	}
	fe_cswap(x2, x3, swap);
	fe_cswap(z2, z3, swap);

	fe_invert(z2, z2);
	fe_mul(x2, x2, z2);
	fe_tobytes(out, x2);

	WIPE_BUFFER(e);
}

static const u8 basepoint[32] = {9};

// ---------------------------------------------------------------------------
// Ed25519 point representation and operations
// ---------------------------------------------------------------------------

struct p3 { fe X, Y, Z, T; };

static const p3 ed_base_point = {
	{ 52811034, 25909283, 16144682, 17082669, 27570973,
	  30858332, 40966398, 8378388, 20764389, 8758491 },
	{ 40265304, 26843545, 13421772, 20132659, 26843545,
	  6710886, 53687091, 13421772, 40265318, 26843545 },
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 28827043, 27438313, 39759291, 244362, 8635006,
	  11264893, 19351346, 13413597, 16611511, 27139452 }
};

static void point_to_bytes(u8 *s, const p3 *p) {
	fe recip, x, y;
	fe_invert(recip, p->Z);
	fe_mul(x, p->X, recip);
	fe_mul(y, p->Y, recip);
	fe_tobytes(s, y);
	u8 xbytes[32];
	fe_tobytes(xbytes, x);
	s[31] ^= ((xbytes[0] & 1) << 7);
}

static int point_from_bytes(p3 *p, const u8 *s) {
	fe u_elem, v, v3, v7, uv7, uv7_p58, cand, vx2, diff;
	fe_frombytes(p->Y, s);
	fe_1(p->Z);

	// u = y^2 - 1
	fe_sq(u_elem, p->Y);
	fe_sub(u_elem, u_elem, p->Z);

	// v = d * y^2 + 1
	fe_sq(v, p->Y);
	fe_mul(v, v, d_const);
	fe_add(v, v, p->Z);

	// v3 = v^3
	fe_sq(v3, v);
	fe_mul(v3, v3, v);

	// v7 = v^7
	fe_sq(v7, v3);
	fe_mul(v7, v7, v);

	// uv7 = u * v^7
	fe_mul(uv7, u_elem, v7);

	// uv7_p58 = (u * v^7)^((p - 5) / 8)
	fe_pow22523(uv7_p58, uv7);

	// cand = u * v^3 * uv7_p58
	fe_mul(cand, u_elem, v3);
	fe_mul(cand, cand, uv7_p58);

	// vx2 = v * cand^2
	fe_sq(vx2, cand);
	fe_mul(vx2, vx2, v);

	fe zero_fe;
	fe_0(zero_fe);

	// Check if vx2 == u
	fe_sub(diff, vx2, u_elem);
	if (fe_isequal(diff, zero_fe)) {
		fe_copy(p->X, cand);
	} else {
		// Check if vx2 == -u
		fe_add(diff, vx2, u_elem);
		if (fe_isequal(diff, zero_fe)) {
			fe_mul(p->X, cand, sqrtm1);
		} else {
			return -1; // neither: decoding fails
		}
	}

	u8 xbytes[32];
	fe_tobytes(xbytes, p->X);
	if ((xbytes[0] & 1) != ((s[31] >> 7) & 1)) {
		fe_neg(p->X, p->X);
	}
	fe_mul(p->T, p->X, p->Y);
	return 0;
}

static void point_add(p3 *r, const p3 *p, const p3 *q)
{
	fe a, b, c, d_elem, e, f, g, h;
	fe_sub(a, p->Y, p->X);
	fe_sub(b, q->Y, q->X);
	fe_mul(a, a, b);
	fe_add(b, p->Y, p->X);
	fe_add(c, q->Y, q->X);
	fe_mul(b, b, c);
	fe_mul(c, p->T, q->T);
	fe_mul(c, c, D2);
	fe_mul(d_elem, p->Z, q->Z);
	fe_add(d_elem, d_elem, d_elem);
	fe_sub(e, b, a);
	fe_sub(f, d_elem, c);
	fe_add(g, d_elem, c);
	fe_add(h, b, a);
	fe_mul(r->X, e, f);
	fe_mul(r->Y, h, g);
	fe_mul(r->Z, g, f);
	fe_mul(r->T, e, h);
}

static void point_double(p3 *r, const p3 *p)
{
	fe a, b, c, d_elem, e, f, g, h;
	fe_sq(a, p->X);
	fe_sq(b, p->Y);
	fe_sq(c, p->Z);
	fe_add(c, c, c);
	fe_add(d_elem, a, b);
	fe_add(e, p->X, p->Y);
	fe_sq(e, e);
	fe_sub(e, e, d_elem);
	fe_sub(f, b, a);
	fe_sub(g, c, f);
	fe_mul(r->X, e, g);
	fe_mul(r->Y, d_elem, f);
	fe_mul(r->Z, f, g);
	fe_mul(r->T, e, d_elem);
}

static const u64 L_64[4] = {
	0x5812631a5cf5d3edULL,
	0x14def9dea2f79cd6ULL,
	0x0000000000000000ULL,
	0x1000000000000000ULL,
};

static void reduce_mod_L(u8 out32[32], const u8 in64[64]) {
	u64 cur[4] = {0, 0, 0, 0};
	for (int i = 511; i >= 0; --i) {
		u64 bit = (in64[i >> 3] >> (i & 7)) & 1;
		u64 c = bit;
		for (int w = 0; w < 4; ++w) {
			u64 next_c = cur[w] >> 63;
			cur[w] = (cur[w] << 1) | c;
			c = next_c;
		}
		bool ge = (c != 0);
		if (!ge) {
			for (int w = 3; w >= 0; --w) {
				if (cur[w] > L_64[w]) { ge = true; break; }
				if (cur[w] < L_64[w]) { ge = false; break; }
				if (w == 0) ge = true;
			}
		}
		if (ge) {
			u64 borrow = 0;
			for (int w = 0; w < 4; ++w) {
				unsigned __int128 diff = (unsigned __int128)cur[w] - L_64[w] - borrow;
				cur[w] = (u64)diff;
				borrow = (diff >> 64) ? 1 : 0;
			}
		}
	}
	for (int w = 0; w < 4; ++w) {
		store64_le(out32 + w * 8, cur[w]);
	}
}

static void crypto_eddsa_reduce(u8 reduced[32], const u8 expanded[64])
{
	reduce_mod_L(reduced, expanded);
}

static void crypto_eddsa_mul_add(u8 r[32], const u8 a[32], const u8 b[32], const u8 c[32])
{
	u64 A[4], B[4], prod[8] = {0};
	for (int i = 0; i < 4; ++i) {
		A[i] = load64_le(a + i * 8);
		B[i] = load64_le(b + i * 8);
	}
	for (int i = 0; i < 4; ++i) {
		u64 carry = 0;
		for (int j = 0; j < 4; ++j) {
			unsigned __int128 t = (unsigned __int128)prod[i + j] + (unsigned __int128)A[i] * B[j] + carry;
			prod[i + j] = (u64)t;
			carry = (u64)(t >> 64);
		}
		prod[i + 4] = carry;
	}
	u64 carry = 0;
	for (int i = 0; i < 4; ++i) {
		u64 cv = load64_le(c + i * 8);
		unsigned __int128 t = (unsigned __int128)prod[i] + cv + carry;
		prod[i] = (u64)t;
		carry = (u64)(t >> 64);
	}
	for (int i = 4; i < 8 && carry; ++i) {
		unsigned __int128 t = (unsigned __int128)prod[i] + carry;
		prod[i] = (u64)t;
		carry = (u64)(t >> 64);
	}
	u8 full_buf[64];
	for (int i = 0; i < 8; ++i) store64_le(full_buf + i * 8, prod[i]);
	reduce_mod_L(r, full_buf);
	WIPE_BUFFER(full_buf);
	WIPE_BUFFER(prod);
	WIPE_BUFFER(A);
	WIPE_BUFFER(B);
}

static void ed_scalarmult(p3 *out, const p3 *base, const u8 *scalar) {
	p3 res;
	fe_0(res.X); fe_1(res.Y); fe_1(res.Z); fe_0(res.T);
	p3 cur = *base;

	for (int i = 0; i < 256; ++i) {
		int bit = (scalar[i >> 3] >> (i & 7)) & 1;
		if (bit) {
			p3 tmp;
			point_add(&tmp, &res, &cur);
			res = tmp;
		}
		p3 dbl;
		point_double(&dbl, &cur);
		cur = dbl;
	}
	*out = res;
}

static void ed_scalarbase(u8 *out, const u8 *scalar) {
	p3 res;
	ed_scalarmult(&res, &ed_base_point, scalar);
	point_to_bytes(out, &res);
}

// ---------------------------------------------------------------------------
// Key conversions
// ---------------------------------------------------------------------------

u256 edToXPrivateKey(const u256 &edSecretKey) {
	u8 az[64];
	SHA512 sha;
	sha.update(edSecretKey.data, 32);
	sha.final(az);
	az[0] &= 248;
	az[31] &= 127;
	az[31] |= 64;
	u256 xpriv(az);
	WIPE_BUFFER(az);
	return xpriv;
}

u256 edToXPublicKey(const u256 &edPublicKey) {
	fe y, one, num, den, den_inv, u_elem;
	fe_frombytes(y, edPublicKey.data);
	fe_1(one);
	fe_add(num, one, y);
	fe_sub(den, one, y);
	fe_invert(den_inv, den);
	fe_mul(u_elem, num, den_inv);
	u256 res;
	fe_tobytes(res.data, u_elem);
	return res;
}

// ---------------------------------------------------------------------------
// XKeypair implementation
// ---------------------------------------------------------------------------

XKeypair::XKeypair() : _cachedPub(), _hasPub(false), secretKey() {}

XKeypair::XKeypair(const u256 &priv) : _cachedPub(), _hasPub(false), secretKey(priv) {}

XKeypair::XKeypair(const String &priv) : _cachedPub(), _hasPub(false), secretKey(priv) {}

XKeypair::XKeypair(const EdKeypair &ed) : _cachedPub(), _hasPub(false), secretKey(edToXPrivateKey(ed.secretKey)) {}

XKeypair::XKeypair(const XKeypair &o) : _cachedPub(o._cachedPub), _hasPub(o._hasPub), secretKey(o.secretKey) {}

XKeypair::XKeypair(XKeypair &&o) noexcept
	: _cachedPub(Move(o._cachedPub)), _hasPub(o._hasPub), secretKey(Move(o.secretKey)) {}

XKeypair &XKeypair::operator=(const XKeypair &o) {
	if (this != &o) {
		secretKey = o.secretKey;
		_cachedPub = o._cachedPub;
		_hasPub = o._hasPub;
	}
	return *this;
}

XKeypair &XKeypair::operator=(XKeypair &&o) noexcept {
	if (this != &o) {
		secretKey = Move(o.secretKey);
		_cachedPub = Move(o._cachedPub);
		_hasPub = o._hasPub;
	}
	return *this;
}

const u256 &XKeypair::publicKey() const {
	if (!_hasPub) {
		x25519_scalarmult(_cachedPub.data, secretKey.data, basepoint);
		_hasPub = true;
	}
	return _cachedPub;
}

XKeypair XKeypair::generate() {
	u256 priv;
	randomFill(priv.data, 32);
	return XKeypair(priv);
}

u256 XKeypair::sharedKey(const u256 &theirPublic) const {
	u256 res;
	x25519_scalarmult(res.data, secretKey.data, theirPublic.data);
	return res;
}

// ---------------------------------------------------------------------------
// EdKeypair implementation
// ---------------------------------------------------------------------------

EdKeypair::EdKeypair() : _cachedPub(), _hasPub(false), secretKey() {}

EdKeypair::EdKeypair(const u256 &priv) : _cachedPub(), _hasPub(false), secretKey(priv) {}

EdKeypair::EdKeypair(const String &priv) : _cachedPub(), _hasPub(false), secretKey(priv) {}

EdKeypair::EdKeypair(const EdKeypair &o) : _cachedPub(o._cachedPub), _hasPub(o._hasPub), secretKey(o.secretKey) {}

EdKeypair::EdKeypair(EdKeypair &&o) noexcept
	: _cachedPub(Move(o._cachedPub)), _hasPub(o._hasPub), secretKey(Move(o.secretKey)) {}

EdKeypair &EdKeypair::operator=(const EdKeypair &o) {
	if (this != &o) {
		secretKey = o.secretKey;
		_cachedPub = o._cachedPub;
		_hasPub = o._hasPub;
	}
	return *this;
}

EdKeypair &EdKeypair::operator=(EdKeypair &&o) noexcept {
	if (this != &o) {
		secretKey = Move(o.secretKey);
		_cachedPub = Move(o._cachedPub);
		_hasPub = o._hasPub;
	}
	return *this;
}

const u256 &EdKeypair::publicKey() const {
	if (!_hasPub) {
		u8 az[64];
		SHA512 sha;
		sha.update(secretKey.data, 32);
		sha.final(az);
		az[0] &= 248;
		az[31] &= 127;
		az[31] |= 64;

		ed_scalarbase(_cachedPub.data, az);
		_hasPub = true;
		WIPE_BUFFER(az);
	}
	return _cachedPub;
}

EdKeypair EdKeypair::generate() {
	u256 priv;
	randomFill(priv.data, 32);
	return EdKeypair(priv);
}

u256 EdKeypair::sharedKey(const u256 &theirPublic) const {
	u256 ourXPriv = edToXPrivateKey(secretKey);
	u256 theirXPub = edToXPublicKey(theirPublic);
	XKeypair xk(ourXPriv);
	return xk.sharedKey(theirXPub);
}

u512 EdKeypair::sign(const String &message) const {
	return edSign(secretKey, message);
}

bool EdKeypair::verify(const String &message, const u512 &signature) const {
	return edVerify(publicKey(), message, signature);
}

u512 edSign(const u256 &secretKey, const String &message) {
	u8 az[64];
	SHA512 sha;
	sha.update(secretKey.data, 32);
	sha.final(az);
	az[0] &= 248;
	az[31] &= 127;
	az[31] |= 64;

	u8 A[32];
	ed_scalarbase(A, az);

	SHA512 shaR;
	shaR.update(az + 32, 32);
	shaR.update((const u8*)message.data(), message.size());
	u8 nonce_hash[64];
	shaR.final(nonce_hash);

	u8 r[32];
	crypto_eddsa_reduce(r, nonce_hash);

	u8 R[32];
	ed_scalarbase(R, r);

	SHA512 shaH;
	shaH.update(R, 32);
	shaH.update(A, 32);
	shaH.update((const u8*)message.data(), message.size());
	u8 h_hash[64];
	shaH.final(h_hash);

	u8 h_reduced[32];
	crypto_eddsa_reduce(h_reduced, h_hash);

	u8 S[32];
	crypto_eddsa_mul_add(S, h_reduced, az, r);

	u512 sig;
	memcpy(sig.data, R, 32);
	memcpy(sig.data + 32, S, 32);

	WIPE_BUFFER(az);
	WIPE_BUFFER(nonce_hash);
	WIPE_BUFFER(h_hash);
	return sig;
}

bool edVerify(const u256 &publicKey, const String &message, const u512 &signature) {
	const u8 *sigData = signature.data;
	const u8 *pubData = publicKey.data;

	p3 A_pt, R_pt;
	if (point_from_bytes(&A_pt, pubData) != 0) return false;
	if (point_from_bytes(&R_pt, sigData) != 0) return false;

	SHA512 shaH;
	shaH.update(sigData, 32);
	shaH.update(pubData, 32);
	shaH.update((const u8*)message.data(), message.size());
	u8 h_hash[64];
	shaH.final(h_hash);

	u8 h[32];
	crypto_eddsa_reduce(h, h_hash);

	// Compute S*B
	u8 sb[32];
	ed_scalarbase(sb, sigData + 32);

	// Compute R + h*A
	p3 hA = A_pt;
	p3 hA_res;
	fe_0(hA_res.X); fe_1(hA_res.Y); fe_1(hA_res.Z); fe_0(hA_res.T);
	for (int i = 0; i < 256; ++i) {
		int bit = (h[i >> 3] >> (i & 7)) & 1;
		if (bit) {
			p3 tmp;
			point_add(&tmp, &hA_res, &hA);
			hA_res = tmp;
		}
		p3 dbl;
		point_double(&dbl, &hA);
		hA = dbl;
	}
	p3 RhA;
	point_add(&RhA, &R_pt, &hA_res);
	u8 rha_bytes[32];
	point_to_bytes(rha_bytes, &RhA);

	return (memcmp(sb, rha_bytes, 32) == 0);
}

} // namespace Ksee

