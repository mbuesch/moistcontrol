#ifndef RV3029_H_
#define RV3029_H_

#include "util.h"
#include "datetime.h"

#include <stdint.h>


void rv3029_write_time(const struct rtc_time *time);
void rv3029_read_time(void);
void rv3029_get_time(struct rtc_time *time);

void rv3029_init(void);

#endif /* RV3029_H_ */
