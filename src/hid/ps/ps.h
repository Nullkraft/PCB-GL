/* $Id: ps.h,v 1.4 2006-12-01 03:36:58 danmc Exp $ */

extern HID ps_hid;
extern void ps_hid_export_to_file (FILE *, HID_Attr_Val *);
extern void ps_start_file (FILE *);

extern void hid_eps_init ();
