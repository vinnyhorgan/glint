CC ?= cc

BUILD_DIR := build
TARGET := $(BUILD_DIR)/gles2_test
LOCAL_SRCS := src/main.c

GLFW_DIR := vendor/glfw
GLAD_DIR := vendor/glad
GLFW_INC := -I$(GLFW_DIR)/include -I$(GLFW_DIR)/src -I$(GLAD_DIR)/include
GLFW_DEFS := -D_GLFW_X11 -D_DEFAULT_SOURCE
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

PKG_CONFIG ?= pkg-config
X11_PACKAGES := x11 xrandr xi xinerama xcursor xext

X11_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(X11_PACKAGES))
X11_LIBS := $(shell $(PKG_CONFIG) --libs $(X11_PACKAGES))

CFLAGS ?= -O2
CFLAGS += -std=c99 -Wall -Wextra -pedantic $(GLFW_INC) $(GLFW_DEFS) $(X11_CFLAGS)
LDLIBS += $(X11_LIBS) -ldl -lpthread -lm

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(LOCAL_SRCS) $(GLFW_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GLFW_SRCS) $(LOCAL_SRCS) -o $@ $(LDLIBS)

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
