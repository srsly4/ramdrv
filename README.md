# ramdrv
Linux kernel module driver for virtual RAM disk

## Requirements
 * Linux Kernel 3.10+ (also headers)
 * CMake 3.0+
 
## Compilation
At first you have to build kernel module. Make sure you had installed Linux headers before by (on Debian):
```
sudo apt-get install linux-headers-[VERSION]
```
To compile Kernel module:
```
cd kmod
make
```

Then, you have to build util library and __ramctl__, so in main directory:
```
cmake .
make
```
## Usage
### Module enabling
To enable compiled kernel module, type in the _kmod_ directory:
```
sudo insmod ramdrv.ko
mknod /dev/ramdrv c 250 0
```
To disable module just type `sudo rmmod ramdrv`.
### Creating ramdrive
In _userprog_ directory type:
```
sudo ./ramctl create [sectors]
```
where sectors is your size of ramdrive (1 sector = 512 bytes).

Now you can format and mount your drive:
```
sudo mkdir /mnt/ramdrv
sudo mkfs -t ext2 /dev/ramdrv0
sudo mount /dev/ramdrv0 /mnt/ramdrv
```

And that's all, you can use your ramdrive now.

### Deleting ramdrive
To delete exact ramdrive just type in _userprog_ directory:
```
sudo ./ramctl delete 0
```
where 0 is the disk minor identifier (in this case - in _ramdrv0_)
