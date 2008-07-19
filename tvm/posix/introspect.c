/*
 * introspect.c - TVM introspection layer
 *
 * Copyright (C) 2008  Carl G. Ritson
 *
 */

#include "tvm_posix.h"
#include <tvm_tbc.h>

#define MT_DEFINES	1
#define MT_TVM		1
#include <mobile_types.h>

/*{{{  Very Limited Protocol Decoder */

/* Channel State Type Pre-declaration */
typedef struct c_state_t c_state_t;

/* Protocol Argument Types */
enum {
	P_A_ARRAY	= 'A',
	P_A_BYTE	= 'b',
	P_A_BYTES	= 'B',
	P_A_MT		= 'M',
	P_A_WORD	= 'w'
};

/* Protocol Argument */
typedef struct p_arg_t {
	int			type;
	int			length;
	union {
		BYTE		*array;
		BYTE		byte;
		BYTE		bytes[4];
		WORDPTR		mt;
		WORD		word;
	} data;
} TVM_PACK p_arg_t;

/* Protocol Case Description */
typedef struct pc_desc_t {
	int		entry;
	int		argc;
	p_arg_t		argv[8];
} pc_desc_t;

/* Protocol Symbol */
typedef struct p_sym_t {
	int	entry;
	char	*symbols;
	int	(*dispatch)(ECTX, c_state_t *);
} p_sym_t;

/* Channel States */
enum {
	C_S_IDLE,
	C_S_ENCODE_ENTRY,
	C_S_ENCODE,
	C_S_DECODE
};

/* Channel Type */
enum {
	C_T_UNKNOWN,
	C_T_BYTECODE,
	C_T_VM,
	C_T_VM_CTL
};


/* Channel State */
struct c_state_t {
	int		state;
	WORDPTR		waiting;
	p_sym_t		*in;
	p_sym_t		*decode;
	pc_desc_t	p;
	int		type;
	union {
		bytecode_t	*bc;
		ECTX		vm;
	} 		data;
};

static int handle_in (ECTX ectx,
	void *data, WORDPTR channel, BYTEPTR address, WORD count)
{
	c_state_t *c = (c_state_t *) data;

	if (c->state == C_S_ENCODE_ENTRY) {
		if (count == 1) {
			write_byte (address, (BYTE) c->p.entry);
			
			if (c->p.argc > 0) {
				c->state = C_S_ENCODE;
			} else {
				c->state = C_S_IDLE;
			}
			
			return ECTX_CONTINUE;
		} else {
			return ectx->set_error_flag (ectx, EFLAG_EXTCHAN);
		}
	} else if (c->state == C_S_ENCODE) {
		int	n	= --c->p.argc;
		p_arg_t *arg	= &(c->p.argv[n]);
		int	i;
		
		if (arg->type == P_A_WORD && count == sizeof (WORD)) {
			write_word ((WORDPTR) address, arg->data.word);
		} else if (arg->type == P_A_BYTE && count == 1) {
			write_byte (address, arg->data.byte);
		} else if (arg->type == P_A_BYTES && count == arg->length) {
			for (i = 0; i < arg->length; ++i) {
				write_byte (address, arg->data.bytes[i]);
				address = byteptr_plus (address, 1);
			}
		} else if (arg->type == P_A_ARRAY && count == arg->length) {
			for (i = 0; i < arg->length; ++i) {
				write_byte (address, arg->data.array[i]);
				address = byteptr_plus (address, 1);
			}
			free (arg->data.array);
		} else {
			return ectx->set_error_flag (ectx, EFLAG_EXTCHAN);
		}
		
		if (c->p.argc == 0) {
			c->state = C_S_IDLE;
		}

		return ECTX_CONTINUE;
	} else {
		WORKSPACE_SET (ectx->wptr, WS_POINTER, (WORD) address);
		WORKSPACE_SET (ectx->wptr, WS_ECTX, (WORD) ectx);
		WORKSPACE_SET (ectx->wptr, WS_PENDING, count);
		WORKSPACE_SET (ectx->wptr, WS_IPTR, (WORD) ectx->iptr);

		c->waiting = ectx->wptr;

		return _ECTX_DESCHEDULE;
	}
}

