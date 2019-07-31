
/* ----------  ---------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lz32.h"


/* ----------  ---------- */


#define LZ32_MAGIC_NUMBER 0xCFF69D2DU



/* ----------  ---------- */



#ifndef LZ32_TYPES
#define LZ32_TYPES 64
#if (! defined (__VMS)) && (defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)))

#include <stdint.h>
#if (! defined (UINT64_MAX)) || (UINT64_MAX != 0xFFFFFFFFFFFFFFFFULL)
#error "this implementation requires 64-bit architecture"
#endif
typedef  uint8_t  u8t;
typedef uint16_t u16t;
typedef uint32_t u32t;
typedef uint64_t u64t;

#else

#include <limits.h>
#if (CHAR_BIT != 8) || (USHRT_MAX != 65535) || (UINT_MAX != 4294967295)
#error "this implementation doesn't support current system sizes of types"
#endif
#if (! defined (ULLONG_MAX)) || (ULLONG_MAX != 0xFFFFFFFFFFFFFFFFULL)
#error "this implementation requires 64-bit architecture"
#endif
typedef unsigned char           u8t;
typedef unsigned short int     u16t;
typedef unsigned int           u32t;
typedef unsigned long long int u64t;

#endif
#endif

/* ----------  ---------- */

#ifndef LZ32_INLINE

#ifdef _MSC_VER
#define LZ32_INLINE static __forceinline
#else
#if defined (__cplusplus) || ( defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) )
#ifdef __GNUC__
#define LZ32_INLINE static inline __attribute__((always_inline))
#else
#define LZ32_INLINE static inline
#endif
#else
#define LZ32_INLINE static
#endif
#endif

#endif

/* ----------  ---------- */

#if (defined(__GNUC__) && (__GNUC__>=3)) || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER>=800)) || defined(__clang__)

#define likely(expr) (__builtin_expect ( (expr) != 0, 1 ))
#define unlikely(expr) (__builtin_expect ( (expr) != 0, 0 ))

#else

#define likely(expr) ((expr) != 0)
#define unlikely(expr) ((expr) != 0)

#endif

/* ----------  ---------- */

#if defined (LZ32_DEBUG) && (LZ32_DEBUG != 0)

#define lz32_assert(expr) do { \
  if ((expr) != 0) break; \
  fprintf ( stderr, "lz32: assert(%s:%d): (" #expr ")\n", __FILE__, __LINE__ ); \
  exit (EXIT_FAILURE); \
} while (0)

#define lz32_debug(...) do { \
  fprintf ( stderr, "lz32: DEBUG(%s:%d): ", __FILE__, __LINE__ ); \
  fprintf ( stderr, __VA_ARGS__ ); \
  fputs ( "\n", stderr ); \
} while (0)

#define lz32_error(code,...) do { \
  fprintf ( stderr, "lz32: error(): " ); \
  fprintf ( stderr, __VA_ARGS__ ); \
  fputs ( "\n", stderr ); \
  return (int)(code); \
} while (0)

#else

#define lz32_assert(expr) do { \
  if ((expr) != 0) break; \
  exit (EXIT_FAILURE); \
} while (0)

#define lz32_error(code,...) do { return (int)(code); } while (0)

#endif



/* ----------  ---------- */



LZ32_INLINE int lz32_is_little_endian ( void ) {
  const union { u32t v32; u8t v8x4[4]; } le_test = { .v32 = 1 };
  return ( (le_test.v8x4[0] != 0) ? 1 : 0 );
}

/* ----------  ---------- */

//TODO : Direct memory access !!!

LZ32_INLINE u8t lz32_read8 ( const void* src ) { 
  u8t val;
  memcpy ( &(val), src, 1 );
  return val;
}
LZ32_INLINE u16t lz32_read16 ( const void* src ) { 
  u16t val;
  memcpy ( &(val), src, 2 );
  return val;
}
LZ32_INLINE u32t lz32_read32 ( const void* src ) { 
  u32t val;
  memcpy ( &(val), src, 4 );
  return val;
}
LZ32_INLINE u64t lz32_read64 ( const void* src ) { 
  u64t val;
  memcpy ( &(val), src, 8 );
  return val;
}

/* ----------  ---------- */

LZ32_INLINE void lz32_write32 ( void* dst, u32t val ) { memcpy ( dst, &(val), 4 ); }

/* ----------  ---------- */

LZ32_INLINE void lz32_copy ( void* dst, const void* src, size_t len ) {
  if ((src == dst) || (len == 0)) return;
  if (dst > src) {
    lz32_assert ((size_t)(dst - src) >= 16);
  }
  memcpy ( dst, src, len );
}

/* ----------  ---------- */

LZ32_INLINE void lz32_setbits0 ( void* ptr, size_t len ) { memset ( ptr, 0, len ); }
LZ32_INLINE void lz32_setbits1 ( void* ptr, size_t len ) { memset ( ptr, 255, len ); }

/* ----------  ---------- */

#define lz32_floor16(len) ((size_t)(len) & ((size_t)0 - 16))
#define lz32_ceil16(len) lz32_floor16 ((size_t)(len) + 15)



/* ----------  ---------- */



const char* lz32_error_string ( int res_val ) {
  
  static const char text[16][64] = {
    "success (no errors occurred)",
    "<error 01>",
    "<error 02>",
    "<error 03>",
    "<error 04>",
    "<error 05>",
    "<error 06>",
    "<error 07>",
    "<error 08>",
    "<error 09>",
    "<error 10>",
    "<error 11>",
    "<error 12>",
    "<error 13>",
    "<error 14>",
    "unknown error"
  };
  
  int idx = res_val;
  if ((idx < 0) || (idx > 15)) 
    idx = 15;
  
  return text[idx];
}


/* ----------  ---------- */


int lz32_compress_bound ( size_t* src_len, size_t* dst_len ) {
  
  if (src_len == NULL) 
    lz32_error (LZ32_EINVAL, "a NULL 'src_len' was passed");
  
  size_t raw_len = *(src_len);
  *(src_len) = 0;
  
  if (dst_len == NULL) 
    lz32_error (LZ32_EINVAL, "a NULL 'src_len' was passed");
  
  *(dst_len) = 0;
  
  if (raw_len < LZ32_RAW_SIZE_MIN) 
    lz32_error (LZ32_EINVAL, "too small");
  
  if (raw_len > LZ32_RAW_SIZE_MAX) 
    raw_len = LZ32_RAW_SIZE_MAX;
  
  size_t blk_bnd = lz32_ceil16 (raw_len + 4);
  
  lz32_assert (blk_bnd >= LZ32_BLK_SIZE_MIN);
  lz32_assert (blk_bnd <= LZ32_BLK_SIZE_MAX);
  
  *(src_len) = raw_len;
  *(dst_len) = blk_bnd;
  
  return LZ32_SUCCESS;
}



