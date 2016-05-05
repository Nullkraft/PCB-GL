#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "global.h"
#include "netclass.h"

static int num_netclass = 9;

static char *netclass_names[9] =
  {
    NULL,
    "PE",
    "BAT+",
    "PHASE1",
    "PHASE2",
    "PHASE3",
    "PHASE4",
    "PHASE5",
    "PHASE6"
  };

static Coord clearances[9][9] =
  {
    {MM_TO_COORD (0.2), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0)},
    {MM_TO_COORD (4.0), MM_TO_COORD (0.2), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0)},
    {MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (0.2), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0)},
    {MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (0.2), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0)},
    {MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (0.2), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0)},
    {MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (0.2), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0)},
    {MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (0.2), MM_TO_COORD (4.0), MM_TO_COORD (4.0)},
    {MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (0.2), MM_TO_COORD (4.0)},
    {MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (4.0), MM_TO_COORD (0.2)}
  };


static int
netclass_index (char *netclass)
{
  int i;

  for (i = 0; i < num_netclass; i++)
    if (netclass == NULL)
      {
        if (netclass_names[i] == NULL)
          return i;
      }
    else if (netclass_names[i] != NULL && strcmp (netclass_names[i], netclass) == 0)
      {
        return i;
      }

  g_return_val_if_fail (netclass != NULL, 0);
  g_warning ("Netclass '%s' not found, returning 0th (default) class", netclass);

  return 0;
}

static Coord
clearance_between_netclasses (int a, int b)
{
  g_return_val_if_fail (a >= 0 && a < num_netclass, 0);
  g_return_val_if_fail (b >= 0 && b < num_netclass, 0);

  return clearances[a][b];
}

Coord
get_clearance_between_netclasses (char *netclass_a, char *netclass_b)
{
  int a = netclass_index (netclass_a);
  int b = netclass_index (netclass_b);

  return clearance_between_netclasses (a, b);
}

char *
get_netclass_for_netname (char *netname)
{
  return NULL;
}

char *
get_netclass_for_pin ()
{
  char *netname = NULL;

//  netname = lookup_netname_for_pin (...);

  return get_netclass_for_netname (netname);
}

char *
get_netclass_at_xy (LayerType *layer, Coord x, Coord y)
{
  char *netname = NULL;

//  netname = find_netname_at_xy (layer, x, y);

  return get_netclass_for_netname (netname);
}
