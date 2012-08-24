/*
 * Copyright (C) 2010 SCALITY SA. All rights reserved.
 * http://www.scality.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SCALITY SA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SCALITY SA OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of SCALITY SA.
 *
 * https://github.com/scality/Droplet
 */
#ifndef __DROPLET_H__
#define __DROPLET_H__ 1

#ifdef __cplusplus
extern "C" {
#endif

#define DPL_VERSION_MAJOR 2
#define DPL_VERSION_MINOR 0

/*
 * dependencies
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/md5.h>
#include <pthread.h>

/*
 * default values
 */
#define DPL_DEFAULT_N_CONN_BUCKETS      1021
#define DPL_DEFAULT_N_CONN_MAX          900
#define DPL_DEFAULT_N_CONN_MAX_HITS     50
#define DPL_DEFAULT_CONN_IDLE_TIME      100
#define DPL_DEFAULT_CONN_TIMEOUT        5
#define DPL_DEFAULT_READ_TIMEOUT        30
#define DPL_DEFAULT_WRITE_TIMEOUT       30
#define DPL_DEFAULT_READ_BUF_SIZE       8192
#define DPL_DEFAULT_MAX_REDIRECTS       10

#define DPL_MAXPATHLEN 1024
#define DPL_MAXNAMLEN  255

#define DPL_DEFAULT_DELIM "/"

/*
 * types
 */
typedef enum
  {
    DPL_SUCCESS              =  0,   /*!< Success */
    DPL_FAILURE              = (-1), /*!< General failure */
    DPL_ENOENT               = (-2), /*!< No such entry */
    DPL_EINVAL               = (-3), /*!< Invalid argument */
    DPL_ETIMEOUT             = (-4), /*!< Operation timeouted */
    DPL_ENOMEM               = (-5), /*!< No memory available */
    DPL_ESYS                 = (-6), /*!< System error */
    DPL_EIO                  = (-7), /*!< I/O error */
    DPL_ELIMIT               = (-8), /*!< Limit has been reached */
    DPL_ENAMETOOLONG         = (-9), /*!< Name is too long */
    DPL_ENOTDIR              = (-10),/*!< Not a directory */
    DPL_ENOTEMPTY            = (-11),/*!< Directory is not empty */
    DPL_EISDIR               = (-12),/*!< Is a directory */
    DPL_EEXIST               = (-13),/*!< Object already exists */
    DPL_ENOTSUPP             = (-14),/*!< Method not supported */
    DPL_EREDIRECT            = (-15),/*!< Redirection */
    DPL_ETOOMANYREDIR        = (-16),/*!< Too many redirects */
  } dpl_status_t;

typedef enum
  {
    DPL_TRACE_CONN  = (1u<<0),  /*!< trace connection */
    DPL_TRACE_IO    = (1u<<1),  /*!< trace I/O */
    DPL_TRACE_HTTP  = (1u<<2),  /*!< trace HTTP */
    DPL_TRACE_SSL   = (1u<<3),  /*!< trace SSL */
    DPL_TRACE_REQ   = (1u<<5),  /*!< trace request builder */
    DPL_TRACE_REST  = (1u<<6),  /*!< trace REST based calls */
    DPL_TRACE_ID    = (1u<<7),  /*!< trace ID based calls */
    DPL_TRACE_VFS   = (1u<<8),  /*!< trace VFS based calls */
  } dpl_trace_t;

typedef void (*dpl_trace_func_t)(pid_t tid, dpl_trace_t, const char *file, const char *func, int lineno, char *buf);

#include <droplet/vec.h>
#include <droplet/dict.h>

typedef enum
  {
    DPL_METHOD_GET,
    DPL_METHOD_PUT,
    DPL_METHOD_DELETE,
    DPL_METHOD_HEAD,
    DPL_METHOD_POST,
  } dpl_method_t;

enum dpl_data_type
  {
    DPL_DATA_TYPE_IN,
    DPL_DATA_TYPE_OUT,
    DPL_DATA_TYPE_STORAGE,
  };
#define DPL_N_DATA_TYPE (DPL_DATA_TYPE_STORAGE+1)

typedef enum
  {
    DPL_LOCATION_CONSTRAINT_UNDEF,
    DPL_LOCATION_CONSTRAINT_US_STANDARD,
    DPL_LOCATION_CONSTRAINT_EU,
    DPL_LOCATION_CONSTRAINT_US_WEST_1,
    DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_1
  } dpl_location_constraint_t;
#define DPL_N_LOCATION_CONSTRAINT (DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_1+1)

typedef enum
  {
    DPL_CANNED_ACL_UNDEF,
    DPL_CANNED_ACL_PRIVATE,
    DPL_CANNED_ACL_PUBLIC_READ,
    DPL_CANNED_ACL_PUBLIC_READ_WRITE,
    DPL_CANNED_ACL_AUTHENTICATED_READ,
    DPL_CANNED_ACL_BUCKET_OWNER_READ,
    DPL_CANNED_ACL_BUCKET_OWNER_FULL_CONTROL,
  } dpl_canned_acl_t;
#define DPL_N_CANNED_ACL (DPL_CANNED_ACL_BUCKET_OWNER_FULL_CONTROL+1)

/*
 * ACL mgmt (follow RFC3530)
 */

typedef enum
  {
    DPL_ACE_TYPE_ALLOW = 0x00000001,
    DPL_ACE_TYPE_DENY  = 0x00000002,
    DPL_ACE_TPE_AUDIT  = 0x00000003,
    DPL_ACE_TPE_ALARM  = 0x00000004
  } dpl_ace_type_t;

typedef enum
  {
    DPL_ACE_FLAG_OBJECT_INHERIT =       0x00000001,
    DPL_ACE_FLAG_CONTAINER_INHERIT =    0x00000002,
    DPL_ACE_FLAG_NO_PROPAGATE =         0x00000004,
    DPL_ACE_FLAG_INHERIT_ONLY =         0x00000008,
    DPL_ACE_FLAG_SUCCESSFUL_ACCESS =    0x00000010,
    DPL_ACE_FLAG_FAILED_ACCESS =        0x00000020,
    DPL_ACE_FLAG_IDENTIFIER_GROUP =     0x00000040,
    DPL_ACE_FLAG_INHERITED =            0x00000080,
  } dpl_ace_flag_t;

typedef enum
  {
    DPL_ACE_MASK_READ_OBJECT =          0x00000001,
    DPL_ACE_MASK_LIST_CONTAINER =       0x00000001,
    DPL_ACE_MASK_WRITE_OBJECT =         0x00000002,
    DPL_ACE_MASK_ADD_OBJECT =           0x00000002,
    DPL_ACE_MASK_APPEND_DATA =          0x00000004,
    DPL_ACE_MASK_ADD_SUBCONTAINER =     0x00000004,
    DPL_ACE_MASK_READ_METADATA =        0x00000008,
    DPL_ACE_MASK_WRITE_METADATA =       0x00000010,
    DPL_ACE_MASK_EXECUTE =              0x00000020,
    DPL_ACE_MASK_DELETE_OBJECT =        0x00000040,
    DPL_ACE_MASK_DELETE_SUBCONTAINER =  0x00000040,
    DPL_ACE_MASK_READ_ATTRIBUTES =      0x00000080,
    DPL_ACE_MASK_WRITE_ATTRIBUTES =     0x00000100,
    DPL_ACE_MASK_WRITE_RETENTION =      0x00000200,
    DPL_ACE_MASK_WRITE_RETENTION_HOLD = 0x00000400,
    DPL_ACE_MASK_DELETE =               0x00010000,
    DPL_ACE_MASK_READ_ACL =             0x00020000,
    DPL_ACE_MASK_WRITE_ACL =            0x00040000,
    DPL_ACE_MASK_WRITE_OWNER =          0x00080000,
    DPL_ACE_MASK_SYNCHRONIZE =          0x00190000
  } dpl_ace_mask_t;

#define DPL_ACE_MASK_READ               (DPL_ACE_MASK_READ_OBJECT|\
                                         DPL_ACE_MASK_READ_METADATA|    \
                                         DPL_ACE_MASK_EXECUTE|          \
                                         DPL_ACE_MASK_READ_ATTRIBUTES)
