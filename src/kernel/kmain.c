#include <bootinfo.h>
#include <stdbool.h>

void kmain(bootinfo_t *bi) {
	(void)bi;

	while (true) {
		__asm__ volatile("hlt");
	}
}