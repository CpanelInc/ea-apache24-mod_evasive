/*
mod_evasive for Apache 2
Copyright (c) by Jonathan A. Zdziarski

LICENSE
                                                                                
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
                                                                                
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
                                                                                
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>

#include "httpd.h"
#include "http_core.h"
#include "http_config.h"
#include "http_log.h"
#include "http_request.h"

module AP_MODULE_DECLARE_DATA evasive20_module;

pid_t getpid(void);
pid_t getppid(void);

/* BEGIN DoS Evasive Maneuvers Definitions */

#define MAILER	"/bin/mail %s"
#define  LOG( A, ... ) { openlog("mod_evasive", LOG_PID, LOG_DAEMON); syslog( A, __VA_ARGS__ ); closelog(); }

#define DEFAULT_HASH_TBL_SIZE   3097ul  // Default hash table size
#define DEFAULT_PAGE_COUNT      2       // Default maximum page hit count per interval
#define DEFAULT_SITE_COUNT      50      // Default maximum site hit count per interval
#define DEFAULT_PAGE_INTERVAL   1       // Default 1 Second page interval
#define DEFAULT_SITE_INTERVAL   1       // Default 1 Second site interval
#define DEFAULT_BLOCKING_PERIOD 10      // Default for Detected IPs; blocked for 10 seconds
#define DEFAULT_LOG_DIR		"/tmp"  // Default temp directory

/* END DoS Evasive Maneuvers Definitions */

/* BEGIN NTT (Named Timestamp Tree) Headers */

enum { ntt_num_primes = 28 };

/* ntt root tree */
struct ntt {
    long size;
    long items;
    struct ntt_node **tbl;
};

/* ntt node (entry in the ntt root tree) */
struct ntt_node {
    char *key;
    time_t timestamp;
    long count;
    struct ntt_node *next;
};

/* ntt cursor */
struct ntt_c {
  long iter_index;
  struct ntt_node *iter_next;
};

struct ntt *ntt_create(long size);
int ntt_destroy(struct ntt *ntt);
struct ntt_node	*ntt_find(struct ntt *ntt, const char *key);
struct ntt_node	*ntt_insert(struct ntt *ntt, const char *key, time_t timestamp);
int ntt_delete(struct ntt *ntt, const char *key);
long ntt_hashcode(struct ntt *ntt, const char *key);	
struct ntt_node *c_ntt_first(struct ntt *ntt, struct ntt_c *c);
struct ntt_node *c_ntt_next(struct ntt *ntt, struct ntt_c *c);

/* END NTT (Named Timestamp Tree) Headers */


/* BEGIN DoS Evasive Maneuvers Globals */

struct ntt *hit_list;	// Our dynamic hash table

static unsigned long hash_table_size = DEFAULT_HASH_TBL_SIZE;
static int page_count = DEFAULT_PAGE_COUNT;
static int page_interval = DEFAULT_PAGE_INTERVAL;
static int site_count = DEFAULT_SITE_COUNT;
static int site_interval = DEFAULT_SITE_INTERVAL;
static int blocking_period = DEFAULT_BLOCKING_PERIOD;
static char *email_notify = NULL;
static char *log_dir = NULL;
static char *system_command = NULL;
static const char *whitelist(cmd_parms *cmd, void *dconfig, const char *ip);
int is_whitelisted(const char *ip);

/* END DoS Evasive Maneuvers Globals */

static void * create_hit_list(apr_pool_t *p, server_rec *s) 
{
    /* Create a new hit list for this listener */

    hit_list = ntt_create(hash_table_size);
}

static const char *whitelist(cmd_parms *cmd, void *dconfig, const char *ip)
{
  char entry[128];
  snprintf(entry, sizeof(entry), "WHITELIST_%s", ip);
  ntt_insert(hit_list, entry, time(NULL));
  
  return NULL;
}


