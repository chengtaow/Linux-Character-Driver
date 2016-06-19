# Linux-Character-Driver

Copy the files char_driver.c, Makefile and userapp.c to a virtual linux machineand follow the following steps:

1) Compile driver module : $ make

2) Load module : $ sudo insmod char_ driver.ko     NUM_ DEVICES = <num_devices>

3) Test driver :
	1. Compile userapp : $ make app
	2. Run userapp : $ sudo ./userapp <device_number>			
		where device_number identifies the id number of the device to be tested.   
