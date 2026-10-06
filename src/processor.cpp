#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <thread>
#include <functional>

struct Kernel {
    int values[3][3];
    int divisor;
};

static Kernel makeKernel(const std::string& filter) {
    if (filter == "blur") {
        return Kernel{{{1, 1, 1},
                       {1, 1, 1},
                       {1, 1, 1}}, 9};
    }

    if (filter == "laplace") {
        return Kernel{{{-1, -1, -1},
                       {-1,  8, -1},
                       {-1, -1, -1}}, 1};
    }

    return Kernel{{{ 0, -1,  0},
                   {-1,  5, -1},
                   { 0, -1,  0}}, 1};
}

class Image {
private:
    std::string magic;
    int width;
    int height;
    int maxColor;
    int channels;
    std::vector<int> pixels;

    void skipComments(std::istream& input) {
        while (input >> std::ws && input.peek() == '#') {
            std::string comment;
            std::getline(input, comment);
        }
    }

    bool readToken(std::istream& input, std::string& token) {
        skipComments(input);
        return static_cast<bool>(input >> token);
    }

    void processRegion(
        Image& result,
        int yStart,
        int yEnd,
        int xStart,
        int xEnd,
        const Kernel& kernel
    ) const {
        for (int y = yStart; y < yEnd; ++y) {
            for (int x = xStart; x < xEnd; ++x) {
                for (int channel = 0; channel < channels; ++channel) {

                    int sum = 0;

                    for (int ky = -1; ky <= 1; ++ky) {
                        for (int kx = -1; kx <= 1; ++kx) {

                            std::size_t index =
                                (static_cast<std::size_t>(y + ky) * width +
                                 (x + kx)) * channels + channel;

                            sum += pixels[index] *
                                   kernel.values[ky + 1][kx + 1];
                        }
                    }

                    sum /= kernel.divisor;

                    sum = std::max(0, std::min(maxColor, sum));

                    std::size_t index =
                        (static_cast<std::size_t>(y) * width + x) *
                        channels + channel;

                    result.pixels[index] = sum;
                }
            }
        }
    }

public:
    Image()
        : width(0), height(0), maxColor(0), channels(0) {}

    bool read(std::istream& input) {
        std::string token;

        if (!readToken(input, magic)) {
            return false;
        }

        if (magic == "P2") {
            channels = 1;
        } else if (magic == "P3") {
            channels = 3;
        } else {
            std::cerr
                << "Error: formato no soportado. Use P2 o P3."
                << std::endl;
            return false;
        }

        if (!readToken(input, token)) {
            return false;
        }
        width = std::stoi(token);

        if (!readToken(input, token)) {
            return false;
        }
        height = std::stoi(token);

        if (!readToken(input, token)) {
            return false;
        }
        maxColor = std::stoi(token);

        if (width <= 0 || height <= 0 || maxColor <= 0) {
            std::cerr
                << "Error: dimensiones o valor maximo invalidos."
                << std::endl;
            return false;
        }

        std::size_t pixelCount =
            static_cast<std::size_t>(width) *
            static_cast<std::size_t>(height) *
            static_cast<std::size_t>(channels);

        pixels.resize(pixelCount);

        for (std::size_t i = 0; i < pixelCount; ++i) {
            if (!readToken(input, token)) {
                std::cerr
                    << "Error: no se pudieron leer todos los pixeles."
                    << std::endl;
                return false;
            }

            pixels[i] = std::stoi(token);

            if (pixels[i] < 0 || pixels[i] > maxColor) {
                std::cerr
                    << "Error: valor de pixel fuera de rango."
                    << std::endl;
                return false;
            }
        }

        return true;
    }

    void write(std::ostream& output) const {
        output << magic << '\n';
        output << width << ' ' << height << '\n';
        output << maxColor << '\n';

        for (std::size_t i = 0; i < pixels.size(); ++i) {
            output << pixels[i];

            if ((i + 1) % channels == 0) {
                output << '\n';
            } else {
                output << ' ';
            }
        }
    }

    Image applyFilterParallel(const std::string& filter) const {
        Image result = *this;

        const Kernel kernel = makeKernel(filter);

        int middleX = width / 2;
        int middleY = height / 2;

        std::thread topLeft(
            &Image::processRegion,
            this,
            std::ref(result),
            1,
            middleY,
            1,
            middleX,
            std::cref(kernel)
        );

        std::thread topRight(
            &Image::processRegion,
            this,
            std::ref(result),
            1,
            middleY,
            middleX,
            width - 1,
            std::cref(kernel)
        );

        std::thread bottomLeft(
            &Image::processRegion,
            this,
            std::ref(result),
            middleY,
            height - 1,
            1,
            middleX,
            std::cref(kernel)
        );

        std::thread bottomRight(
            &Image::processRegion,
            this,
            std::ref(result),
            middleY,
            height - 1,
            middleX,
            width - 1,
            std::cref(kernel)
        );

        topLeft.join();
        topRight.join();
        bottomLeft.join();
        bottomRight.join();

        return result;
    }
};

int main(int argc, char* argv[]) {

    if (argc != 5) {
        std::cerr
            << "Uso: " << argv[0]
            << " input_image output_image --f filtro"
            << std::endl;

        std::cerr
            << "Filtros disponibles: blur, laplace, sharpen"
            << std::endl;

        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    std::string filterOption = argv[3];
    std::string filter = argv[4];

    if (filterOption != "--f") {
        std::cerr
            << "Error: debe utilizar --f para seleccionar el filtro."
            << std::endl;

        return 1;
    }

    if (filter != "blur" &&
        filter != "laplace" &&
        filter != "sharpen") {

        std::cerr
            << "Error: filtro no reconocido: "
            << filter
            << std::endl;

        std::cerr
            << "Filtros disponibles: blur, laplace, sharpen"
            << std::endl;

        return 1;
    }

    std::ifstream inputFile(inputPath);

    if (!inputFile) {
        std::cerr
            << "Error: no se pudo abrir la imagen de entrada: "
            << inputPath
            << std::endl;

        return 1;
    }

    Image image;

    try {
        if (!image.read(inputFile)) {
            std::cerr
                << "Error: no se pudo leer la imagen."
                << std::endl;

            return 1;
        }
    } catch (const std::exception&) {
        std::cerr
            << "Error: archivo de imagen con formato invalido."
            << std::endl;

        return 1;
    }

    inputFile.close();

    std::clock_t cpuStart = std::clock();
    auto wallStart = std::chrono::steady_clock::now();

    Image result = image.applyFilterParallel(filter);

    auto wallEnd = std::chrono::steady_clock::now();
    std::clock_t cpuEnd = std::clock();

    std::ofstream outputFile(outputPath);

    if (!outputFile) {
        std::cerr
            << "Error: no se pudo crear la imagen de salida: "
            << outputPath
            << std::endl;

        return 1;
    }

    result.write(outputFile);
    outputFile.close();

    double cpuTime =
        static_cast<double>(cpuEnd - cpuStart) /
        CLOCKS_PER_SEC;

    double totalTime =
        std::chrono::duration<double>(
            wallEnd - wallStart
        ).count();

    std::cerr
        << "Filtro: "
        << filter
        << std::endl;

    std::cerr
        << "Tiempo CPU: "
        << cpuTime
        << " segundos"
        << std::endl;

    std::cerr
        << "Tiempo total de filtrado: "
        << totalTime
        << " segundos"
        << std::endl;

    return 0;
}