#define DPL_ACE_MASK_READ_ALL           (DPL_ACE_MASK_READ|             \
                                         DPL_ACE_MASK_READ_ACL)
#define DPL_ACE_MASK_WRITE              (DPL_ACE_MASK_WRITE_OBJECT|     \
                                         DPL_ACE_MASK_ADD_SUBCONTAINER| \
                                         DPL_ACE_MASK_WRITE_METADATA)
#define DPL_ACE_MASK_WRITE_ALL          (DPL_ACE_MASK_WRITE|            \
                                         DPL_ACE_MASK_DELETE_OBJECT|    \
                                         DPL_ACE_MASK_DELETE|           \
                                         DPL_ACE_MASK_WRITE_ATTRIBUTES| \
                                         DPL_ACE_MASK_WRITE_RETENTION|  \
                                         DPL_ACE_MASK_WRITE_RETENTION_HOLD| \
                                         DPL_ACE_MASK_WRITE_ACL|        \
                                         DPL_ACE_MASK_WRITE_OWNER)
#define DPL_ACE_MASK_RW                 (DPL_ACE_MASK_READ|DPL_ACE_MASK_WRITE)
#define DPL_ACE_MASK_RW_ALL             (DPL_ACE_MASK_READ_ALL|DPL_ACE_MASK_WRITE_ALL)
#define DPL_ACE_MASK_ALL                (DPL_ACE_MASK_RW_ALL|DPL_ACE_MASK_SYNCHRONIZE)

