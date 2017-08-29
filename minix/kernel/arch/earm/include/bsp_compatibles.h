#ifndef __BSP_COMPAT_H_
#define __BSP_COMPAT_H__

char *pi_compat_tb[] = {
	"brcm,bcm2709",
	"brcm,bcm2710",
	NULL
};

char **compatible_table[] = {
	pi_compat_tb,
	NULL
};

#endif