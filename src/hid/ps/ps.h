/* $Id: ps.h,v 1.5 2007-04-20 22:39:35 danmc Exp $ */

extern HID ps_hid;
extern void ps_hid_export_to_file (FILE *, HID_Attr_Val *);
extern void ps_start_file (FILE *);
extern void ps_calibrate_1 (double , double , int);
extern void hid_eps_init ();