/* ----------  ---------- */



LZ32_INLINE size_t lz32_count_common_bytes ( u64t xdif ) {
  
  size_t cnt = 0;
  
  if (lz32_is_little_endian () != 0) {
    
#if defined(_MSC_VER) && defined(_WIN64)
    
    _BitScanForward64 ( &(cnt), xdif );
    return (cnt >> 3);
    
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3)))
    
    cnt = __builtin_ctzll (xdif);
    return (cnt >> 3);
    
#else
    
    static const u8t bitcnt_map[64] = { 0, 0, 0, 0, 0, 1, 1, 2, 
                                        0, 3, 1, 3, 1, 4, 2, 7, 
                                        0, 2, 3, 6, 1, 5, 3, 5, 
                                        1, 3, 4, 4, 2, 5, 6, 7, 
                                        7, 0, 1, 2, 3, 3, 4, 6, 
                                        2, 6, 5, 5, 3, 4, 5, 6, 
                                        7, 1, 2, 4, 6, 4, 4, 5, 
                                        7, 2, 6, 5, 7, 6, 7, 7 };
    u64t val = ~(xdif);
    val = (val + 1) & xdif;
    val *= 0x218A392CDABBD3FULL;
    val >>= 58;
    return (size_t)val;

#endif
    
  } else {
    
//    TODO
    
  }
  
  return 0;
}



LZ32_INLINE size_t lz32_count_match_255 ( const void* mtc_ptr, const void* cur_ptr, const void* lim_ptr ) {
  
  lz32_assert (mtc_ptr != NULL);
  lz32_assert (mtc_ptr < cur_ptr);
  lz32_assert (cur_ptr < lim_ptr);
  
  const char* mptr = (const char*)mtc_ptr;
  const char* cptr = (const char*)cur_ptr;
  const char* lptr = (const char*)lim_ptr;
  
  size_t mlim = (size_t)(lptr - cptr);
  if ( likely (mlim > 255) ) mlim = 255;
  size_t mlen = 0;
  
  while (mlim > 7) {
    u64t mbuf = lz32_read64 (mptr);
    u64t cbuf = lz32_read64 (cptr);
    u64t xdif = mbuf ^ cbuf;
    if (xdif != 0) {
      size_t dlen = lz32_count_common_bytes (xdif);
      return (mlen + dlen);
    }
    mptr += 8; cptr += 8;
    mlim -= 8; mlen += 8;
  }
  
  if (mlim > 3) {
    u32t mbuf = lz32_read32 (mptr);
    u32t cbuf = lz32_read32 (cptr);
    if (mbuf == cbuf) {
      mptr += 4; cptr += 4;
      mlim -= 4; mlen += 4;
    }
  }
  
  if (mlim > 1) {
    u16t mbuf = lz32_read16 (mptr);
    u16t cbuf = lz32_read16 (cptr);
    if (mbuf == cbuf) {
      mptr += 2; cptr += 2;
      mlim -= 2; mlen += 2;
    }
  }
  
  if (mlim != 0) {
    u8t mbuf = lz32_read8 (mptr);
    u8t cbuf = lz32_read8 (cptr);
    if (mbuf == cbuf) {
      mlen += 1;
    }
  }
  
  return mlen;
}



/* ----------  ---------- */



#define LZ32_HASH40_PRIME 0xF78DBDB1EFULL
#define LZ32_HASH40_MULTIPLIER ((u64t)LZ32_HASH40_PRIME << 12)

LZ32_INLINE size_t hash_40 ( u64t seq, int width ) {
  u64t hash = seq << 12;
  hash *= LZ32_HASH40_MULTIPLIER;
  hash >>= (64 - width);
  return (size_t)hash;
}


LZ32_INLINE u32t lz32_encode_token ( size_t lit_len, size_t mtc_len, size_t mtc_off ) {
  lz32_assert (lit_len < 256);
  lz32_assert (mtc_len < 256);
  lz32_assert (mtc_off < 65536);
  lz32_assert ( (mtc_off == 0) ? (mtc_len == 0) : (mtc_len > 4) );
  u32t tkn = (u32t)mtc_off; tkn <<= 8;
  tkn |= (u32t)mtc_len; tkn <<= 8;
  tkn |= (u32t)lit_len; return tkn;
}



/* ----------  ---------- */



#define LZ32_RAW_SIZE_PROC_MIN (1ULL << 8)
#define LZ32_BLK_SIZE_PROC_MIN (1ULL << 6)

#define LZ32_COMPR_LEVEL_UNSET 0
#define LZ32_COMPR_LEVEL_MIN 1
#define LZ32_COMPR_LEVEL_MAX 9
#define LZ32_COMPR_LEVEL_HIGH 4

#define LZ32_HTB_LOG_FAST 14
#define LZ32_HTB_LOG_HIGH 15

#define LZ32_WINDOW_LOG_FAST 16
#define LZ32_WINDOW_LOG_HIGH 16

#define LZ32_HTB_NOMATCH (u32t)0xFFFFFFFFU
#define LZ32_CTB_NOMATCH (u16t)0xFFFF





/* ---------- MEMORY COMPRESSION ---------- */


/* ---------- Internal compression sub-routine for fast-compress algorithm ---------- */