static int access_checker(request_rec *r) 
{
    int ret = OK;

    /* BEGIN DoS Evasive Maneuvers Code */

    if (r->prev == NULL && r->main == NULL && hit_list != NULL) {
      char hash_key[2048];
      struct ntt_node *n;
      time_t t = time(NULL);

      /* Check whitelist */
      if (is_whitelisted(r->useragent_ip))
        return OK;

      /* First see if the IP itself is on "hold" */
      n = ntt_find(hit_list, r->useragent_ip);

      if (n != NULL && t-n->timestamp<blocking_period) {
 
        /* If the IP is on "hold", make it wait longer in 403 land */
        ret = HTTP_FORBIDDEN;
        n->timestamp = time(NULL);

      /* Not on hold, check hit stats */
      } else {

        /* Has URI been hit too much? */
        snprintf(hash_key, 2048, "%s_%s", r->useragent_ip, r->uri);
        n = ntt_find(hit_list, hash_key);
        if (n != NULL) {

          /* If URI is being hit too much, add to "hold" list and 403 */
          if (t-n->timestamp<page_interval && n->count>=page_count) {
            ret = HTTP_FORBIDDEN;
            ntt_insert(hit_list, r->useragent_ip, time(NULL));
          } else {

            /* Reset our hit count list as necessary */
            if (t-n->timestamp>=page_interval) {
              n->count=0;
            }
          }
          n->timestamp = t;
          n->count++;
        } else {
          ntt_insert(hit_list, hash_key, t);
        }

        /* Has site been hit too much? */
        snprintf(hash_key, 2048, "%s_SITE", r->useragent_ip);
        n = ntt_find(hit_list, hash_key);
        if (n != NULL) {

          /* If site is being hit too much, add to "hold" list and 403 */
          if (t-n->timestamp<site_interval && n->count>=site_count) {
            ret = HTTP_FORBIDDEN;
            ntt_insert(hit_list, r->useragent_ip, time(NULL));
          } else {

            /* Reset our hit count list as necessary */
            if (t-n->timestamp>=site_interval) {
              n->count=0;
            }
          }
          n->timestamp = t;
          n->count++;
        } else {
          ntt_insert(hit_list, hash_key, t);
        }
      }

      /* Perform email notification and system functions */
      if (ret == HTTP_FORBIDDEN) {
        char filename[1024];
        struct stat s;
        FILE *file;

        snprintf(filename, sizeof(filename), "%s/dos-%s", log_dir != NULL ? log_dir : DEFAULT_LOG_DIR, r->useragent_ip);
        if (stat(filename, &s)) {
          file = fopen(filename, "w");
          if (file != NULL) {
            fprintf(file, "%d\n", getpid());
            fclose(file);

            LOG(LOG_ALERT, "Blacklisting address %s: possible DoS attack.", r->useragent_ip);
            if (email_notify != NULL) {
              snprintf(filename, sizeof(filename), MAILER, email_notify);
              file = popen(filename, "w");
              if (file != NULL) {
                fprintf(file, "To: %s\n", email_notify);
                fprintf(file, "Subject: HTTP BLACKLIST %s\n\n", r->useragent_ip);
                fprintf(file, "mod_evasive HTTP Blacklisted %s\n", r->useragent_ip);
                pclose(file);
              }
            }

            if (system_command != NULL) {
              snprintf(filename, sizeof(filename), system_command, r->useragent_ip);
              system(filename);
            }
 
          } else {
            LOG(LOG_ALERT, "Couldn't open logfile %s: %s",filename, strerror(errno));
	  }

        } /* if (temp file does not exist) */

      } /* if (ret == HTTP_FORBIDDEN) */

    } /* if (r->prev == NULL && r->main == NULL && hit_list != NULL) */

    /* END DoS Evasive Maneuvers Code */

    if (ret == HTTP_FORBIDDEN
	&& (ap_satisfies(r) != SATISFY_ANY || !ap_some_auth_required(r))) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
            "client denied by server configuration: %s",
            r->filename);
    }

    return ret;
}

