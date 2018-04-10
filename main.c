/*
    Program name: Simread
    Brief: Reads an IAR Simple Code (.sim) format file and displays it in a human-readable form.
    Usage: simread [FILE] [OPTION]
    Author: Mikael Stewart
    Date: 10 April 2018
    Version: 1.0
    Repository: https://github.com/mmmstew/simread

    References:
        http://netstorage.iar.com/SuppDB/Public/UPDINFO/006220/simple_code.htm
*/

#include <stdio.h>
#include <stdlib.h>

#define HEADER_SIZE_BYTES 14

#define DATA_RECORD_TAG 0x01
#define ENTRY_RECORD_TAG 0x02
#define END_RECORD_TAG 0x03

#define MAX_FILE_SIZE_BYTES 1000000       //1MB

unsigned char hide_program_bytes;

int DisplayFileSize(FILE* fp)
{
    printf("\r\n");

    fseek(fp, 0, SEEK_END);

    int size = (int)ftell(fp);

    if (size >= MAX_FILE_SIZE_BYTES)
    {
        printf("File size too large (%i > %i)", size, MAX_FILE_SIZE_BYTES);
        return 1;
    }

    printf("File size = %i\r\n", size);
    return 0;
}

int DisplayHeader(FILE* fp)
{
    unsigned char bf[30];
    size_t rb;

    unsigned int magic_number;
    unsigned int program_flags;
    unsigned int number_of_program_bytes;
    unsigned int version_information;

    printf("\r\n");

    fseek(fp, 0, SEEK_SET);
    rb = fread(bf, 1, HEADER_SIZE_BYTES, fp);

    if (rb != HEADER_SIZE_BYTES)
    {
        printf("Could not read header.");
        return 1;
    }

    magic_number = bf[0]<<24 | bf[1]<<16 | bf[2]<<8 | bf[3]<<0;
    program_flags = bf[4]<<24 | bf[5]<<16 | bf[6]<<8 | bf[7]<<0;
    number_of_program_bytes = bf[8]<<24 | bf[9]<<16 | bf[10]<<8 | bf[11]<<0;
    version_information =  bf[12]<<8 | bf[13]<<0;

    printf("Header\r\n");
    printf("Magic number = 0x%08x\r\n", magic_number);
    printf("Program flags = 0x%08x\r\n", program_flags);
    printf("Number of Program Bytes = %u\r\n", number_of_program_bytes);
    printf("Version Information = 0x%04x\r\n", version_information);

    return 0;
}

unsigned char * DisplayRecord(unsigned char * record)
{
    int unsigned i;

    printf("\r\n");

    unsigned int record_tag = record[0];

    if (record_tag == DATA_RECORD_TAG)
    {
        unsigned int segment_type;
        unsigned int record_flags;
        unsigned int record_start_address;
        unsigned int number_of_program_bytes;

        segment_type = record[1];
        record_flags = record[2]<<8 | record[3]<<0;
        record_start_address = record[4]<<24 | record[5]<<16 | record[6]<<8 | record[7]<<0;
        number_of_program_bytes = record[8]<<24 | record[9]<<16 | record[10]<<8 | record[11]<<0;

        printf("Data record\r\n");
        printf("Segment type = 0x%02x\r\n", segment_type);
        printf("Record flags = 0x%04x\r\n", record_flags);
        printf("Record start address = 0x%08x\r\n", record_start_address);
        printf("Number of program bytes = %u\r\n", number_of_program_bytes);
        if (hide_program_bytes)
        {
            printf("[Program bytes hidden]\r\n");
        }
        else
        {
            printf("Program bytes = ");
            for(i=12; i<(12+number_of_program_bytes); i++)
            {
                printf("0x%02x ", record[i]);
            }
            printf("\r\n");
        }

        return record+12+number_of_program_bytes; // start of next record
    }
    else if (record_tag == ENTRY_RECORD_TAG)
    {
        // *** entry record untested as of 10/April/2018 ***

        unsigned int entry_record;
        unsigned int segment_type;

        entry_record = record[1]<<24 | record[2]<<16 | record[3]<<8 | record[4]<<0;
        segment_type = record[5];

        printf("Entry record\r\n");
        printf("Entry address = 0x%08x\r\n", entry_record);
        printf("Segment type = 0x%02x\r\n", segment_type);

        return record+6; // start of next record
    }
    else if (record_tag == END_RECORD_TAG)
    {
        unsigned int checksum;

        checksum = record[1]<<24 | record[2]<<16 | record[3]<<8 | record[4]<<0;

        printf("End record\r\n");
        printf("Checksum = 0x%08x\r\n",checksum);

        return 0;   // no more records
    }

    return 0;
}

int DisplayRecords(FILE* fp)
{
    unsigned char bf[MAX_FILE_SIZE_BYTES - HEADER_SIZE_BYTES];
    size_t rb;
    unsigned char * nextRecord;

    fseek(fp, HEADER_SIZE_BYTES, SEEK_SET);    // after header

    rb = fread(bf, sizeof(char), sizeof(bf)/sizeof(char), fp);

    // print records
    if (rb)     // should check for minimum size
    {
        nextRecord = bf;
        while(nextRecord)
        {
            nextRecord = DisplayRecord(nextRecord);
        }
        return 0;
    }

    return 1;
}

int DisplayCalculatedChecksum(FILE* fp)
{
    //unsigned char bf[4];
    unsigned int i;
    unsigned int checksum = 0;  // 4 byte quantity
    unsigned char fileByte;

    printf("\r\n----\r\n");

    fseek(fp, 0, SEEK_END);
    unsigned int crcPos = (unsigned int)ftell(fp) - 4;     // crc position 4 bytes from the end of the file

    fseek(fp, 0, SEEK_SET);
    for (i=0;i<crcPos;i++)
    {
        if (fread(&fileByte, 1, 1, fp) == 0)
        {
            printf("Could not calculate checksum.\r\n");
            return 1;
        }
        checksum += fileByte;
    }
    checksum = ~checksum +1;

    printf("Calculated checksum = 0x%08x\r\n", checksum);
    return 0;
}

int main (int argc, char *argv[])
{
    hide_program_bytes = 0;

    FILE * fp = NULL;
    if (argc != 2 && argc != 3)
    {
        goto paramError;
    }

    if (argc == 3)
    {
        if (argv[2][0] == '-')
        {
            if (argv[2][1] == 'h')
            {
                hide_program_bytes = 1;
            }
            else
            {
                goto paramError;
            }
        }
        else
        {
            goto paramError;
        }
    }

    fp = fopen(argv[1], "rb");
    if (fp == NULL)
    {
        printf("Could not open file.\r\n");
        return 1;
    }

    if (DisplayFileSize(fp))
        goto close;
    if (DisplayHeader(fp))
        goto close;
    if (DisplayRecords(fp))
        goto close;
    if (DisplayCalculatedChecksum(fp))
        goto close;

close:
    fclose(fp);
    return 0;


paramError:
    printf("Usage: simread [FILE] [OPTION]\r\n");
    printf("Options:\r\n");
    printf("  -h                        hide program bytes.\r\n");
    return 1;
}