typedef enum
  {
    DPL_ACE_WHO_OWNER,
    DPL_ACE_WHO_GROUP,
    DPL_ACE_WHO_EVERYONE,
    DPL_ACE_WHO_INTERACTIVE,
    DPL_ACE_WHO_NETWORK,
    DPL_ACE_WHO_DIALUP,
    DPL_ACE_WHO_BATCH,
    DPL_ACE_WHO_ANONYMOUS,
    DPL_ACE_WHO_AUTHENTICATED,
    DPL_ACE_WHO_SERVICE,
    DPL_ACE_WHO_ADMINISTRATOR,
    DPL_ACE_WHO_ADMINUSERS,
  } dpl_ace_who_t;

typedef struct
{
  dpl_ace_type_t type;
  dpl_ace_flag_t flag;
  dpl_ace_mask_t access_mask;
  dpl_ace_who_t who;
} dpl_ace_t;

typedef enum
  {
    DPL_STORAGE_CLASS_UNDEF,
    DPL_STORAGE_CLASS_STANDARD,
    DPL_STORAGE_CLASS_REDUCED_REDUNDANCY,
  } dpl_storage_class_t;
#define DPL_N_STORAGE_CLASS (DPL_STORAGE_CLASS_REDUCED_REDUNDANCY+1)

/**/