static int handle_out (ECTX ectx,
	void *data, WORDPTR channel, BYTEPTR address, WORD count)
{
	c_state_t *c = (c_state_t *) data;
	int i;

	if (c->state == C_S_IDLE && count == 1) {
		int entry = read_byte (address);
		
		for (i = 0; c->in[i].entry != entry; ++i) {
			if (c->in[i].symbols == NULL) {
				return ectx->set_error_flag (ectx, EFLAG_EXTCHAN);
			}
		}

		c->p.entry	= entry;
		c->p.argc	= 0;

		if (c->in[i].symbols[0] == '\0') {
			return c->in[i].dispatch (ectx, c);
		} else {
			c->state	= C_S_DECODE;
			c->decode 	= &(c->in[i]);
			return ECTX_CONTINUE;
		}
	} else if (c->state == C_S_DECODE) {
		int n		= c->p.argc++;
		p_arg_t	*arg	= &(c->p.argv[n]);
		
		arg->length	= 0;

		switch (c->decode->symbols[n]) {
			case '4': arg->length++;
			case '3': arg->length++;
			case '2': arg->length++;
			case '1': arg->length++;
			case '0': 
				arg->type = 'B';
				break;
			default:
				arg->type = c->decode->symbols[n];
				break;
		}

		if (arg->type == P_A_WORD && count == sizeof (WORD)) {
			arg->data.word = read_word ((WORDPTR) address);
		} else if (arg->type == P_A_BYTE && count == 1) {
			arg->data.byte = read_byte (address);
		} else if (arg->type == P_A_BYTES && count == arg->length) {
			for (i = 0; i < arg->length; ++i) {
				arg->data.bytes[i] = read_byte (address);
				address = byteptr_plus (address, 1);
			}
		} else if (arg->type == P_A_ARRAY) {
			arg->length	= count;
			arg->data.array = (BYTE *) malloc (count);
			for (i = 0; i < arg->length; ++i) {
				arg->data.array[i] = read_byte (address);
				address = byteptr_plus (address, 1);
			}
		} else {
			return ectx->set_error_flag (ectx, EFLAG_EXTCHAN);
		}
		
		if (c->decode->symbols[n+1] == '\0') {
			c->state = C_S_IDLE;
			return c->decode->dispatch (ectx, c);
		} else {
			return ECTX_CONTINUE;
		}
	}

	return ectx->set_error_flag (ectx, EFLAG_EXTCHAN);
}

static int handle_mt_in (ECTX ectx, 
	void *data, WORDPTR channel, WORDPTR address)
{
	c_state_t *c = (c_state_t *) data;
	
	if (c->state == C_S_ENCODE) {
		int	n	= --c->p.argc;
		p_arg_t *arg	= &(c->p.argv[n]);
		
		if (arg->type == P_A_MT) {
			write_word (address, (WORD) arg->data.mt);

			if (c->p.argc == 0) {
				c->state = C_S_IDLE;
			}
			
			return ECTX_CONTINUE;
		}
	}
	
	return ectx->set_error_flag (ectx, EFLAG_EXTCHAN);
}

static int handle_mt_out (ECTX ectx, 
	void *data, WORDPTR channel, WORDPTR address)
{
	c_state_t *c = (c_state_t *) data;

	if (c->state == C_S_DECODE) {
		int n		= c->p.argc++;
		p_arg_t	*arg	= &(c->p.argv[n]);
		
		arg->type	= c->decode->symbols[n];
		arg->length	= 0;

		if (arg->type == P_A_MT) {
			arg->data.word = read_word ((WORDPTR) address);
			/* FIXME: really we should write NULL_P back to the
			 * 	  address when the mobile type is not shared.
			 */
			if (c->decode->symbols[n+1] == '\0') {
				c->state = C_S_IDLE;
				return c->decode->dispatch (ectx, c);
			} else {
				return ECTX_CONTINUE;
			}
		}
	}

	return ectx->set_error_flag (ectx, EFLAG_EXTCHAN);
}

