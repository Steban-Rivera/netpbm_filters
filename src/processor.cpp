#include <iostream>
#include <vector>
#include <string>

class Image {
private:
    std::string magic;
    int width;
    int height;
    int maxColor;
    int channels;
    std::vector<int> pixels;

    void skipComments() {
        char c;

        while (std::cin >> std::ws && std::cin.peek() == '#') {
            std::string comment;
            std::getline(std::cin, comment);
        }
    }

    bool readToken(std::string& token) {
        skipComments();

        if (!(std::cin >> token)) {
            return false;
        }

        return true;
    }

public:
    Image()
        : width(0),
          height(0),
          maxColor(0),
          channels(0) {
    }

    bool read() {
        std::string token;

        if (!readToken(magic)) {
            return false;
        }

        if (magic == "P2") {
            channels = 1;
        } else if (magic == "P3") {
            channels = 3;
        } else {
            std::cerr << "Error: formato no soportado. Use P2 o P3." << std::endl;
            return false;
        }

        if (!readToken(token)) {
            return false;
        }
        width = std::stoi(token);

        if (!readToken(token)) {
            return false;
        }
        height = std::stoi(token);

        if (!readToken(token)) {
            return false;
        }
        maxColor = std::stoi(token);

        if (width <= 0 || height <= 0 || maxColor <= 0) {
            std::cerr << "Error: dimensiones o valor maximo invalidos." << std::endl;
            return false;
        }

        const std::size_t pixelCount =
            static_cast<std::size_t>(width) *
            static_cast<std::size_t>(height) *
            static_cast<std::size_t>(channels);

        pixels.resize(pixelCount);

        for (std::size_t i = 0; i < pixelCount; ++i) {
            if (!readToken(token)) {
                std::cerr << "Error: no se pudieron leer todos los pixeles." << std::endl;
                return false;
            }

            pixels[i] = std::stoi(token);

            if (pixels[i] < 0 || pixels[i] > maxColor) {
                std::cerr << "Error: valor de pixel fuera de rango." << std::endl;
                return false;
            }
        }

        return true;
    }

    void write() const {
        std::cout << magic << '\n';
        std::cout << width << ' ' << height << '\n';
        std::cout << maxColor << '\n';

        for (std::size_t i = 0; i < pixels.size(); ++i) {
            std::cout << pixels[i];

            if ((i + 1) % channels == 0) {
                std::cout << '\n';
            } else {
                std::cout << ' ';
            }
        }
    }
};

int main() {
    Image image;

    if (!image.read()) {
        return 1;
    }

    image.write();

    return 0;

    
}