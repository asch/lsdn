#ifndef LSDN_ERRORS_H
#define LSDN_ERRORS_H

enum lsdn_err {
	LSDNE_OK = 0,
	LSDNE_PARSE,
	LSDNE_NOMEM,
	/* Attributes required for attaching a phys to given type of net are missing */
	LSDNE_MISSING_ATTR,
	/* Can not connect virt to a network trough phys which is not attached to that network */
	LSDNE_NOT_ATTACHED,
	/* Internal usage */
	LSDNE_NOIF,
};

typedef enum lsdn_err lsdn_err_t;

#endif
