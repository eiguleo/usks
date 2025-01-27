#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
  struct udev *udev;
  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;
  struct udev_device *dev;

  // 创建udev对象
  udev = udev_new();
  if (!udev) {
    fprintf(stderr, "Cannot create udev\n");
    return 1;
  }

  // 创建枚举器
  enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, "usb");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);

  // 遍历设备列表
  udev_list_entry_foreach(dev_list_entry, devices) {
    const char *path = udev_list_entry_get_name(dev_list_entry);
    dev = udev_device_new_from_syspath(udev, path);

    const char *devnode = udev_device_get_devnode(dev);
    const char *idVendor = udev_device_get_sysattr_value(dev, "idVendor");
    const char *idProduct = udev_device_get_sysattr_value(dev, "idProduct");
    const char *manufacturer =
        udev_device_get_sysattr_value(dev, "manufacturer");
    const char *product = udev_device_get_sysattr_value(dev, "product");
    const char *serial = udev_device_get_sysattr_value(dev, "serial");

    printf("Device Node Path: %s\n", devnode);
    printf("  Vendor ID: %s\n", idVendor);
    printf("  Product ID: %s\n", idProduct);
    printf("  Manufacturer: %s\n", manufacturer);
    printf("  Product: %s\n", product);
    printf("  Serial: %s\n", serial);

    udev_device_unref(dev);
  }

  // 释放资源
  udev_enumerate_unref(enumerate);
  udev_unref(udev);

  return 0;
}