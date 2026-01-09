/* vblock_ioctl.h */

#ifndef _VBLOCK_IOCTL_H_
#define _VBLOCK_IOCTL_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#define VBLOCK_SIZE         4096
#define VBLOCK_REGION_SIZE  512
#define VBLOCK_NUM_REGIONS  (VBLOCK_SIZE / VBLOCK_REGION_SIZE)

/* IOCTL magic */
#define VBLOCK_IOC_MAGIC    'v'

/* IOCTLs */

#define VBLOCK_LOCK_REGION    _IOW(VBLOCK_IOC_MAGIC, 1, int)
#define VBLOCK_UNLOCK_REGION  _IOW(VBLOCK_IOC_MAGIC, 2, int)
#define VBLOCK_BACKUP        _IOW('v', 20, char *)
#define VBLOCK_READ_MIRROR   _IOW('v', 21, struct vblock_region)
/* Read full 512 B region:
 * user passes .region_index; kernel fills .data[].
 */
struct vblock_region {
    __u32 region_index;
    __u8  data[VBLOCK_REGION_SIZE];
};

#define VBLOCK_READ_REGION    _IOWR(VBLOCK_IOC_MAGIC, 3, struct vblock_region)

/* Get info bitmap & geometry */
struct vblock_info {
    __u32 size;          /* total size bytes */
    __u32 region_size;   /* region size bytes (512) */
    __u32 num_regions;   /* 8 */
    __u8  lock_bitmap;   /* bit i = 1 => region i locked */
};

#define VBLOCK_GET_INFO      _IOR(VBLOCK_IOC_MAGIC, 4, struct vblock_info)

/* Erase (zero) a region: arg = int region_index */
#define VBLOCK_ERASE_REGION  _IOW(VBLOCK_IOC_MAGIC, 5, int)

#endif /* _VBLOCK_IOCTL_H_ */

