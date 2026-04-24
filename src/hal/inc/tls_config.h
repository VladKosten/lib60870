/*
 *  tls_config.h
 *
 *  Minimal TLS configuration forward declarations for builds without TLS stack.
 */

#ifndef TLS_CONFIG_H_
#define TLS_CONFIG_H_

#include "hal_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sTLSConfiguration* TLSConfiguration;

#ifdef __cplusplus
}
#endif

#endif /* TLS_CONFIG_H_ */
