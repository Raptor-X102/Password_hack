# Компилятор
CXX = x86_64-w64-mingw32-g++

# Флаги компиляции
CXXFLAGS = -Dmain=SDL_main -Wall -Wextra -std=c++17

# Флаги линковки
LDFLAGS = -lavcodec -lavformat -lavutil -lswscale -lswresample -lm -lpthread -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -mwindows


# Пути к заголовочным файлам
HEADERS = headers
HEADERS_SDL2 = /mingw64/include/SDL2
HEADERS_FFMPEG = /mingw64/include

# Пути к библиотекам
LIBS_DIR = /mingw64/lib

# Исходные файлы
SRCDIR = src
SRC = $(SRCDIR)/video_main.cpp $(SRCDIR)/Patcher.cpp $(SRCDIR)/GetFileSize2.cpp

# Выходной файл
OUTDIR = build
OUT = $(OUTDIR)/main.exe

# Цель по умолчанию
all: $(OUT)

$(OUT): $(SRC)
	mkdir -p $(OUTDIR)
	$(CXX) -g -o $(OUT) $(SRC) -I$(HEADERS_SDL2) -I$(HEADERS_FFMPEG) -I $(HEADERS) -L$(LIBS_DIR) $(CXXFLAGS) $(LDFLAGS)

# Очистка
clean:
	rm -rf $(OUTDIR)/*.exe $(OUTDIR)/*.o
