#include <stdio.h>
#include <string.h>

// card parameters (first block data)
typedef struct
{
   unsigned int id;    // card id (must be BU_ID)
   unsigned int size;  // card size in 128 blocks
   unsigned int serial;// card serial number
   unsigned int number;// card number for joined cards
} bu_first_block_t;

// Define the block structure
struct block {
    char data[8];
};


int main() {
    // Create a file and open for writing
    FILE *file = fopen("output.mcd", "wb");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }
    // Prepare the header
    bu_first_block_t header = {
        0x1234,       // id
        0x00020000,   // size
        0x00010000,   // serial
        0x01000000    // number
    };


    // Write the header to the file
    fwrite(&header, sizeof(header), 1, file);

    // Fill the first block with the specified pattern
    struct block pattern_block;
    memset(pattern_block.data, 0xFF, sizeof(pattern_block.data));
    fwrite(&pattern_block, sizeof(pattern_block), 1, file);

    // Calculate how many bytes are needed to reach 128 KB after the header
    int remaining_bytes = 128 * 1024 - sizeof(header) - sizeof(pattern_block);

    // Fill the remaining space with zeros
    char zero_block[remaining_bytes];
    memset(zero_block, 0, sizeof(zero_block));
    fwrite(zero_block, sizeof(zero_block), 1, file);

    // Close the file
    fclose(file);

    return 0;
}
// PSX joined card: Bad card sequence - found 50331648 instead 0