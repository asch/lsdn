#ifndef LSDN_ERRORS_H
#define LSDN_ERRORS_H

enum lsdn_err{
        LSDNE_OK = 0,
        LSDNE_PARSE,
        LSDNE_NOMEM,
};

typedef enum lsdn_err lsdn_err_t;

#endif
