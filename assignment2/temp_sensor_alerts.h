#ifndef _TEMP_SENSOR_ALERTS_H_
#define _TEMP_SENSOR_ALERTS_H_

#include <linux/ioctl.h>

/* Must match driver definitions */

#define TEMP_IOC_MAGIC          'T'

#define TEMP_IOC_SET_THRESHOLDS _IOW(TEMP_IOC_MAGIC, 1, struct temp_thresholds)
#define TEMP_IOC_GET_STATE      _IOR(TEMP_IOC_MAGIC, 2, struct temp_state)

/* Alert Type */
enum temp_alert_type {
    ALERT_NONE = 0,
    ALERT_LOW  = 1,
    ALERT_HIGH = 2,
};

/* Thresholds (in milli-degree C) */
struct temp_thresholds {
    int low_mdeg;
    int high_mdeg;
};

/* Snapshot returned from GET_STATE ioctl */
struct temp_state {
    int  current_temp_mdeg;
    int  low_mdeg;
    int  high_mdeg;
    enum temp_alert_type last_alert_type;
};

/* Structure returned to userspace via read() */
struct temp_alert_user {
    int  alert_id;
    int  temp_mdeg;
    enum temp_alert_type type;
};

#endif