int is_whitelisted(const char *ip) {
  char hashkey[128];
  char octet[4][4];
  char *dip;
  char *oct;
  int i = 0;

  memset(octet, 0, 16);
  dip = strdup(ip);
  if (dip == NULL)
    return 0;

  oct = strtok(dip, ".");
  while(oct != NULL && i<4) {
    if (strlen(oct)<=3) 
      strcpy(octet[i], oct);
    i++;
    oct = strtok(NULL, ".");
  }
  free(dip);

  /* Exact Match */
  snprintf(hashkey, sizeof(hashkey), "WHITELIST_%s", ip); 
  if (ntt_find(hit_list, hashkey)!=NULL)
    return 1;

  /* IPv4 Wildcards */ 
  snprintf(hashkey, sizeof(hashkey), "WHITELIST_%s.*.*.*", octet[0]);
  if (ntt_find(hit_list, hashkey)!=NULL)
    return 1;

  snprintf(hashkey, sizeof(hashkey), "WHITELIST_%s.%s.*.*", octet[0], octet[1]);
  if (ntt_find(hit_list, hashkey)!=NULL)
    return 1;

  snprintf(hashkey, sizeof(hashkey), "WHITELIST_%s.%s.%s.*", octet[0], octet[1], octet[2]);
  if (ntt_find(hit_list, hashkey)!=NULL)
    return 1;

  /* No match */
  return 0;
}

static apr_status_t destroy_hit_list(void *not_used) {
  ntt_destroy(hit_list);
  free(email_notify);
  free(system_command);
}


/* BEGIN NTT (Named Timestamp Tree) Functions */

static unsigned long ntt_prime_list[ntt_num_primes] = 
{
    53ul,         97ul,         193ul,       389ul,       769ul,
    1543ul,       3079ul,       6151ul,      12289ul,     24593ul,
    49157ul,      98317ul,      196613ul,    393241ul,    786433ul,
    1572869ul,    3145739ul,    6291469ul,   12582917ul,  25165843ul,
    50331653ul,   100663319ul,  201326611ul, 402653189ul, 805306457ul,
    1610612741ul, 3221225473ul, 4294967291ul
};


/* Find the numeric position in the hash table based on key and modulus */

long ntt_hashcode(struct ntt *ntt, const char *key) {
    unsigned long val = 0;
    for (; *key; ++key) val = 5 * val + *key;
    return(val % ntt->size);
}

/* Creates a single node in the tree */

struct ntt_node *ntt_node_create(const char *key) {
    char *node_key;
    struct ntt_node* node;

    node = (struct ntt_node *) malloc(sizeof(struct ntt_node));
    if (node == NULL) {
	return NULL;
    }
    if ((node_key = strdup(key)) == NULL) {
        free(node);
	return NULL;
    }
    node->key = node_key;
    node->timestamp = time(NULL);
    node->next = NULL;
    return(node);
}

/* Tree initializer */

struct ntt *ntt_create(long size) {
    long i = 0;
    struct ntt *ntt = (struct ntt *) malloc(sizeof(struct ntt));

    if (ntt == NULL)
        return NULL;
    while (ntt_prime_list[i] < size) { i++; }
    ntt->size  = ntt_prime_list[i];
    ntt->items = 0;
    ntt->tbl   = (struct ntt_node **) calloc(ntt->size, sizeof(struct ntt_node *));
    if (ntt->tbl == NULL) {
        free(ntt);
        return NULL;
    }
    return(ntt);
}

/* Find an object in the tree */

struct ntt_node *ntt_find(struct ntt *ntt, const char *key) {
    long hash_code;
    struct ntt_node *node;

    if (ntt == NULL) return NULL;

    hash_code = ntt_hashcode(ntt, key);
    node = ntt->tbl[hash_code];

    while (node) {
        if (!strcmp(key, node->key)) {
            return(node);
        }
        node = node->next;
    }
    return((struct ntt_node *)NULL);
}

/* Insert a node into the tree */

struct ntt_node *ntt_insert(struct ntt *ntt, const char *key, time_t timestamp) {
    long hash_code;
    struct ntt_node *parent;
    struct ntt_node *node;
    struct ntt_node *new_node = NULL;