typedef enum
  {
    DPL_SYSMD_MASK_CANNED_ACL    = (1u<<0),
    DPL_SYSMD_MASK_STORAGE_CLASS = (1u<<1),
    DPL_SYSMD_MASK_SIZE          = (1u<<2),
    DPL_SYSMD_MASK_ATIME         = (1u<<3),
    DPL_SYSMD_MASK_MTIME         = (1u<<4),
    DPL_SYSMD_MASK_CTIME         = (1u<<5),
    DPL_SYSMD_MASK_MD5           = (1u<<6),
    DPL_SYSMD_MASK_LOCATION_CONSTRAINT = (1u<<8),
    DPL_SYSMD_MASK_LAZY          = (1u<<9), /*!< lazy operation */
    DPL_SYSMD_MASK_OWNER         = (1u<<10),
    DPL_SYSMD_MASK_GROUP         = (1u<<11),
    DPL_SYSMD_MASK_ACL           = (1u<<12),
    DPL_SYSMD_MASK_ID            = (1u<<13)
  } dpl_sysmd_mask_t;

typedef struct
{
  dpl_sysmd_mask_t mask;
  dpl_canned_acl_t canned_acl;
  dpl_storage_class_t storage_class;
  uint64_t size;
  time_t atime;
  time_t mtime;
  time_t ctime;
  char md5[MD5_DIGEST_LENGTH];
  dpl_location_constraint_t location_constraint;
#define DPL_SYSMD_OWNER_SIZE 32
  char owner[DPL_SYSMD_OWNER_SIZE+1];
#define DPL_SYSMD_GROUP_SIZE 32
  char group[DPL_SYSMD_OWNER_SIZE+1];
#define DPL_SYSMD_ACE_MAX 10
  uint32_t n_aces;
  dpl_ace_t aces[DPL_SYSMD_ACE_MAX];
#define DPL_SYSMD_ID_SIZE 64
  char id[DPL_SYSMD_ID_SIZE+1];
} dpl_sysmd_t;

/**/

typedef struct
{
#define DPL_CONDITION_IF_MODIFIED_SINCE   (1u<<0)
#define DPL_CONDITION_IF_UNMODIFIED_SINCE (1u<<1)
#define DPL_CONDITION_IF_MATCH            (1u<<2)
#define DPL_CONDITION_IF_NONE_MATCH       (1u<<3)
#define DPL_CONDITION_LAZY                (1u<<4) /*!< perform a lazy operation */
  unsigned int mask;
  time_t time;
  char etag[MD5_DIGEST_LENGTH];
} dpl_condition_t;

typedef struct dpl_ino
{
  char key[DPL_MAXPATHLEN];
} dpl_ino_t;

typedef struct
{
  const char *buf;
  unsigned int len;
} dpl_chunk_t;

typedef struct
{
#define DPL_UNDEF -1
  int start;
  int end;
} dpl_range_t;

typedef enum
  {
    DPL_COPY_DIRECTIVE_UNDEF,
    DPL_COPY_DIRECTIVE_COPY,
    DPL_COPY_DIRECTIVE_METADATA_REPLACE,
    DPL_COPY_DIRECTIVE_HARDLINK,
    DPL_COPY_DIRECTIVE_SYMLINK,
    DPL_COPY_DIRECTIVE_MOVE,
  } dpl_copy_directive_t;

typedef struct
{
  char *name;
  time_t creation_time;
} dpl_bucket_t;

typedef struct
{
  char *key;
  time_t last_modified;
  size_t size;
  char *etags;
} dpl_object_t;

typedef struct
{
  char *prefix;
} dpl_common_prefix_t;

typedef enum
  {
    DPL_FTYPE_UNDEF,
    DPL_FTYPE_ANY,
    DPL_FTYPE_REG,
    DPL_FTYPE_DIR,
    DPL_FTYPE_CAP,
  } dpl_ftype_t;

typedef struct 
{
  uint32_t time_low;
  uint16_t time_mid;
  uint16_t time_hi_and_version;
  uint8_t clock_seq_hi_and_reserved;
  uint8_t clock_seq_low;
  uint8_t node_addr[6];
} dpl_uuid_t;

struct dpl_backend_s;

