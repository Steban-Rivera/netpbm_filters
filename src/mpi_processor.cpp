#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>

struct Kernel {
    int values[9];
    int divisor;
};

Kernel makeKernel(const char* filter) {
    Kernel kernel;

    if (std::strcmp(filter, "blur") == 0) {
        int values[9] = {
            1, 1, 1,
            1, 1, 1,
            1, 1, 1
        };

        for (int i = 0; i < 9; ++i) {
            kernel.values[i] = values[i];
        }

        kernel.divisor = 9;
    }
    else if (std::strcmp(filter, "laplace") == 0) {
        int values[9] = {
             0, -1,  0,
            -1,  4, -1,
             0, -1,  0
        };

        for (int i = 0; i < 9; ++i) {
            kernel.values[i] = values[i];
        }

        kernel.divisor = 1;
    }
    else {
        int values[9] = {
             0, -1,  0,
            -1,  5, -1,
             0, -1,  0
        };

        for (int i = 0; i < 9; ++i) {
            kernel.values[i] = values[i];
        }

        kernel.divisor = 1;
    }

    return kernel;
}

int clampValue(int value) {
    if (value < 0) {
        return 0;
    }

    if (value > 255) {
        return 255;
    }

    return value;
}

class Image {
private:
    char magic[3];
    int width;
    int height;
    int maxColor;
    int channels;
    int* pixels;

public:
    Image()
        : width(0),
          height(0),
          maxColor(255),
          channels(1),
          pixels(nullptr) {
        magic[0] = '\0';
        magic[1] = '\0';
        magic[2] = '\0';
    }

    ~Image() {
        delete[] pixels;
    }

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    void clear() {
        delete[] pixels;
        pixels = nullptr;

        width = 0;
        height = 0;
        maxColor = 255;
        channels = 1;

        magic[0] = '\0';
        magic[1] = '\0';
        magic[2] = '\0';
    }

    bool read(const char* filename) {
        FILE* file = std::fopen(filename, "r");

        if (file == nullptr) {
            return false;
        }

        clear();

        char format[3];

        if (std::fscanf(file, "%2s", format) != 1) {
            std::fclose(file);
            return false;
        }

        if (std::strcmp(format, "P2") == 0) {
            channels = 1;
        }
        else if (std::strcmp(format, "P3") == 0) {
            channels = 3;
        }
        else {
            std::fclose(file);
            return false;
        }

        magic[0] = format[0];
        magic[1] = format[1];
        magic[2] = '\0';

        if (!readNextInteger(file, width) ||
            !readNextInteger(file, height) ||
            !readNextInteger(file, maxColor)) {
            std::fclose(file);
            return false;
        }

        if (width <= 0 || height <= 0 || maxColor <= 0) {
            std::fclose(file);
            return false;
        }

        int totalPixels = width * height * channels;

        pixels = new int[totalPixels];

        for (int i = 0; i < totalPixels; ++i) {
            if (!readNextInteger(file, pixels[i])) {
                delete[] pixels;
                pixels = nullptr;
                std::fclose(file);
                return false;
            }
        }

        std::fclose(file);
        return true;
    }

    bool write(const char* filename) const {
        FILE* file = std::fopen(filename, "w");

        if (file == nullptr) {
            return false;
        }

        std::fprintf(file, "%s\n", magic);
        std::fprintf(file, "%d %d\n", width, height);
        std::fprintf(file, "%d\n", maxColor);

        int totalPixels = width * height * channels;

        for (int i = 0; i < totalPixels; ++i) {
            std::fprintf(file, "%d", pixels[i]);

            if ((i + 1) % channels == 0) {
                std::fprintf(file, "\n");
            }
            else {
                std::fprintf(file, " ");
            }
        }

        std::fclose(file);
        return true;
    }

    int getWidth() const {
        return width;
    }

    int getHeight() const {
        return height;
    }

    int getChannels() const {
        return channels;
    }

    int getMaxColor() const {
        return maxColor;
    }

    const char* getMagic() const {
        return magic;
    }

    void setMagic(const char* format) {
        magic[0] = format[0];
        magic[1] = format[1];
        magic[2] = '\0';
    }

    void setMaxColor(int value) {
        maxColor = value;
    }

    int* getPixels() {
        return pixels;
    }

    const int* getPixels() const {
        return pixels;
    }

