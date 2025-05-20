#gcc freeglut makefile
#####################################################
target := tank.exe
project_path := ./test
#####################################################
#project_build := $(wildcard $(project_path)/*.c)
# 使用 shell 命令递归查找 src 目录下的所有 .c 文件
project_build := $(shell find $(project_path) -name "*.c")

$(info all .c files: $(project_build))
# 过滤掉符合排除模式的文件
# 使用 find 命令结合 shell 函数来实现递归查找和过滤
exclude_files := $(shell find $(project_path) -name "not_make_*.c")
project_build := $(filter-out $(exclude_files), $(project_build))

# 打印过滤后的待编译文件列表
$(info Filtered project build(.c): $(project_build))

LDFLAGS = -lm -lbsd  # 添加链接数学库等选项

project_build_o := $(patsubst %.c,%.o,$(project_build))

$(info Filtered project build(.o): $(project_build_o))

SDL_FLAGS = $(shell pkg-config --cflags --libs sdl2 SDL2_ttf)

# 指定头文件搜索路径列表(针对的是project_path变量目录下的.c文件)
#header_paths := ./src/view ./src/event
header_paths := $(shell find $(project_path) -type d -mindepth 1 -printf '%p\n')
# 生成 -I 选项字符串
CFLAGS = $(foreach path,$(header_paths),-I "$(path)")
$(info CFLAGS: $(CFLAGS))

all:$(project_build_o)
	gcc $(project_build_o) \
	$(SDL_FLAGS) -o $(target) -L. $(LDFLAGS)

$(project_path)/%.o:$(project_path)/%.c
	gcc -c $^ -o $@ $(CFLAGS) -g

clean:
	@echo "Cleaning..."
	@find $(project_path) -name "*.o" -type f -delete
	rm $(target)

