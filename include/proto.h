#ifndef PROTO_H
#define PROTO_H

// Protocol constants and defaults
#define DEFAULT_PORT 5050
#define BACKLOG 16
#define MAX_LINE 1024

// Response prefixes
#define RESP_OK "OK"
#define RESP_ERR "ERR"

// Error codes
#define ERR_INV "EINV"  // invalid input
#define ERR_ZDV "EZDV"  // division by zero
#define ERR_SRV "ESRV"  // internal server error

#endif // PROTO_H
