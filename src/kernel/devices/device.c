// device.c - minimal device tree (device.h).
#include <kernel/devices/device.h>
#include <kernel/klog.h>
#include <libk/stdio.h>

void dev_init_root(device_t *root) {
	if (root == NULL) {
		return;
	}

	root->name = "root";
	root->parent = NULL;
	root->sibling = NULL;
	root->children = NULL;
	root->drv = NULL;
	root->priv = NULL;
}

// Children are kept in a singly linked list; the newest child is the head.
void dev_add_child(device_t *parent, device_t *child) {
	if (parent == NULL || child == NULL) {
		return;
	}

	child->parent = parent;
	child->sibling = parent->children;
	parent->children = child;
}

void dev_remove_child(device_t *parent, device_t *child) {
	if (parent == NULL || child == NULL) {
		return;
	}

	device_t **link = &parent->children;
	while (*link != NULL) {
		if (*link == child) {
			*link = child->sibling;
			child->parent = NULL;
			child->sibling = NULL;
			return;
		}
		link = &(*link)->sibling;
	}
}

// Debug dump: one log line per device, indented two spaces per level.
void device_walk(device_t *dev, int depth) {
	char line[128];
	size_t used;

	for (; dev != NULL; dev = dev->sibling) {
		used = 0;
		for (int i = 0; i < depth && used + 2 < sizeof(line); i++) {
			line[used++] = ' ';
			line[used++] = ' ';
		}

		snprintf(line + used, sizeof(line) - used, "%s%s", dev->name != NULL ? dev->name : "(unnamed)",
						 dev->drv != NULL ? " [bound]" : "");
		printk("%s", line);

		device_walk(dev->children, depth + 1);
	}
}