    if (ntt == NULL) return NULL;

    hash_code = ntt_hashcode(ntt, key);
    parent	= NULL;
    node	= ntt->tbl[hash_code];

    while (node != NULL) {
        if (strcmp(key, node->key) == 0) { 
            new_node = node;
            node = NULL;
        }

	if (new_node == NULL) {
          parent = node;
          node = node->next;
        }
    }

    if (new_node != NULL) {
        new_node->timestamp = timestamp;
        new_node->count = 0;
        return new_node; 
    }

    /* Create a new node */
    new_node = ntt_node_create(key);
    new_node->timestamp = timestamp;
    new_node->timestamp = 0;

    ntt->items++;

    /* Insert */
    if (parent) {  /* Existing parent */
	parent->next = new_node;
        return new_node;  /* Return the locked node */
    }

    /* No existing parent; add directly to hash table */
    ntt->tbl[hash_code] = new_node;
    return new_node;
}

/* Tree destructor */

int ntt_destroy(struct ntt *ntt) {
    struct ntt_node *node, *next;
    struct ntt_c c;

    if (ntt == NULL) return -1;

    node = c_ntt_first(ntt, &c);
    while(node != NULL) {
        next = c_ntt_next(ntt, &c);
        ntt_delete(ntt, node->key);
        node = next;
    }

    free(ntt->tbl);
    free(ntt);
    ntt = (struct ntt *) NULL;

    return 0;
}

/* Delete a single node in the tree */

int ntt_delete(struct ntt *ntt, const char *key) {
    long hash_code;
    struct ntt_node *parent = NULL;
    struct ntt_node *node;
    struct ntt_node *del_node = NULL;

    if (ntt == NULL) return -1;

    hash_code = ntt_hashcode(ntt, key);
    node        = ntt->tbl[hash_code];

    while (node != NULL) {
        if (strcmp(key, node->key) == 0) {
            del_node = node;
            node = NULL;
        }

        if (del_node == NULL) {
          parent = node;
          node = node->next;
        }
    }

    if (del_node != NULL) {

        if (parent) {
            parent->next = del_node->next;
        } else {
            ntt->tbl[hash_code] = del_node->next;
        }

        free(del_node->key);
        free(del_node);
        ntt->items--;

        return 0;
    }

    return -5;
}

/* Point cursor to first item in tree */

struct ntt_node *c_ntt_first(struct ntt *ntt, struct ntt_c *c) {

    c->iter_index = 0;
    c->iter_next = (struct ntt_node *)NULL;
    return(c_ntt_next(ntt, c));
}

/* Point cursor to next iteration in tree */

struct ntt_node *c_ntt_next(struct ntt *ntt, struct ntt_c *c) {
    long index;
    struct ntt_node *node = c->iter_next;

    if (ntt == NULL) return NULL;

    if (node) {
        if (node != NULL) {
            c->iter_next = node->next;
            return (node);
        }
    }

    if (! node) {
        while (c->iter_index < ntt->size) {
            index = c->iter_index++;

            if (ntt->tbl[index]) {
                c->iter_next = ntt->tbl[index]->next;
                return(ntt->tbl[index]);
            }
        }
    }
    return((struct ntt_node *)NULL);
}

/* END NTT (Named Pointer Tree) Functions */


/* BEGIN Configuration Functions */

static const char *
get_hash_tbl_size(cmd_parms *cmd, void *dconfig, const char *value) {
  long n = strtol(value, NULL, 0);

  if (n<=0) {
    hash_table_size = DEFAULT_HASH_TBL_SIZE;
  } else  {
    hash_table_size = n;
  }

  return NULL;
}

static const char *
get_page_count(cmd_parms *cmd, void *dconfig, const char *value) {
  long n = strtol(value, NULL, 0);
  if (n<=0) {
    page_count = DEFAULT_PAGE_COUNT;
  } else {
    page_count = n;
  }

  return NULL;
}