static void handle_free (ECTX ectx, void *data)
{
	c_state_t *c = (c_state_t *) data;
	if (c->type == C_T_BYTECODE) {
		free_bytecode (c->data.bc);
	} else if (c->type == C_T_VM_CTL) {
		free_ectx (c->data.vm);
	}
	free (c);
}

static int send_message (ECTX ectx, c_state_t *c, p_sym_t *sym, ...)
{
	va_list args;
	char	*fmt	= sym->symbols;
	int	i;

	c->state	= C_S_ENCODE_ENTRY;
	c->p.entry	= sym->entry;
	c->p.argc	= strlen (fmt);
	i		= c->p.argc;

	va_start (args, sym);
	
	while (i) {
		int type = (int) *fmt;

		i--;
		c->p.argv[i].type = type;

		if (type == P_A_ARRAY) {
			c->p.argv[i].data.array = va_arg (args, BYTE *); 
			c->p.argv[i].length	= va_arg (args, int);
		} else if (type == P_A_BYTES) {
			BYTE *bytes		= va_arg (args, BYTE *);
			int length		= va_arg (args, int);
			int j;
			
			c->p.argv[i].length	= length;
			
			for (j = 0; j < length; ++j) {
				c->p.argv[i].data.bytes[j] = bytes[j];
			}
		} else {
			c->p.argv[i].length	= 0;
			c->p.argv[i].data.word	= va_arg (args, WORD);
		}

		fmt++;
	}

	va_end (args);

	if (c->waiting != NOT_PROCESS_P) {
		WORDPTR wptr	= c->waiting;
		BYTEPTR address = (BYTEPTR) WORKSPACE_GET (wptr, WS_POINTER);
		WORD count	= WORKSPACE_GET (wptr, WS_PENDING);

		c->waiting = NOT_PROCESS_P;

		ectx->add_to_queue_external (ectx, ectx, wptr);

		return handle_in (ectx, c, NULL_P, address, count);
	} else {
		return ECTX_CONTINUE;
	}
}

static EXT_CB_INTERFACE cb_interface = {
	.in	= handle_in,
	.out	= handle_out,
	.swap	= NULL,
	.mt_in	= handle_mt_in,
	.mt_out	= handle_mt_out,
	.xable	= NULL,
	.xin	= NULL,
	.mt_xin	= NULL,
	.free	= handle_free
};

#define MT_CB_CONFIG (MT_SIMPLE |\
			MT_MAKE_TYPE (MT_CB) |\
			MT_CB_EXTERNAL |\
			(2 << MT_CB_CHANNELS_SHIFT))

static WORDPTR allocate_cb (ECTX ectx, p_sym_t *in, c_state_t **c_ptr)
{
	c_state_t 	*c = (c_state_t *) malloc (sizeof (c_state_t));
	WORDPTR		cb = tvm_mt_alloc (ectx, MT_CB_CONFIG, 2);
	WORDPTR		cb_base;

	if (cb == (WORDPTR) NULL_P) {
		free (c);
		return (WORDPTR) NULL_P;
	}

	c->state	= C_S_IDLE;
	c->waiting	= (WORDPTR) NOT_PROCESS_P;
	c->in		= in;
	c->decode	= NULL;
	c->p.argc	= 0;
	*c_ptr		= c;

	cb_base = wordptr_minus (cb, MT_CB_PTR_OFFSET + (sizeof (mt_cb_ext_t) / sizeof (WORD)));

	write_offset (cb_base, mt_cb_ext_t, interface, (WORD) &cb_interface);
	write_offset (cb_base, mt_cb_ext_t, data, (WORD) c);

	return cb;
}

WORDPTR str_to_mt (ECTX ectx, const char *str)
{
	WORDPTR mt = (WORDPTR) NULL_P;
	
	if (str != NULL) {
		int length = strlen (str);
		
		/* Allocate mobile type */
		mt = tvm_mt_alloc (ectx, MT_MAKE_ARRAY_TYPE (1, MT_NUM_BYTE), length);
		/* Copy string into it */
		memcpy (
			wordptr_real_address ((WORDPTR) read_word (mt)),
			str,
			length
		);
		/* Set array dimension to string length */
		write_word (wordptr_plus (mt, 1), length);
	}
	
	return mt;
}
/*}}}*/