    bool allocate(int newWidth, int newHeight, int newChannels) {
        clear();

        width = newWidth;
        height = newHeight;
        channels = newChannels;
        maxColor = 255;

        int totalPixels = width * height * channels;

        pixels = new int[totalPixels];

        return pixels != nullptr;
    }

private:
    static bool readNextInteger(FILE* file, int& value) {
        int character;

        while ((character = std::fgetc(file)) != EOF) {
            if (character == '#') {
                while ((character = std::fgetc(file)) != EOF &&
                       character != '\n') {
                }
            }
            else if (character >= '0' && character <= '9') {
                std::ungetc(character, file);
                break;
            }
        }

        return std::fscanf(file, "%d", &value) == 1;
    }
};

void applyFilterToRegion(
    const int* localPixels,
    int* localResult,
    int localRows,
    int width,
    int channels,
    int globalStartRow,
    int globalHeight,
    const Kernel& kernel
) {
    int totalLocalPixels = localRows * width * channels;

    for (int i = 0; i < totalLocalPixels; ++i) {
        localResult[i] = localPixels[i];
    }

    for (int localY = 1; localY < localRows - 1; ++localY) {
        int globalY = globalStartRow + localY - 1;

        if (globalY <= 0 || globalY >= globalHeight - 1) {
            continue;
        }

        for (int x = 1; x < width - 1; ++x) {
            for (int channel = 0; channel < channels; ++channel) {
                int sum = 0;

                for (int ky = -1; ky <= 1; ++ky) {
                    for (int kx = -1; kx <= 1; ++kx) {
                        int localIndex =
                            ((localY + ky) * width + (x + kx)) * channels
                            + channel;

                        int kernelIndex = (ky + 1) * 3 + (kx + 1);

                        sum += localPixels[localIndex] *
                               kernel.values[kernelIndex];
                    }
                }

                sum /= kernel.divisor;

                int outputIndex =
                    (localY * width + x) * channels + channel;

                localResult[outputIndex] = clampValue(sum);
            }
        }
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank = 0;
    int size = 0;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 5) {
        if (rank == 0) {
            std::cerr
                << "Uso: mpirun -np 4 ./mpi_processor "
                << "<entrada> <salida> --f <blur|laplace|sharpen>\n";
        }

        MPI_Finalize();
        return 1;
    }

    const char* inputFile = argv[1];
    const char* outputFile = argv[2];
    const char* filterName = argv[4];

    if (std::strcmp(argv[3], "--f") != 0) {
        if (rank == 0) {
            std::cerr << "Error: debe utilizar --f para seleccionar el filtro.\n";
        }

        MPI_Finalize();
        return 1;
    }

    if (std::strcmp(filterName, "blur") != 0 &&
        std::strcmp(filterName, "laplace") != 0 &&
        std::strcmp(filterName, "sharpen") != 0) {
        if (rank == 0) {
            std::cerr << "Filtro no valido. Use blur, laplace o sharpen.\n";
        }

        MPI_Finalize();
        return 1;
    }

    Image image;

    int width = 0;
    int height = 0;
    int channels = 0;
    int maxColor = 255;
    char magic[3] = "P2";

    if (rank == 0) {
        if (!image.read(inputFile)) {
            std::cerr << "No se pudo leer la imagen: "
                      << inputFile << "\n";

            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        width = image.getWidth();
        height = image.getHeight();
        channels = image.getChannels();
        maxColor = image.getMaxColor();

        magic[0] = image.getMagic()[0];
        magic[1] = image.getMagic()[1];
        magic[2] = '\0';
    }

    MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&channels, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&maxColor, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(magic, 3, MPI_CHAR, 0, MPI_COMM_WORLD);

    int baseRows = height / size;
    int extraRows = height % size;

    int* coreStart = new int[size];
    int* coreRows = new int[size];
    int* sendCounts = new int[size];
    int* displacements = new int[size];

    int currentRow = 0;

    for (int process = 0; process < size; ++process) {
        coreStart[process] = currentRow;

        coreRows[process] =
            baseRows + (process < extraRows ? 1 : 0);

        sendCounts[process] =
            coreRows[process] * width * channels;

        displacements[process] =
            currentRow * width * channels;

        currentRow += coreRows[process];
    }

    int myStart = coreStart[rank];
    int myRows = coreRows[rank];

    int haloStart = myStart > 0 ? myStart - 1 : myStart;
    int haloEnd = myStart + myRows < height
                    ? myStart + myRows
                    : height - 1;

    int localRows = haloEnd - haloStart + 1;

    int localElements = localRows * width * channels;

    int* localPixels = new int[localElements];
    int* localResult = new int[localElements];

    int* globalPixels = nullptr;
    int* globalResult = nullptr;

    if (rank == 0) {
        globalPixels = image.getPixels();

        globalResult = new int[width * height * channels];

        for (int i = 0; i < width * height * channels; ++i) {
            globalResult[i] = globalPixels[i];
        }
    }

MPI_Request* requests = new MPI_Request[size * 2];
int requestCount = 0;

if (rank == 0) {
    for (int process = 0; process < size; ++process) {
        int processStart = coreStart[process];
        int processRows = coreRows[process];

        int processHaloStart =
            processStart > 0 ? processStart - 1 : processStart;

        int processHaloEnd =
            processStart + processRows < height
                ? processStart + processRows
                : height - 1;

        int processLocalRows =
            processHaloEnd - processHaloStart + 1;

        int processElements =
            processLocalRows * width * channels;

        if (process == 0) {
            std::memcpy(
                localPixels,
                globalPixels +
                    processHaloStart * width * channels,
                processElements * sizeof(int)
            );
        }
        else {
            MPI_Isend(
                globalPixels +
                    processHaloStart * width * channels,
                processElements,
                MPI_INT,
                process,
                100,
                MPI_COMM_WORLD,
                &requests[requestCount++]
            );
        }
    }
}
else {
    MPI_Recv(
        localPixels,
        localElements,
        MPI_INT,
        0,
        100,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE
    );
}

    MPI_Barrier(MPI_COMM_WORLD);

    Kernel kernel = makeKernel(filterName);

    std::clock_t cpuStart = std::clock();
    double wallStart = MPI_Wtime();

    applyFilterToRegion(
        localPixels,
        localResult,
        localRows,
        width,
        channels,
        haloStart,
        height,
        kernel
    );

    double wallEnd = MPI_Wtime();
    std::clock_t cpuEnd = std::clock();

    double cpuTime =
        static_cast<double>(cpuEnd - cpuStart) /
        static_cast<double>(CLOCKS_PER_SEC);

    double wallTime = wallEnd - wallStart;

    if (requestCount > 0) {
        MPI_Waitall(
            requestCount,
            requests,
            MPI_STATUSES_IGNORE
        );
    }

    int myOutputCount =
        myRows * width * channels;

    if (rank == 0) {
        int ownOffset =
            (myStart - haloStart) * width * channels;

        std::memcpy(
            globalResult + myStart * width * channels,
            localResult + ownOffset,
            myOutputCount * sizeof(int)
        );

        for (int process = 1; process < size; ++process) {
            int processStart = coreStart[process];
            int processRows = coreRows[process];

            int processOutputCount =
                processRows * width * channels;

            MPI_Recv(
                globalResult +
                    processStart * width * channels,
                processOutputCount,
                MPI_INT,
                process,
                200,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            );
        }

        image.clear();

        if (!image.allocate(width, height, channels)) {
            std::cerr << "No se pudo reservar memoria para la salida.\n";

            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        image.setMagic(magic);
        image.setMaxColor(maxColor);

        std::memcpy(
            image.getPixels(),
            globalResult,
            width * height * channels * sizeof(int)
        );

        if (!image.write(outputFile)) {
            std::cerr << "No se pudo escribir la imagen: "
                    << outputFile << "\n";

            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }
    }
    else {
        MPI_Send(
            localResult +
                (myStart - haloStart) * width * channels,
            myOutputCount,
            MPI_INT,
            0,
            200,
            MPI_COMM_WORLD
        );
    }

    double* allCpuTimes = nullptr;
    double* allWallTimes = nullptr;

    if (rank == 0) {
        allCpuTimes = new double[size];
        allWallTimes = new double[size];
    }

    MPI_Gather(
        &cpuTime,
        1,
        MPI_DOUBLE,
        allCpuTimes,
        1,
        MPI_DOUBLE,
        0,
        MPI_COMM_WORLD
    );

    MPI_Gather(
        &wallTime,
        1,
        MPI_DOUBLE,
        allWallTimes,
        1,
        MPI_DOUBLE,
        0,
        MPI_COMM_WORLD
    );

    if (rank == 0) {
        std::cout << "\nMPI - filtro: " << filterName << "\n";
        std::cout << "Procesos: " << size << "\n";
        std::cout << "Imagen: " << inputFile << "\n";
        std::cout << "Tamano: " << width << "x" << height << "\n\n";

        for (int process = 0; process < size; ++process) {
            std::cout
                << "Rank " << process
                << " | CPU: " << allCpuTimes[process]
                << " s"
                << " | Ejecucion: " << allWallTimes[process]
                << " s\n";
        }
    }

    delete[] allCpuTimes;
    delete[] allWallTimes;

    delete[] localPixels;
    delete[] localResult;
    delete[] coreStart;
    delete[] coreRows;
    delete[] sendCounts;
    delete[] displacements;
    delete[] requests;
    delete[] globalResult;

    MPI_Finalize();

    return 0;
}