typedef struct dpl_ctx
{
  /*
   * profile
   */
  int n_conn_buckets;         /*!< number of buckets         */
  int n_conn_max;             /*!< max connexions            */
  int n_conn_max_hits;        /*!< before auto-close         */
  int conn_idle_time;         /*!< auto-close after (sec)    */
  int conn_timeout;           /*!< connection timeout (sec)  */
  int read_timeout;           /*!< read timeout (sec)        */
  int write_timeout;          /*!< write timeout (sec)       */
  int use_https;
  char *host;
  int port;
  char *base_path;            /*!< or RootURI */
  char *access_key;
  char *secret_key;
  char *ssl_cert_file;
  char *ssl_key_file;
  char *ssl_password;
  char *ssl_ca_list;
  unsigned int trace_level;
  int trace_buffers;
  char *pricing;              /*!< might be NULL */
  unsigned int read_buf_size;
  char *encrypt_key;
  char *delim;               /*!< vdir delimiter */
  int light_mode;            /*!< do not resolve pathes */
  int encode_slashes;        /*!< client wants slashes encoded */
  int keep_alive;            /*!< client supports keep-alive */
  int url_encoding;          /*!< some servers does not handle url encoding */
  int base_path_in_refs;     /*!< include base_path in references */
  int max_redirects;         /*!< maximum number of redirects */
  struct dpl_backend_s *backend;

  /*
   * pricing
   */
  dpl_vec_t *request_pricing;
  dpl_vec_t *data_pricing[DPL_N_DATA_TYPE];

  /*
   * ssl
   */
  BIO *ssl_bio_err;
  SSL_CTX *ssl_ctx;
  BIO *ssl_bio;

  /*
   * conn pool
   */
  struct dpl_conn **conn_buckets;  /*!< idle connections buckets  */
  int n_conn_fds;                  /*!< number of active fds      */

  /*
   * vdir
   */
  dpl_dict_t *cwds;
  char *cur_bucket;

  /*
   * common
   */
  char *droplet_dir;
  char *profile_name;

  char *pricing_dir; /*!< where charged events should be logged, empty string to disable logging */
  FILE *event_log; /*!< log charged events */

  dpl_trace_func_t trace_func;

  pthread_mutex_t lock;

} dpl_ctx_t;

/**/

typedef enum
  {
    DPL_BEHAVIOR_MD5 =         (1u<<0),     /*!< MD5 is computed for object */
    DPL_BEHAVIOR_EXPECT =      (1u<<1),     /*!< Use the Expect: 100-Continue */
    DPL_BEHAVIOR_VIRTUAL_HOSTING = (1u<<2), /*!< Use virtual hosting instead of path-style access */
    DPL_BEHAVIOR_KEEP_ALIVE =  (1u<<3),     /*!< Reuse connections */
    DPL_BEHAVIOR_QUERY_STRING = (1u<<4),    /*!< Build a query string instead of a request */
    DPL_BEHAVIOR_COPY =        (1u<<5),     /*!< It is a server side copy request */
    DPL_BEHAVIOR_HTTP_COMPAT = (1u<<6),     /*!< Use the HTTP compatibility mode */
    DPL_BEHAVIOR_MDONLY =      (1u<<7)      /*!< Some REST server like it this way */
  } dpl_behavior_flag_t;

typedef struct
{
  dpl_ctx_t *ctx;

  dpl_behavior_flag_t behavior_flags;
  
  dpl_method_t method;
  char *bucket;
  char *resource;
  char *subresource;

  char *cache_control;
  char *content_disposition;
  char *content_encoding;
  dpl_ftype_t object_type;
  char *content_type;
  dpl_location_constraint_t location_constraint;
  dpl_canned_acl_t canned_acl;
  dpl_condition_t condition;
  dpl_storage_class_t storage_class;

  dpl_dict_t *metadata;
  dpl_chunk_t *chunk;

#define DPL_RANGE_MAX 10
  dpl_range_t ranges[DPL_RANGE_MAX];
  int n_ranges;

  time_t expires; /*!< for query strings */

  /*
   * server side copy
   */
  char *src_bucket;
  char *src_resource;
  char *src_subresource;
  dpl_copy_directive_t copy_directive;
  dpl_condition_t copy_source_condition;

} dpl_req_t;

