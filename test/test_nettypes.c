#include <lsdn.h>

static void check_ip(lsdn_ip_t ip, const char* str)
{
	lsdn_ip_t actual;
	if(lsdn_parse_ip(&actual, str) != LSDNE_OK)
		abort();
	if(!lsdn_ip_eq(ip, actual))
		abort();
}

int main()
{
	check_ip(LSDN_MK_IPV4(1, 1, 1, 1), "1.1.1.1");
	check_ip(LSDN_MK_IPV4(128, 1, 1, 1), "128.1.1.1");
	check_ip(LSDN_MK_IPV4(12, 1, 1, 1), "12.1.1.1");
	check_ip(LSDN_MK_IPV4(1, 1, 1, 1), "001.1.1.1");
}
