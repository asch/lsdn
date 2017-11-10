#include <unistd.h>
#include <linux/reboot.h>
#include <sys/reboot.h>

int main()
{
	sync();
	reboot(RB_POWER_OFF);
	return 0;
}
