#include <sys/types.h>
#include "bsp_init.h"
#include "bsp_padconf.h"
#include "omap_rtc.h"
#include "bsp_reset.h"

void
omap_init(void)
{

	/* map memory for padconf */
	omap_padconf_init();

	/* map memory for rtc */
	omap3_rtc_init();

	/* map memory for reset control */
	omap_reset_init();

	/* disable watchdog */
	omap_disable_watchdog();
}
