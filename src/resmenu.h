/* $Id: resmenu.h,v 1.2 2004-04-29 23:50:22 danmc Exp $ */

#ifndef __RESMENU_INCLUDED_
#define __RESMENU_INCLUDED_ 1

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

Widget MenuCreateFromResource(Widget menu, Resource *res,
			      Widget top, Widget left, int chain);

  void MenuSetFlag(char *flag, int value);

#ifdef __cplusplus
}
#endif

#endif
