# Libraries:
#   - curl:   HTTP and HTTPS support.
#   - ssl:    TLS support.
#   - crypto: Cryptographic dependency of SSL.
#   - glfw:   (sudo apt install libglfw-dev)

SOURCE   = src/*.c src/*/*.c src/*.cpp src/*/*.cpp
TARGET   = bin/nprofiler
CC       = g++
CPPFLAGS = -Iinclude -Isrc
CXXFLAGS = -DIMGUI_IMPL_OPENGL_LOADER_GL3W `pkg-config --cflags glfw3` -pthread
LDFLAGS  = -Llib -lcurl -lssl -lcrypto -lGL `pkg-config --static --libs glfw3`

build:
	rm -f $(TARGET)
	$(CC) $(SOURCE) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET)
