#ifndef LZ32_H
#define LZ32_H
//#ifdef  __cplusplus
//extern "C" {
//#endif

#include <stddef.h>

/* ----------  ---------- */

#define LZ32_DEBUG 1

/* ----------  ---------- */

#define LZ32_SUCCESS            0
#define LZ32_EINVAL             1
#define LZ32_EDATA              2
#define LZ32_EUNKNOWN           15

const char* lz32_error_string ( int res_val );

/* ----------  ---------- */

#define LZ32_LENGTH_MAX (1ULL << 45)

#define LZ32_RAW_SIZE_MIN 1
#define LZ32_RAW_SIZE_MAX (1 << 30)

#define LZ32_BLK_SIZE_MIN 16
#define LZ32_BLK_SIZE_MAX (1 << 30)

/* ----------  ---------- */

int lz32_compress_bound ( size_t* src_len, size_t* dst_len );

int lz32_compress_fast ( const void* src_ptr, size_t* src_len, void* dst_ptr, size_t* dst_len );

int lz32_compress_high ( const void* src_ptr, size_t* src_len, void* dst_ptr, size_t* dst_len );

/* ----------  ---------- */

int lz32_decompress_fast ( const void* src_ptr, size_t src_len, void* dst_ptr, size_t dst_len );

int lz32_decompress_safe ( const void* src_ptr, size_t src_len, void* dst_ptr, size_t dst_len );

/* ----------  ---------- */

int lz32d_compress_bound ( size_t* src_len, size_t* dst_len );

int lz32d_compress_fast ( const void* src_ptr, size_t* src_len, void* dst_ptr, size_t* dst_len );

int lz32d_compress_high ( const void* src_ptr, size_t* src_len, void* dst_ptr, size_t* dst_len );

/* ----------  ---------- */

//int xxh64_hash_low32 ( const void* src_ptr, size_t src_len, void* dst_ptr );

int lz32d_decompress_size ( const void* src_ptr, size_t* src_len, size_t* dst_len );

int lz32d_decompress_fast ( const void* src_ptr, size_t* src_len, void* dst_ptr, size_t* dst_len );

int lz32d_decompress_safe ( const void* src_ptr, size_t* src_len, void* dst_ptr, size_t* dst_len );




//#ifdef  __cplusplus
//}
//#endif
#endif /* LZ32_H */