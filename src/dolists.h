/* $Id: dolists.h,v 1.3 2006-03-22 22:49:29 danmc Exp $ */

#undef REGISTER_ACTIONS
#undef REGISTER_ATTRIBUTES
#undef REGISTER_FLAGS

#define REGISTER_ACTIONS(a) extern void HIDCONCAT(register_,a)();HIDCONCAT(register_,a)()
#define REGISTER_ATTRIBUTES(a) extern void HIDCONCAT(register_,a)();HIDCONCAT(register_,a)()
#define REGISTER_FLAGS(a) extern void HIDCONCAT(register_,a)();HIDCONCAT(register_,a)()

