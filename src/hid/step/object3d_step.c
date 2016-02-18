#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include <glib.h>

#include "hid/common/step_id.h"
#include "hid/common/quad.h"
#include "hid/common/vertex3d.h"
#include "hid/common/contour3d.h"
#include "hid/common/appearance.h"
#include "hid/common/face3d.h"
#include "hid/common/edge3d.h"
#include "hid/common/object3d.h"
#include "polygon.h"
#include "data.h"

#include "step_writer.h"

#include "pcb-printf.h"

#include "object3d_step.h"