/*
VAL INT CLOCK.STEP IS 1:
DATA TYPE ADDR IS INT:
DATA TYPE IPTR IS INT:

DATA TYPE VM.STATE
  PACKED RECORD
    INT        state:
    [3]INT     stack:
    [4]BYTE    type:
    INT        oreg:
    ADDR       wptr:
    IPTR       iptr:
    INT        icount:
:
*/

typedef struct vm_state_t {
	WORD	state;
	WORD	stack[3];
	BYTE	type[4];
	WORD	oreg;
	WORD	wptr;
	WORD	iptr;
	WORD	icount;
} vm_state_t;

/*
PROTOCOL P.VM.CTL.RQ
  CASE
    run; INT               -- run until for N instructions or until breakpoint
    step                   -- step traced instruction
    dispatch; INT          -- dispatch an arbitrary instruction
    set.bp; IPTR           -- set break point
    clear.bp; IPTR         -- clear break point
    get.clock              -- get clock details
    set.clock; INT; INT    -- set clock type and frequency
    trace; INT; BOOL       -- enable/disable trace type (instruction)
    get.state              -- get VM state
    set.state; VM.STATE    -- set VM state
    read.word; ADDR        -- read word at address
    read.byte; ADDR        -- read byte at address
    read.int16; ADDR       -- read int16 at address
    read.type; ADDR        -- read type of memory at address
    return.param; INT      -- release parameter N 
                           -- set parameter N to channel
    set.param.chan; INT; MOBILE.CHAN
:
PROTOCOL P.VM.CTL.RE
  CASE
    decoded; IPTR; INT     -- decode instruction, new IPTR
    dispatched; IPTR; ADDR; INT  -- dispatched instruction, new IPTR and WPTR
    bp; IPTR               -- break pointer IPTR reached
    clock; INT; INT        -- clock type and frequency
    ok
    error; INT
    state; VM.STATE
    word; INT
    byte; BYTE
    int16; INT16
    type; INT
    channel; MOBILE.CHAN
:
CHAN TYPE CT.VM.CTL
  MOBILE RECORD
    CHAN P.VM.CTL.RQ request?:
    CHAN P.VM.CTL.RE response!:
:
*/

/*
PROTOCOL P.BYTECODE.RQ
  CASE
    run            = 0
    get.symbol     = 1; MOBILE []BYTE   -- look up symbol name
    get.symbol.at  = 2; IPTR            -- look up symbol at bytecode offset
    get.file       = 3; INT             -- translate file number to name
    get.line.info  = 4; IPTR            -- get file/line number of address
:
PROTOCOL P.BYTECODE.RE
  CASE
    vm             = 0; CT.VM.CTL
    error          = 1; INT
    file           = 2; MOBILE []BYTE
    line.info      = 3; INT; INT       -- file, line
                                       -- offset, name, definition, ws, vs
    symbol         = 4; INT; MOBILE []BYTE; MOBILE []BYTE; INT; INT
:
CHAN TYPE CT.BYTECODE
  MOBILE RECORD
    CHAN P.BYTECODE.RQ request?:
    CHAN P.BYTECODE.RE response!:
:
*/

static int bytecode_run (ECTX, c_state_t *);
static int bytecode_get_symbol (ECTX, c_state_t *);
static int bytecode_get_file (ECTX, c_state_t *);
static int bytecode_get_line_info (ECTX, c_state_t *);

static p_sym_t bytecode_rq[] = {
	{ .entry	= 0, .symbols = "", .dispatch = bytecode_run },
	{ .entry	= 1, .symbols = "M", .dispatch = bytecode_get_symbol },
	{ .entry	= 2, .symbols = "w", .dispatch = bytecode_get_symbol },
	{ .entry	= 3, .symbols = "w", .dispatch = bytecode_get_file },
	{ .entry	= 4, .symbols = "w", .dispatch = bytecode_get_line_info },
	{ .symbols = NULL }
};

