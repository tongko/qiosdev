#pragma once

// Forward declarations
struct device;
struct driver;

typedef struct device {
	const char *name;
	struct device *parent;
	struct device *sibling;
	struct device *children;
	struct driver *drv; // bound driver
	void *priv;			  // private per-device data
} device_t;

typedef struct driver {
	const char *name;
	// probe: called when device added to tree
	int (*probe)(device_t *dev);
	void (*remove)(device_t *dev);
} driver_t;

// ==== Device tree API ====
void dev_init_root(device_t *root);
void dev_add_child(device_t *parent, device_t *child);
void dev_remove_child(device_t *parent, device_t *child);
void device_walk(device_t *dev, int depth); // debug dump tree