LZ32_INLINE size_t lz32_compress_internal_balanced 
      ( const void* src_ptr, size_t src_cap, 
              void* dst_ptr, size_t dst_cap, 
           size_t* head_len, size_t* tail_len ) 
{
  
/* -----  ----- */
  
  lz32_assert (src_ptr != NULL);
  
  lz32_assert (src_cap >= LZ32_RAW_SIZE_MIN);
  lz32_assert (src_cap <= LZ32_RAW_SIZE_MAX);
  lz32_assert (src_cap >= LZ32_RAW_SIZE_PROC_MIN);
  
  lz32_assert (dst_ptr != NULL);
  lz32_assert ( ((size_t)dst_ptr & 3) == 0 );
  
  lz32_assert (dst_cap >= LZ32_BLK_SIZE_MIN);
  lz32_assert (dst_cap <= LZ32_BLK_SIZE_MAX);
  lz32_assert (dst_cap >= LZ32_BLK_SIZE_PROC_MIN);
  lz32_assert ( (dst_cap & 15) == 0 );
  
  lz32_assert (head_len != NULL);
  lz32_assert ( *(head_len) == 0 );
  
  lz32_assert (tail_len != NULL);
  lz32_assert ( *(tail_len) == 0 );
  
  lz32_assert ( (size_t)src_ptr != (size_t)dst_ptr );
  lz32_assert ( ((size_t)src_ptr < (size_t)dst_ptr) 
              ? (((size_t)src_ptr + src_cap) <= (size_t)dst_ptr) 
              : (((size_t)dst_ptr + dst_cap) <= (size_t)src_ptr) );
  
/* -----  ----- */
  
  size_t htb_bsize = 4;
  htb_bsize <<= LZ32_HTB_LOG_FAST;
  char htb_buf[htb_bsize];
  lz32_setbits1 ( htb_buf, htb_bsize );
  u32t* htb_ptr = (u32t*)htb_buf;
  
/* -----  ----- */
  
  const char* inp_beg = (const char*)src_ptr;
  const char* inp_end = (const char*)src_ptr + src_cap;
  const char* inp_lit = inp_beg;
  const char* inp_cur = inp_beg;
  const char* inp_lim = inp_end - 15;
  
  char* out_beg = (char*)dst_ptr;
  char* out_end = (char*)dst_ptr + dst_cap;
  char* out_lit = out_beg;
  char* out_tkn = out_end;
  char* out_bnd;
  
/* -----  ----- */
  
  size_t off_lim = (size_t)1 << LZ32_WINDOW_LOG_FAST;
  size_t cur_pos = 0, mtc_pos, htb_idx;
  size_t lit_len, mtc_len, mtc_off;
  size_t upd_cnt = 0, upd_idx[4];
  u64t cur_seq;
  u32t cur_tkn, htb_prev, htb_next;
  
/* -----  ----- */
  
  out_tkn -= 4;
  lz32_write32 (out_tkn, 0);
  
  while (inp_cur < inp_lim) {
    
/* -----  ----- */
    
    lz32_assert (inp_cur >= inp_lit);
    lit_len = (size_t)(inp_cur - inp_lit);
    lz32_assert (lit_len <= 256);
    
    out_bnd = out_lit + (lit_len + 15);
    if ( unlikely (out_bnd > out_tkn) ) break;
    
/* -----  ----- */
    
    if ( unlikely (lit_len == 256) ) {
      
      lz32_copy ( out_lit, inp_lit, 256 );
      inp_lit += 255; out_lit += 255;
      
      cur_tkn = lz32_encode_token (255, 0, 0);
      lz32_write32 (out_tkn, cur_tkn);
      
      out_tkn -= 4;
      lz32_write32 (out_tkn, 0);
      
      lit_len -= 255;
    }
    
/* -----  ----- */
    
    cur_seq = lz32_read64 (inp_cur);
    htb_idx = hash_40 (cur_seq, LZ32_HTB_LOG_FAST);
    
    htb_prev = htb_ptr[htb_idx];
    htb_next = (u32t)cur_pos;
    
    htb_ptr[htb_idx] = htb_next;
    
/* -----  ----- */
    
    mtc_len = 0;
    
    if (htb_prev != LZ32_HTB_NOMATCH) {
      
      mtc_pos = htb_prev;
      lz32_assert (mtc_pos < cur_pos);
      
      mtc_off = cur_pos - mtc_pos;
      
      if (mtc_off < off_lim) {
        mtc_len = lz32_count_match_255 ( (inp_beg + mtc_pos), inp_cur, inp_lim );
      }
    }
    
/* -----  ----- */
    
    if (mtc_len > 4) {
      
      out_bnd = out_lit + (lit_len + mtc_len + 15);
      if ( unlikely (out_bnd > out_tkn) ) break;
      
      lz32_copy ( out_lit, inp_lit, lz32_ceil16 (lit_len) );
      inp_lit += lit_len; out_lit += lit_len;
      
      inp_lit += mtc_len;
      
      cur_tkn = lz32_encode_token (lit_len, mtc_len, mtc_off);
      lz32_write32 (out_tkn, cur_tkn);
      
      out_tkn -= 4;
      lz32_write32 (out_tkn, 0);
      
/* -----  ----- */
      
      upd_cnt = mtc_len - 1;
      
      while (upd_cnt > 3) {
        
        cur_seq = lz32_read64 (inp_cur + 1);
        inp_cur += 4;
        
        upd_idx[0] = hash_40 (cur_seq, LZ32_HTB_LOG_FAST); cur_seq >>= 8;
        upd_idx[1] = hash_40 (cur_seq, LZ32_HTB_LOG_FAST); cur_seq >>= 8;
        upd_idx[2] = hash_40 (cur_seq, LZ32_HTB_LOG_FAST); cur_seq >>= 8;
        upd_idx[3] = hash_40 (cur_seq, LZ32_HTB_LOG_FAST);
        
        htb_ptr[upd_idx[0]] = (u32t)(cur_pos + 1);
        htb_ptr[upd_idx[1]] = (u32t)(cur_pos + 2);
        htb_ptr[upd_idx[2]] = (u32t)(cur_pos + 3);
        htb_ptr[upd_idx[3]] = (u32t)(cur_pos + 4);
        cur_pos += 4;
        
        upd_cnt -= 4;
      }
      
      while (upd_cnt != 0) {
        
        inp_cur += 1;
        cur_seq = lz32_read64 (inp_cur);
        
        htb_idx = hash_40 (cur_seq, LZ32_HTB_LOG_FAST);
        
        cur_pos += 1; htb_next = (u32t)cur_pos;
        htb_ptr[htb_idx] = htb_next;
        
        upd_cnt -= 1;
      }
      
    }
    
/* -----  ----- */
    
    inp_cur += 1;
    cur_pos += 1;
  }
  
/* -----  ----- */
  
  size_t head_len_val = (size_t)(out_lit - out_beg);
  *(head_len) = head_len_val;
  
  size_t tail_len_val = (size_t)(out_end - out_tkn);
  *(tail_len) = tail_len_val;
  
  size_t inp_len_val = (size_t)(inp_lit - inp_beg);
  
  return inp_len_val;
}


/* ---------- Internal compression sub-routine for high-ratio algorithm ---------- */


