#ifndef _GPXE_BIOS_NAP_H
#define _GPXE_BIOS_NAP_H

/** @file
 *
 * BIOS CPU sleeping
 *
 */

#ifdef NAP_PCBIOS
#define NAP_PREFIX_pcbios
#else
#define NAP_PREFIX_pcbios __pcbios_
#endif

#endif /* _GPXE_BIOS_NAP_H */
