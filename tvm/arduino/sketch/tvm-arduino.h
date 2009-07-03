#ifndef TVM_ARDUINO_H
#define TVM_ARDUINO_H

// Define for lots of useful printed-out stuff.
// #define DEBUG

#include <WProgram.h>
#include <avr/io.h>

// The Wiring stuff defines BYTE as 0, but the Transterpreter headers use it as
// a type name.
#undef BYTE

extern "C" {
	#include <tvm.h>
}

enum {
	TVM_INTR_TIMER = 1 << (SFLAG_USER_P + 0)
};
#define TVM_INTR_SFLAGS \
	(SFLAG_INTR | \
	TVM_INTR_TIMER)

//{{{  ffi.cpp
extern "C" {
	extern SFFI_FUNCTION sffi_table[];
	extern const int sffi_table_length;
}
//}}}
//{{{  interrupts.cpp
extern void clear_pending_interrupts (void);
extern bool waiting_on_interrupts (void);
extern "C" {
	extern EXT_CHAN_ENTRY ext_chan_table[];
	extern const int ext_chan_table_length;
}
//}}}
//{{{  serial.cpp
extern void serial_stdout_init(long speed);
//}}}
//{{{  tbc.cpp
extern int init_context_from_tbc (ECTX context, const prog_char *data, WORDPTR memory, UWORD memory_size);
//}}}
//{{{  tvm.cpp
extern tvm_ectx_t context;
extern int main (void);
//}}}

#endif
