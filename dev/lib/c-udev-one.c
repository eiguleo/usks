#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>

void print_device_properties(struct udev_device *dev) {
    struct udev_list_entry *properties, *entry;

    // Get the list of device properties
    properties = udev_device_get_properties_list_entry(dev);
    udev_list_entry_foreach(entry, properties) {
        const char *name = udev_list_entry_get_name(entry);
        const char *value = udev_list_entry_get_value(entry);
        printf("%s: %s\n", name, value);
    }
}

int main() {
    struct udev *udev;
    struct udev_device *dev;

    // Create a udev object
    udev = udev_new();
    if (!udev) {
        fprintf(stderr, "Cannot create udev context.\n");
        return 1;
    }

    // Create a udev device object for /dev/ttyUSB0
    dev = udev_device_new_from_syspath(udev, "/sys/class/tty/ttyUSB0");
    if (!dev) {
        fprintf(stderr, "Cannot create udev device for /dev/ttyUSB0.\n");
        udev_unref(udev);
        return 1;
    }

    // Print the device properties
    print_device_properties(dev);

    // Clean up
    udev_device_unref(dev);
    udev_unref(udev);

    return 0;
}