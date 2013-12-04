#ifndef MAIN_H_
#define MAIN_H_

#include "util.h"

#include <stdint.h>


/* Timekeeping */
typedef uint32_t	jiffies_t;
typedef int32_t		s_jiffies_t;

/* time_after(a, b) returns true if the time a is after time b. */
#define time_after(a, b)        ((s_jiffies_t)(b) - (s_jiffies_t)(a) < 0)
#define time_before(a, b)       time_after(b, a)

/* Number of jiffies-per-second */
#define JPS			((jiffies_t)200)

/* Convert milliseconds to jiffies. */
#define msec_to_jiffies(ms)  ((jiffies_t)div_round_up(JPS * (uint32_t)(ms), (uint32_t)1000))
/* Convert seconds to jiffies. */
#define sec_to_jiffies(s)    ((jiffies_t)(JPS * (uint32_t)(s)))

jiffies_t jiffies_get(void);

#endif /* MAIN_H_ */
