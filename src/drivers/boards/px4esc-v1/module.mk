#
# Board-specific startup code for the PX4ESC
#

SRCS		 = px4esc_can.c \
		   px4esc_init.c \
		   px4esc_led.c \
		   ../../../drivers/device/cdev.cpp \
		   ../../../drivers/device/device.cpp \
		   ../../../modules/systemlib/up_cxxinitialize.c

MAXOPTIMIZATION	 = -Os
EXTRACFLAGS	= -Wno-packed
