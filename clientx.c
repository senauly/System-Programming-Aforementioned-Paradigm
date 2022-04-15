#include "client.h"

int main(int argc, char const *argv[])
{
    // get arguments
    if (argc != 5)
    {
        fprintf(stderr, "Usage: ./client -s pathToServerFifo -o pathToDataFile");
        exit(EXIT_FAILURE);
    }

    const char *filename = argv[4];
    const char *fifoname = argv[2];
    struct Matrix *matrix = readMatrix(filename);
    // create fifo with pid

    char uniqFifo[100];
    sprintf(uniqFifo, "%s%d", "fifo_p", getpid());

    int fd = mkfifo(uniqFifo, 0666);
    if (fd == -1)
    {
        fprintf(stderr, "Error opening FIFO %s\n", uniqFifo);
        exit(EXIT_FAILURE);
    }

    writeFIFO(fifoname, matrix, getpid());
    freeMatrix(matrix);
    printResponse(uniqFifo);

    return 0;
}

int **createMatrix(int row)
{
    int **matrix = (int **)malloc(row * sizeof(int *));
    for (int i = 0; i < row; i++)
    {
        matrix[i] = (int *)malloc(row * sizeof(int));
    }

    return matrix;
}

struct Matrix *readMatrix(const char *fileName)
{
    FILE *file;
    int count = 0, row = 0;
    int **matrix;
    int lineCap = 100;
    char *line = malloc(sizeof(char) * lineCap);
    struct Matrix *matrixStruct;
    file = fopen(fileName, "r");
    if (file == NULL)
    {
        fprintf(stderr, "Error opening file %s\n", fileName);
        exit(EXIT_FAILURE);
    }
    // read the first line
    char ch = ',';
    while ((ch = fgetc(file)) != '\n' && ch != EOF)
    {
        if (count > 100)
        {
            line = (char *)realloc(line, sizeof(char) * (lineCap*=2));
        }
        line[count] = ch;
        if (ch == ',')
        {
            row++;
        }

        count++;
    }

    row++;

    // create the matrix
    matrix = createMatrix(row);
    if (separeteValues(line, matrix, row, 0) == -1)
    {
        fprintf(stderr, "Matrix must be square.\n");
        exit(EXIT_FAILURE);
    }

    // read the rest of the file
    int i = 1;
    while (i < row)
    {
        count = 0;
        ch = ',';
        
        while ((ch = fgetc(file)) != EOF && ch != '\n')
        {
            if (count > lineCap)
            {
                line = (char *)realloc(line, sizeof(char) * (lineCap*=2));
            }
            line[count] = ch;
            count++;
        }
        line[count] = '\0';
        if (separeteValues(line, matrix, row, i) == -1)
        {
            fprintf(stderr, "Matrix must be square.\n");
            exit(EXIT_FAILURE);
        }
        i++;
    }

    if(fgetc(file) != EOF){
        fprintf(stderr, "Matrix must be square.\n");
        exit(EXIT_FAILURE);
    }

    // create matrix struct
    matrixStruct = (struct Matrix *)malloc(sizeof(struct Matrix));
    matrixStruct->row = row;
    matrixStruct->col = row;
    matrixStruct->matrix = matrix;

    fclose(file);
    free(line);
    return matrixStruct;
}

// to do -1 case
int separeteValues(char *line, int **matrix, int row, int count)
{
    // seperate string by comma
    char *token;
    int i = 0;
    token = strtok(line, ",");
    while (token != NULL)
    {
        if (i >= row)
            return -1;
        matrix[count][i] = atoi(token);
        token = strtok(NULL, ",");
        i++;
    }

    if (i < row)
        return -1;

    return 0;
}

void writeFIFO(const char *fifoName, struct Matrix *m, int client_id)
{
    // open FIFO
    int fd = open(fifoName, O_WRONLY);
    if (fd == -1)
    {
        fprintf(stderr, "Error opening FIFO %s\n", fifoName);
        exit(EXIT_FAILURE);
    }

    // write to FIFO
    int *res = (int *)malloc(sizeof(int) * (m->row * m->col + 3));
    res[2] = client_id;
    res[1] = m->row;
    res[0] = m->col;
    for (int i = 0; i < m->row; i++)
    {
        for (int j = 0; j < m->col; j++)
        {
            res[i * m->col + j + 3] = m->matrix[i][j];
        }
    }

    if (write(fd, res, sizeof(int) * (m->row * m->col + 3)) == -1)
    {
        fprintf(stderr, "Error writing to FIFO %s\n", fifoName);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void freeMatrix(struct Matrix *matrix)
{
    for (int i = 0; i < matrix->row; i++)
    {
        free(matrix->matrix[i]);
    }
    free(matrix->matrix);
    free(matrix);
}

void printResponse(char *uniqFifo)
{
    // open fifo for reading
    int fd = open(uniqFifo, O_RDONLY);
    if (fd == -1)
    {
        fprintf(stderr, "Error opening FIFO %s\n", uniqFifo);
        exit(EXIT_FAILURE);
    }
    // read from fifo
    int res;
    if (read(fd, &res, sizeof(int)) == -1)
    {
        fprintf(stderr, "Error reading from FIFO %s\n", uniqFifo);
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "%d\n", res);
}