/*
 * vdir
 */
#define DPL_ROOT_INO  ((dpl_ino_t) {.key = ""})

typedef struct
{
  dpl_ino_t ino;
  dpl_ctx_t *ctx;
  dpl_vec_t *files;
  dpl_vec_t *directories;
  int files_cursor;
  int directories_cursor;
} dpl_dir_t;

typedef struct
{
  char name[DPL_MAXNAMLEN];
  dpl_ino_t ino;
  dpl_ftype_t type;
  time_t last_modified;
  size_t size;
} dpl_dirent_t;

/*
 * vfile
 */
#include <droplet/conn.h>
#include <droplet/httprequest.h>
#include <droplet/httpreply.h>

typedef enum
  {
    DPL_VFILE_FLAG_CREAT =   (1u<<0),     /*!< create file if it doesnt exist */
    DPL_VFILE_FLAG_EXCL =    (1u<<1),     /*!< exclusive creation */
    DPL_VFILE_FLAG_MD5 =     (1u<<2),     /*!< check MD5 */
    DPL_VFILE_FLAG_ENCRYPT = (1u<<3),     /*!< encrypt on the fly */
    DPL_VFILE_FLAG_POST =    (1u<<4),     /*!< use POST to write/creat file */
    DPL_VFILE_FLAG_RANGE =   (1u<<5),     /*!< use specified range (get only) */
    DPL_VFILE_FLAG_ONESHOT = (1u<<6),     /*!< get/put file in one-shot */
  } dpl_vfile_flag_t;

typedef struct
{
  dpl_ctx_t *ctx;

  unsigned int flags;

  dpl_conn_t *conn;

  /*
   * MD5
   */
  MD5_CTX md5_ctx;

  /*
   * encrypt
   */
  unsigned char salt[PKCS5_SALT_LEN];
  EVP_CIPHER_CTX *cipher_ctx;
  int header_done;

  /*
   * read
   */
  dpl_dict_t *headers_reply;
  dpl_buffer_func_t buffer_func;
  void *cb_arg;

  /*
   * for one shot
   */
  char *bucket;
  char *resource;
  dpl_ftype_t obj_type;
  dpl_dict_t *metadata;
  dpl_sysmd_t *sysmd;
  dpl_dict_t *query_params;
  
} dpl_vfile_t;

#define DPL_ENCRYPT_MAGIC "Salted__"

/*
 * public functions
 */
#include <droplet/converters.h>
#include <droplet/req.h>
#include <droplet/rest.h>
#include <droplet/sysmd.h>
#include <droplet/id.h>
#include <droplet/vfs.h>

/* PROTO droplet.c */
/* src/droplet.c */
const char *dpl_status_str(dpl_status_t status);
dpl_status_t dpl_init(void);
void dpl_free(void);
dpl_ctx_t *dpl_ctx_new(const char *droplet_dir, const char *profile_name);
void dpl_ctx_free(dpl_ctx_t *ctx);
double dpl_price_storage(dpl_ctx_t *ctx, size_t size);
char *dpl_price_storage_str(dpl_ctx_t *ctx, size_t size);
char *dpl_size_str(uint64_t size);
struct dpl_backend_s *dpl_backend_find(const char *name);
void dpl_bucket_free(dpl_bucket_t *bucket);
void dpl_vec_buckets_free(dpl_vec_t *vec);
void dpl_object_free(dpl_object_t *object);
void dpl_vec_objects_free(dpl_vec_t *vec);
void dpl_common_prefix_free(dpl_common_prefix_t *common_prefix);
void dpl_vec_common_prefixes_free(dpl_vec_t *vec);

#ifdef __cplusplus
}
#endif

#endif