LZ32_INLINE size_t lz32_compress_internal_highcompress 
      ( const void* src_ptr, size_t src_cap, 
              void* dst_ptr, size_t dst_cap, 
           size_t* head_len, size_t* tail_len ) 
{
  
/* -----  ----- */
  
  lz32_assert (src_ptr != NULL);
  
  lz32_assert (src_cap >= LZ32_RAW_SIZE_MIN);
  lz32_assert (src_cap <= LZ32_RAW_SIZE_MAX);
  lz32_assert (src_cap >= LZ32_RAW_SIZE_PROC_MIN);
  
  lz32_assert (dst_ptr != NULL);
  lz32_assert ( ((size_t)dst_ptr & 3) == 0 );
  
  lz32_assert (dst_cap >= LZ32_BLK_SIZE_MIN);
  lz32_assert (dst_cap <= LZ32_BLK_SIZE_MAX);
  lz32_assert (dst_cap >= LZ32_BLK_SIZE_PROC_MIN);
  lz32_assert ( (dst_cap & 15) == 0 );
  
  lz32_assert (head_len != NULL);
  lz32_assert ( *(head_len) == 0 );
  
  lz32_assert (tail_len != NULL);
  lz32_assert ( *(tail_len) == 0 );
  
  lz32_assert ( (size_t)src_ptr != (size_t)dst_ptr );
  lz32_assert ( ((size_t)src_ptr < (size_t)dst_ptr) 
              ? (((size_t)src_ptr + src_cap) <= (size_t)dst_ptr) 
              : (((size_t)dst_ptr + dst_cap) <= (size_t)src_ptr) );
  
/* -----  ----- */
  
  size_t htb_bsize = 4;
  htb_bsize <<= LZ32_HTB_LOG_HIGH;
  char htb_buf[htb_bsize];
  lz32_setbits1 ( htb_buf, htb_bsize );
  u32t* htb_ptr = (u32t*)htb_buf;
  
  size_t ctb_bsize = 2;
  ctb_bsize <<= LZ32_WINDOW_LOG_HIGH;
  char ctb_buf[ctb_bsize];
  lz32_setbits1 ( ctb_buf, ctb_bsize );
  u16t* ctb_ptr = (u16t*)ctb_buf;
  
/* -----  ----- */
  
  const char* inp_beg = (const char*)src_ptr;
  const char* inp_end = (const char*)src_ptr + src_cap;
  const char* inp_lit = inp_beg;
  const char* inp_cur = inp_beg;
  const char* inp_lim = inp_end - 15;
  const char* inp_mtc;
  
  char* out_beg = (char*)dst_ptr;
  char* out_end = (char*)dst_ptr + dst_cap;
  char* out_lit = out_beg;
  char* out_tkn = out_end;
  char* out_bnd;
  
/* -----  ----- */
  
  size_t off_lim = (size_t)1 << LZ32_WINDOW_LOG_HIGH;
  size_t cur_pos = 0, mtc_pos, upd_cnt = 0;
  size_t htb_idx, ctb_idx, mtc_idx;
  size_t lit_len, mtc_len, mtc_off;
  size_t cur_mtc, cur_off, ctb_dist;
  u64t cur_seq;
  u32t cur_tkn, htb_prev, htb_next;
  u16t ctb_next, ctb_prev;
  
/* -----  ----- */
  
  out_tkn -= 4;
  lz32_write32 (out_tkn, 0);
  
  while (inp_cur < inp_lim) {
    
/* -----  ----- */
    
    lz32_assert (inp_cur >= inp_lit);
    lit_len = (size_t)(inp_cur - inp_lit);
    lz32_assert (lit_len <= 256);
    
    out_bnd = out_lit + (lit_len + 15);
    if ( unlikely (out_bnd > out_tkn) ) break;
    
/* -----  ----- */
    
    if ( unlikely (lit_len == 256) ) {
      
      lz32_copy ( out_lit, inp_lit, 256 );
      inp_lit += 255; out_lit += 255;
      
      cur_tkn = lz32_encode_token (255, 0, 0);
      lz32_write32 (out_tkn, cur_tkn);
      
      out_tkn -= 4;
      lz32_write32 (out_tkn, 0);
      
      lit_len -= 255;
    }
    
/* -----  ----- */
    
    cur_seq = lz32_read64 (inp_cur);
    htb_idx = hash_40 (cur_seq, LZ32_HTB_LOG_HIGH);
    
    htb_prev = htb_ptr[htb_idx];
    htb_next = (u32t)cur_pos;
    
    htb_ptr[htb_idx] = htb_next;
    
    ctb_idx = cur_pos % off_lim;
    
    ctb_next = LZ32_CTB_NOMATCH;
    
/* -----  ----- */
    
    mtc_len = 0;
    
    if (htb_prev != LZ32_HTB_NOMATCH) {
      
      mtc_pos = htb_prev;
      lz32_assert (mtc_pos < cur_pos);
      cur_off = cur_pos - mtc_pos;
      
      if (cur_off < off_lim) 
        ctb_next = (u16t)cur_off;
    
/* -----  ----- */
      
      inp_mtc = inp_beg + mtc_pos;
      
      while (cur_off < off_lim) {
        
        cur_mtc = lz32_count_match_255 (inp_mtc, inp_cur, inp_lim);
        
        if (cur_mtc > mtc_len) {
          mtc_len = cur_mtc;
          mtc_off = cur_off;
        }
    
/* -----  ----- */
        
        mtc_idx = mtc_pos % off_lim;
        ctb_prev = ctb_ptr[mtc_idx];
        
        if (ctb_prev == LZ32_CTB_NOMATCH) break;
        
        ctb_dist = ctb_prev;
        inp_mtc -= ctb_dist;
        mtc_pos -= ctb_dist;
        
        cur_off += ctb_dist;
      }
    }
    
    ctb_ptr[ctb_idx] = ctb_next;
    
/* -----  ----- */
    
    if (mtc_len > 4) {
      
      out_bnd = out_lit + (lit_len + mtc_len + 15);
      if ( unlikely (out_bnd > out_tkn) ) break;
      
      lz32_copy ( out_lit, inp_lit, lz32_ceil16 (lit_len) );
      inp_lit += lit_len; out_lit += lit_len;
      
      inp_lit += mtc_len;
      
      cur_tkn = lz32_encode_token (lit_len, mtc_len, mtc_off);
      lz32_write32 (out_tkn, cur_tkn);
      
      out_tkn -= 4;
      lz32_write32 (out_tkn, 0);
      
/* -----  ----- */
      
      upd_cnt = mtc_len - 1;
      
//      TODO : while (upd_cnt > 3) 
      
      while (upd_cnt != 0) {
        
        inp_cur += 1;
        cur_seq = lz32_read64 (inp_cur);
        
        htb_idx = hash_40 (cur_seq, LZ32_HTB_LOG_HIGH);
        htb_prev = htb_ptr[htb_idx];
        
        cur_pos += 1;
        htb_next = (u32t)cur_pos;
        htb_ptr[htb_idx] = htb_next;
        
        ctb_next = LZ32_CTB_NOMATCH;
        ctb_idx = cur_pos % off_lim;
        
        if (htb_prev != LZ32_HTB_NOMATCH) {
          
          mtc_pos = htb_prev;
          lz32_assert (mtc_pos < cur_pos);
          cur_off = cur_pos - mtc_pos;
          
          if (cur_off < off_lim) {
            ctb_next = (u16t)cur_off;
          }
        }
        
        ctb_ptr[ctb_idx] = ctb_next;
        
        upd_cnt -= 1;
      }
    }
    
/* -----  ----- */
    
    inp_cur += 1;
    cur_pos += 1;
  }
  
/* -----  ----- */
  
  size_t head_len_val = (size_t)(out_lit - out_beg);
  *(head_len) = head_len_val;
  
  size_t tail_len_val = (size_t)(out_end - out_tkn);
  *(tail_len) = tail_len_val;
  
  size_t inp_len_val = (size_t)(inp_lit - inp_beg);
  
  return inp_len_val;
}