static const char *
get_site_count(cmd_parms *cmd, void *dconfig, const char *value) {
  long n = strtol(value, NULL, 0);
  if (n<=0) {
    site_count = DEFAULT_SITE_COUNT;
  } else {
    site_count = n;
  }

  return NULL;
}

static const char *
get_page_interval(cmd_parms *cmd, void *dconfig, const char *value) {
  long n = strtol(value, NULL, 0);
  if (n<=0) {
    page_interval = DEFAULT_PAGE_INTERVAL;
  } else {
    page_interval = n;
  }

  return NULL;
}

static const char *
get_site_interval(cmd_parms *cmd, void *dconfig, const char *value) {
  long n = strtol(value, NULL, 0);
  if (n<=0) {
    site_interval = DEFAULT_SITE_INTERVAL;
  } else {
    site_interval = n;
  }

  return NULL;
}

static const char *
get_blocking_period(cmd_parms *cmd, void *dconfig, const char *value) {
  long n = strtol(value, NULL, 0);
  if (n<=0) {
    blocking_period = DEFAULT_BLOCKING_PERIOD;
  } else {
    blocking_period = n;
  }

  return NULL;
}

static const char *
get_log_dir(cmd_parms *cmd, void *dconfig, const char *value) {
  if (value != NULL && value[0] != 0) {
    if (log_dir != NULL)
      free(log_dir);
    log_dir = strdup(value);
  }

  return NULL;
}

static const char *
get_email_notify(cmd_parms *cmd, void *dconfig, const char *value) {
  if (value != NULL && value[0] != 0) {
    if (email_notify != NULL)
      free(email_notify);
    email_notify = strdup(value);
  }

  return NULL;
}

static const char *
get_system_command(cmd_parms *cmd, void *dconfig, const char *value) {
  if (value != NULL && value[0] != 0) {
    if (system_command != NULL)
      free(system_command);
    system_command = strdup(value);
  }
 
  return NULL;
} 

/* END Configuration Functions */

static const command_rec access_cmds[] =
{
	AP_INIT_TAKE1("DOSHashTableSize", get_hash_tbl_size, NULL, RSRC_CONF, 
		"Set size of hash table"),

        AP_INIT_TAKE1("DOSPageCount", get_page_count, NULL, RSRC_CONF,
		"Set maximum page hit count per interval"),

        AP_INIT_TAKE1("DOSSiteCount", get_site_count, NULL, RSRC_CONF,
		"Set maximum site hit count per interval"),

        AP_INIT_TAKE1("DOSPageInterval", get_page_interval, NULL, RSRC_CONF,
		"Set page interval"),

	AP_INIT_TAKE1("DOSSiteInterval", get_site_interval, NULL, RSRC_CONF,
		"Set site interval"),

        AP_INIT_TAKE1("DOSBlockingPeriod", get_blocking_period, NULL, RSRC_CONF,
		"Set blocking period for detected DoS IPs"),

	AP_INIT_TAKE1("DOSEmailNotify", get_email_notify, NULL, RSRC_CONF,
		"Set email notification"),

	AP_INIT_TAKE1("DOSLogDir", get_log_dir, NULL, RSRC_CONF,
		"Set log dir"),

	AP_INIT_TAKE1("DOSSystemCommand", get_system_command, NULL, RSRC_CONF,
		"Set system command on DoS"),

        AP_INIT_ITERATE("DOSWhitelist", whitelist, NULL, RSRC_CONF,
                "IP-addresses wildcards to whitelist"),

	{ NULL }
};

static void register_hooks(apr_pool_t *p) {
  ap_hook_access_checker(access_checker, NULL, NULL, APR_HOOK_MIDDLE);
  apr_pool_cleanup_register(p, NULL, apr_pool_cleanup_null, destroy_hit_list);
};

module AP_MODULE_DECLARE_DATA evasive20_module =
{
    STANDARD20_MODULE_STUFF,
    NULL,
    NULL,
    create_hit_list,
    NULL,
    access_cmds,
    register_hooks
};