enum {
	BYTECODE_RE_VM		= 0,
	BYTECODE_RE_ERROR	= 1,
	BYTECODE_RE_FILE	= 2,
	BYTECODE_RE_INFO	= 3,
	BYTECODE_RE_SYMBOL	= 4
};
static p_sym_t bytecode_re[] = {
	{ .entry = BYTECODE_RE_VM,	.symbols = "M", .dispatch = NULL },
	{ .entry = BYTECODE_RE_ERROR,	.symbols = "w", .dispatch = NULL },
	{ .entry = BYTECODE_RE_FILE,	.symbols = "M", .dispatch = NULL },
	{ .entry = BYTECODE_RE_INFO,	.symbols = "ww", .dispatch = NULL },
	{ .entry = BYTECODE_RE_SYMBOL,	.symbols = "wMMww", .dispatch = NULL },
	{ .symbols = NULL }
};

static int bytecode_run (ECTX ectx, c_state_t *c)
{
	return send_message (ectx, c, &(bytecode_re[BYTECODE_RE_ERROR]), 0);
}

static int bytecode_get_symbol (ECTX ectx, c_state_t *c)
{
	tbc_t		*tbc = c->data.bc->tbc;
	tbc_sym_t 	*sym = tbc->symbols;

	if (c->p.argv[0].type == P_A_MT) {
		/* symbol name */
		WORDPTR mt	= c->p.argv[0].data.mt;
		char *str	= (char *) 
			wordptr_real_address ((WORDPTR) read_word (wordptr_plus (mt, 0)));
		int length	= read_word (wordptr_plus (mt, 1));

		while (sym != NULL) {
			if (memcmp (sym->name, str, length) == 0) {
				if (sym->name[length] == '\0')
					break;
			}
			sym = sym->next;
		}

		tvm_mt_release (ectx, mt);
	} else {
		/* bytecode offset */
		unsigned int iptr = (unsigned int) c->p.argv[0].data.word;
		
		if (iptr < tbc->bytecode_len) {
			while (sym != NULL) {
				if (sym->offset <= iptr)
					break;
				sym = sym->next;
			}
		}
	}

	if (sym != NULL) {
		return send_message (ectx, c, &(bytecode_re[BYTECODE_RE_SYMBOL]),
			(WORD) sym->offset,
			str_to_mt (ectx, sym->name),
			str_to_mt (ectx, sym->definition),
			(WORD) sym->ws,
			(WORD) sym->vs
		);
	} else {
		return send_message (ectx, c, &(bytecode_re[BYTECODE_RE_ERROR]), 0);
	}
}

static int bytecode_get_file (ECTX ectx, c_state_t *c)
{
	if (c->data.bc->tbc->debug != NULL) {
		tenc_str_t *file	= c->data.bc->tbc->debug->files;
		int n			= c->p.argv[0].data.word;
		
		while (file != NULL) {
			if (n == 0) {
				return 	send_message (ectx, c, 
						&(bytecode_re[BYTECODE_RE_FILE]),
						str_to_mt (ectx, file->str)
				);
			}
			file = file->next;
			n--;
		}
	}
	
	return send_message (ectx, c, &(bytecode_re[BYTECODE_RE_ERROR]), 0);
}

static int bytecode_get_line_info (ECTX ectx, c_state_t *c)
{
	if (c->data.bc->tbc->debug != NULL) {
		tbc_dbg_t 	*dbg = c->data.bc->tbc->debug;
		unsigned int	iptr = (unsigned int) c->p.argv[0].data.word;
		int 		i;

		/* FIXME: use a binary search instead of a linear scan */

		for (i = 0; i < dbg->n_lnd; ++i) {
			if (dbg->lnd[i].offset <= iptr) {
				return send_message (ectx, c, &(bytecode_re[BYTECODE_RE_INFO]),
					dbg->lnd[i].file,
					dbg->lnd[i].line
				);
			}
		}
	}
	
	return send_message (ectx, c, &(bytecode_re[BYTECODE_RE_ERROR]), 0);
}
/*}}}*/

/*{{{  CT.VM */
/*
PROTOCOL P.VM.RQ
  CASE
    decode.bytecode = 0; MOBILE []BYTE -- TBC data
    load.bytecode   = 1; MOBILE []BYTE -- resource path
:
PROTOCOL P.VM.RE
  CASE
    bytecode        = 0; CT.BYTECODE!
    error           = 1; INT
:
CHAN TYPE CT.VM
  MOBILE RECORD
    CHAN P.VM.RQ request?:
    CHAN P.VM.RE response!:
:
*/

