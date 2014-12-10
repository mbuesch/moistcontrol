#ifndef ONOFFSWITCH_H_
#define ONOFFSWITCH_H_

enum onoff_state {
	ONOFF_IS_OFF,		/* Switch is "off". */
	ONOFF_IS_ON,		/* Switch is "on". */
	ONOFF_SWITCHED_OFF,	/* Switch was just turned "off". */
	ONOFF_SWITCHED_ON,	/* Switch was just turned "on". */
};

void onoffswitch_init(void);
void onoffswitch_work(void);
enum onoff_state onoffswitch_get_state();

#endif /* ONOFFSWITCH_H_ */