/* ---------- Internal compression routine ---------- */


LZ32_INLINE int lz32_compress_internal 
      ( const void* src_ptr, size_t src_cap, size_t* src_len, 
              void* dst_ptr, size_t dst_cap, size_t* dst_len, int cmr_lvl ) 
{
  
/* -----  ----- */
  
  lz32_assert (src_ptr != NULL);
  
  lz32_assert (src_cap >= LZ32_RAW_SIZE_MIN);
  lz32_assert (src_cap <= LZ32_LENGTH_MAX);
  
  lz32_assert (src_len != NULL);
  lz32_assert ( *(src_len) == 0 );
  
  lz32_assert (dst_ptr != NULL);
  lz32_assert ( ((size_t)dst_ptr & 3) == 0 );
  
  lz32_assert (dst_cap >= LZ32_BLK_SIZE_MIN);
  lz32_assert (dst_cap <= LZ32_LENGTH_MAX);
  
  lz32_assert (dst_len != NULL);
  lz32_assert ( *(dst_len) == 0 );
  
  lz32_assert ( (cmr_lvl == LZ32_COMPR_LEVEL_UNSET) || 
               ((cmr_lvl >= LZ32_COMPR_LEVEL_MIN) && (cmr_lvl <= LZ32_COMPR_LEVEL_MAX)) );
  
/* -----  ----- */
  
  const char* sptr = (const char*)src_ptr;
  
  size_t scap = src_cap;
  if (scap > LZ32_RAW_SIZE_MAX) scap = LZ32_RAW_SIZE_MAX;
  
  size_t slen = 0;
  
  char* dptr = (char*)dst_ptr;
  
  size_t dcap = lz32_floor16 (dst_cap);
  if (dcap > LZ32_BLK_SIZE_MAX) dcap = lz32_floor16 (LZ32_BLK_SIZE_MAX);
  
  size_t dlen = 0;
  
  int calg = 5;
  if ( (cmr_lvl >= LZ32_COMPR_LEVEL_MIN) && (cmr_lvl <= LZ32_COMPR_LEVEL_MAX) ) {
    if (cmr_lvl >= LZ32_COMPR_LEVEL_HIGH) calg = 9;
  }
  if ( (scap < LZ32_RAW_SIZE_PROC_MIN) || (dcap < LZ32_BLK_SIZE_PROC_MIN) ) calg = 1;
  
/* -----  ----- */
  
//  TODO : UNIFIED RAW COMPRESSION !!!!!
  
  size_t rlen, tlen, plen, mlen;
  size_t hlen = 0, flen = 0;
  
/* -----  ----- */
  
  if (calg == 5) {
    rlen = lz32_compress_internal_balanced ( sptr, scap, dptr, dcap, &(hlen), &(flen) );
  }
  
  if (calg == 9) {
    rlen = lz32_compress_internal_highcompress ( sptr, scap, dptr, dcap, &(hlen), &(flen) );
  }
  
/* -----  ----- */
  
  if (rlen != 0) {
    
    plen = dcap - (hlen + flen);
    tlen = scap - rlen;
    if (tlen > plen) tlen = plen;
    
    if (tlen != 0) {
      lz32_copy ( (dptr + hlen), (sptr + rlen), tlen );
      plen -= tlen;
    }
    
    mlen = lz32_floor16 (plen);
    plen -= mlen;
    if (plen != 0) {
      lz32_setbits0 ( (dptr + hlen + tlen), plen );
    }
    
    if (mlen != 0) {
      lz32_copy ( (dptr + (hlen + tlen + plen)), (dptr + (dcap - flen)), flen );
    }
    
  }
  
/* -----  ----- */
  
  slen = rlen + tlen;
  dlen = hlen + tlen + plen + flen;
  
  if ((slen + 4) < dlen) calg = 1;
  
  if (calg == 1) {
    
    slen = dcap - 4;
    if (scap < slen) slen = scap;
    
    lz32_copy ( dptr, sptr, slen );
    
    dlen = lz32_ceil16 (slen + 4); // TODO : ASSERT !!!
    
    lz32_setbits0 ( (dptr + slen), (dlen - slen) );
  }
  
/* -----  ----- */
  
  *(src_len) = slen;
  *(dst_len) = dlen;
  
  return 0;
}






/* ---------- MEMORY DECOMPRESSION ---------- */

/* TODO: 1) copy512_by8() - better algorithm ??? */

/* ---------- Internal decompression routine ---------- */


