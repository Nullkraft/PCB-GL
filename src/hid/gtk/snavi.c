/*
 * spacenavi.c - a proof-of-concept hack to access the
 * 3dconnexion space navigator
 *
 * Written by Simon Budig, placed in the public domain.
 * it helps to read http://www.frogmouth.net/hid-doco/linux-hid.html .
 *
 * Adapted to control pcb by Peter Clifton
 *
 * For the LED to work a patch to the linux kernel is needed:
 *   http://www.home.unix-ag.org/simon/files/spacenavigator-hid.patch
 *
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <linux/types.h>
#include <linux/input.h>
/*
Additional defines for a 3D joystick for (older) linux versions which do not
have these defines in <linux/input.h>
*/
#ifndef REL_RX
#define REL_RX                  0x03
#endif
#ifndef REL_RY
#define REL_RY                  0x04
#endif
#ifndef REL_RZ
#define REL_RZ                  0x05
#endif

#include <glib.h>

#define test_bit(bit, array)  (array [bit / 8] & (1 << (bit % 8)))

static void (*update_pan_cb)(int, int, int, gpointer);
static void (*update_roll_cb)(int, int, int, gpointer);
static void (*update_done_cb)(gpointer);
static void (*button_cb)(int, int, gpointer);
static gpointer cb_userdata;

int snavi_set_led (GIOChannel *snavi, int led_state)
{
  struct input_event event;
  GError      *error = NULL;
  gsize        bytes_written;

  event.type  = EV_LED;
  event.code  = LED_MISC;
  event.value = led_state;

  g_io_channel_seek_position (snavi, 0, G_SEEK_END, &error);
  if (error) {
    g_printerr ("Error: %s\n", error->message);
    /* FIXME: FREE THE ERROR??? */
    return FALSE;
  }

  g_io_channel_write_chars (snavi,
                            (gchar *) &event,
                            sizeof (struct input_event),
                            &bytes_written,
                            &error);

  if (error) {
    g_printerr ("Error: %s\n", error->message);
    /* FIXME: FREE THE ERROR??? */
    return FALSE;
  }

  g_io_channel_flush (snavi, &error);

  if (error) {
    g_printerr ("Error: %s\n", error->message);
    /* FIXME: FREE THE ERROR??? */
    return FALSE;
  }

  return bytes_written < sizeof (struct input_event);
}


#define BAND 5

gboolean snavi_event (GIOChannel   *source,
                      GIOCondition  condition,
                      gpointer      data)
{
  static gint axes[6] = { 0, 0, 0, 0, 0, 0 };
  static gint buttons[2] = { 0, 0 };
  int i = 0;

  struct       input_event event;
  GError      *error = NULL;
  gsize        bytes_read;

  g_io_channel_read_chars (source,
                           (gchar *) &event,
                           sizeof (struct input_event),
                           &bytes_read,
                           &error);

  if (error)
    {
      g_printerr ("%s\n", error->message);
      return FALSE;
    }

  switch (event.type)
    {
      case EV_REL:
        if (event.code <= REL_RZ)
          axes[event.code - REL_X] = event.value;
        break;

      case EV_KEY:
        if (event.code >= BTN_0 && event.code <= BTN_1)
          buttons[event.code - BTN_0] = event.value;

          button_cb (event.code - BTN_0, event.value, cb_userdata);
        break;

      case EV_SYN:
        /*
         * if multiple axes change simultaneously the linux
         * input system sends multiple EV_REL events.
         * EV_SYN indicates that all changes have been reported.
         */

        /* Deadband */
        for (i = 0; i < 6; i++) {
          if (axes[i] > -BAND &&
              axes[i] < BAND)
            axes[i] = 0;
        }

        update_pan_cb (axes[0] / 100.0,
                       axes[2] / 100.0,
                       axes[1] / 100.0, cb_userdata);
        update_roll_cb (axes[5] / 100.0,
                        axes[3] / 100.0,
                        axes[4] / 100.0, cb_userdata);
        update_done_cb (cb_userdata);

        axes[0] = axes[1] = axes[2] = axes[3] = axes[4] = axes[5] = 0;
        break;

      default:
        break;
    }


  return TRUE;
}


GIOChannel *
setup_snavi (void (*update_pan)(int, int, int, gpointer),
             void (*update_roll)(int, int, int, gpointer),
             void (*update_done)(gpointer),
             void (*button)(int, int, gpointer),
             gpointer data)
{
  int event_id;
  GIOChannel *snavi;

  update_pan_cb = update_pan;
  update_roll_cb = update_roll;
  update_done_cb = update_done;
  button_cb = button;
  cb_userdata = data;

  snavi = g_io_channel_new_file ("/dev/input/spacenavigator", "r+", NULL);
  if (snavi)
    {
      g_io_channel_set_encoding (snavi, NULL, NULL);
      event_id = g_io_add_watch (snavi, G_IO_IN, snavi_event, NULL);
    }

  return snavi;
}