static int vm_rq_decode_bytecode (ECTX, c_state_t *);
static int vm_rq_load_bytecode (ECTX, c_state_t *);

static p_sym_t vm_rq[] = {
	{ .entry = 0, .symbols = "M", .dispatch = vm_rq_decode_bytecode },
	{ .entry = 1, .symbols = "M", .dispatch = vm_rq_load_bytecode },
	{ .symbols = NULL }
};

enum {
	VM_RE_BYTECODE	= 0,
	VM_RE_ERROR	= 1
};
static p_sym_t vm_re[] = {
	{ .entry = VM_RE_BYTECODE,	.symbols = "M", .dispatch = NULL },
	{ .entry = VM_RE_ERROR,		.symbols = "w", .dispatch = NULL },
	{ .symbols = NULL }
};

static int vm_rq_decode_bytecode (ECTX ectx, c_state_t *c)
{
	WORDPTR 	mt	= c->p.argv[0].data.mt;
	bytecode_t	*bc	= (bytecode_t *) malloc (sizeof (bytecode_t));
	c_state_t	*cb_c;
	WORDPTR 	cb;

	/* FIXME: verify mobile type */
	
	bc->refcount	= 1;
	bc->source 	= NULL;
	bc->length	= (int)	read_word (wordptr_plus (mt, 1));
	bc->data	= (BYTE *) malloc (bc->length);
	memcpy (bc->data, 
		(BYTE *) wordptr_real_address ((WORDPTR) read_word (wordptr_plus (mt, 0))),
		bc->length
	);

	tvm_mt_release (ectx, mt);

	if ((bc->tbc = decode_tbc (bc->data, bc->length)) == NULL) {
		free_bytecode (bc);
		return send_message (ectx, c, &(vm_re[VM_RE_ERROR]), 0);
	}

	cb		= allocate_cb (ectx, bytecode_rq, &cb_c);
	cb_c->type 	= C_T_BYTECODE;
	cb_c->data.bc	= bc;

	return send_message (ectx, c, &(vm_re[VM_RE_BYTECODE]), cb);
}

static int vm_rq_load_bytecode (ECTX ectx, c_state_t *c)
{
	WORDPTR 	mt	= c->p.argv[0].data.mt;
	bytecode_t	*bc;
	c_state_t	*cb_c;
	WORDPTR 	cb;
	char		*file;
	int		length;

	length	= (int) read_word (wordptr_plus (mt, 1));
	file	= (char *) malloc (length + 1);
	memcpy (file, 
		(BYTE *) wordptr_real_address ((WORDPTR) read_word (wordptr_plus (mt, 0))),
		length
	);
	file[length] = '\0';
	tvm_mt_release (ectx, mt);

	bc = load_bytecode (file);
	free (file);

	if (bc == NULL) {
		return send_message (ectx, c, &(vm_re[VM_RE_ERROR]), 0);
	}

	cb		= allocate_cb (ectx, bytecode_rq, &cb_c);
	cb_c->type 	= C_T_BYTECODE;
	cb_c->data.bc	= bc;

	return send_message (ectx, c, &(vm_re[VM_RE_BYTECODE]), cb);
}
/*}}}*/


/*{{{  Virtual Channel 0 - outputs VM channel bundles */
int vc0_mt_in (ECTX ectx, WORDPTR address)
{
	c_state_t 	*c;
	WORDPTR 	cb = allocate_cb (ectx, vm_rq, &c);

	if (cb == (WORDPTR) NULL_P) {
		return ectx->set_error_flag (ectx, EFLAG_MT);
	}

	/* Setup as VM type */
	c->type 	= C_T_VM;
	c->data.vm 	= ectx;

	/* Reduce reference count to 1 */
	tvm_mt_release (ectx, cb);
	
	/* Pass remaining reference to client */
	write_word (address, (WORD) cb);

	return ECTX_CONTINUE;
}
/*}}}*/