LZ32_INLINE int lz32_decompress_internal 
      ( const void* src_ptr, size_t src_len, void* dst_ptr, size_t dst_len, const int safe_flag ) 
{
  
/* -----  ----- */
  
  lz32_assert (src_ptr != NULL);
  lz32_assert (((size_t)src_ptr & 3) == 0);
  
  lz32_assert (src_len >= LZ32_BLK_SIZE_MIN);
  lz32_assert (src_len <= LZ32_BLK_SIZE_MAX);
  lz32_assert ((src_len & 15) == 0);
  
  lz32_assert (dst_ptr != NULL);
  
  lz32_assert (dst_len >= LZ32_RAW_SIZE_MIN);
  lz32_assert (dst_len <= LZ32_RAW_SIZE_MAX);
  
  lz32_assert ((size_t)src_ptr != (size_t)dst_ptr);
  lz32_assert ( ((size_t)src_ptr < (size_t)dst_ptr) 
              ? (((size_t)src_ptr + src_len) < (size_t)dst_ptr) 
              : (((size_t)dst_ptr + dst_len) < (size_t)src_ptr) );
  
/* -----  ----- */
  
  static const size_t off_map[16] = {
     0, 16, 16, 18, 16, 20, 18, 21, 
    16, 18, 20, 22, 24, 26, 28, 30, 
  };
  
/* -----  ----- */
  
  const char* const inp_beg = (const char*)src_ptr;
  const char* const inp_end = (const char*)src_ptr + src_len;
  const char* inp_lit = inp_beg;
  const char* inp_tkn = inp_end;
  const char* inp_tmp;
  
  char* const out_beg = (char*)dst_ptr;
  char* const out_end = (char*)dst_ptr + dst_len;
  char* out_cur = out_beg;
  char* out_tmp;
  
  size_t lit_len, mtc_len, mtc_off;
  size_t head_len, tail_len;
  size_t inp_bnd, out_bnd, off_bnd;
  u32t cur_tkn;
  
/* -----  ----- */
  
  inp_tkn -= 4;
  cur_tkn = lz32_read32 (inp_tkn);
  
  while (cur_tkn != 0) {
    
/* -----  ----- */
    
    if (lz32_is_little_endian () != 0) {
      
      lit_len = (size_t)((cur_tkn & 0x000000FFU) >>  0);
      mtc_len = (size_t)((cur_tkn & 0x0000FF00U) >>  8);
      mtc_off = (size_t)((cur_tkn & 0xFFFF0000U) >> 16);
      
    } else {
      
      lit_len = (size_t)((cur_tkn & 0xFF000000U) >> 24);
      mtc_len = (size_t)((cur_tkn & 0x00FF0000U) >> 16);
      mtc_off = (size_t)(((cur_tkn & 0x0000FF00U) >> 8) + ((cur_tkn & 0x000000FFU) << 8));
      
    }
    
//    lz32_debug ( "TK[%zu] = { LL=%zu, ML=%zu, OF=%zu }", \
                 ((size_t)(inp_end - inp_tkn) >> 2), lit_len, mtc_len, mtc_off );
    
/* -----  ----- */
    
    if (safe_flag != 0) {
      
      lz32_assert (lit_len < 256);
      lz32_assert (mtc_len < 256);
      lz32_assert (mtc_off < 65536);
      if (mtc_off != 0) {
        if ( unlikely (mtc_len < 5) ) return 2;
      } else {
        if ( unlikely (mtc_len != 0) ) return 2;
      }
      
      lz32_assert (inp_beg <= inp_lit);
      lz32_assert (inp_lit <= inp_tkn);
      lz32_assert (inp_tkn <= inp_end);
      inp_bnd = (size_t)(inp_tkn - inp_lit);
      if ( unlikely ((lit_len + 4) > inp_bnd) ) return 3;
      
      lz32_assert (out_beg <= out_cur);
      off_bnd = (size_t)(out_cur - out_beg);
      if ( unlikely ((off_bnd + lit_len) < mtc_off) ) return 3;
      lz32_assert (out_cur <= out_end);
      out_bnd = (size_t)(out_end - out_cur);
      if ( unlikely ((lit_len + mtc_len) > out_bnd) ) return 3;
      
    }
    
/* -----  ----- */
    
    inp_tmp = inp_lit; inp_lit += lit_len;
    out_tmp = out_cur; out_cur += lit_len;
    
    lz32_copy ( out_tmp, inp_tmp, 16 );
    inp_tmp += 16; out_tmp += 16;
    
    while (inp_tmp < inp_lit) {
      lz32_copy ( out_tmp, inp_tmp, 16 );
      inp_tmp += 16; out_tmp += 16;
    }
    
/* -----  ----- */
    
    inp_tmp = out_cur - mtc_off;
    out_tmp = out_cur;
    
    out_cur += mtc_len;
    
/* -----  ----- */
    
    if (mtc_off < 16) {
      
      *(out_tmp +  0) = *(inp_tmp +  0); *(out_tmp +  1) = *(inp_tmp +  1);
      *(out_tmp +  2) = *(inp_tmp +  2); *(out_tmp +  3) = *(inp_tmp +  3);
      *(out_tmp +  4) = *(inp_tmp +  4); *(out_tmp +  5) = *(inp_tmp +  5);
      *(out_tmp +  6) = *(inp_tmp +  6); *(out_tmp +  7) = *(inp_tmp +  7);
      *(out_tmp +  8) = *(inp_tmp +  8); *(out_tmp +  9) = *(inp_tmp +  9);
      *(out_tmp + 10) = *(inp_tmp + 10); *(out_tmp + 11) = *(inp_tmp + 11);
      *(out_tmp + 12) = *(inp_tmp + 12); *(out_tmp + 13) = *(inp_tmp + 13);
      *(out_tmp + 14) = *(inp_tmp + 14); *(out_tmp + 15) = *(inp_tmp + 15);
      
      out_tmp += 16;
      
      inp_tmp = out_tmp - off_map[mtc_off];
      
    } else {
      
      lz32_copy ( out_tmp, inp_tmp, 16 );
      
      inp_tmp += 16;
      out_tmp += 16;
    }
    
/* -----  ----- */
    
    while (out_tmp < out_cur) {
      
      lz32_copy ( out_tmp, inp_tmp, 16 ); // TODO : MUST BE BY 16 ??!!!
      
      inp_tmp += 16;
      out_tmp += 16;
    }
    
/* -----  ----- */
    
    inp_tkn -= 4;
    cur_tkn = lz32_read32 (inp_tkn);
  }
  
/* -----  ----- */
  
  lz32_assert (out_beg <= out_cur);
  lz32_assert (out_cur <= out_end);
  head_len = (size_t)(out_cur - out_beg);
  
  lz32_assert (head_len <= dst_len);
  tail_len = dst_len - head_len;
  
  lz32_assert (inp_beg <= inp_lit);
  lz32_assert (inp_lit <= inp_tkn);
  lz32_assert (inp_tkn <= inp_end);
  inp_bnd = (size_t)(inp_tkn - inp_lit);
  
  if (tail_len > inp_bnd) return 1;
  
/* -----  ----- */
  
  lz32_copy ( out_cur, inp_lit, tail_len );
  
  return 0;
}


/* ---------- MEMORY COMPRESS/DECOMPRESS INTERFACES ---------- */

/* ---------- Fast (low) memory compression interface ---------- */


