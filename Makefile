CC ?= cc
AR ?= ar

PROJECT := glint
BUILD_DIR := build
TARGET := $(BUILD_DIR)/$(PROJECT)
GLFW_LIB := $(BUILD_DIR)/libglfw_x11.a

GLFW_DIR := vendor/glfw
GLAD_DIR := vendor/glad

APP_SRCS := src/main.c
GLFW_SRCS := \
	$(GLFW_DIR)/src/context.c \
	$(GLFW_DIR)/src/init.c \
	$(GLFW_DIR)/src/input.c \
	$(GLFW_DIR)/src/monitor.c \
	$(GLFW_DIR)/src/platform.c \
	$(GLFW_DIR)/src/vulkan.c \
	$(GLFW_DIR)/src/window.c \
	$(GLFW_DIR)/src/egl_context.c \
	$(GLFW_DIR)/src/glx_context.c \
	$(GLFW_DIR)/src/osmesa_context.c \
	$(GLFW_DIR)/src/null_init.c \
	$(GLFW_DIR)/src/null_joystick.c \
	$(GLFW_DIR)/src/null_monitor.c \
	$(GLFW_DIR)/src/null_window.c \
	$(GLFW_DIR)/src/linux_joystick.c \
	$(GLFW_DIR)/src/posix_module.c \
	$(GLFW_DIR)/src/posix_poll.c \
	$(GLFW_DIR)/src/posix_thread.c \
	$(GLFW_DIR)/src/posix_time.c \
	$(GLFW_DIR)/src/x11_init.c \
	$(GLFW_DIR)/src/x11_monitor.c \
	$(GLFW_DIR)/src/x11_window.c \
	$(GLFW_DIR)/src/xkb_unicode.c

APP_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(APP_SRCS))
GLFW_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(GLFW_SRCS))

CPPFLAGS += -I$(GLFW_DIR)/include -I$(GLFW_DIR)/src -I$(GLAD_DIR)/include
CPPFLAGS += -D_GLFW_X11 -D_DEFAULT_SOURCE -DNDEBUG

COMMON_CFLAGS := -std=c11 -Os -ffunction-sections -fdata-sections
APP_CFLAGS := -Wall -Wextra -Wpedantic -Werror
VENDOR_CFLAGS := -w

LDFLAGS += -Wl,--gc-sections -Wl,-s
LDLIBS := -lm

Q := @
ifneq ($(V),)
Q :=
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(APP_OBJS) $(GLFW_LIB)
	$(Q)$(CC) $(LDFLAGS) $(APP_OBJS) $(GLFW_LIB) -o $@ $(LDLIBS)

$(GLFW_LIB): $(GLFW_OBJS)
	$(Q)$(AR) rcs $@ $(GLFW_OBJS)

$(BUILD_DIR)/src/%.o: src/%.c
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_CFLAGS) $(APP_CFLAGS) -c $< -o $@

$(BUILD_DIR)/vendor/glfw/src/%.o: vendor/glfw/src/%.c
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_CFLAGS) $(VENDOR_CFLAGS) -c $< -o $@

clean:
	$(Q)rm -rf $(BUILD_DIR)
