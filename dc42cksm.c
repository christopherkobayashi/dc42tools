/* dc42cksm

   A command-line program to view the checksums in a Macintosh Disk Copy 4.2
   disk image file, and optionally to update the checksums if they’re not
   correct.

   Originally written by Steve Chamberlin <steve@bigmessowires.com>
   64-bit-cleaned and lightly converted from C++ to C by Christopher RYU
*/

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/types.h>

u_int8_t Read8(FILE* file)
{
    u_int8_t b;
    fread(&b, 1, 1, file);
    return b;
}

u_int16_t ReadBigEndian16(FILE* file)
{
    u_int8_t b[2];
    fread(&b, 1, 2, file);
    return (((u_int16_t)b[0]) << 8) | b[1];
}

u_int32_t ReadBigEndian32(FILE* file)
{
    u_int8_t b[4];  
    fread(&b, 1, 4, file);
    return (((u_int32_t)b[0]) << 24) | (((u_int32_t)b[1]) << 16) | (((u_int32_t)b[2]) << 8) | b[3];
}

void SetBigEndian32(u_int8_t* pDest, u_int32_t value)
{
    pDest[0] = (value >> 24) & 0xFF;
    pDest[1] = (value >> 16) & 0xFF;
    pDest[2] = (value >> 8) & 0xFF;
    pDest[3] = value & 0xFF;
}

int main(int argc, char* argv[])
{
    /* parse the command line */
    if (argc != 2)
    {
        printf("Disk Copy 4.2 checksum tool:\nVerifies and optionally updates checksums for DC42 disk image files.\n\n");
        printf("Usage: %s filename\n", argv[0]);
        return -1;
    }

    char *sourceFilename = argv[1];

    printf("Verifying Disk Copy 4.2 disk image checksums for %s\n", sourceFilename);
	
    FILE *pFile = fopen(sourceFilename, "rb");
    if (!pFile)
    {
        printf("Could not open source file %s\n", sourceFilename);
        return -1;
    }

    /* check the DC42 signature bytes */
    fseek(pFile, 0x52, SEEK_SET);
    u_int16_t magic0 = Read8(pFile);
    u_int16_t magic1 = Read8(pFile);

    if (magic0 != 1 || magic1 != 0)
    {
        printf("This doesn't look like a valid DC42 disk image file. Aborting.\n");
        fclose(pFile);
        return 0;
    }

    /* return to start */
    fseek(pFile, 0, SEEK_SET);

    /* disk image name */
    u_int8_t nameLen = Read8(pFile);
    unsigned char name[64];
    fread(name, 1, 63, pFile);
    if (nameLen < 64)
        name[nameLen] = 0;
    else
        name[63] = 0;
    printf("     stored image name: %s\n", name);

    /* size of data and tag regions */
    u_int32_t dataSize = ReadBigEndian32(pFile);
    printf("             data size: %08X\n", dataSize);
    u_int32_t tagSize = ReadBigEndian32(pFile);
    printf("              tag size: %08X\n", tagSize);

    /* stored checksums */
    u_int32_t storedDataChecksum = ReadBigEndian32(pFile);
    u_int32_t storedTagChecksum = ReadBigEndian32(pFile);

    /* disk encoding */
    u_int8_t encoding = Read8(pFile);
    switch (encoding)
    {
    case 0:
        printf("              encoding: 00, GCR single-sided double density 400K\n");
        break;
    case 1:
        printf("              encoding: 01, GCR double-sided double density 800K\n");
        break;
    case 2:
        printf("              encoding: 02, MFM double-sided double density 720K\n");
        break;
    case 3:
        printf("              encoding: 03, MFM double-sided high density 1440K\n");
        break;
    default:
        printf("              encoding: %02X, unknown\n", encoding);
        break;
    }

    /* disk format */
    u_int8_t format = Read8(pFile);
    switch (format)
    {
    case 0x02:
        printf("                format: 02, Macintosh/Lisa 400K\n");
        break;
    case 0x22:
        printf("                format: 22, Macintosh 800K\n");
        break;
    case 0x24:
        printf("                format: 24, ProDOS 800K\n");
        break;
    default:
        printf("                format: %02X, unknown\n", format);
        break;
    }

    /* ignore the magic number */
    ReadBigEndian16(pFile);

    /* compute the data checksum */
    u_int32_t dataChecksum = 0;
    for (u_int32_t i=0; i<dataSize; i+=2)
    {
        dataChecksum += ReadBigEndian16(pFile);
        dataChecksum = (dataChecksum >> 1) | ((dataChecksum & 1) << 31);
    }

    /* compute the tag checksum */
    u_int32_t tagChecksum = 0;
    if (tagSize > 12)
    {
        /* skip the first 12 tag bytes */
        for (int i=0; i<12; i++)
        {
            Read8(pFile);
        }

        for (u_int32_t i=12; i<tagSize; i+=2)
        {
            tagChecksum += ReadBigEndian16(pFile);
            tagChecksum = (tagChecksum >> 1) | ((tagChecksum & 1) << 31);
        }
    }

    /* show the results */
    printf("  stored data checksum: %08X\n", storedDataChecksum);
    printf("computed data checksum: %08X\n", dataChecksum);
    printf("   stored tag checksum: %08X\n", storedTagChecksum);
    printf(" computed tag checksum: %08X\n", tagChecksum);

    if (storedDataChecksum == dataChecksum && storedTagChecksum == tagChecksum)
    {
        printf("\nVerification succeeded, no errors.\n");
    }
    else
    {
        printf("\nThe stored checksums do not match the computed checksums.\n");
        printf("\nUpdate the stored checksums? (Y/N) ");
        u_int8_t a = getchar();
        if (a == 'y' || a =='Y')
        {
            /* read the whole file into memory */
            u_int32_t bufSize = 0x54 + dataSize + tagSize;
            u_int8_t* pBuffer = (u_int8_t*)malloc(bufSize);
            if (pBuffer == 0)
            {
                printf("error: couldn't allocate %u bytes\n", bufSize);
                return -1;
            }
            fseek(pFile, 0, SEEK_SET);
            fread(pBuffer, bufSize, 1, pFile);
            fclose(pFile);

            /* modify the checksums */
            SetBigEndian32(&pBuffer[0x48], dataChecksum);
            SetBigEndian32(&pBuffer[0x4C], tagChecksum);

            /* write the modified file */
            FILE *pFile = fopen(sourceFilename, "wb");
            if (!pFile)
            {
                printf("error: could not re-open file for writing\n");
                return -1;
            }
            fwrite(pBuffer, bufSize, 1, pFile);
   
            printf("%s was modified, checksums updated.\n", sourceFilename);
        }
        else
        {
            printf("%s was not modified.\n", sourceFilename);
        }
    }

    fclose(pFile);
    return 0;
}