int lz32_compress_fast ( const void* src_ptr, size_t* src_len, void* dst_ptr, size_t* dst_len ) {
  
/* -----  ----- */
  
  if (src_ptr == NULL) lz32_error (LZ32_EINVAL, "lz32_compress_fast(): ");
  if (src_len == NULL) lz32_error (LZ32_EINVAL, "lz32_compress_fast(): ");
  if (dst_ptr == NULL) lz32_error (LZ32_EINVAL, "lz32_compress_fast(): ");
  if (dst_len == NULL) lz32_error (LZ32_EINVAL, "lz32_compress_fast(): ");
  
/* -----  ----- */
  
  const char* sptr = (const char*)src_ptr;
  size_t scap = *(src_len);
  size_t slen = 0;
  *(src_len) = slen;
  
  char* dptr = (char*)dst_ptr;
  size_t dcap = *(dst_len);
  size_t dlen = 0;
  *(dst_len) = dlen;
  
/* -----  ----- */
  
  if (scap > LZ32_RAW_SIZE_MAX) scap = LZ32_RAW_SIZE_MAX;
  if (scap < LZ32_RAW_SIZE_MIN) lz32_error (LZ32_EINVAL, "lz32_compress_fast(): ");
  
  if (((size_t)dptr & 3) != 0) lz32_error (LZ32_EINVAL, "lz32_compress_fast(): ");
  
  dcap = lz32_floor16 (dcap);
  if (dcap > LZ32_BLK_SIZE_MAX) dcap = lz32_floor16 (LZ32_BLK_SIZE_MAX);
  if (dcap < LZ32_BLK_SIZE_MIN) lz32_error (LZ32_EINVAL, "lz32_compress_fast(): ");
  
/* -----  ----- */
  
  int res = lz32_compress_internal ( sptr, scap, &(slen), dptr, dcap, &(dlen), 1 );
  
  switch (res) {
    case 0: break;
//    TODO
    default: return LZ32_EUNKNOWN;
  }
  
/* -----  ----- */
  
  *(src_len) = slen;
  *(dst_len) = dlen;
  
  return LZ32_SUCCESS;
}


/* ---------- High (slow) memory compression interface ---------- */


int lz32_compress_high ( const void* src_ptr, size_t* src_len, void* dst_ptr, size_t* dst_len ) {
  
/* -----  ----- */
  
  if (src_ptr == NULL) lz32_error (LZ32_EINVAL, "lz32_compress_high(): ");
  if (src_len == NULL) lz32_error (LZ32_EINVAL, "lz32_compress_high(): ");
  if (dst_ptr == NULL) lz32_error (LZ32_EINVAL, "lz32_compress_high(): ");
  if (dst_len == NULL) lz32_error (LZ32_EINVAL, "lz32_compress_high(): ");
  
/* -----  ----- */
  
  const char* sptr = (const char*)src_ptr;
  size_t scap = *(src_len);
  size_t slen = 0;
  *(src_len) = slen;
  
  char* dptr = (char*)dst_ptr;
  size_t dcap = *(dst_len);
  size_t dlen = 0;
  *(dst_len) = dlen;
  
/* -----  ----- */
  
  if (scap > LZ32_RAW_SIZE_MAX) scap = LZ32_RAW_SIZE_MAX;
  if (scap < LZ32_RAW_SIZE_MIN) lz32_error (LZ32_EINVAL, "lz32_compress_high(): ");
  
  if (((size_t)dptr & 3) != 0) lz32_error (LZ32_EINVAL, "lz32_compress_high(): ");
  
  dcap = lz32_floor16 (dcap);
  if (dcap > LZ32_BLK_SIZE_MAX) dcap = lz32_floor16 (LZ32_BLK_SIZE_MAX);
  if (dcap < LZ32_BLK_SIZE_MIN) lz32_error (LZ32_EINVAL, "lz32_compress_high(): ");
  
/* -----  ----- */
  
  int res = lz32_compress_internal ( sptr, scap, &(slen), dptr, dcap, &(dlen), 9 );
  
  switch (res) {
    case 0: break;
//    TODO
    default: return LZ32_EUNKNOWN;
  }
  
/* -----  ----- */
  
  *(src_len) = slen;
  *(dst_len) = dlen;
  
  return LZ32_SUCCESS;
}


/* ---------- Fast (unsafe) memory decompression interface ---------- */


int lz32_decompress_fast ( const void* src_ptr, size_t src_len, void* dst_ptr, size_t dst_len ) {
  
/* -----  ----- */
  
  const char* sptr = NULL;
  if (src_ptr != NULL) {
    if (((size_t)src_ptr & 3) == 0) {
      sptr = (const char*)src_ptr;
    }
  }
  if (sptr == NULL) lz32_error (LZ32_EINVAL, "lz32_decompress_fast(): invalid value of 'src_ptr' parameter: NULL");
  
  size_t slen = 0;
  if ((src_len >= LZ32_BLK_SIZE_MIN) && (src_len <= LZ32_BLK_SIZE_MAX)) {
    if ((src_len & 15) == 0) slen = src_len;
  }
  if (slen == 0) lz32_error (LZ32_EINVAL, "lz32_decompress_fast(): ");
  
  char* dptr = NULL;
  if (dst_ptr != NULL) {
    dptr = (char*)dst_ptr;
  }
  if (dptr == NULL) lz32_error (LZ32_EINVAL, "lz32_decompress_fast(): ");
  
  size_t dlen = 0;
  if ((dst_len >= LZ32_RAW_SIZE_MIN) && (dst_len <= LZ32_RAW_SIZE_MAX)) {
    if (dst_len >= (src_len - 4)) dlen = dst_len;
  }
  
/* -----  ----- */
  
  int res = lz32_decompress_internal ( sptr, slen, dptr, dlen, 0 );
  
/* -----  ----- */
  
  switch (res) {
    case 0: return LZ32_SUCCESS;
    case 1: lz32_error (LZ32_EDATA, "lz32_decompress_fast: decompression stream overlap"); // TODO : "...'s" ???
    case 2: lz32_error (LZ32_EDATA, "lz32_decompress_fast: invalid sequence token");
    case 3: lz32_error (LZ32_EDATA, "lz32_decompress_fast: data copy overlap");
  }
  
  return LZ32_EUNKNOWN;
}


/* ---------- Safe (slow) memory decompression interface ---------- */


int lz32_decompress_safe ( const void* src_ptr, size_t src_len, void* dst_ptr, size_t dst_len ) {
  
/* -----  ----- */
  
  const char* sptr = NULL;
  if (src_ptr != NULL) {
    if (((size_t)src_ptr & 3) == 0) {
      sptr = (const char*)src_ptr;
    }
  }
  if (sptr == NULL) lz32_error (LZ32_EINVAL, "lz32_decompress_safe(): invalid value of 'src_ptr' parameter: NULL");
  
  size_t slen = 0;
  if ((src_len >= LZ32_BLK_SIZE_MIN) && (src_len <= LZ32_BLK_SIZE_MAX)) {
    if ((src_len & 15) == 0) slen = src_len;
  }
  if (slen == 0) lz32_error (LZ32_EINVAL, "lz32_decompress_safe(): ");
  
  char* dptr = NULL;
  if (dst_ptr != NULL) {
    dptr = (char*)dst_ptr;
  }
  if (dptr == NULL) lz32_error (LZ32_EINVAL, "lz32_decompress_safe(): ");
  
  size_t dlen = 0;
  if ((dst_len >= LZ32_RAW_SIZE_MIN) && (dst_len <= LZ32_RAW_SIZE_MAX)) {
    if (dst_len >= (src_len - 4)) dlen = dst_len;
  }
  
/* -----  ----- */
  
  int res = lz32_decompress_internal ( sptr, slen, dptr, dlen, 1 );
  
/* -----  ----- */
  
  switch (res) {
    case 0: return LZ32_SUCCESS;
    case 1: lz32_error (LZ32_EDATA, "lz32_decompress_safe(): decompression stream overlap"); // TODO : "...'s" ???
    case 2: lz32_error (LZ32_EDATA, "lz32_decompress_safe(): invalid sequence token");
    case 3: lz32_error (LZ32_EDATA, "lz32_decompress_safe(): data copy overlap");
  }
  
  return LZ32_EUNKNOWN;
}



/* ---------- DATA COMPRESS/DECOMPRESS INTERFACES ---------- */

#define LZ32D_MAGIC_NUMBER 0xCDF69D2DU

#define LZ32D_RAW_SIZE_MIN 1
#define LZ32D_RAW_SIZE_MAX ((1 << 30) - 20)
#define LZ32D_BLK_SIZE_MIN 32
#define LZ32D_BLK_SIZE_MAX (1 << 30)

/* ----------  ---------- */

int lz32d_compress_bound ( size_t* src_len, size_t* dst_len ) {
  
/* -----  ----- */
  
  if (src_len == NULL) {
    
    return LZ32_EINVAL;
  }
  size_t scap = *(src_len);
  size_t slen = 0;
  *(src_len) = slen;
  
  if (dst_len == NULL) {
    
    return LZ32_EINVAL;
  }
  size_t dcap = *(dst_len);
  size_t dlen = 0;
  *(dst_len) = dlen;
  
  if (scap > LZ32D_RAW_SIZE_MAX) 
    scap = LZ32D_RAW_SIZE_MAX;
  if (scap < LZ32D_RAW_SIZE_MIN) {
    
    return LZ32_EINVAL;
  }
  
  dcap = lz32_floor16 (dcap);
  if (dcap > LZ32D_BLK_SIZE_MAX) 
    dcap = lz32_floor16 (LZ32D_BLK_SIZE_MAX);
  if (dcap < LZ32D_BLK_SIZE_MIN) {
    
    return LZ32_EINVAL;
  }
  
  size_t bnd = dcap - 20;
  if (scap <= bnd) slen = scap;
  else slen = bnd;
  dlen = lz32_ceil16 (slen + 20);
  
  *(src_len) = slen;
  *(dst_len) = dlen;
  
  return LZ32_SUCCESS;
}

/* ---------- Fast (low) data compression interface ---------- */

int lz32d_compress_fast ( const void* src_ptr, size_t* src_len, void* dst_ptr, size_t* dst_len ) {
  
  
  
  return LZ32_SUCCESS;
}

/* ---------- High (slow) data compression interface ---------- */

int lz32d_compress_high ( const void* src_ptr, size_t* src_len, void* dst_ptr, size_t* dst_len ) {
  
  
  
  return LZ32_SUCCESS;
}

/* ----------  ---------- */

int lz32d_decompress_size ( const void* src_ptr, size_t* src_len, size_t* dst_len ) {
  
  if (src_ptr == NULL) lz32_error (LZ32_EINVAL, "lz32d_decompress_size(): ");
  const char* sptr = (const char*)src_ptr;
  if (((size_t)sptr & 3) != 0) lz32_error (LZ32_EINVAL, "lz32d_decompress_size(): ");
  
  if (src_len == NULL) lz32_error (LZ32_EINVAL, "lz32d_decompress_size(): ");
  size_t scap = *(src_len);
  *(src_len) = 0;
  
  if (dst_len == NULL) lz32_error (LZ32_EINVAL, "lz32d_decompress_size(): ");
  size_t dcap = *(dst_len);
  *(dst_len) = 0;
  
  if (scap < 16) lz32_error (LZ32_EINVAL, "lz32d_decompress_size(): ");
  u32t mnum = lz32_read32 (sptr + 0); // TODO : READ LE ??!
  if (mnum != LZ32D_MAGIC_NUMBER) lz32_error (LZ32_EINVAL, "lz32d_decompress_size(): ");
  
  size_t blen = lz32_read32 (sptr + 4);
  if (blen < LZ32D_BLK_SIZE_MIN) lz32_error (LZ32_EINVAL, "lz32d_decompress_size(): ");
  if (blen > LZ32D_BLK_SIZE_MAX) lz32_error (LZ32_EINVAL, "lz32d_decompress_size(): ");
  if ((blen & 15) != 0) lz32_error (LZ32_EINVAL, "lz32d_decompress_size(): ");
  *(src_len) = blen;
  
  if (scap < blen) lz32_error (LZ32_EINVAL, "lz32d_decompress_size(): ");
  size_t rlen = lz32_read32 (sptr + blen - 8);
  if (rlen < LZ32D_RAW_SIZE_MIN) lz32_error (LZ32_EINVAL, "lz32d_decompress_size(): ");
  if (rlen > LZ32D_RAW_SIZE_MAX) lz32_error (LZ32_EINVAL, "lz32d_decompress_size(): ");
  *(dst_len) = rlen;
  
  return LZ32_SUCCESS;
}

/* ---------- Fast (unsafe) data decompression interface ---------- */

int lz32d_decompress_fast ( const void* src_ptr, size_t* src_len, void* dst_ptr, size_t* dst_len ) {
  
  
  
  return LZ32_SUCCESS;
}

/* ---------- Safe (slow) data decompression interface ---------- */

int lz32d_decompress_safe ( const void* src_ptr, size_t* src_len, void* dst_ptr, size_t* dst_len ) {
  
  
  
  return LZ32_SUCCESS;
}

/* ----------  ---------- */
